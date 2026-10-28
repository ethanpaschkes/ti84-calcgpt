/* DIAG6: runs the solver's maths one step at a time and shows how far it got.
 * If the calculator crashes, tell me the LAST "OK" number that was on the screen. */
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <stdint.h>
#include <string.h>
#include "expr.h"
#include "mathx.h"
#include "poly.h"
#include "solve.h"
#include "triangle.h"

static int line_y = 28;

static void ok(const char *what)
{
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY(what, 10, line_y);
    line_y += 12;
    gfx_SwapDraw();
    gfx_SetDrawBuffer();
}

static real pf(real x, void *c)
{
    return mx_run((const struct mx_prog *)c, x);
}

int main(void)
{
    struct mx_prog p;
    struct poly po;
    struct poly_roots pr;
    struct mx_eval ev;
    static struct mx_solution sol;
    struct tri_in in;
    static char out[1800];
    real v;

    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("DIAG6  step-by-step maths test", 10, 10);
    gfx_SwapDraw();
    gfx_FillScreen(255);
    gfx_PrintStringXY("DIAG6  step-by-step maths test", 10, 10);

    ok("1 started");
    if (!mx_compile("x^2-4x+3", 0, &p)) ok("compile FAILED"); else ok("2 compile ok");
    v = mx_run(&p, 2.0L);
    ok(v == -1.0L ? "3 run ok (value -1)" : "3 run gave a wrong value");
    if (poly_fit(pf, &p, &po)) ok("4 poly_fit ok"); else ok("4 poly_fit said no");
    poly_solve(&po, &pr);
    ok(pr.nrat == 2 ? "5 poly_solve ok (2 roots)" : "5 poly_solve wrong");
    mx_evaluate("[3|4]+[5|6]", &ev);
    ok(strcmp(ev.exact, "[19|12]") == 0 ? "6 fractions ok (19/12)" : "6 fractions wrong");
    mx_degrees = true;
    mx_solve("x^2", "4x-3", &sol);
    ok(sol.kind == SK_LIST && sol.n == 2 ? "7 solve x^2=4x-3 ok" : "7 solve wrong");
    mx_solve("sin^2 x+cos^2 x", "1", &sol);
    ok(sol.kind == SK_ALL ? "8 identity check ok" : "8 identity wrong");
    ok("9 starting trig solve (slow)...");
    mx_solve("sin(x)", "[1|2]", &sol);
    ok(sol.kind == SK_LIST && sol.n == 2 ? "10 trig solve sin x=1/2 ok" : "10 trig solve wrong");
    memset(&in, 0, sizeof(in));
    in.has_s[0] = in.has_s[1] = in.has_s[2] = true;
    in.s[0] = 6; in.s[1] = 8; in.s[2] = 11;
    tri_work(&in, out, sizeof(out));
    ok("11 triangle ok");
    ok("ALL DONE. Press any key.");
    while (!os_GetCSC()) {
    }
    gfx_End();
    return 0;
}
