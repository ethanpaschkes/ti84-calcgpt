#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* ---- constants ---- */
#define N_PARTICLES 30
#define BOX_TOP     30
#define BOX_BOTTOM  210
#define BOX_LEFT    20
#define PISTON_MIN  120
#define PISTON_MAX  300
#define PISTON_W    8
#define MAX_SPEED   3.0f
#define MIN_SPEED   0.1f
#define TEMP_SCALE  1.1f
#define PRESSURE_WINDOW 60   /* ~1 s at 60 fps */

/* ---- particle ---- */
typedef struct {
    float x, y;
    float vx, vy;
} particle_t;

/* ---- globals (static, small) ---- */
static particle_t particles[N_PARTICLES];
static uint8_t  piston_x = 220;
static uint8_t  was_left  = 0, was_right = 0;
static uint8_t  was_up    = 0, was_down  = 0;
static uint8_t  was_enter = 0;

/* pressure measurement: ring buffer of per-frame momentum sums */
static float mom_buf[PRESSURE_WINDOW];
static uint8_t mom_idx = 0;
static uint8_t mom_count = 0;

/* ---- helpers ---- */

static void init_particles(void)
{
    uint8_t i;
    for (i = 0; i < N_PARTICLES; i++) {
        /* random position inside the box */
        particles[i].x = (float)(BOX_LEFT + 5 + (rand() % (piston_x - BOX_LEFT - 10)));
        particles[i].y = (float)(BOX_TOP + 5 + (rand() % (BOX_BOTTOM - BOX_TOP - 10)));
        /* random velocity direction, moderate speed */
        {
            float angle = (float)(rand() % 360) * 3.14159265f / 180.0f;
            float speed = 0.8f + (float)(rand() % 100) / 100.0f;
            particles[i].vx = speed * cosf(angle);
            particles[i].vy = speed * sinf(angle);
        }
    }
}

static void reset_all(void)
{
    piston_x = 220;
    mom_idx = 0;
    mom_count = 0;
    init_particles();
}

/* scale every particle speed by factor, clamping */
static void scale_speeds(float factor)
{
    uint8_t i;
    for (i = 0; i < N_PARTICLES; i++) {
        float sp = sqrtf(particles[i].vx * particles[i].vx +
                         particles[i].vy * particles[i].vy);
        sp *= factor;
        if (sp > MAX_SPEED) sp = MAX_SPEED;
        if (sp < MIN_SPEED) sp = MIN_SPEED;
        if (sp > 0.001f) {
            particles[i].vx = particles[i].vx / sp * sp;
            particles[i].vy = particles[i].vy / sp * sp;
        }
    }
}

/* advance one particle; return momentum magnitude if it hit a wall, else 0 */
static float step_particle(particle_t *p)
{
    float mom = 0.0f;
    p->x += p->vx;
    p->y += p->vy;

    /* left wall */
    if (p->x < BOX_LEFT + 2) {
        p->x = BOX_LEFT + 2;
        p->vx = -p->vx;
        mom = fabsf(p->vx);
    }
    /* right wall (piston) */
    if (p->x > piston_x - PISTON_W - 2) {
        p->x = piston_x - PISTON_W - 2;
        p->vx = -p->vx;
        mom = fabsf(p->vx);
    }
    /* top wall */
    if (p->y < BOX_TOP + 2) {
        p->y = BOX_TOP + 2;
        p->vy = -p->vy;
        mom += fabsf(p->vy);
    }
    /* bottom wall */
    if (p->y > BOX_BOTTOM - 2) {
        p->y = BOX_BOTTOM - 2;
        p->vy = -p->vy;
        mom += fabsf(p->vy);
    }
    return mom;
}

/* ---- main ---- */
int main(void)
{
    uint8_t i;
    float frame_mom;
    float avg_mom;
    float mean_ke;
    float volume, temperature, pressure, pv;
    int32_t vol_int, temp_int, pres_int, pv_int;

    gfx_Begin();
    gfx_SetDrawBuffer();
    srand(12345);
    init_particles();

    for (;;) {
        /* ---- input ---- */
        kb_Scan();

        /* edge-detect arrow keys */
        uint8_t left_now  = (kb_Data[7] & kb_Left)  ? 1 : 0;
        uint8_t right_now = (kb_Data[7] & kb_Right) ? 1 : 0;
        uint8_t up_now    = (kb_Data[7] & kb_Up)    ? 1 : 0;
        uint8_t down_now  = (kb_Data[7] & kb_Down)  ? 1 : 0;
        uint8_t enter_now = (kb_Data[6] & kb_Enter) ? 1 : 0;

        if (left_now && !was_left)  piston_x -= 3;
        if (right_now && !was_right) piston_x += 3;
        if (up_now && !was_up)       scale_speeds(TEMP_SCALE);
        if (down_now && !was_down)   scale_speeds(1.0f / TEMP_SCALE);
        if (enter_now && !was_enter) reset_all();

        if (piston_x < PISTON_MIN) piston_x = PISTON_MIN;
        if (piston_x > PISTON_MAX) piston_x = PISTON_MAX;

        was_left  = left_now;
        was_right = right_now;
        was_up    = up_now;
        was_down  = down_now;
        was_enter = enter_now;

        if (kb_Data[6] & kb_Clear) break;

        /* ---- physics: one step per frame ---- */
        frame_mom = 0.0f;
        for (i = 0; i < N_PARTICLES; i++) {
            frame_mom += step_particle(&particles[i]);
        }

        /* store in ring buffer */
        mom_buf[mom_idx] = frame_mom;
        mom_idx = (mom_idx + 1) % PRESSURE_WINDOW;
        if (mom_count < PRESSURE_WINDOW) mom_count++;

        /* ---- compute averages ---- */
        avg_mom = 0.0f;
        for (i = 0; i < mom_count; i++) avg_mom += mom_buf[i];
        if (mom_count > 0) avg_mom /= (float)mom_count;

        /* mean kinetic energy (proportional to temperature) */
        mean_ke = 0.0f;
        for (i = 0; i < N_PARTICLES; i++) {
            float sp2 = particles[i].vx * particles[i].vx +
                        particles[i].vy * particles[i].vy;
            mean_ke += sp2;
        }
        mean_ke /= (float)N_PARTICLES;

        /* derived quantities */
        volume    = (float)(piston_x - PISTON_W - BOX_LEFT);
        temperature = mean_ke * 100.0f;
        pressure  = avg_mom * 100.0f;
        pv        = pressure * volume;

        vol_int  = (int32_t)volume;
        temp_int = (int32_t)temperature;
        pres_int = (int32_t)pressure;
        pv_int   = (int32_t)pv;

        /* ---- draw ---- */
        gfx_FillScreen(0);

        /* box outline */
        gfx_SetColor(0x10);
        gfx_Rectangle(BOX_LEFT, BOX_TOP,
                      piston_x - PISTON_W - BOX_LEFT,
                      BOX_BOTTOM - BOX_TOP);

        /* piston */
        gfx_SetColor(0xE0);
        gfx_FillRectangle(piston_x - PISTON_W, BOX_TOP, PISTON_W,
                          BOX_BOTTOM - BOX_TOP);

        /* particles */
        gfx_SetColor(0x03);
        for (i = 0; i < N_PARTICLES; i++) {
            gfx_FillCircle((int16_t)particles[i].x,
                           (int16_t)particles[i].y, 2);
        }

        /* readouts */
        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("V=", 5, 215);
        gfx_SetTextXY(20, 215);
        gfx_PrintInt(vol_int, 3);

        gfx_SetTextXY(70, 215);
        gfx_PrintStringXY("T=", 70, 215);
        gfx_SetTextXY(85, 215);
        gfx_PrintInt(temp_int, 4);

        gfx_SetTextXY(140, 215);
        gfx_PrintStringXY("P=", 140, 215);
        gfx_SetTextXY(155, 215);
        gfx_PrintInt(pres_int, 4);

        gfx_SetTextXY(210, 215);
        gfx_PrintStringXY("PV=", 210, 215);
        gfx_SetTextXY(235, 215);
        gfx_PrintInt(pv_int, 5);

        /* hint */
        gfx_SetTextFGColor(0x10);
        gfx_PrintStringXY("L/R piston  U/D temp  Enter reset", 5, 232);
        gfx_PrintStringXY("P*V~const if T const", 180, 232);

        gfx_SwapDraw();
        delay(16);
    }

    gfx_End();
    return 0;
}
