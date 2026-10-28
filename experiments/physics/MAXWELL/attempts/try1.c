#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Maxwell-Boltzmann speed distribution demo
 * ~50 equal-mass particles in a 2D box with elastic wall bounces
 * and pairwise elastic collisions.  Start with identical speeds in
 * random directions; the distribution spreads into a bell shape.
 * Live 10-bin histogram on the right, mean speed shown as integer.
 * Up/Down scale all speeds (temperature).  Clear quits.
 */

#define N        50
#define BINS     10
#define BOX_X    10
#define BOX_Y    10
#define BOX_W    180
#define BOX_H    220
#define HIST_X   200
#define HIST_Y   10
#define HIST_W   110
#define HIST_H   220
#define MAX_SPD  120.0f   /* speed cap for histogram scaling */

static struct {
    float x, y, vx, vy;
} p[N];

static int  hist[BINS];
static int  histMax = 1;
static int  wasUp = 0, wasDown = 0;
static int  frame = 0;

static void initParticles(float speed)
{
    int i;
    for (i = 0; i < N; i++) {
        float a = (float)(rand() % 3600) * 0.0017453292f; /* 0..2pi */
        p[i].x  = BOX_X + 10.0f + (float)(rand() % (BOX_W - 20));
        p[i].y  = BOX_Y + 10.0f + (float)(rand() % (BOX_H - 20));
        p[i].vx = speed * cosf(a);
        p[i].vy = speed * sinf(a);
    }
}

/* 2D elastic collision between two equal-mass particles.
 * If they overlap, separate them and swap the normal components. */
static void collide(int i, int j)
{
    float dx = p[j].x - p[i].x;
    float dy = p[j].y - p[i].y;
    float d2 = dx * dx + dy * dy;
    float r  = 4.0f;            /* particle radius */
    float minD = r + r;

    if (d2 >= minD * minD || d2 < 0.0001f) return;

    float d  = sqrtf(d2);
    float nx = dx / d;
    float ny = dy / d;

    /* separate */
    float overlap = minD - d;
    p[i].x -= nx * overlap * 0.5f;
    p[i].y -= ny * overlap * 0.5f;
    p[j].x += nx * overlap * 0.5f;
    p[j].y += ny * overlap * 0.5f;

    /* relative velocity along normal */
    float dvx = p[j].vx - p[i].vx;
    float dvy = p[j].vy - p[i].vy;
    float vn  = dvx * nx + dvy * ny;

    if (vn > 0.0f) return;      /* already separating */

    /* equal masses: swap normal components */
    p[i].vx += vn * nx;
    p[i].vy += vn * ny;
    p[j].vx -= vn * nx;
    p[j].vy -= vn * ny;
}

static void step(void)
{
    int i, j;

    /* move + wall bounce */
    for (i = 0; i < N; i++) {
        p[i].x += p[i].vx;
        p[i].y += p[i].vy;

        if (p[i].x < BOX_X + 4.0f) {
            p[i].x = BOX_X + 4.0f;
            p[i].vx = -p[i].vx;
        } else if (p[i].x > BOX_X + BOX_W - 4.0f) {
            p[i].x = BOX_X + BOX_W - 4.0f;
            p[i].vx = -p[i].vx;
        }
        if (p[i].y < BOX_Y + 4.0f) {
            p[i].y = BOX_Y + 4.0f;
            p[i].vy = -p[i].vy;
        } else if (p[i].y > BOX_Y + BOX_H - 4.0f) {
            p[i].y = BOX_Y + BOX_H - 4.0f;
            p[i].vy = -p[i].vy;
        }
    }

    /* pairwise collisions O(n^2) */
    for (i = 0; i < N; i++)
        for (j = i + 1; j < N; j++)
            collide(i, j);
}

static void buildHist(void)
{
    int i, b;
    for (b = 0; b < BINS; b++) hist[b] = 0;

    for (i = 0; i < N; i++) {
        float s = sqrtf(p[i].vx * p[i].vx + p[i].vy * p[i].vy);
        b = (int)(s / MAX_SPD * BINS);
        if (b >= BINS) b = BINS - 1;
        if (b < 0) b = 0;
        hist[b]++;
    }

    histMax = 1;
    for (b = 0; b < BINS; b++)
        if (hist[b] > histMax) histMax = hist[b];
}

static int meanSpeed(void)
{
    int i;
    float sum = 0.0f;
    for (i = 0; i < N; i++)
        sum += sqrtf(p[i].vx * p[i].vx + p[i].vy * p[i].vy);
    return (int)(sum / N + 0.5f);
}

int main(void)
{
    int i, b;
    int curUp, curDown;

    gfx_Begin();
    gfx_SetDrawBuffer();

    initParticles(30.0f);   /* all same speed, random directions */

    for (;;) {
        kb_Scan();

        /* edge-detect Up/Down for temperature scaling */
        curUp   = (kb_Data[7] & kb_Up)   ? 1 : 0;
        curDown = (kb_Data[7] & kb_Down) ? 1 : 0;

        if (curUp && !wasUp) {
            for (i = 0; i < N; i++) {
                p[i].vx *= 1.15f;
                p[i].vy *= 1.15f;
            }
        }
        if (curDown && !wasDown) {
            for (i = 0; i < N; i++) {
                p[i].vx *= 0.87f;
                p[i].vy *= 0.87f;
            }
        }
        wasUp   = curUp;
        wasDown = curDown;

        if (kb_Data[6] & kb_Clear) break;

        /* physics: a few sub-steps per frame for smoother collisions */
        for (i = 0; i < 2; i++) step();

        /* rebuild histogram every 3 frames to save CPU */
        frame++;
        if (frame % 3 == 0) buildHist();

        /* ---- draw ---- */
        gfx_FillScreen(0);

        /* box outline */
        gfx_SetColor(0x10);
        gfx_Rectangle(BOX_X, BOX_Y, BOX_W, BOX_H);

        /* particles */
        gfx_SetColor(0xE0);
        for (i = 0; i < N; i++) {
            int ix = (int)p[i].x;
            int iy = (int)p[i].y;
            gfx_FillCircle(ix, iy, 3);
        }

        /* histogram panel */
        gfx_SetColor(0x03);
        gfx_Rectangle(HIST_X, HIST_Y, HIST_W, HIST_H);

        /* histogram bars */
        gfx_SetColor(0x10);
        {
            int barW = HIST_W / BINS;
            int barH, bx, by;
            for (b = 0; b < BINS; b++) {
                barH = (hist[b] * (HIST_H - 30)) / histMax;
                if (barH < 0) barH = 0;
                bx = HIST_X + b * barW + 1;
                by = HIST_Y + HIST_H - 20 - barH;
                gfx_FillRectangle(bx, by, barW - 2, barH);
            }
        }

        /* histogram baseline */
        gfx_SetColor(0x03);
        gfx_Line(HIST_X, HIST_Y + HIST_H - 20,
                 HIST_X + HIST_W, HIST_Y + HIST_H - 20);

        /* labels */
        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("SPEED HIST", HIST_X + 10, HIST_Y + 2);

        gfx_SetTextFGColor(0xE0);
        {
            int ms = meanSpeed();
            char buf[24];
            /* build "MEAN=NN" manually */
            int pos = 0;
            buf[pos++] = 'M'; buf[pos++] = 'E';
            buf[pos++] = 'A'; buf[pos++] = 'N';
            buf[pos++] = '=';
            if (ms >= 100) {
                buf[pos++] = '0' + ms / 100;
                buf[pos++] = '0' + (ms / 10) % 10;
                buf[pos++] = '0' + ms % 10;
            } else if (ms >= 10) {
                buf[pos++] = '0' + ms / 10;
                buf[pos++] = '0' + ms % 10;
            } else {
                buf[pos++] = '0' + ms;
            }
            buf[pos] = 0;
            gfx_PrintStringXY(buf, HIST_X + 10, HIST_Y + HIST_H - 16);
        }

        /* x-axis label for histogram */
        gfx_SetTextFGColor(0x03);
        gfx_PrintStringXY("0", HIST_X + 2, HIST_Y + HIST_H - 18);
        gfx_PrintStringXY("MAX", HIST_X + HIST_W - 18, HIST_Y + HIST_H - 18);

        /* help line at bottom */
        gfx_SetTextFGColor(0xE0);
        gfx_PrintStringXY("UP/DN=T  CLR=QUIT", 10, 230);

        gfx_SwapDraw();
        delay(10);
    }

    gfx_End();
    return 0;
}
