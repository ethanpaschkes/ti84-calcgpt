#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Charles's Law demo: 25 particles in a cylinder with a movable piston.
   Constant external pressure (piston weight) => V/T stays roughly constant. */

#define N_PARTICLES 25
#define CYL_X       110
#define CYL_W        90
#define CYL_TOP      30
#define CYL_BOTTOM  210
#define PISTON_H     10
#define PISTON_MASS  20.0f
#define PARTICLE_M   1.0f
#define PARTICLE_R    3
#define G_DOWN       0.02f      /* constant downward accel on piston (gravity) */
#define RESTITUTION  0.98f
#define MAX_SPEED    3.0f
#define MIN_SPEED    0.3f

typedef struct {
    float x, y, vx, vy;
} particle_t;

static particle_t p[N_PARTICLES];
static float piston_y = 150.0f;   /* top of piston */
static float piston_vy = 0.0f;
static float temperature = 300.0f; /* Kelvin, integer display */
static int   was_up = 0, was_down = 0;

/* Seed particles with a given temperature (speeds scale with sqrt(T)) */
static void seed_particles(float T)
{
    float base = sqrtf(T / 300.0f);
    for (int i = 0; i < N_PARTICLES; i++) {
        p[i].x = CYL_X + PARTICLE_R + 5 + (float)(i * 3) % CYL_W;
        p[i].y = CYL_TOP + PARTICLE_R + 5 + (float)(i * 7) % 100;
        float ang = (float)i * 2.399963f; /* golden angle for spread */
        p[i].vx = cosf(ang) * base * 1.5f;
        p[i].vy = sinf(ang) * base * 1.5f;
    }
}

/* Keep particle inside cylinder walls and below piston */
static void constrain_particle(int i)
{
    float left  = CYL_X + PARTICLE_R;
    float right = CYL_X + CYL_W - PARTICLE_R;
    float top   = CYL_TOP + PARTICLE_R;
    float bot   = piston_y - PARTICLE_R;

    if (p[i].x < left)  { p[i].x = left;  p[i].vx = -p[i].vx * RESTITUTION; }
    if (p[i].x > right) { p[i].x = right; p[i].vx = -p[i].vx * RESTITUTION; }
    if (p[i].y < top)   { p[i].y = top;   p[i].vy = -p[i].vy * RESTITUTION; }
    if (p[i].y > bot)   { p[i].y = bot;   p[i].vy = -p[i].vy * RESTITUTION; }
}

/* One physics step: move particles, collide with piston, move piston */
static void step_physics(void)
{
    /* Move particles */
    for (int i = 0; i < N_PARTICLES; i++) {
        p[i].x += p[i].vx;
        p[i].y += p[i].vy;
        constrain_particle(i);
    }

    /* Piston: gravity pulls it down */
    piston_vy += G_DOWN;

    /* Particle-piston collisions: transfer momentum to piston */
    float bot = piston_y - PARTICLE_R;
    for (int i = 0; i < N_PARTICLES; i++) {
        if (p[i].y > bot) {
            p[i].y = bot;
            /* Reflect particle upward */
            p[i].vy = -fabsf(p[i].vy) * RESTITUTION;
            /* Transfer momentum to piston (push it up = negative vy) */
            piston_vy -= (p[i].vy * PARTICLE_M) / PISTON_MASS;
        }
    }

    /* Move piston */
    piston_y += piston_vy;
    piston_vy *= 0.95f; /* damping so it settles */

    /* Piston bounds */
    float min_y = CYL_TOP + PISTON_H + 10;
    float max_y = CYL_BOTTOM - PISTON_H - 10;
    if (piston_y < min_y) { piston_y = min_y; piston_vy = 0.0f; }
    if (piston_y > max_y) { piston_y = max_y; piston_vy = 0.0f; }
}

/* Scale all particle speeds to match new temperature */
static void set_temperature(float new_T)
{
    if (new_T < 50.0f)  new_T = 50.0f;
    if (new_T > 1000.0f) new_T = 1000.0f;
    float ratio = sqrtf(new_T / temperature);
    for (int i = 0; i < N_PARTICLES; i++) {
        p[i].vx *= ratio;
        p[i].vy *= ratio;
        /* Clamp speed */
        float sp = sqrtf(p[i].vx * p[i].vx + p[i].vy * p[i].vy);
        if (sp > MAX_SPEED) {
            p[i].vx *= MAX_SPEED / sp;
            p[i].vy *= MAX_SPEED / sp;
        }
        if (sp < MIN_SPEED && sp > 0.0f) {
            p[i].vx *= MIN_SPEED / sp;
            p[i].vy *= MIN_SPEED / sp;
        }
    }
    temperature = new_T;
}

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();

    seed_particles(300.0f);
    piston_y = 150.0f;

    for (;;) {
        kb_Scan();

        /* Quit */
        if (kb_Data[6] & kb_Clear) break;

        /* Edge-detect Up/Down for temperature changes */
        int up_now   = (kb_Data[7] & kb_Up)   ? 1 : 0;
        int down_now = (kb_Data[7] & kb_Down) ? 1 : 0;

        if (up_now && !was_up) {
            set_temperature(temperature + 50.0f);
        }
        if (down_now && !was_down) {
            set_temperature(temperature - 50.0f);
        }
        was_up   = up_now;
        was_down = down_now;

        /* Physics: a few sub-steps per frame for stability */
        for (int s = 0; s < 3; s++) {
            step_physics();
        }

        /* --- Draw --- */
        gfx_FillScreen(0); /* black background */

        /* Cylinder walls */
        gfx_SetColor(0x10); /* blue */
        gfx_Line(CYL_X, CYL_TOP, CYL_X, CYL_BOTTOM);
        gfx_Line(CYL_X + CYL_W, CYL_TOP, CYL_X + CYL_W, CYL_BOTTOM);
        gfx_Line(CYL_X, CYL_BOTTOM, CYL_X + CYL_W, CYL_BOTTOM);

        /* Piston */
        gfx_SetColor(0xE0); /* red */
        gfx_FillRectangle(CYL_X, (int)piston_y, CYL_W, PISTON_H);

        /* Particles */
        gfx_SetColor(0x03); /* green */
        for (int i = 0; i < N_PARTICLES; i++) {
            gfx_FillCircle((int)p[i].x, (int)p[i].y, PARTICLE_R);
        }

        /* Labels and readouts */
        gfx_SetTextFGColor(0xE0);
        gfx_PrintStringXY("CHARLES'S LAW", 10, 5);

        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("T=", 10, 25);
        gfx_SetTextXY(25, 25);
        gfx_PrintInt((int)temperature, 4);
        gfx_PrintStringXY("K", 65, 25);

        /* Volume = piston height from bottom (in pixels, scaled) */
        int vol = (int)((CYL_BOTTOM - piston_y) * 10);
        gfx_PrintStringXY("V=", 10, 40);
        gfx_SetTextXY(25, 40);
        gfx_PrintInt(vol, 5);

        /* V/T ratio (V in pixel*10, T in K) */
        float ratio = (float)vol / temperature;
        int ratio_x10 = (int)(ratio * 10.0f);
        gfx_PrintStringXY("V/T=", 10, 55);
        gfx_SetTextXY(45, 55);
        gfx_PrintInt(ratio_x10, 4);
        gfx_PrintStringXY(".0", 85, 55);

        /* Help line at bottom */
        gfx_SetTextFGColor(0x10);
        gfx_PrintStringXY("UP/DN: T +/- 50K  CLR: quit", 10, 225);

        gfx_SwapDraw();
        delay(10);
    }

    gfx_End();
    return 0;
}
