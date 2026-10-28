#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Two-body orbit: planet around a fixed sun at screen center.
 * GM = 1000 (pixel^3 / s^2), distances in pixels.
 * Velocity Verlet integration.
 * Up/Down adjust launch speed, Enter relaunches, Clear quits. */

#define GM        1000.0f
#define DT        0.05f          /* integration time step */
#define STEPS     4             /* sub-steps per frame */
#define TRAIL_LEN 120
#define SUN_R     8
#define PLANET_R  4
#define START_X   120.0f        /* initial planet x offset from center */
#define START_Y   0.0f          /* initial planet y offset from center */
#define SPEED_MIN 1.0f
#define SPEED_MAX 12.0f
#define SPEED_STEP 0.2f

/* Trail storage: x, y, age (0=newest, TRAIL_LEN-1=oldest) */
static float trail_x[TRAIL_LEN];
static float trail_y[TRAIL_LEN];
static uint8_t trail_age[TRAIL_LEN];
static int trail_head = 0;      /* next slot to write */
static int trail_count = 0;     /* how many slots filled */

/* Edge-detect flags */
static int was_up = 0, was_down = 0, was_enter = 0;

/* Launch speed (pixels per time unit) */
static float launch_speed = 5.0f;

/* Planet state */
static float px, py, vx, vy;
static float ax, ay;

/* Center of screen (sun position) */
#define CX 160.0f
#define CY 120.0f

/* Compute acceleration at (x, y) relative to sun at origin */
static void compute_accel(float x, float y, float *ax_out, float *ay_out)
{
    float r2 = x * x + y * y;
    float r = sqrtf(r2);
    if (r < 1.0f) r = 1.0f;  /* avoid singularity */
    float inv_r3 = 1.0f / (r2 * r);
    *ax_out = -GM * x * inv_r3;
    *ay_out = -GM * y * inv_r3;
}

/* Initialize planet at starting point with given speed (perpendicular to radius) */
static void launch_planet(void)
{
    px = CX + START_X;
    py = CY + START_Y;
    /* Velocity perpendicular to radius vector (tangent), magnitude = launch_speed */
    /* Radius direction: (START_X, START_Y) normalized */
    float r = sqrtf(START_X * START_X + START_Y * START_Y);
    float rx = START_X / r;
    float ry = START_Y / r;
    /* Perpendicular (rotate 90 deg CCW): (-ry, rx) */
    vx = -ry * launch_speed;
    vy = rx * launch_speed;
    compute_accel(px - CX, py - CY, &ax, &ay);

    /* Clear trail */
    trail_count = 0;
    trail_head = 0;
}

/* Add current position to trail */
static void add_to_trail(void)
{
    trail_x[trail_head] = px;
    trail_y[trail_head] = py;
    trail_age[trail_head] = 0;
    trail_head = (trail_head + 1) % TRAIL_LEN;
    if (trail_count < TRAIL_LEN) trail_count++;
}

/* Age all trail points */
static void age_trail(void)
{
    for (int i = 0; i < TRAIL_LEN; i++) {
        if (trail_age[i] < 255) trail_age[i]++;
    }
}

/* Velocity Verlet step */
static void verlet_step(void)
{
    float ax_old, ay_old;
    ax_old = ax;
    ay_old = ay;

    /* Update position: x += v*dt + 0.5*a*dt^2 */
    px += vx * DT + 0.5f * ax_old * DT * DT;
    py += vy * DT + 0.5f * ay_old * DT * DT;

    /* Compute new acceleration */
    float ax_new, ay_new;
    compute_accel(px - CX, py - CY, &ax_new, &ay_new);

    /* Update velocity: v += 0.5*(a_old + a_new)*dt */
    vx += 0.5f * (ax_old + ax_new) * DT;
    vy += 0.5f * (ay_old + ay_new) * DT;

    ax = ax_new;
    ay = ay_new;
}

/* Determine orbit type from specific orbital energy */
static int get_orbit_type(void)
{
    float r = sqrtf((px - CX) * (px - CX) + (py - CY) * (py - CY));
    float v2 = vx * vx + vy * vy;
    float energy = 0.5f * v2 - GM / r;
    if (energy < -1.0f) return 0;      /* elliptical (bound) */
    if (energy > 1.0f) return 2;       /* escaping (hyperbolic) */
    return 1;                          /* circular (near zero energy) */
}

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();

    launch_planet();

    for (;;) {
        kb_Scan();

        /* Edge-detect Up/Down for speed adjustment */
        int up_now = (kb_Data[7] & kb_Up) ? 1 : 0;
        int down_now = (kb_Data[7] & kb_Down) ? 1 : 0;
        int enter_now = (kb_Data[6] & kb_Enter) ? 1 : 0;

        if (up_now && !was_up) {
            launch_speed += SPEED_STEP;
            if (launch_speed > SPEED_MAX) launch_speed = SPEED_MAX;
        }
        if (down_now && !was_down) {
            launch_speed -= SPEED_STEP;
            if (launch_speed < SPEED_MIN) launch_speed = SPEED_MIN;
        }
        if (enter_now && !was_enter) {
            launch_planet();
        }

        was_up = up_now;
        was_down = down_now;
        was_enter = enter_now;

        /* Quit on Clear */
        if (kb_Data[6] & kb_Clear) break;

        /* Integrate physics */
        for (int i = 0; i < STEPS; i++) {
            verlet_step();
        }

        /* Add to trail every frame */
        add_to_trail();
        age_trail();

        /* --- Draw --- */
        gfx_FillScreen(0);  /* black background */

        /* Draw fading trail */
        for (int i = 0; i < TRAIL_LEN; i++) {
            int idx = (trail_head - 1 - i + TRAIL_LEN) % TRAIL_LEN;
            if (i >= trail_count) break;
            uint8_t age = trail_age[idx];
            if (age > 200) continue;
            /* Color fades from green (0x03) to dark as age increases */
            uint8_t brightness = 255 - (uint8_t)(age * 255 / 200);
            /* Use a simple fade: green channel decreases */
            uint8_t color;
            if (brightness > 200) color = 0x03;       /* bright green */
            else if (brightness > 100) color = 0x02;  /* medium green */
            else if (brightness > 50) color = 0x01;   /* dim green */
            else continue;
            gfx_SetColor(color);
            gfx_SetPixel((int)trail_x[idx], (int)trail_y[idx]);
        }

        /* Draw sun */
        gfx_SetColor(0xE0);  /* red */
        gfx_FillCircle((int)CX, (int)CY, SUN_R);
        gfx_SetColor(0xFF);  /* white glow */
        gfx_FillCircle((int)CX, (int)CY, 3);

        /* Draw planet */
        gfx_SetColor(0x10);  /* blue */
        gfx_FillCircle((int)px, (int)py, PLANET_R);

        /* Compute display values */
        float dist = sqrtf((px - CX) * (px - CX) + (py - CY) * (py - CY));
        float speed = sqrtf(vx * vx + vy * vy);
        int orbit_type = get_orbit_type();

        /* Display info */
        gfx_SetTextFGColor(0xFF);
        gfx_PrintStringXY("Two-Body Orbit", 5, 2);

        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("Speed:", 5, 14);
        /* Show speed * 10 as integer for one decimal */
        gfx_SetTextXY(50, 14);
        gfx_PrintInt((int)(speed * 10.0f), 4);
        gfx_SetTextXY(90, 14);
        gfx_PrintStringXY(".0", 0, 0);  /* placeholder, overwritten below */

        /* Better: print speed as integer part and decimal */
        gfx_SetTextFGColor(0);
        gfx_FillRectangle(50, 14, 45, 8);
        gfx_SetTextFGColor(0x03);
        int speed_int = (int)speed;
        int speed_dec = (int)((speed - (float)speed_int) * 10.0f);
        gfx_SetTextXY(50, 14);
        gfx_PrintInt(speed_int, 2);
        gfx_SetTextXY(70, 14);
        gfx_PrintStringXY(".", 0, 0);
        gfx_SetTextXY(76, 14);
        gfx_PrintInt(speed_dec, 1);

        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("Dist:", 5, 24);
        int dist_int = (int)dist;
        gfx_SetTextXY(45, 24);
        gfx_PrintInt(dist_int, 3);

        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("Launch:", 5, 34);
        int ls_int = (int)launch_speed;
        int ls_dec = (int)((launch_speed - (float)ls_int) * 10.0f);
        gfx_SetTextXY(55, 34);
        gfx_PrintInt(ls_int, 2);
        gfx_SetTextXY(75, 34);
        gfx_PrintStringXY(".", 0, 0);
        gfx_SetTextXY(81, 34);
        gfx_PrintInt(ls_dec, 1);

        /* Orbit type label */
        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("Type:", 5, 44);
        if (orbit_type == 0) {
            gfx_SetTextFGColor(0x03);
            gfx_PrintStringXY("Elliptical", 40, 44);
        } else if (orbit_type == 1) {
            gfx_SetTextFGColor(0xE0);
            gfx_PrintStringXY("Circular", 40, 44);
        } else {
            gfx_SetTextFGColor(0x10);
            gfx_PrintStringXY("Escaping", 40, 44);
        }

        /* Help line at bottom */
        gfx_SetTextFGColor(0x80);
        gfx_PrintStringXY("Up/Dn:Speed  Enter:Launch  Clr:Quit", 5, 230);

        gfx_SwapDraw();
        delay(10);
    }

    gfx_End();
    return 0;
}
