#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Graham's law of effusion demo.
 * Left chamber: 20 red (mass=1) + 20 blue (mass=4) particles.
 * Speed ∝ 1/sqrt(mass), so red moves 2× faster than blue.
 * A hole in the wall lets particles escape to the right chamber.
 * Shows escaped counts and the red/blue ratio.
 * Enter restarts, Clear quits. */

#define N_RED   20
#define N_BLUE  20
#define N_TOTAL 40

/* Chamber geometry (320×240 screen) */
#define WALL_X      160
#define HOLE_Y0     100
#define HOLE_Y1     140
#define HOLE_H      (HOLE_Y1 - HOLE_Y0)

/* Particle bounds */
#define LEFT_MIN_X  2
#define LEFT_MAX_X  (WALL_X - 2)
#define RIGHT_MIN_X (WALL_X + 2)
#define RIGHT_MAX_X 318
#define TOP_Y       2
#define BOT_Y       238

/* Speeds: v ∝ 1/√m.  Base speed for mass-4 particles. */
#define BASE_SPEED  1.0f
/* Red (mass 1): speed = BASE_SPEED * sqrt(4/1) = 2.0 */
/* Blue (mass 4): speed = BASE_SPEED * sqrt(1/4) = 0.5 */

typedef struct {
    float x, y;
    float vx, vy;
    uint8_t escaped;   /* 0 = left chamber, 1 = right chamber */
} Particle;

static Particle particles[N_TOTAL];
static uint8_t esc_red = 0;
static uint8_t esc_blue = 0;
static int was_enter = 0;

/* Random float in [0,1) */
static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

/* Random float in [a,b) */
static float rand_range(float a, float b) {
    return a + randf() * (b - a);
}

/* Initialise one particle in the left chamber with a given speed magnitude */
static void init_particle(Particle *p, float speed) {
    float angle = randf() * 2.0f * 3.14159265f;
    p->x  = rand_range((float)LEFT_MIN_X, (float)LEFT_MAX_X);
    p->y  = rand_range((float)TOP_Y, (float)BOT_Y);
    p->vx = speed * cosf(angle);
    p->vy = speed * sinf(angle);
    p->escaped = 0;
}

static void reset_experiment(void) {
    esc_red = 0;
    esc_blue = 0;
    for (int i = 0; i < N_RED; i++)
        init_particle(&particles[i], BASE_SPEED * 2.0f);       /* mass 1 */
    for (int i = N_RED; i < N_TOTAL; i++)
        init_particle(&particles[i], BASE_SPEED * 0.5f);       /* mass 4 */
}

/* Move one particle and handle wall / hole collisions */
static void update_particle(Particle *p, int is_red) {
    p->x += p->vx;
    p->y += p->vy;

    /* Top / bottom bounce */
    if (p->y < TOP_Y) {
        p->y = TOP_Y;
        p->vy = -p->vy;
    } else if (p->y > BOT_Y) {
        p->y = BOT_Y;
        p->vy = -p->vy;
    }

    if (!p->escaped) {
        /* Left wall bounce */
        if (p->x < LEFT_MIN_X) {
            p->x = LEFT_MIN_X;
            p->vx = -p->vx;
        }

        /* Right side: check hole or wall */
        if (p->x >= WALL_X) {
            if (p->y >= HOLE_Y0 && p->y <= HOLE_Y1) {
                /* Passed through the hole */
                p->escaped = 1;
                if (is_red)
                    esc_red++;
                else
                    esc_blue++;
            } else {
                /* Bounce off the wall */
                p->x = LEFT_MAX_X;
                p->vx = -p->vx;
            }
        }
    } else {
        /* In the right chamber: bounce off right wall and the wall segments */
        if (p->x > RIGHT_MAX_X) {
            p->x = RIGHT_MAX_X;
            p->vx = -p->vx;
        }
        if (p->x < RIGHT_MIN_X) {
            p->x = RIGHT_MIN_X;
            p->vx = -p->vx;
        }
        /* Bounce off the wall segments above and below the hole */
        if (p->x >= WALL_X && p->x <= WALL_X + 2) {
            if (p->y < HOLE_Y0) {
                p->y = HOLE_Y0;
                p->vy = -p->vy;
            } else if (p->y > HOLE_Y1) {
                p->y = HOLE_Y1;
                p->vy = -p->vy;
            }
        }
    }
}

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();

    srand(12345);
    reset_experiment();

    for (;;) {
        kb_Scan();

        /* Edge-detect Enter to restart */
        int enter_now = (kb_Data[6] & kb_Enter) ? 1 : 0;
        if (enter_now && !was_enter) {
            reset_experiment();
        }
        was_enter = enter_now;

        if (kb_Data[6] & kb_Clear) break;

        /* Update all particles */
        for (int i = 0; i < N_RED; i++)
            update_particle(&particles[i], 1);
        for (int i = N_RED; i < N_TOTAL; i++)
            update_particle(&particles[i], 0);

        /* --- Draw --- */
        gfx_FillScreen(0);  /* black background */

        /* Draw chamber walls */
        gfx_SetColor(0x30);  /* grey */
        gfx_Line(WALL_X, 0, WALL_X, HOLE_Y0 - 1);
        gfx_Line(WALL_X, HOLE_Y1, WALL_X, 239);

        /* Draw chamber labels */
        gfx_SetTextFGColor(0x30);
        gfx_PrintStringXY("LEFT", 10, 5);
        gfx_PrintStringXY("RIGHT", 200, 5);

        /* Draw particles */
        for (int i = 0; i < N_RED; i++) {
            gfx_SetColor(0xE0);  /* red */
            gfx_FillCircle((int)particles[i].x, (int)particles[i].y, 2);
        }
        for (int i = N_RED; i < N_TOTAL; i++) {
            gfx_SetColor(0x10);  /* blue */
            gfx_FillCircle((int)particles[i].x, (int)particles[i].y, 2);
        }

        /* Draw stats */
        gfx_SetTextFGColor(0xE0);
        gfx_PrintStringXY("Red esc:", 5, 200);
        gfx_PrintInt(esc_red, 3);

        gfx_SetTextFGColor(0x10);
        gfx_PrintStringXY("Blue esc:", 5, 210);
        gfx_PrintInt(esc_blue, 3);

        /* Ratio red/blue with one decimal: multiply by 10, print as integer */
        gfx_SetTextFGColor(0x03);  /* green */
        gfx_PrintStringXY("Ratio R/B:", 100, 200);
        if (esc_blue > 0) {
            int ratio_x10 = (int)((float)esc_red / (float)esc_blue * 10.0f + 0.5f);
            gfx_PrintInt(ratio_x10, 3);
        } else {
            gfx_PrintStringXY("N/A", 160, 200);
        }

        /* Help line */
        gfx_SetTextFGColor(0x30);
        gfx_PrintStringXY("Enter=Restart  Clear=Quit", 5, 230);

        gfx_SwapDraw();
        delay(10);
    }

    gfx_End();
    return 0;
}
