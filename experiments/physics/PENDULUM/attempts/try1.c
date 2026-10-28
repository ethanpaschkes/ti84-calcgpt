#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Simple pendulum: exact ODE theta'' = -(g/L) sin(theta), velocity Verlet.
 * Left/Right: length 0.5-2.0 m.  Up/Down: start angle 5-80 deg.
 * Enter: release.  Clear: quit. */

#define G 9.81f
#define DT 0.005f
#define MAXPTS 300
#define PI 3.14159265f

static float theta, omega, L, startAngle;
static int running = 0;
static int t = 0;
static int t0 = 0;
static int t1 = 0;
static int measured = 0;
static int prevSign = 0;
static int prevOmega = 0;
static int prevTheta = 0;
static int wasEnter = 0;
static int wasUp = 0, wasDown = 0, wasLeft = 0, wasRight = 0;

/* ring buffer of recent angles for the graph */
static float hist[MAXPTS];
static int histHead = 0;
static int histCount = 0;

static void resetSim(void)
{
    theta = startAngle * PI / 180.0f;
    omega = 0.0f;
    t = 0;
    t0 = 0;
    t1 = 0;
    measured = 0;
    prevSign = 0;
    prevOmega = 0;
    prevTheta = 0;
    histHead = 0;
    histCount = 0;
}

static void step(void)
{
    float alpha = -(G / L) * sinf(theta);
    /* velocity Verlet */
    theta += omega * DT + 0.5f * alpha * DT * DT;
    float alphaNew = -(G / L) * sinf(theta);
    omega += 0.5f * (alpha + alphaNew) * DT;
    t++;

    /* store in ring buffer */
    hist[histHead] = theta;
    histHead = (histHead + 1) % MAXPTS;
    if (histCount < MAXPTS) histCount++;

    /* detect upward zero crossing: theta changes from negative to positive
       and omega > 0 */
    int sign = (theta >= 0.0f) ? 1 : -1;
    if (prevSign == -1 && sign == 1 && omega > 0.0f) {
        if (t0 > 0) {
            t1 = t;
            measured = 1;
        } else {
            t0 = t;
        }
    }
    prevSign = sign;
}

static void drawPendulum(void)
{
    /* pivot at (160, 40), scale: 1 m = 80 px */
    int px = 160;
    int py = 40;
    float scale = 80.0f;
    int bx = (int)(px + L * scale * sinf(theta));
    int by = (int)(py + L * scale * cosf(theta));

    /* string */
    gfx_SetColor(0x10);
    gfx_Line(px, py, bx, by);

    /* pivot */
    gfx_SetColor(0);
    gfx_FillCircle(px, py, 3);

    /* bob */
    gfx_SetColor(0xE0);
    gfx_FillCircle(bx, by, 8);
}

static void drawGraph(void)
{
    /* graph area: x from 10 to 310, y from 130 to 220 */
    int gx0 = 10, gx1 = 310, gy0 = 130, gy1 = 220;
    int gw = gx1 - gx0;
    int gh = gy1 - gy0;
    int midY = (gy0 + gy1) / 2;

    /* background */
    gfx_SetColor(0);
    gfx_FillRectangle(gx0, gy0, gw, gh);

    /* axes */
    gfx_SetColor(0x03);
    gfx_Line(gx0, midY, gx1, midY);
    gfx_Line(gx0, gy0, gx0, gy1);

    /* plot angle vs time */
    gfx_SetColor(0xE0);
    int n = histCount;
    for (int i = 0; i < n; i++) {
        int idx = (histHead - n + i + MAXPTS) % MAXPTS;
        float a = hist[idx];
        /* map angle (-pi..pi) to y (gy1..gy0) */
        int y = midY - (int)(a / PI * (gh / 2));
        int x = gx0 + (int)((float)i / (float)MAXPTS * (float)gw);
        gfx_SetPixel(x, y);
    }
}

static void drawInfo(void)
{
    gfx_SetTextFGColor(0);
    gfx_SetTextXY(10, 5);
    gfx_PrintStringXY("Pendulum", 10, 5);

    /* length */
    int Lx100 = (int)(L * 100.0f);
    gfx_SetTextXY(10, 18);
    gfx_PrintStringXY("L=", 10, 18);
    gfx_PrintInt(Lx100, 3);
    gfx_PrintStringXY(" cm", 50, 18);

    /* start angle */
    int ang100 = (int)(startAngle * 100.0f);
    gfx_SetTextXY(10, 30);
    gfx_PrintStringXY("a=", 10, 30);
    gfx_PrintInt(ang100, 3);
    gfx_PrintStringXY(" deg", 50, 30);

    /* current angle */
    int curAng = (int)(theta * 180.0f / PI * 100.0f);
    gfx_SetTextXY(10, 125);
    gfx_PrintStringXY("th=", 10, 125);
    gfx_PrintInt(curAng, 4);
    gfx_PrintStringXY(" deg", 50, 125);

    /* small-angle period T = 2*pi*sqrt(L/g) */
    float Tsmall = 2.0f * PI * sqrtf(L / G);
    int Tsmall100 = (int)(Tsmall * 100.0f);
    gfx_SetTextXY(10, 225);
    gfx_PrintStringXY("T(small)=", 10, 225);
    gfx_PrintInt(Tsmall100, 4);
    gfx_PrintStringXY(" s", 80, 225);

    /* measured period */
    if (measured) {
        float Tmeas = (float)(t1 - t0) * DT;
        int Tmeas100 = (int)(Tmeas * 100.0f);
        gfx_SetTextXY(160, 225);
        gfx_PrintStringXY("T(meas)=", 160, 225);
        gfx_PrintInt(Tmeas100, 4);
        gfx_PrintStringXY(" s", 230, 225);
    } else {
        gfx_SetTextXY(160, 225);
        gfx_PrintStringXY("T(meas)= --", 160, 225);
    }

    /* help */
    gfx_SetTextFGColor(0x03);
    gfx_SetTextXY(10, 235);
    gfx_PrintStringXY("<L/R>len <U/D>ang <Ent>go <Cl>quit", 10, 235);
}

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();

    L = 1.0f;
    startAngle = 30.0f;
    running = 0;
    resetSim();

    for (;;) {
        kb_Scan();

        /* edge-detect */
        int enter = kb_Data[6] & kb_Enter;
        int clear = kb_Data[6] & kb_Clear;
        int up = kb_Data[7] & kb_Up;
        int down = kb_Data[7] & kb_Down;
        int left = kb_Data[7] & kb_Left;
        int right = kb_Data[7] & kb_Right;

        if (clear) break;

        if (enter && !wasEnter) {
            if (!running) {
                running = 1;
                resetSim();
            } else {
                running = 0;
            }
        }
        wasEnter = enter;

        if (!running) {
            if (left && !wasLeft) {
                L -= 0.05f;
                if (L < 0.5f) L = 0.5f;
            }
            wasLeft = left;
            if (right && !wasRight) {
                L += 0.05f;
                if (L > 2.0f) L = 2.0f;
            }
            wasRight = right;
            if (up && !wasUp) {
                startAngle += 1.0f;
                if (startAngle > 80.0f) startAngle = 80.0f;
            }
            wasUp = up;
            if (down && !wasDown) {
                startAngle -= 1.0f;
                if (startAngle < 5.0f) startAngle = 5.0f;
            }
            wasDown = down;
        } else {
            /* run several sub-steps per frame for speed */
            int steps = 4;
            for (int i = 0; i < steps; i++) {
                step();
            }
        }

        /* draw */
        gfx_FillScreen(0);
        drawPendulum();
        drawGraph();
        drawInfo();

        gfx_SwapDraw();
        delay(15);
    }

    gfx_End();
    return 0;
}
