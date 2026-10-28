/* TRIGTOOL: unit circle explorer and inverse trig for the TI-84 Plus CE.
 *   Left/Right: angle -/+ 15 deg (or value -/+ 0.05)
 *   Up/Down:    angle +/- 1 deg  (or value +/- 0.01)
 *   Enter:      switch between Unit Circle and Inverse mode
 *   Clear:      quit
 */
#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdio.h>

#define BLACK  0x00
#define WHITE  0xFF
#define RED    0xE0
#define GREEN  0x03
#define BLUE   0x10
#define GRAY   0xB6

#define CX 80
#define CY 120
#define R  68
#define TEXT_X 165
#define PI_F 3.14159265f

enum { MODE_CIRCLE, MODE_INVERSE };

static int gcd(int a, int b)
{
    while (b) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* Write v to buf with 3 decimals, e.g. -0.707 */
static void fmt(char *buf, float v)
{
    int neg = v < 0;
    long scaled = (long)(fabsf(v) * 1000.0f + 0.5f);
    sprintf(buf, "%s%d.%03d", (neg && scaled) ? "-" : "", (int)(scaled / 1000), (int)(scaled % 1000));
}

/* Exact value of sin or cos at a multiple of 30 or 45 degrees, else NULL. */
static const char *exact_value(int deg, int want_sin, int *negative)
{
    static const char *const sin_ref[] = {"0", "1/2", "sqrt2/2", "sqrt3/2", "1"};
    static const char *const cos_ref[] = {"1", "sqrt3/2", "sqrt2/2", "1/2", "0"};
    int ref, sin_neg, cos_neg, index;

    if (deg % 30 && deg % 45) {
        return NULL;
    }
    if (deg <= 90) {
        ref = deg; sin_neg = 0; cos_neg = 0;
    } else if (deg <= 180) {
        ref = 180 - deg; sin_neg = 0; cos_neg = 1;
    } else if (deg <= 270) {
        ref = deg - 180; sin_neg = 1; cos_neg = 1;
    } else {
        ref = 360 - deg; sin_neg = 1; cos_neg = 0;
    }
    index = ref == 0 ? 0 : ref == 30 ? 1 : ref == 45 ? 2 : ref == 60 ? 3 : 4;
    *negative = want_sin ? sin_neg : cos_neg;
    if (want_sin ? (index == 0) : (index == 4)) {
        *negative = 0;
    }
    return want_sin ? sin_ref[index] : cos_ref[index];
}

static void draw_circle_view(float rad, int show_projection)
{
    int px = CX + (int)(R * cosf(rad));
    int py = CY - (int)(R * sinf(rad));

    gfx_SetColor(GRAY);
    gfx_HorizLine(CX - R - 8, CY, 2 * R + 16);
    gfx_VertLine(CX, CY - R - 8, 2 * R + 16);
    gfx_SetColor(BLACK);
    gfx_Circle(CX, CY, R);

    if (show_projection) {
        gfx_SetColor(RED);
        gfx_VertLine(px, py < CY ? py : CY, py < CY ? CY - py : py - CY);
        gfx_SetColor(GREEN);
        gfx_HorizLine(px < CX ? px : CX, py, px < CX ? CX - px : px - CX);
    }
    gfx_SetColor(BLUE);
    gfx_Line(CX, CY, px, py);
    gfx_FillCircle(px, py, 3);
}

static void line(const char *label, float value, int y)
{
    char num[16];
    gfx_PrintStringXY(label, TEXT_X, y);
    fmt(num, value);
    gfx_PrintStringXY(num, TEXT_X + 40, y);
}

static void draw_unit_circle(int deg)
{
    float rad = deg * PI_F / 180.0f;
    char buf[40];
    int g = gcd(deg, 180);
    int neg;
    const char *ex;

    draw_circle_view(rad, 1);

    gfx_PrintStringXY("UNIT CIRCLE", TEXT_X, 12);
    sprintf(buf, "%d deg", deg);
    gfx_PrintStringXY(buf, TEXT_X, 32);
    if (deg == 0) {
        sprintf(buf, "= 0 rad");
    } else if (deg == 180) {
        sprintf(buf, "= pi rad");
    } else if (deg / g == 1) {
        sprintf(buf, "= pi/%d rad", 180 / g);
    } else {
        sprintf(buf, "= %d pi/%d rad", deg / g, 180 / g);
    }
    gfx_PrintStringXY(buf, TEXT_X, 44);
    line("rad", rad, 56);

    line("sin", sinf(rad), 80);
    gfx_SetTextFGColor(RED);
    ex = exact_value(deg, 1, &neg);
    if (ex) {
        sprintf(buf, "= %s%s", neg ? "-" : "", ex);
        gfx_PrintStringXY(buf, TEXT_X, 92);
    }
    gfx_SetTextFGColor(BLACK);

    line("cos", cosf(rad), 112);
    gfx_SetTextFGColor(GREEN);
    ex = exact_value(deg, 0, &neg);
    if (ex) {
        sprintf(buf, "= %s%s", neg ? "-" : "", ex);
        gfx_PrintStringXY(buf, TEXT_X, 124);
    }
    gfx_SetTextFGColor(BLACK);

    if (deg == 90 || deg == 270) {
        gfx_PrintStringXY("tan  undefined", TEXT_X, 144);
    } else {
        line("tan", tanf(rad), 144);
    }

    gfx_PrintStringXY("L/R 15  U/D 1", TEXT_X, 200);
    gfx_PrintStringXY("Enter: inverse", TEXT_X, 212);
    gfx_PrintStringXY("Clear: quit", TEXT_X, 224);
}

static void draw_inverse(int hundredths)
{
    float x = hundredths / 100.0f;
    float as = asinf(x);
    float ac = acosf(x);
    float at = atanf(x);
    char buf[24];

    draw_circle_view(as, 1);

    gfx_PrintStringXY("INVERSE TRIG", TEXT_X, 12);
    line("x", x, 32);

    gfx_PrintStringXY("arcsin(x)", TEXT_X, 56);
    line("rad", as, 68);
    sprintf(buf, "deg %d", (int)(as * 180.0f / PI_F + (as < 0 ? -0.5f : 0.5f)));
    gfx_PrintStringXY(buf, TEXT_X, 80);

    gfx_PrintStringXY("arccos(x)", TEXT_X, 104);
    line("rad", ac, 116);
    sprintf(buf, "deg %d", (int)(ac * 180.0f / PI_F + 0.5f));
    gfx_PrintStringXY(buf, TEXT_X, 128);

    gfx_PrintStringXY("arctan(x)", TEXT_X, 152);
    line("rad", at, 164);
    sprintf(buf, "deg %d", (int)(at * 180.0f / PI_F + (at < 0 ? -0.5f : 0.5f)));
    gfx_PrintStringXY(buf, TEXT_X, 176);

    gfx_PrintStringXY("L/R .05  U/D .01", TEXT_X, 200);
    gfx_PrintStringXY("Enter: unit circle", TEXT_X, 212);
    gfx_PrintStringXY("Clear: quit", TEXT_X, 224);
}

int main(void)
{
    int mode = MODE_CIRCLE;
    int deg = 45;
    int value = 50; /* hundredths, -100..100 */
    int enter_was_down = 0;

    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(WHITE);
    gfx_SetTextTransparentColor(WHITE);
    gfx_SetTextFGColor(BLACK);

    for (;;) {
        int enter_down;

        kb_Scan();
        if (kb_Data[6] & kb_Clear) {
            break;
        }

        enter_down = (kb_Data[6] & kb_Enter) != 0;
        if (enter_down && !enter_was_down) {
            mode = mode == MODE_CIRCLE ? MODE_INVERSE : MODE_CIRCLE;
        }
        enter_was_down = enter_down;

        if (mode == MODE_CIRCLE) {
            if (kb_Data[7] & kb_Right) deg += 15;
            if (kb_Data[7] & kb_Left)  deg -= 15;
            if (kb_Data[7] & kb_Up)    deg += 1;
            if (kb_Data[7] & kb_Down)  deg -= 1;
            deg = ((deg % 360) + 360) % 360;
        } else {
            if (kb_Data[7] & kb_Right) value += 5;
            if (kb_Data[7] & kb_Left)  value -= 5;
            if (kb_Data[7] & kb_Up)    value += 1;
            if (kb_Data[7] & kb_Down)  value -= 1;
            if (value > 100) value = 100;
            if (value < -100) value = -100;
        }

        gfx_FillScreen(WHITE);
        if (mode == MODE_CIRCLE) {
            draw_unit_circle(deg);
        } else {
            draw_inverse(value);
        }
        gfx_SwapDraw();
        delay(120);
    }

    gfx_End();
    return 0;
}
