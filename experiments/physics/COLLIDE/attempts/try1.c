#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* One-dimensional collision simulator for TI-84 Plus CE */
/* Up/Down: left cart mass (1-10 kg) */
/* Left/Right: right cart mass (1-10 kg) */
/* Enter: run collision */
/* Add: toggle elastic / inelastic */
/* Clear: quit */

/* Track geometry */
#define TRACK_Y       140
#define TRACK_H       30
#define CART_H        24
#define CART_W        30
#define TRACK_LEFT    20
#define TRACK_RIGHT   300
#define TRACK_WIDTH   (TRACK_RIGHT - TRACK_LEFT)

/* Physics constants */
#define V_LEFT_INIT   5.0f   /* m/s, left cart initial velocity */
#define V_RIGHT_INIT  0.0f   /* m/s, right cart initial velocity */
#define DT            0.016f /* animation time step (~60 fps) */
#define SCALE         40.0f  /* pixels per meter */

/* Colors */
#define COL_BG        0x00
#define COL_TRACK     0x10
#define COL_CART_L    0xE0
#define COL_CART_R    0x03
#define COL_TEXT      0xFF
#define COL_HL        0x10

/* State */
typedef enum {
    STATE_SETUP,
    STATE_SIMULATING,
    STATE_DONE
} sim_state_t;

static sim_state_t state = STATE_SETUP;
static int mass_left = 2;    /* kg */
static int mass_right = 3;   /* kg */
static int elastic = 1;      /* 1 = elastic, 0 = inelastic */

/* Cart positions in meters (0 = left end of track) */
static float pos_left = 0.0f;
static float pos_right = 0.0f;

/* Velocities in m/s */
static float vel_left = V_LEFT_INIT;
static float vel_right = V_RIGHT_INIT;

/* Pre-collision values */
static float p_before = 0.0f;
static float ke_before = 0.0f;

/* Post-collision values */
static float p_after = 0.0f;
static float ke_after = 0.0f;

/* Edge-detect flags */
static int was_enter = 0;
static int was_add = 0;

/* Convert meters to pixel x on screen */
static int m_to_px(float m) {
    return TRACK_LEFT + (int)(m * SCALE);
}

/* Compute momentum (kg*m/s) */
static float total_momentum(float ml, float vr, float vl, float vr2) {
    return ml * vl + vr * vr2;
}

/* Compute kinetic energy (Joules) */
static float total_ke(float ml, float vl, float mr, float vr) {
    return 0.5f * ml * vl * vl + 0.5f * mr * vr * vr;
}

/* Print an integer value (value * mult) at x,y with label */
static void print_val(const char *label, float value, int mult, int x, int y) {
    int iv = (int)(value * mult + 0.5f);
    char buf[16];
    buf[0] = '\0';
    /* Simple integer to string */
    int neg = 0;
    if (iv < 0) { neg = 1; iv = -iv; }
    int len = 0;
    if (iv == 0) {
        buf[len++] = '0';
    } else {
        char tmp[12];
        int tlen = 0;
        while (iv > 0) {
            tmp[tlen++] = (char)('0' + iv % 10);
            iv /= 10;
        }
        for (int i = tlen - 1; i >= 0; i--) {
            buf[len++] = tmp[i];
        }
    }
    buf[len] = '\0';
    gfx_PrintStringXY(label, x, y);
    gfx_PrintStringXY(buf, x + 60, y);
}

/* Draw the track and carts */
static void draw_scene(void) {
    /* Track */
    gfx_SetColor(COL_TRACK);
    gfx_FillRectangle(TRACK_LEFT, TRACK_Y, TRACK_WIDTH, TRACK_H);

    /* Cart positions in pixels */
    int px_l = m_to_px(pos_left);
    int px_r = m_to_px(pos_right);

    /* Left cart */
    gfx_SetColor(COL_CART_L);
    gfx_FillRectangle(px_l, TRACK_Y + (TRACK_H - CART_H) / 2, CART_W, CART_H);

    /* Right cart */
    gfx_SetColor(COL_CART_R);
    gfx_FillRectangle(px_r, TRACK_Y + (TRACK_H - CART_H) / 2, CART_W, CART_H);

    /* Mass labels on carts */
    gfx_SetTextFGColor(COL_BG);
    char buf[8];
    buf[0] = '\0';
    {
        int v = mass_left;
        int len = 0;
        if (v == 0) buf[len++] = '0';
        else {
            char tmp[4];
            int tl = 0;
            while (v > 0) { tmp[tl++] = (char)('0' + v % 10); v /= 10; }
            for (int i = tl - 1; i >= 0; i--) buf[len++] = tmp[i];
        }
        buf[len] = '\0';
    }
    gfx_PrintStringXY(buf, px_l + 10, TRACK_Y + 4);

    buf[0] = '\0';
    {
        int v = mass_right;
        int len = 0;
        if (v == 0) buf[len++] = '0';
        else {
            char tmp[4];
            int tl = 0;
            while (v > 0) { tmp[tl++] = (char)('0' + v % 10); v /= 10; }
            for (int i = tl - 1; i >= 0; i--) buf[len++] = tmp[i];
        }
        buf[len] = '\0';
    }
    gfx_PrintStringXY(buf, px_r + 10, TRACK_Y + 4);
}

/* Draw info panel */
static void draw_info(void) {
    gfx_SetTextFGColor(COL_TEXT);

    /* Title */
    gfx_PrintStringXY("1D COLLISION", 100, 5);

    /* Masses */
    gfx_PrintStringXY("mL=", 5, 30);
    {
        char buf[4];
        int v = mass_left, len = 0;
        if (v == 0) buf[len++] = '0';
        else { char t[3]; int tl=0; while(v>0){t[tl++]=(char)('0'+v%10);v/=10;} for(int i=tl-1;i>=0;i--)buf[len++]=t[i]; }
        buf[len] = '\0';
        gfx_PrintStringXY(buf, 30, 30);
    }
    gfx_PrintStringXY("kg", 50, 30);

    gfx_PrintStringXY("mR=", 5, 45);
    {
        char buf[4];
        int v = mass_right, len = 0;
        if (v == 0) buf[len++] = '0';
        else { char t[3]; int tl=0; while(v>0){t[tl++]=(char)('0'+v%10);v/=10;} for(int i=tl-1;i>=0;i--)buf[len++]=t[i]; }
        buf[len] = '\0';
        gfx_PrintStringXY(buf, 30, 45);
    }
    gfx_PrintStringXY("kg", 50, 45);

    /* Collision type */
    gfx_PrintStringXY(elastic ? "ELASTIC" : "INELASTIC", 120, 30);

    /* Velocities */
    gfx_PrintStringXY("vL=", 5, 60);
    {
        int iv = (int)(vel_left * 10 + (vel_left >= 0 ? 0.5f : -0.5f));
        char buf[6];
        int neg = 0;
        if (iv < 0) { neg = 1; iv = -iv; }
        int len = 0;
        if (neg) buf[len++] = '-';
        if (iv == 0) buf[len++] = '0';
        else { char t[4]; int tl=0; while(iv>0){t[tl++]=(char)('0'+iv%10);iv/=10;} for(int i=tl-1;i>=0;i--)buf[len++]=t[i]; }
        buf[len] = '\0';
        gfx_PrintStringXY(buf, 25, 60);
    }
    gfx_PrintStringXY("m/s", 55, 60);

    gfx_PrintStringXY("vR=", 5, 75);
    {
        int iv = (int)(vel_right * 10 + (vel_right >= 0 ? 0.5f : -0.5f));
        char buf[6];
        int neg = 0;
        if (iv < 0) { neg = 1; iv = -iv; }
        int len = 0;
        if (neg) buf[len++] = '-';
        if (iv == 0) buf[len++] = '0';
        else { char t[4]; int tl=0; while(iv>0){t[tl++]=(char)('0'+iv%10);iv/=10;} for(int i=tl-1;i>=0;i--)buf[len++]=t[i]; }
        buf[len] = '\0';
        gfx_PrintStringXY(buf, 25, 75);
    }
    gfx_PrintStringXY("m/s", 55, 75);

    /* Momentum and KE */
    gfx_SetTextFGColor(COL_HL);
    gfx_PrintStringXY("BEFORE:", 5, 100);
    gfx_SetTextFGColor(COL_TEXT);
    print_val("p=", p_before, 10, 5, 115);
    gfx_PrintStringXY("kg*m/s", 80, 115);
    print_val("KE=", ke_before, 10, 5, 130);
    gfx_PrintStringXY("J", 80, 130);

    if (state == STATE_DONE || state == STATE_SIMULATING) {
        gfx_SetTextFGColor(COL_HL);
        gfx_PrintStringXY("AFTER:", 5, 155);
        gfx_SetTextFGColor(COL_TEXT);
        print_val("p=", p_after, 10, 5, 170);
        gfx_PrintStringXY("kg*m/s", 80, 170);
        print_val("KE=", ke_after, 10, 5, 185);
        gfx_PrintStringXY("J", 80, 185);
    }

    /* Help line */
    gfx_SetTextFGColor(COL_HL);
    gfx_PrintStringXY("U/D:mL L/R:mR Ent:run +:type Clr:quit", 20, 225);
}

/* Initialize simulation */
static void init_sim(void) {
    pos_left = 0.0f;
    pos_right = 6.0f;  /* 6 meters from left end */
    vel_left = V_LEFT_INIT;
    vel_right = V_RIGHT_INIT;

    p_before = total_momentum((float)mass_left, (float)mass_right, vel_left, vel_right);
    ke_before = total_ke((float)mass_left, vel_left, (float)mass_right, vel_right);
    p_after = 0.0f;
    ke_after = 0.0f;
}

/* Perform collision calculation */
static void do_collision(void) {
    float ml = (float)mass_left;
    float mr = (float)mass_right;
    float v1 = vel_left;
    float v2 = vel_right;

    if (elastic) {
        /* Elastic collision formulas */
        float v1_new = ((ml - mr) * v1 + 2.0f * mr * v2) / (ml + mr);
        float v2_new = (2.0f * ml * v1 + (mr - ml) * v2) / (ml + mr);
        vel_left = v1_new;
        vel_right = v2_new;
    } else {
        /* Perfectly inelastic: carts stick together */
        float v_common = (ml * v1 + mr * v2) / (ml + mr);
        vel_left = v_common;
        vel_right = v_common;
    }

    p_after = total_momentum(ml, mr, vel_left, vel_right);
    ke_after = total_ke(ml, vel_left, mr, vel_right);
}

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();

    init_sim();

    for (;;) {
        kb_Scan();

        /* Check for Clear (quit) */
        if (kb_Data[6] & kb_Clear) break;

        /* Edge-detect Enter */
        int enter_now = (kb_Data[6] & kb_Enter) ? 1 : 0;
        int add_now = (kb_Data[6] & kb_Add) ? 1 : 0;

        /* Handle state transitions */
        if (state == STATE_SETUP) {
            /* Adjust masses */
            if (kb_Data[7] & kb_Up) {
                if (mass_left < 10) mass_left++;
            }
            if (kb_Data[7] & kb_Down) {
                if (mass_left > 1) mass_left--;
            }
            if (kb_Data[7] & kb_Right) {
                if (mass_right < 10) mass_right++;
            }
            if (kb_Data[7] & kb_Left) {
                if (mass_right > 1) mass_right--;
            }

            /* Toggle elastic/inelastic (edge-detected) */
            if (add_now && !was_add) {
                elastic = !elastic;
            }

            /* Start simulation (edge-detected) */
            if (enter_now && !was_enter) {
                init_sim();
                state = STATE_SIMULATING;
            }
        } else if (state == STATE_SIMULATING) {
            /* Move carts */
            pos_left += vel_left * DT;
            pos_right += vel_right * DT;

            /* Check for collision: left cart's right edge meets right cart's left edge */
            float left_right_edge = pos_left + CART_W / SCALE;
            float right_left_edge = pos_right;

            if (left_right_edge >= right_left_edge) {
                /* Collision detected */
                do_collision();
                state = STATE_DONE;
            }

            /* Check if carts have left the track */
            if (pos_left > 8.0f || pos_right > 8.0f || pos_left < -1.0f || pos_right < -1.0f) {
                state = STATE_DONE;
            }
        } else if (state == STATE_DONE) {
            /* Wait for Enter to reset */
            if (enter_now && !was_enter) {
                state = STATE_SETUP;
                init_sim();
            }
        }

        /* Update edge-detect flags */
        was_enter = enter_now;
        was_add = add_now;

        /* Draw */
        gfx_FillScreen(COL_BG);
        draw_scene();
        draw_info();
        gfx_SwapDraw();

        delay(15);
    }

    gfx_End();
    return 0;
}
