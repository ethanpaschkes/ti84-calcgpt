#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Projectile motion simulator for TI-84 Plus CE */

#define MAX_TRAIL 200
#define G 9.8f
#define ANGLE_MIN 5.0f
#define ANGLE_MAX 85.0f
#define SPEED_MIN 5.0f
#define SPEED_MAX 40.0f
#define ANGLE_STEP 1.0f
#define SPEED_STEP 1.0f

static uint8_t wasLeft, wasRight, wasUp, wasDown, wasEnter;
static uint8_t trailX[MAX_TRAIL], trailY[MAX_TRAIL];
static uint8_t trailCount;
static float angle, speed;
static float worldW, worldH, scale;
static float px, py;
static uint8_t flying;
static float flightTime, maxH, range;

static void reset(void)
{
    flying = 0;
    trailCount = 0;
    px = 0.0f;
    py = 0.0f;
    flightTime = 0.0f;
    maxH = 0.0f;
    range = 0.0f;
}

static void fire(void)
{
    float rad = angle * 3.14159265f / 180.0f;
    float vx = speed * cosf(rad);
    float vy = speed * sinf(rad);
    flightTime = 2.0f * vy / G;
    maxH = vy * vy / (2.0f * G);
    range = vx * flightTime;

    /* Scale world to fit screen with margins */
    worldW = range > 1.0f ? range : 1.0f;
    worldH = maxH > 1.0f ? maxH : 1.0f;
    float sx = (320.0f - 40.0f) / worldW;
    float sy = (240.0f - 60.0f) / worldH;
    scale = sx < sy ? sx : sy;

    px = 0.0f;
    py = 0.0f;
    trailCount = 0;
    flying = 1;
}

static void updateTrail(void)
{
    if (trailCount < MAX_TRAIL) {
        trailX[trailCount] = (uint8_t)(px * scale + 20);
        trailY[trailCount] = (uint8_t)(220 - py * scale);
        trailCount++;
    }
}

static void drawTrail(void)
{
    uint8_t i;
    for (i = 1; i < trailCount; i++) {
        gfx_SetColor(0x03);
        gfx_SetPixel(trailX[i - 1], trailY[i - 1]);
        gfx_SetPixel(trailX[i], trailY[i]);
    }
}

static void drawProjectile(void)
{
    uint8_t x = (uint8_t)(px * scale + 20);
    uint8_t y = (uint8_t)(220 - py * scale);
    gfx_SetColor(0xE0);
    gfx_FillCircle(x, y, 3);
}

static void drawGround(void)
{
    gfx_SetColor(0x03);
    gfx_Line(0, 220, 320, 220);
}

static void drawLabels(void)
{
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("Angle:", 5, 5);
    gfx_PrintStringXY("Speed:", 5, 15);
    gfx_PrintStringXY("Time:", 160, 5);
    gfx_PrintStringXY("MaxH:", 160, 15);
    gfx_PrintStringXY("Range:", 160, 25);
    gfx_PrintStringXY("X:", 5, 230);
    gfx_PrintStringXY("Y:", 100, 230);
}

static void drawValues(void)
{
    gfx_SetTextFGColor(0xE0);
    gfx_SetTextXY(55, 5);
    gfx_PrintInt((int)(angle + 0.5f), 3);
    gfx_SetTextXY(85, 5);
    gfx_PrintStringXY("deg", 85, 5);
    gfx_SetTextXY(55, 15);
    gfx_PrintInt((int)(speed + 0.5f), 2);
    gfx_SetTextXY(85, 15);
    gfx_PrintStringXY("m/s", 85, 15);

    gfx_SetTextXY(200, 5);
    gfx_PrintInt((int)(flightTime * 100 + 0.5f), 4);
    gfx_SetTextXY(235, 5);
    gfx_PrintStringXY("cs", 235, 5);
    gfx_SetTextXY(200, 15);
    gfx_PrintInt((int)(maxH * 100 + 0.5f), 4);
    gfx_SetTextXY(235, 15);
    gfx_PrintStringXY("cm", 235, 15);
    gfx_SetTextXY(200, 25);
    gfx_PrintInt((int)(range * 100 + 0.5f), 4);
    gfx_SetTextXY(235, 25);
    gfx_PrintStringXY("cm", 235, 25);

    gfx_SetTextXY(25, 230);
    gfx_PrintInt((int)(px * 100 + 0.5f), 4);
    gfx_SetTextXY(120, 230);
    gfx_PrintInt((int)(py * 100 + 0.5f), 4);
}

static void drawHelp(void)
{
    gfx_SetTextFGColor(0x10);
    gfx_PrintStringXY("L/R:Angle U/D:Speed Enter:Fire", 5, 215);
}

int main(void)
{
    angle = 45.0f;
    speed = 20.0f;
    reset();

    gfx_Begin();
    gfx_SetDrawBuffer();

    for (;;) {
        kb_Scan();

        /* Edge-detect keys */
        uint8_t leftNow = kb_Data[7] & kb_Left;
        uint8_t rightNow = kb_Data[7] & kb_Right;
        uint8_t upNow = kb_Data[7] & kb_Up;
        uint8_t downNow = kb_Data[7] & kb_Down;
        uint8_t enterNow = kb_Data[6] & kb_Enter;
        uint8_t clearNow = kb_Data[6] & kb_Clear;

        if (clearNow) break;

        /* Adjust angle */
        if (leftNow && !wasLeft) {
            angle -= ANGLE_STEP;
            if (angle < ANGLE_MIN) angle = ANGLE_MIN;
        }
        if (rightNow && !wasRight) {
            angle += ANGLE_STEP;
            if (angle > ANGLE_MAX) angle = ANGLE_MAX;
        }

        /* Adjust speed */
        if (upNow && !wasUp) {
            speed += SPEED_STEP;
            if (speed > SPEED_MAX) speed = SPEED_MAX;
        }
        if (downNow && !wasDown) {
            speed -= SPEED_STEP;
            if (speed < SPEED_MIN) speed = SPEED_MIN;
        }

        /* Fire projectile */
        if (enterNow && !wasEnter) {
            fire();
        }

        wasLeft = leftNow;
        wasRight = rightNow;
        wasUp = upNow;
        wasDown = downNow;
        wasEnter = enterNow;

        /* Update projectile if flying */
        if (flying) {
            float rad = angle * 3.14159265f / 180.0f;
            float vx = speed * cosf(rad);
            float vy = speed * sinf(rad);
            float dt = 0.05f;
            px += vx * dt;
            /* Recalculate py properly */
            float t = px / (vx > 0.01f ? vx : 0.01f);
            py = vy * t - 0.5f * G * t * t;
            if (py < 0.0f) py = 0.0f;
            updateTrail();
            if (px >= range - 0.01f) {
                px = range;
                py = 0.0f;
                flying = 0;
            }
        }

        /* Draw everything */
        gfx_FillScreen(255);
        drawGround();
        drawTrail();
        drawProjectile();
        drawLabels();
        drawValues();
        drawHelp();
        gfx_SwapDraw();

        delay(15);
    }

    gfx_End();
    return 0;
}
