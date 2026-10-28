/* DIAG9: times the maths on the calculator. Each line is printed BEFORE it runs, so if the
 * calculator freezes, the last line on screen is the one that hangs. */
#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "mathx.h"
#include "expr.h"
#include "solve.h"

#define BIG_STACK 20000
uint8_t big_stack[BIG_STACK];
unsigned saved_sp;

static int line_y = 18;

static void say(const char *s)
{
    gfx_SetColor(255);
    gfx_FillRectangle(0, line_y, 320, 10);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY(s, 4, line_y);
}

static void done(unsigned ms)
{
    gfx_PrintString("  ");
    gfx_PrintUInt(ms, 1);
    gfx_PrintString(" ms");
    line_y += 10;
}

static unsigned now_ms(void)
{
    return (unsigned)(timer_Get(1) / 33);
}

static void time_eval(const char *expr)
{
    static struct mx_eval ev;
    unsigned t;
    say(expr);
    t = now_ms();
    mx_evaluate(expr, &ev);
    done(now_ms() - t);
}

static void time_solve(const char *l, const char *r)
{
    static struct mx_solution sol;
    unsigned t;
    char buf[40] = "solve ";
    unsigned i = 6;
    const char *p;
    for (p = l; *p && i < 20; p++) buf[i++] = *p;
    buf[i++] = '=';
    for (p = r; *p && i < 38; p++) buf[i++] = *p;
    buf[i] = 0;
    say(buf);
    t = now_ms();
    mx_solve(l, r, &sol);
    done(now_ms() - t);
}

static int run(void)
{
    volatile long double x = 0.6L;
    unsigned t;
    int i;

    gfx_Begin();
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("DIAG9 timing (ms)", 4, 4);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);
    mx_degrees = true;

    say("10 x sinl");
    t = now_ms();
    for (i = 0; i < 10; i++) x = sinl(x);
    done(now_ms() - t);
    {
        volatile long double a = 1.234L, b = 0.987L;
        volatile float fa = 1.234f, fb = 0.987f;
        volatile double da = 0.6;
        say("1000 x long double mul");
        t = now_ms();
        for (i = 0; i < 1000; i++) a = a * b;
        done(now_ms() - t);
        say("1000 x long double div");
        t = now_ms();
        for (i = 0; i < 1000; i++) a = a / b;
        done(now_ms() - t);
        say("1000 x float mul");
        t = now_ms();
        for (i = 0; i < 1000; i++) fa = fa * fb;
        done(now_ms() - t);
        say("10 x float sinf");
        t = now_ms();
        for (i = 0; i < 10; i++) fa = sinf(fa);
        done(now_ms() - t);
        say("10 x double sin");
        t = now_ms();
        for (i = 0; i < 10; i++) da = sin(da);
        done(now_ms() - t);
        say("10 x long double sqrtl");
        t = now_ms();
        for (i = 0; i < 10; i++) a = sqrtl(2.0L + i);
        done(now_ms() - t);
    }
    say("10 x powl");
    t = now_ms();
    for (i = 0; i < 10; i++) x = powl(2.5L, 1.3L);
    done(now_ms() - t);

    time_eval("sin(0.6)");
    time_eval("0.");
    time_eval("sin(0)");
    time_eval("sin(0.)");
    time_eval("3/4+5/6");
    time_eval("x^2-4x+3");
    time_solve("x^2", "4x-3");
    time_solve("2x", "6");
    time_solve("sin(x)", "5");
    time_solve("sin(x)", "1/2");
    say("ALL DONE - press a key");
    while (!os_GetCSC()) {
    }
    gfx_End();
    return 0;
}

int main(void)
{
    int r;
    __asm__ volatile("ld (_saved_sp), sp");
    __asm__ volatile("ld hl, _big_stack + 20000\n\tld sp, hl" ::: "hl");
    r = run();
    __asm__ volatile("ld sp, (_saved_sp)");
    return r;
}
