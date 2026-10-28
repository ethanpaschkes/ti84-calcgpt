#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Dalton's Law of Partial Pressures
 * Three gas species (red, green, blue) bounce elastically in a box.
 * Partial pressure = wall-collision momentum per second for each species.
 * Total pressure = sum of partials.
 *
 * Controls:
 *   Left  = add red particle
 *   Up    = add green particle
 *   Right = add blue particle
 *   Down  = remove one particle of the most numerous color
 *   Clear = quit
 */

#define MAX_PARTICLES 60
#define BOX_X 10
#define BOX_Y 30
#define BOX_W 200
#define BOX_H 150
#define MEASURE_MS 1000
#define BAR_X 220
#define BAR_Y 30
#define BAR_W 80
#define BAR_H 150
#define MAX_PRESSURE 50000.0f

static struct {
    float x, y, vx, vy;
    uint8_t type; /* 0=red, 1=green, 2=blue */
} particles[MAX_PARTICLES];

static int num_particles = 0;
static int count_red = 0, count_green = 0, count_blue = 0;

/* Momentum accumulators for each species (per second) */
static float mom_red = 0.0f, mom_green = 0.0f, mom_blue = 0.0f;
static float mom_total = 0.0f;

/* Edge-detect flags */
static int was_left = 0, was_up = 0, was_right = 0, was_down = 0;

/* Timer for pressure measurement window */
static uint32_t measure_start = 0;
static int measuring = 0;

/* Displayed pressure values (smoothed) */
static float p_red = 0.0f, p_green = 0.0f, p_blue = 0.0f, p_total = 0.0f;

static void add_particle(uint8_t type)
{
    if (num_particles >= MAX_PARTICLES) return;
    int idx = num_particles++;
    particles[idx].type = type;
    /* Random position within box */
    particles[idx].x = BOX_X + 5.0f + (float)(rand() % (BOX_W - 10));
    particles[idx].y = BOX_Y + 5.0f + (float)(rand() % (BOX_H - 10));
    /* Random velocity, magnitude ~2-4 px/frame */
    float angle = (float)(rand() % 360) * 0.0174533f; /* to radians */
    float speed = 2.0f + (float)(rand() % 3);
    particles[idx].vx = speed * cosf(angle);
    particles[idx].vy = speed * sinf(angle);
    if (type == 0) count_red++;
    else if (type == 1) count_green++;
    else count_blue++;
}

static void remove_most_numerous(void)
{
    /* Find which color has the most particles */
    int max_count = -1;
    int max_type = -1;
    if (count_red > max_count) { max_count = count_red; max_type = 0; }
    if (count_green > max_count) { max_count = count_green; max_type = 1; }
    if (count_blue > max_count) { max_count = count_blue; max_type = 2; }
    if (max_type < 0) return;

    /* Remove last particle of that type */
    for (int i = num_particles - 1; i >= 0; i--) {
        if (particles[i].type == max_type) {
            /* Shift remaining particles down */
            for (int j = i; j < num_particles - 1; j++) {
                particles[j] = particles[j + 1];
            }
            num_particles--;
            if (max_type == 0) count_red--;
            else if (max_type == 1) count_green--;
            else count_blue--;
            return;
        }
    }
}

static void update_particles(void)
{
    for (int i = 0; i < num_particles; i++) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;

        /* Bounce off walls, accumulate momentum */
        if (particles[i].x < BOX_X + 2) {
            particles[i].x = BOX_X + 2;
            particles[i].vx = -particles[i].vx;
            float mom = fabsf(particles[i].vx) * 2.0f; /* momentum change = 2*m*v, m=1 */
            if (particles[i].type == 0) mom_red += mom;
            else if (particles[i].type == 1) mom_green += mom;
            else mom_blue += mom;
            mom_total += mom;
        } else if (particles[i].x > BOX_X + BOX_W - 2) {
            particles[i].x = BOX_X + BOX_W - 2;
            particles[i].vx = -particles[i].vx;
            float mom = fabsf(particles[i].vx) * 2.0f;
            if (particles[i].type == 0) mom_red += mom;
            else if (particles[i].type == 1) mom_green += mom;
            else mom_blue += mom;
            mom_total += mom;
        }

        if (particles[i].y < BOX_Y + 2) {
            particles[i].y = BOX_Y + 2;
            particles[i].vy = -particles[i].vy;
            float mom = fabsf(particles[i].vy) * 2.0f;
            if (particles[i].type == 0) mom_red += mom;
            else if (particles[i].type == 1) mom_green += mom;
            else mom_blue += mom;
            mom_total += mom;
        } else if (particles[i].y > BOX_Y + BOX_H - 2) {
            particles[i].y = BOX_Y + BOX_H - 2;
            particles[i].vy = -particles[i].vy;
            float mom = fabsf(particles[i].vy) * 2.0f;
            if (particles[i].type == 0) mom_red += mom;
            else if (particles[i].type == 1) mom_green += mom;
            else mom_blue += mom;
            mom_total += mom;
        }
    }
}

static void check_measure_window(void)
{
    uint32_t now = get_ms();
    if (!measuring) {
        measure_start = now;
        measuring = 1;
        mom_red = mom_green = mom_blue = mom_total = 0.0f;
    }
    if (now - measure_start >= MEASURE_MS) {
        /* Convert accumulated momentum to pressure (momentum per second) */
        p_red = mom_red;
        p_green = mom_green;
        p_blue = mom_blue;
        p_total = mom_total;
        measuring = 0;
    }
}

static void draw_bar(int x, int y, int w, int h, float value, uint8_t color, const char *label)
{
    /* Draw bar background */
    gfx_SetColor(0x10);
    gfx_FillRectangle(x, y, w, h);

    /* Draw bar fill */
    int bar_h = (int)((value / MAX_PRESSURE) * (float)h);
    if (bar_h > h) bar_h = h;
    if (bar_h < 0) bar_h = 0;
    gfx_SetColor(color);
    gfx_FillRectangle(x, y + h - bar_h, w, bar_h);

    /* Draw label */
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY(label, x, y - 12);

    /* Draw value as integer (value * 10 for one decimal) */
    int val_int = (int)(value * 10);
    gfx_SetTextXY(x, y + h + 2);
    gfx_PrintInt(val_int, 4);
}

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();

    /* Initialize with a few particles */
    add_particle(0);
    add_particle(0);
    add_particle(1);
    add_particle(1);
    add_particle(2);
    add_particle(2);

    measure_start = get_ms();
    measuring = 1;

    for (;;) {
        kb_Scan();

        /* Edge-detect key presses */
        int left_now = kb_Data[7] & kb_Left;
        int up_now = kb_Data[7] & kb_Up;
        int right_now = kb_Data[7] & kb_Right;
        int down_now = kb_Data[7] & kb_Down;

        if (left_now && !was_left) add_particle(0);
        if (up_now && !was_up) add_particle(1);
        if (right_now && !was_right) add_particle(2);
        if (down_now && !was_down) remove_most_numerous();

        was_left = left_now;
        was_up = up_now;
        was_right = right_now;
        was_down = down_now;

        if (kb_Data[6] & kb_Clear) break;

        /* Update physics */
        update_particles();
        check_measure_window();

        /* Draw */
        gfx_FillScreen(0); /* black background */

        /* Draw box */
        gfx_SetColor(0x80);
        gfx_Rectangle(BOX_X, BOX_Y, BOX_W, BOX_H);

        /* Draw particles */
        for (int i = 0; i < num_particles; i++) {
            if (particles[i].type == 0) gfx_SetColor(0xE0); /* red */
            else if (particles[i].type == 1) gfx_SetColor(0x03); /* green */
            else gfx_SetColor(0x10); /* blue */
            gfx_FillCircle((int)particles[i].x, (int)particles[i].y, 3);
        }

        /* Draw pressure bars */
        draw_bar(BAR_X, BAR_Y, BAR_W, BAR_H, p_red, 0xE0, "P_red");
        draw_bar(BAR_X, BAR_Y + BAR_H + 20, BAR_W, BAR_H, p_green, 0x03, "P_green");
        draw_bar(BAR_X, BAR_Y + 2 * (BAR_H + 20), BAR_W, BAR_H, p_blue, 0x10, "P_blue");

        /* Draw total pressure bar (wider, at bottom) */
        int total_y = BAR_Y + 3 * (BAR_H + 20);
        if (total_y + BAR_H > 240) total_y = 240 - BAR_H;
        draw_bar(BAR_X, total_y, BAR_W, BAR_H, p_total, 0xFF, "P_total");

        /* Show Dalton's law: P_total = P_red + P_green + P_blue */
        gfx_SetTextFGColor(0xE0);
        gfx_PrintStringXY("Dalton: Ptot = Pr+Pg+Pb", 10, 220);

        /* Show particle counts */
        gfx_SetTextFGColor(0x80);
        gfx_PrintStringXY("R:", BOX_X + BOX_W + 5, BOX_Y);
        gfx_PrintInt(count_red, 2);
        gfx_PrintStringXY("G:", BOX_X + BOX_W + 5, BOX_Y + 12);
        gfx_PrintInt(count_green, 2);
        gfx_PrintStringXY("B:", BOX_X + BOX_W + 5, BOX_Y + 24);
        gfx_PrintInt(count_blue, 2);

        /* Help line */
        gfx_SetTextFGColor(0x80);
        gfx_PrintStringXY("L+R U+G R+B D-rm Clear=quit", 10, 232);

        gfx_SwapDraw();
        delay(15);
    }

    gfx_End();
    return 0;
}
