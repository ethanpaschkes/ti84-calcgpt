#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Mass on a spring with damping.
 * Left/Right: damping c (0..2.0)
 * Up/Down:    spring constant k (1..50)
 * Enter:      pull mass down and release
 * Clear:      quit
 *
 * Physics: x'' = -(k/m)x - (c/m)v
 * m = 1 kg, dt = 1/60 s, 4 sub-steps per frame (RK4)
 *
 * Left half:  spring + mass drawing
 * Right half: displacement vs time graph
 */

#define M      1.0f
#define DT     (1.0f/60.0f)
#define SUBSTEPS 4
#define SDT    (DT / SUBSTEPS)
#define GRAPH_W 150
#define GRAPH_H 180
#define GRAPH_X 165
#define GRAPH_Y 30
#define GRAPH_MAX_PTS 150
#define K_MIN  1.0f
#define K_MAX  50.0f
#define C_MIN  0.0f
#define C_MAX  2.0f
#define PULL_DIST 40.0f   /* pixels the mass is pulled down on Enter */

static float x_pos;        /* displacement in pixels, positive = down */
static float v_pos;        /* velocity in pixels/s */
static float k_val;        /* spring constant */
static float c_val;        /* damping coefficient */
static int   running;      /* simulation active */
static int   frame_count;  /* frames since last release */

/* Graph ring buffer: stores displacement samples */
static int16_t g_buf[GRAPH_MAX_PTS];
static int     g_head;
static int     g_count;

/* Previous key states for edge detection */
static int prev_left, prev_right, prev_up, prev_down, prev_enter;

/* RK4 step for x'' = -(k/m)x - (c/m)v
 * State: [x, v]
 * Derivatives: [v, -(k/m)*x - (c/m)*v]
 */
static void rk4_step(float *x, float *v, float dt)
{
    float k_m = k_val / M;
    float c_m = c_val / M;
    float x0 = *x, v0 = *v;

    /* k1 */
    float k1x = v0;
    float k1v = -k_m * x0 - c_m * v0;

    /* k2 */
    float x2 = x0 + 0.5f * dt * k1x;
    float v2 = v0 + 0.5f * dt * k1v;
    float k2x = v2;
    float k2v = -k_m * x2 - c_m * v2;

    /* k3 */
    float x3 = x0 + 0.5f * dt * k2x;
    float v3 = v0 + 0.5f * dt * k2v;
    float k3x = v3;
    float k3v = -k_m * x3 - c_m * v3;

    /* k4 */
    float x4 = x0 + dt * k3x;
    float v4 = v0 + dt * k3v;
    float k4x = v4;
    float k4v = -k_m * x4 - c_m * v4;

    *x += (dt / 6.0f) * (k1x + 2.0f * k2x + 2.0f * k3x + k4x);
    *v += (dt / 6.0f) * (k1v + 2.0f * k2v + 2.0f * k3v + k4v);
}

/* Draw a zig-zag spring from (cx, y_top) to (cx, y_bot) with n coils */
static void draw_spring(int cx, int y_top, int y_bot, int coils)
{
    int len = y_bot - y_top;
    if (len < 1) len = 1;
    int seg = len / (coils * 2);
    if (seg < 1) seg = 1;
    int amp = 8;
    int y = y_top;
    int i;
    gfx_SetColor(0x10); /* blue */
    gfx_Line(cx, y_top, cx, y_top + seg);
    for (i = 0; i < coils * 2; i++) {
        int x_off = (i & 1) ? amp : -amp;
        gfx_Line(cx, y, cx + x_off, y + seg);
        y += seg;
    }
    gfx_Line(cx, y, cx, y_bot);
}

/* Draw the mass as a filled square */
static void draw_mass(int cx, int cy, int size)
{
    gfx_SetColor(0xE0); /* red */
    gfx_FillRectangle(cx - size / 2, cy - size / 2, size, size);
    gfx_SetColor(0);
    gfx_Line(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2);
    gfx_Line(cx + size / 2, cy - size / 2, cx - size / 2, cy + size / 2);
}

/* Determine damping regime */
static const char *damping_regime(void)
{
    float crit = 2.0f * sqrtf(k_val * M);
    if (c_val < crit - 0.01f) return "Underdamped";
    if (c_val > crit + 0.01f) return "Overdamped";
    return "Critically damped";
}

/* Format a float as integer*100 for display (e.g. 3.50 -> 350) */
static int16_t f100(float f)
{
    return (int16_t)(f * 100.0f + 0.5f);
}

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();

    /* Initial state */
    k_val = 10.0f;
    c_val = 0.3f;
    x_pos = 0.0f;
    v_pos = 0.0f;
    running = 0;
    frame_count = 0;
    g_head = 0;
    g_count = 0;
    prev_left = prev_right = prev_up = prev_down = prev_enter = 0;

    for (;;) {
        kb_Scan();

        /* Edge-detect keys */
        int cur_left  = (kb_Data[7] & kb_Left)  ? 1 : 0;
        int cur_right = (kb_Data[7] & kb_Right) ? 1 : 0;
        int cur_up    = (kb_Data[7] & kb_Up)    ? 1 : 0;
        int cur_down  = (kb_Data[7] & kb_Down)  ? 1 : 0;
        int cur_enter = (kb_Data[6] & kb_Enter) ? 1 : 0;

        /* Quit */
        if (kb_Data[6] & kb_Clear) break;

        /* Adjust damping (Left/Right) */
        if (cur_left && !prev_left) {
            c_val -= 0.1f;
            if (c_val < C_MIN) c_val = C_MIN;
        }
        if (cur_right && !prev_right) {
            c_val += 0.1f;
            if (c_val > C_MAX) c_val = C_MAX;
        }

        /* Adjust spring constant (Up/Down) */
        if (cur_up && !prev_up) {
            k_val -= 1.0f;
            if (k_val < K_MIN) k_val = K_MIN;
        }
        if (cur_down && !prev_down) {
            k_val += 1.0f;
            if (k_val > K_MAX) k_val = K_MAX;
        }

        /* Enter: pull mass down and release */
        if (cur_enter && !prev_enter) {
            x_pos = PULL_DIST;
            v_pos = 0.0f;
            running = 1;
            frame_count = 0;
            g_head = 0;
            g_count = 0;
        }

        /* Save previous states */
        prev_left = cur_left;
        prev_right = cur_right;
        prev_up = cur_up;
        prev_down = cur_down;
        prev_enter = cur_enter;

        /* Integrate physics */
        if (running) {
            int s;
            for (s = 0; s < SUBSTEPS; s++) {
                rk4_step(&x_pos, &v_pos, SDT);
            }
            frame_count++;

            /* Store graph sample */
            int16_t sample = (int16_t)(x_pos * 10.0f); /* scale for graph */
            g_buf[g_head] = sample;
            g_head = (g_head + 1) % GRAPH_MAX_PTS;
            if (g_count < GRAPH_MAX_PTS) g_count++;

            /* Stop when nearly at rest */
            if (fabsf(x_pos) < 0.1f && fabsf(v_pos) < 0.1f) {
                x_pos = 0.0f;
                v_pos = 0.0f;
                running = 0;
            }
        }

        /* ---- Draw ---- */
        gfx_FillScreen(255); /* white background */

        /* Title */
        gfx_SetTextFGColor(0);
        gfx_PrintStringXY("Spring + Damping", 100, 2);

        /* Left panel: spring and mass */
        {
            int cx = 60;
            int anchor_y = 50;
            int rest_y = 140;
            int mass_y = rest_y + (int)(x_pos + 0.5f);
            int mass_size = 24;
            int coils = 8;

            /* Ceiling bar */
            gfx_SetColor(0);
            gfx_FillRectangle(cx - 30, anchor_y - 4, 60, 4);

            /* Spring */
            draw_spring(cx, anchor_y, mass_y - mass_size / 2, coils);

            /* Mass */
            draw_mass(cx, mass_y, mass_size);

            /* Equilibrium line (dashed) */
            gfx_SetColor(0x03); /* green */
            int i;
            for (i = 20; i < 100; i += 6) {
                gfx_SetPixel(i, rest_y);
            }

            /* Labels */
            gfx_SetTextFGColor(0);
            gfx_PrintStringXY("Equilibrium", 10, rest_y + 4);
            gfx_PrintStringXY("Mass", cx - 10, mass_y + 16);
        }

        /* Right panel: displacement vs time graph */
        {
            int gx = GRAPH_X;
            int gy = GRAPH_Y;
            int gw = GRAPH_W;
            int gh = GRAPH_H;
            int mid_y = gy + gh / 2;

            /* Graph border */
            gfx_SetColor(0);
            gfx_Line(gx, gy, gx + gw, gy);
            gfx_Line(gx, gy + gh, gx + gw, gy + gh);
            gfx_Line(gx, gy, gx, gy + gh);
            gfx_Line(gx + gw, gy, gx + gw, gy + gh);

            /* Center line (zero displacement) */
            gfx_SetColor(0x03);
            gfx_Line(gx, mid_y, gx + gw, mid_y);

            /* Label */
            gfx_SetTextFGColor(0);
            gfx_PrintStringXY("x(t)", gx + gw / 2 - 8, gy - 12);

            /* Plot the ring buffer */
            gfx_SetColor(0x10); /* blue trace */
            if (g_count > 1) {
                int i;
                int start = (g_head - g_count + GRAPH_MAX_PTS) % GRAPH_MAX_PTS;
                int prev_px = gx;
                int prev_py = mid_y - g_buf[start];
                for (i = 1; i < g_count; i++) {
                    int idx = (start + i) % GRAPH_MAX_PTS;
                    int px = gx + (i * gw) / (g_count - 1);
                    int py = mid_y - g_buf[idx];
                    /* Clamp */
                    if (py < gy) py = gy;
                    if (py > gy + gh) py = gy + gh;
                    gfx_Line(prev_px, prev_py, px, py);
                    prev_px = px;
                    prev_py = py;
                }
            }
        }

        /* Info panel at bottom */
        {
            int16_t k_disp = f100(k_val);
            int16_t c_disp = f100(c_val);
            float crit = 2.0f * sqrtf(k_val * M);
            int16_t crit_disp = f100(crit);
            const char *reg = damping_regime();

            gfx_SetTextFGColor(0);
            gfx_PrintStringXY("k=", 5, 210);
            gfx_SetTextXY(18, 210);
            gfx_PrintInt(k_disp, 3);
            gfx_PrintStringXY(".00 N/m", 48, 210);

            gfx_PrintStringXY("c=", 5, 222);
            gfx_SetTextXY(18, 222);
            gfx_PrintInt(c_disp, 3);
            gfx_PrintStringXY(".00", 48, 222);

            gfx_PrintStringXY("2*sqrt(km)=", 110, 210);
            gfx_SetTextXY(148, 210);
            gfx_PrintInt(crit_disp, 3);
            gfx_PrintStringXY(".00", 178, 210);

            gfx_PrintStringXY(reg, 110, 222);
        }

        /* Help line */
        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("< > c  ^ v k  Enter=release  Clear=quit", 5, 236);

        gfx_SwapDraw();
        delay(15);
    }

    gfx_End();
    return 0;
}
