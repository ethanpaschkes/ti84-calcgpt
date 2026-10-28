/* GRAPHER: a live, Desmos-style function grapher for the TI-84 Plus CE.
 *
 * Top panel: your equations (y = ...). The graph below updates as you type.
 *   Enter       types =  (y=sin(x), or A=B to see where two things are equal)
 *   Up/Down     pick a row (Down on the last row starts a new one, up to 8)
 *   Left/Right  move the cursor      2nd+X types y
 *   /           makes a stacked fraction from the term to its left
 *   ( and sin( etc. close themselves; ) or Right steps over the closer
 *   Del         backspace           Clear       empty row (again = delete row)
 *   Graph       sliders: rows like a=2 (letters a b c d k m n); use a in other
 *               rows, then Left/Right change it, Up/Down pick a slider, Enter step
 *   Stat        value table for your functions
 *   x^2+y^2=4   curves with y are drawn as implicit curves (2nd+X types y)
 *   Window      pan (arrows) and zoom (+ / -), Window or Clear to return
 *   Trace       step through the numbered key points, Trace or Clear to return
 *   Zoom        reset the view      Mode        quit
 * Key points: zeros, y-intercept, maxima, minima and where curves cross.
 * Amplitude, midline and period are shown only when a curve really repeats.
 */
#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <ti/getcsc.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* palette indices, defined in set_palette() */
#define BLACK  0
#define RED    1
#define BLUE   2
#define GREEN  3
#define ORANGE 4
#define PURPLE 5
#define TEAL   6
#define PINK   7
#define BROWN  8
#define GRID   9
#define SELBG  10
#define DKGRAY 11
#define WHITE  255

#define NUM_EQ   8
#define EQ_LEN   34
#define SCREEN_W 320
#define SCREEN_H 240
#define ROW_H    26
#define GY       52
#define SB_Y     228
#define GH       (SB_Y - GY)
#define MAX_PT   24
#define MAX_EXT  8

enum { M_EDIT, M_POINTS, M_SLIDER };

static const char *notice;
static float slider_step = 0.1f;
enum { K_ZERO, K_YINT, K_MAX, K_MIN, K_CROSS, K_SOL };

static const uint8_t eq_color[NUM_EQ] = {RED, BLUE, GREEN, ORANGE, PURPLE, TEAL, PINK, BROWN};
static const char *const kind_name[] = {"zero", "y-int", "max", "min", "cross", "sol"};

static char eq[NUM_EQ][EQ_LEN + 1] = {"sin(x)", "", "", "", "", "", "", ""};
static int nrows = 1;
static int sel = 0;
static int cur = 6;
static bool alt = false;
static bool alpha = false;
static int mode = M_EDIT;
static int psel = 0;

static float xmin, xmax, ymin, ymax;
static float ycache[NUM_EQ][SCREEN_W];
static bool curve_ok[NUM_EQ];

struct point {
    float x, y;
    int sx, sy;
    uint8_t kind, eq;
};
static struct point pts[MAX_PT];
static int npts;

static float maxx[NUM_EQ][MAX_EXT], maxy[NUM_EQ][MAX_EXT];
static float minx[NUM_EQ][MAX_EXT], miny[NUM_EQ][MAX_EXT];
static int nmax[NUM_EQ], nmin[NUM_EQ];
static int eq_kind[NUM_EQ];
static unsigned long eq_uses[NUM_EQ];
static int eq_rhs[NUM_EQ];

static void set_palette(void)
{
    gfx_palette[RED]    = gfx_RGBTo1555(220, 40, 40);
    gfx_palette[BLUE]   = gfx_RGBTo1555(40, 90, 220);
    gfx_palette[GREEN]  = gfx_RGBTo1555(30, 150, 60);
    gfx_palette[ORANGE] = gfx_RGBTo1555(240, 140, 0);
    gfx_palette[PURPLE] = gfx_RGBTo1555(150, 50, 200);
    gfx_palette[TEAL]   = gfx_RGBTo1555(0, 160, 170);
    gfx_palette[PINK]   = gfx_RGBTo1555(210, 60, 150);
    gfx_palette[BROWN]  = gfx_RGBTo1555(130, 90, 40);
    gfx_palette[GRID]   = gfx_RGBTo1555(220, 220, 220);
    gfx_palette[SELBG]  = gfx_RGBTo1555(232, 238, 255);
    gfx_palette[DKGRAY] = gfx_RGBTo1555(120, 120, 120);
}

/* ---------- expression compiler and evaluator ---------- */

/* Each equation is compiled once into a small postfix program, then run for
 * every point. Sliders are read at run time, so moving one needs no recompile. */

#define NUM_FN 10
#define PARAMS "abcdfghijklmnopqrstuvwz"
#define NUM_PARAMS 23
#define PARAM_MASK 0x7FFFFFUL
#define VAR_Y 0x1000000UL
#define VAR_X 0x2000000UL

#define PROG_OPS 80
#define PROG_CONSTS 20
#define STACK_DEPTH 16

enum { OP_CONST, OP_X, OP_Y, OP_PARAM, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_NEG, OP_FN };

struct prog {
    uint8_t op[PROG_OPS];
    uint8_t arg[PROG_OPS];
    float k[PROG_CONSTS];
    uint8_t nops, nk;
};

static const char *const fn_names[NUM_FN] = {
    "asin", "acos", "atan", "sin", "cos", "tan", "sqrt", "ln", "log", "abs"
};
static float param[NUM_PARAMS];
static float yv;
static struct prog eq_prog[NUM_EQ][2];

static const char *p;
static struct prog *cp;
static int depth;
static bool perr;

static void emit(uint8_t op, uint8_t arg)
{
    if (cp->nops >= PROG_OPS) {
        perr = true;
        return;
    }
    cp->op[cp->nops] = op;
    cp->arg[cp->nops] = arg;
    cp->nops++;
    if (op <= OP_PARAM) {
        if (++depth > STACK_DEPTH) {
            perr = true;
        }
    } else if (op <= OP_POW) {
        depth--;
    }
}

static void emit_const(float v)
{
    if (cp->nk >= PROG_CONSTS) {
        perr = true;
        return;
    }
    cp->k[cp->nk] = v;
    emit(OP_CONST, cp->nk++);
}

static float apply_fn(int f, float v)
{
    switch (f) {
    case 0: return asinf(v);
    case 1: return acosf(v);
    case 2: return atanf(v);
    case 3: return sinf(v);
    case 4: return cosf(v);
    case 5: return tanf(v);
    case 6: return sqrtf(v);
    case 7: return logf(v);
    case 8: return log10f(v);
    default: return fabsf(v);
    }
}

/* Small whole-number powers by multiplication: much faster than powf. */
static float power(float base, float e)
{
    int n = (int)e;
    if (e == (float)n && n >= -16 && n <= 16) {
        float r = 1.0f;
        int m = n < 0 ? -n : n;
        while (m) {
            if (m & 1) {
                r *= base;
            }
            base *= base;
            m >>= 1;
        }
        return n < 0 ? 1.0f / r : r;
    }
    return powf(base, e);
}

static float run(const struct prog *pr, float x)
{
    float st[STACK_DEPTH + 1];
    int sp = 0;
    int i;

    for (i = 0; i < pr->nops; i++) {
        switch (pr->op[i]) {
        case OP_CONST: st[sp++] = pr->k[pr->arg[i]]; break;
        case OP_X:     st[sp++] = x; break;
        case OP_Y:     st[sp++] = yv; break;
        case OP_PARAM: st[sp++] = param[pr->arg[i]]; break;
        case OP_ADD:   sp--; st[sp - 1] += st[sp]; break;
        case OP_SUB:   sp--; st[sp - 1] -= st[sp]; break;
        case OP_MUL:   sp--; st[sp - 1] *= st[sp]; break;
        case OP_DIV:   sp--; st[sp - 1] /= st[sp]; break;
        case OP_POW:   sp--; st[sp - 1] = power(st[sp - 1], st[sp]); break;
        case OP_NEG:   st[sp - 1] = -st[sp - 1]; break;
        default:       st[sp - 1] = apply_fn(pr->arg[i], st[sp - 1]); break;
        }
    }
    return st[0];
}

/* Which variables a string uses: bit i = PARAMS[i], plus VAR_X and VAR_Y. */
static unsigned long scan_vars(const char *s)
{
    unsigned long mask = 0;

    while (*s) {
        if (*s >= 'a' && *s <= 'z') {
            bool skipped = false;
            int f;
            for (f = 0; f < NUM_FN && !skipped; f++) {
                size_t len = strlen(fn_names[f]);
                if (!strncmp(s, fn_names[f], len) && s[len] == '(') {
                    s += len;
                    skipped = true;
                }
            }
            if (skipped) {
                continue;
            }
            if (s[0] == 'p' && s[1] == 'i') {
                s += 2;
                continue;
            }
            if (*s == 'y') {
                mask |= VAR_Y;
            } else if (*s == 'x') {
                mask |= VAR_X;
            } else {
                const char *q = strchr(PARAMS, *s);
                if (q) {
                    mask |= 1UL << (q - PARAMS);
                }
            }
        }
        s++;
    }
    return mask;
}

static void parse_expr(void);

static void skip(void)
{
    while (*p == ' ') {
        p++;
    }
}

static bool starts_atom(char c)
{
    return (c >= '0' && c <= '9') || c == '.' || c == '(' || c == '[' || (c >= 'a' && c <= 'z');
}

static void parse_atom(void)
{
    skip();
    if ((*p >= '0' && *p <= '9') || *p == '.') {
        float v = 0.0f;
        while (*p >= '0' && *p <= '9') {
            v = v * 10.0f + (*p - '0');
            p++;
        }
        if (*p == '.') {
            float scale = 0.1f;
            p++;
            while (*p >= '0' && *p <= '9') {
                v += (*p - '0') * scale;
                scale *= 0.1f;
                p++;
            }
        }
        emit_const(v);
        return;
    }
    if (*p == '(') {
        p++;
        parse_expr();
        skip();
        if (*p == ')') {
            p++;
        }
        return;
    }
    if (*p == '[') {
        p++;
        parse_expr();
        if (*p == '|') {
            p++;
        } else {
            perr = true;
        }
        parse_expr();
        if (*p == ']') {
            p++;
        } else {
            perr = true;
        }
        emit(OP_DIV, 0);
        return;
    }
    if (*p == 'x') {
        p++;
        emit(OP_X, 0);
        return;
    }
    if (*p == 'y') {
        p++;
        emit(OP_Y, 0);
        return;
    }
    if (*p == 'e') {
        p++;
        emit_const(2.71828183f);
        return;
    }
    if (p[0] == 'p' && p[1] == 'i') {
        p += 2;
        emit_const(3.14159265f);
        return;
    }
    if (*p >= 'a' && *p <= 'z') {
        const char *q;
        int f;
        for (f = 0; f < NUM_FN; f++) {
            size_t len = strlen(fn_names[f]);
            if (!strncmp(p, fn_names[f], len) && p[len] == '(') {
                p += len + 1;
                parse_expr();
                skip();
                if (*p == ')') {
                    p++;
                }
                emit(OP_FN, (uint8_t)f);
                return;
            }
        }
        q = strchr(PARAMS, *p);
        if (q) {
            p++;
            emit(OP_PARAM, (uint8_t)(q - PARAMS));
            return;
        }
    }
    perr = true;
    if (*p) {
        p++;
    }
}

static void parse_unary(void);

static void parse_pow(void)
{
    parse_atom();
    skip();
    if (*p == '^') {
        p++;
        parse_unary();
        emit(OP_POW, 0);
    }
}

static void parse_unary(void)
{
    skip();
    if (*p == '-') {
        p++;
        parse_unary();
        emit(OP_NEG, 0);
        return;
    }
    if (*p == '+') {
        p++;
    }
    parse_pow();
}

static void parse_term(void)
{
    parse_unary();
    while (!perr) {
        skip();
        if (*p == '*') {
            p++;
            parse_unary();
            emit(OP_MUL, 0);
        } else if (*p == '/') {
            p++;
            parse_unary();
            emit(OP_DIV, 0);
        } else if (starts_atom(*p)) {
            parse_pow();
            emit(OP_MUL, 0);
        } else {
            break;
        }
    }
}

static void parse_expr(void)
{
    parse_term();
    while (!perr) {
        skip();
        if (*p == '+') {
            p++;
            parse_term();
            emit(OP_ADD, 0);
        } else if (*p == '-') {
            p++;
            parse_term();
            emit(OP_SUB, 0);
        } else {
            break;
        }
    }
}

/* Compile s up to the character `end` ('\0' or '='). False if malformed. */
static bool compile(const char *s, char end, struct prog *out)
{
    p = s;
    cp = out;
    cp->nops = 0;
    cp->nk = 0;
    depth = 0;
    perr = false;
    parse_expr();
    skip();
    return !perr && *p == end && depth == 1;
}

/* What a row is:
 *   f(x) or y=f(x)   a curve
 *   a=2              a slider (any letter except x, y, e)
 *   A=B              solutions of A=B in x, drawn as vertical lines
 *   A=B with a y     an implicit curve such as x^2+y^2=4 */
enum { R_BAD = -1, R_FUNC, R_Y, R_SOLVE, R_PARAM, R_IMPLICIT };

/* Compile row i into eq_prog[i] (0 = left side or whole, 1 = right side). */
static int analyze(int i)
{
    const char *s = eq[i];
    const char *e = strchr(s, '=');
    unsigned long rhs_vars;

    eq_rhs[i] = 0;
    if (!e) {
        return (!(scan_vars(s) & VAR_Y) && compile(s, 0, &eq_prog[i][0])) ? R_FUNC : R_BAD;
    }
    if (strchr(e + 1, '=')) {
        return R_BAD;
    }
    eq_rhs[i] = (int)(e - s) + 1;
    if (!compile(s + eq_rhs[i], 0, &eq_prog[i][1])) {
        return R_BAD;
    }
    rhs_vars = scan_vars(s + eq_rhs[i]);
    if (e - s == 1 && s[0] == 'y' && !(rhs_vars & VAR_Y)) {
        return R_Y;
    }
    if (e - s == 1 && strchr(PARAMS, s[0]) && rhs_vars == 0) {
        return R_PARAM;
    }
    if (!compile(s, '=', &eq_prog[i][0])) {
        return R_BAD;
    }
    return (scan_vars(s) & VAR_Y) ? R_IMPLICIT : R_SOLVE;
}

/* ---------- number formatting ---------- */

static void fmt_num(char *buf, float v)
{
    int neg = v < 0;
    long s;
    int ip, fp;
    if (neg) {
        v = -v;
    }
    s = (long)(v * 1000.0f + 0.5f);
    ip = (int)(s / 1000);
    fp = (int)(s % 1000);
    if (fp == 0) {
        sprintf(buf, "%s%d", (neg && s) ? "-" : "", ip);
    } else if (fp % 100 == 0) {
        sprintf(buf, "%s%d.%d", neg ? "-" : "", ip, fp / 100);
    } else if (fp % 10 == 0) {
        sprintf(buf, "%s%d.%02d", neg ? "-" : "", ip, fp / 10);
    } else {
        sprintf(buf, "%s%d.%03d", neg ? "-" : "", ip, fp);
    }
}

static void fmt_period(char *buf, float per)
{
    float m = per / 3.14159265f;
    int d;
    for (d = 1; d <= 6; d++) {
        float scaled = m * d;
        int n = (int)(scaled + 0.5f);
        if (n > 0 && fabsf(scaled - n) < 0.01f) {
            if (d == 1) {
                if (n == 1) strcpy(buf, "pi"); else sprintf(buf, "%dpi", n);
            } else {
                if (n == 1) sprintf(buf, "pi/%d", d); else sprintf(buf, "%dpi/%d", n, d);
            }
            return;
        }
    }
    fmt_num(buf, per);
}

/* ---------- view ---------- */

static void set_view(float cx, float cy, float w)
{
    float h = w * GH / SCREEN_W;
    xmin = cx - w / 2.0f;
    xmax = cx + w / 2.0f;
    ymin = cy - h / 2.0f;
    ymax = cy + h / 2.0f;
}

static float screen_x(float x)
{
    return (x - xmin) / (xmax - xmin) * SCREEN_W;
}

static float screen_y(float y)
{
    return GY + (ymax - y) / (ymax - ymin) * GH;
}

static float px_to_x(int px)
{
    return xmin + (xmax - xmin) * px / (SCREEN_W - 1);
}

/* ---------- curves and key points ---------- */

static float row_eval(int i, float x)
{
    switch (eq_kind[i]) {
    case R_FUNC:  return run(&eq_prog[i][0], x);
    case R_Y:     return run(&eq_prog[i][1], x);
    case R_PARAM: return run(&eq_prog[i][1], 0.0f);
    default:      return run(&eq_prog[i][0], x) - run(&eq_prog[i][1], x);
    }
}

/* Implicit curves: sample A-B on a coarse grid, then connect the sign changes. */
#define CELL 8
#define MAX_SEG 120

struct seg {
    int16_t x0, y0, x1, y1;
};
static struct seg segs[NUM_EQ][MAX_SEG];
static int nsegs[NUM_EQ];

static float implicit_value(int i, int sx, int sy)
{
    float x = xmin + sx * (xmax - xmin) / SCREEN_W;
    yv = ymax - (sy - GY) * (ymax - ymin) / GH;
    return run(&eq_prog[i][0], x) - run(&eq_prog[i][1], x);
}

static void add_seg(int i, int x0, int y0, int x1, int y1)
{
    if (nsegs[i] < MAX_SEG) {
        struct seg *s = &segs[i][nsegs[i]++];
        s->x0 = (int16_t)x0;
        s->y0 = (int16_t)y0;
        s->x1 = (int16_t)x1;
        s->y1 = (int16_t)y1;
    }
}

static void march_cell(int i, int x, int y, float v0, float v1, float v2, float v3)
{
    int qx[4], qy[4];
    int n = 0;

    if (!isfinite(v0) || !isfinite(v1) || !isfinite(v2) || !isfinite(v3)) {
        return;
    }
    if ((v0 < 0) != (v1 < 0)) {
        qx[n] = x + (int)(v0 / (v0 - v1) * CELL + 0.5f);
        qy[n++] = y;
    }
    if ((v1 < 0) != (v2 < 0)) {
        qx[n] = x + CELL;
        qy[n++] = y + (int)(v1 / (v1 - v2) * CELL + 0.5f);
    }
    if ((v3 < 0) != (v2 < 0)) {
        qx[n] = x + (int)(v3 / (v3 - v2) * CELL + 0.5f);
        qy[n++] = y + CELL;
    }
    if ((v0 < 0) != (v3 < 0)) {
        qx[n] = x;
        qy[n++] = y + (int)(v0 / (v0 - v3) * CELL + 0.5f);
    }
    if (n >= 2) {
        add_seg(i, qx[0], qy[0], qx[1], qy[1]);
    }
    if (n == 4) {
        add_seg(i, qx[2], qy[2], qx[3], qy[3]);
    }
}

static void compute_implicit(int i)
{
    static float rowv[2][SCREEN_W / CELL + 1];
    int gw = SCREEN_W / CELL;
    int gh = GH / CELL;
    int gx, gy, top = 0;

    nsegs[i] = 0;
    for (gx = 0; gx <= gw; gx++) {
        rowv[0][gx] = implicit_value(i, gx * CELL, GY);
    }
    for (gy = 0; gy < gh; gy++) {
        int bot = top ^ 1;
        for (gx = 0; gx <= gw; gx++) {
            rowv[bot][gx] = implicit_value(i, gx * CELL, GY + (gy + 1) * CELL);
        }
        for (gx = 0; gx < gw; gx++) {
            march_cell(i, gx * CELL, GY + gy * CELL,
                       rowv[top][gx], rowv[top][gx + 1], rowv[bot][gx + 1], rowv[bot][gx]);
        }
        top = bot;
    }
    yv = 0.0f;
}

static void analyze_row(int i)
{
    eq_kind[i] = (i < nrows && eq[i][0]) ? analyze(i) : R_BAD;
    eq_uses[i] = eq_kind[i] != R_BAD ? scan_vars(eq[i]) : 0;
}

static void update_params(void)
{
    int i;
    for (i = 0; i < NUM_PARAMS; i++) {
        param[i] = 1.0f;
    }
    for (i = 0; i < nrows; i++) {
        if (eq_kind[i] == R_PARAM) {
            param[strchr(PARAMS, eq[i][0]) - PARAMS] = run(&eq_prog[i][1], 0.0f);
        }
    }
}

static void build_curve(int i)
{
    int px;
    curve_ok[i] = eq_kind[i] == R_FUNC || eq_kind[i] == R_Y
                  || eq_kind[i] == R_SOLVE || eq_kind[i] == R_IMPLICIT;
    if (!curve_ok[i]) {
        return;
    }
    if (eq_kind[i] == R_IMPLICIT) {
        compute_implicit(i);
        return;
    }
    for (px = 0; px < SCREEN_W; px++) {
        ycache[i][px] = row_eval(i, px_to_x(px));
    }
}

static void compute_curve(int i)
{
    analyze_row(i);
    build_curve(i);
}

static void add_point(int kind, int i, float x, float y)
{
    int sx = (int)screen_x(x);
    float sy = screen_y(y);
    if (npts >= MAX_PT || sy < GY || sy > SB_Y - 1 || sx < 0 || sx >= SCREEN_W) {
        return;
    }
    pts[npts].x = x;
    pts[npts].y = y;
    pts[npts].sx = sx;
    pts[npts].sy = (int)sy;
    pts[npts].kind = (uint8_t)kind;
    pts[npts].eq = (uint8_t)i;
    npts++;
}

static float refine_zero(int i, float a, float b)
{
    float fa = row_eval(i, a);
    int n;
    for (n = 0; n < 16; n++) {
        float m = (a + b) / 2.0f;
        float fm = row_eval(i, m);
        if (fm == 0.0f) {
            return m;
        }
        if ((fa < 0) == (fm < 0)) {
            a = m;
            fa = fm;
        } else {
            b = m;
        }
    }
    return (a + b) / 2.0f;
}

static float cross_diff(int i, int j, float x)
{
    return row_eval(i, x) - row_eval(j, x);
}

static float refine_cross(int i, int j, float a, float b)
{
    float fa = cross_diff(i, j, a);
    int n;
    for (n = 0; n < 16; n++) {
        float m = (a + b) / 2.0f;
        float fm = cross_diff(i, j, m);
        if ((fa < 0) == (fm < 0)) {
            a = m;
            fa = fm;
        } else {
            b = m;
        }
    }
    return (a + b) / 2.0f;
}

static float refine_extreme(int i, float lo, float hi, bool want_max)
{
    int n;
    for (n = 0; n < 16; n++) {
        float m1 = lo + (hi - lo) / 3.0f;
        float m2 = hi - (hi - lo) / 3.0f;
        float f1 = row_eval(i, m1);
        float f2 = row_eval(i, m2);
        if (want_max ? (f1 < f2) : (f1 > f2)) {
            lo = m1;
        } else {
            hi = m2;
        }
    }
    return (lo + hi) / 2.0f;
}

static void sort_points(void)
{
    int a, b;
    for (a = 1; a < npts; a++) {
        struct point t = pts[a];
        b = a - 1;
        while (b >= 0 && pts[b].x > t.x) {
            pts[b + 1] = pts[b];
            b--;
        }
        pts[b + 1] = t;
    }
}

static void find_points(void)
{
    float h = ymax - ymin;
    int i, j, px;

    npts = 0;
    for (i = 0; i < NUM_EQ; i++) {
        nmax[i] = 0;
        nmin[i] = 0;
    }

    for (i = 0; i < NUM_EQ; i++) {
        const float *y = ycache[i];
        if (!curve_ok[i] || eq_kind[i] == R_IMPLICIT) {
            continue;
        }
        if (eq_kind[i] == R_SOLVE) {
            float marker_y = (ymin < 0 && ymax > 0) ? 0.0f : (ymin + ymax) / 2.0f;
            for (px = 1; px < SCREEN_W; px++) {
                float a = y[px - 1];
                float b = y[px];
                if (isfinite(a) && isfinite(b) && ((a < 0 && b >= 0) || (a > 0 && b <= 0))) {
                    float x = refine_zero(i, px_to_x(px - 1), px_to_x(px));
                    float g = row_eval(i, x);
                    if (isfinite(g) && fabsf(g) < 0.1f * fmaxf(fabsf(a), fabsf(b)) + 0.000001f) {
                        add_point(K_SOL, i, x, marker_y);
                    }
                }
            }
            continue;
        }
        if (xmin < 0 && xmax > 0) {
            float v = row_eval(i, 0.0f);
            if (isfinite(v) && fabsf(v) > 0.001f * h) {
                add_point(K_YINT, i, 0.0f, v);
            }
        }
        for (px = 1; px < SCREEN_W; px++) {
            float a = y[px - 1];
            float b = y[px];
            if (!isfinite(a) || !isfinite(b) || fabsf(b - a) > 0.5f * h) {
                continue;
            }
            if ((a < 0 && b >= 0) || (a > 0 && b <= 0)) {
                add_point(K_ZERO, i, refine_zero(i, px_to_x(px - 1), px_to_x(px)), 0.0f);
            }
        }
        for (px = 1; px < SCREEN_W - 1; px++) {
            float a = y[px - 1];
            float b = y[px];
            float c = y[px + 1];
            bool is_max, is_min;
            if (!isfinite(a) || !isfinite(b) || !isfinite(c)
                || fabsf(b - a) > 0.5f * h || fabsf(b - c) > 0.5f * h) {
                continue;
            }
            is_max = b > a && b >= c;
            is_min = b < a && b <= c;
            if (is_max || is_min) {
                float x = refine_extreme(i, px_to_x(px - 1), px_to_x(px + 1), is_max);
                float v = row_eval(i, x);
                if (!isfinite(v)) {
                    continue;
                }
                if (is_max && nmax[i] < MAX_EXT) {
                    maxx[i][nmax[i]] = x;
                    maxy[i][nmax[i]] = v;
                    nmax[i]++;
                } else if (is_min && nmin[i] < MAX_EXT) {
                    minx[i][nmin[i]] = x;
                    miny[i][nmin[i]] = v;
                    nmin[i]++;
                }
                add_point(is_max ? K_MAX : K_MIN, i, x, v);
            }
        }
    }

    for (i = 0; i < NUM_EQ; i++) {
        for (j = i + 1; j < NUM_EQ; j++) {
            if (!curve_ok[i] || !curve_ok[j]
                || eq_kind[i] >= R_SOLVE || eq_kind[j] >= R_SOLVE) {
                continue;
            }
            for (px = 1; px < SCREEN_W; px++) {
                float a = ycache[i][px - 1] - ycache[j][px - 1];
                float b = ycache[i][px] - ycache[j][px];
                if (!isfinite(a) || !isfinite(b) || fabsf(b - a) > 0.5f * h) {
                    continue;
                }
                if ((a < 0 && b > 0) || (a > 0 && b < 0)) {
                    float x = refine_cross(i, j, px_to_x(px - 1), px_to_x(px));
                    add_point(K_CROSS, i, x, row_eval(i, x));
                }
            }
        }
    }
    sort_points();
    if (psel >= npts) {
        psel = npts ? npts - 1 : 0;
    }
}

static void compute_all(void)
{
    int i;
    for (i = 0; i < NUM_EQ; i++) {
        analyze_row(i);
    }
    update_params();
    for (i = 0; i < NUM_EQ; i++) {
        build_curve(i);
    }
    find_points();
}

/* After a slider moves: redo only the rows that use a slider. */
static void compute_dependent(void)
{
    int i;
    update_params();
    for (i = 0; i < NUM_EQ; i++) {
        if (eq_uses[i] & PARAM_MASK) {
            build_curve(i);
        }
    }
    find_points();
}

static void equation_changed(void)
{
    int old = eq_kind[sel];
    compute_curve(sel);
    if (eq_kind[sel] == R_PARAM || old == R_PARAM) {
        compute_dependent();
    } else {
        find_points();
    }
}

/* A curve counts as repeating only if its peaks match in height and spacing. */
static bool consistent(const float *xs, const float *ys, int n, float h, float *period)
{
    float d0;
    int k;
    if (n < 2) {
        return false;
    }
    d0 = xs[1] - xs[0];
    if (d0 <= 0) {
        return false;
    }
    for (k = 1; k < n; k++) {
        if (fabsf(ys[k] - ys[0]) > 0.005f * h + 0.0001f) {
            return false;
        }
    }
    for (k = 1; k < n - 1; k++) {
        if (fabsf((xs[k + 1] - xs[k]) - d0) > 0.02f * d0) {
            return false;
        }
    }
    *period = d0;
    return true;
}

static bool equation_info(int i, char *out)
{
    float h = ymax - ymin;
    float per = 0.0f;
    char a[12], m[12], t[16];

    if (!consistent(maxx[i], maxy[i], nmax[i], h, &per)
        && !consistent(minx[i], miny[i], nmin[i], h, &per)) {
        return false;
    }
    fmt_period(t, per);
    if (nmax[i] && nmin[i]) {
        fmt_num(a, (maxy[i][0] - miny[i][0]) / 2.0f);
        fmt_num(m, (maxy[i][0] + miny[i][0]) / 2.0f);
        sprintf(out, "Y%d amp %s mid %s per %s", i + 1, a, m, t);
    } else {
        sprintf(out, "Y%d period %s", i + 1, t);
    }
    return true;
}

/* ---------- text editing ---------- */

static void erase_at(char *s, int idx)
{
    memmove(s + idx, s + idx + 1, strlen(s + idx));
}

static void insert_text(char *s, int *c, const char *t)
{
    int n = (int)strlen(s);
    int m = (int)strlen(t);
    bool paren = m && t[m - 1] == '(';
    int total = m + (paren ? 1 : 0);

    if (!m || n + total > EQ_LEN) {
        return;
    }
    memmove(s + *c + total, s + *c, n - *c + 1);
    memcpy(s + *c, t, m);
    if (paren) {
        s[*c + m] = ')';
    }
    *c += m;
}

static bool inside_fraction(const char *s, int c)
{
    int depth = 0, i;
    for (i = 0; i < c; i++) {
        if (s[i] == '[') depth++;
        if (s[i] == ']') depth--;
    }
    return depth > 0;
}

static void make_fraction(char *s, int *c)
{
    char tmp[EQ_LEN + 4];
    int n = (int)strlen(s);
    int i = *c;
    int depth = 0;

    if (inside_fraction(s, *c)) {
        insert_text(s, c, "/");
        return;
    }
    if (n + 3 > EQ_LEN) {
        return;
    }
    while (i > 0) {
        char ch = s[i - 1];
        if (ch == ')' || ch == ']') {
            depth++;
        } else if (ch == '(' || ch == '[') {
            if (depth == 0) break;
            depth--;
        } else if (depth == 0 && (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '|')) {
            break;
        }
        i--;
    }
    memcpy(tmp, s, i);
    tmp[i] = '[';
    memcpy(tmp + i + 1, s + i, *c - i);
    tmp[*c + 1] = '|';
    tmp[*c + 2] = ']';
    strcpy(tmp + *c + 3, s + *c);
    strcpy(s, tmp);
    *c = (*c == i) ? i + 1 : *c + 2;
}

static void backspace(char *s, int *c)
{
    int n = (int)strlen(s);
    char ch;

    if (*c == 0) {
        return;
    }
    ch = s[*c - 1];
    if (ch == ')' || ch == ']') {
        (*c)--;
        return;
    }
    if (ch == '(' && s[*c] == ')') {
        int st = *c - 1;
        while (st > 0 && s[st - 1] >= 'a' && s[st - 1] <= 'z') {
            st--;
        }
        memmove(s + st, s + *c + 1, n - (*c + 1) + 1);
        *c = st;
        return;
    }
    if (ch == '[') {
        if (s[*c] == '|' && s[*c + 1] == ']') {
            memmove(s + *c - 1, s + *c + 2, n - (*c + 2) + 1);
            (*c)--;
        } else {
            (*c)--;
        }
        return;
    }
    if (ch == '|') {
        if (s[*c] == ']') {
            int st = *c - 2;
            while (st >= 0 && s[st] != '[') {
                st--;
            }
            if (st >= 0) {
                erase_at(s, *c);
                erase_at(s, *c - 1);
                erase_at(s, st);
                *c -= 2;
                return;
            }
        }
        (*c)--;
        return;
    }
    erase_at(s, *c - 1);
    (*c)--;
}

/* Letters printed above the keys in green: press Alpha, then the key. */
static const char *alpha_text(uint8_t k)
{
    switch (k) {
    case sk_Math: return "a";
    case sk_Apps: return "b";
    case sk_Prgm: return "c";
    case sk_Recip: return "d";
    case sk_Sin: return "e";
    case sk_Cos: return "f";
    case sk_Tan: return "g";
    case sk_Power: return "h";
    case sk_Square: return "i";
    case sk_Comma: return "j";
    case sk_LParen: return "k";
    case sk_RParen: return "l";
    case sk_Div: return "m";
    case sk_Log: return "n";
    case sk_7: return "o";
    case sk_8: return "p";
    case sk_9: return "q";
    case sk_Mul: return "r";
    case sk_Ln: return "s";
    case sk_4: return "t";
    case sk_5: return "u";
    case sk_6: return "v";
    case sk_Sub: return "w";
    case sk_Store: return "x";
    case sk_1: return "y";
    case sk_2: return "z";
    default: return "";
    }
}

static const char *key_text(uint8_t k)
{
    switch (k) {
    case sk_0: return "0";
    case sk_1: return "1";
    case sk_2: return "2";
    case sk_3: return "3";
    case sk_4: return "4";
    case sk_5: return "5";
    case sk_6: return "6";
    case sk_7: return "7";
    case sk_8: return "8";
    case sk_9: return "9";
    case sk_DecPnt: return ".";
    case sk_Add: return "+";
    case sk_Sub: return "-";
    case sk_Chs: return "-";
    case sk_Mul: return "*";
    case sk_LParen: return "(";
    case sk_Power: return alt ? "pi" : "^(";
    case sk_GraphVar: return alt ? "y" : "x";
    case sk_Square: return alt ? "sqrt(" : "^2";
    case sk_Recip: return "^(-1)";
    case sk_Sin: return alt ? "asin(" : "sin(";
    case sk_Cos: return alt ? "acos(" : "cos(";
    case sk_Tan: return alt ? "atan(" : "tan(";
    case sk_Log: return "log(";
    case sk_Ln: return alt ? "e^(" : "ln(";
    case sk_Math: return "abs(";
    case sk_Comma: return alt ? "e" : "";
    default: return "";
    }
}

static void insert_row_after(int r)
{
    int k;
    for (k = nrows; k > r + 1; k--) {
        strcpy(eq[k], eq[k - 1]);
    }
    eq[r + 1][0] = 0;
    nrows++;
}

static void delete_row(int r)
{
    int k;
    for (k = r; k < nrows - 1; k++) {
        strcpy(eq[k], eq[k + 1]);
    }
    nrows--;
    eq[nrows][0] = 0;
}

/* ---------- drawing ---------- */

static float nice_step(float range)
{
    float raw = range / 10.0f;
    float mag = 1.0f;
    while (raw >= 10.0f) {
        raw /= 10.0f;
        mag *= 10.0f;
    }
    while (raw < 1.0f) {
        raw *= 10.0f;
        mag /= 10.0f;
    }
    if (raw < 1.5f) return mag;
    if (raw < 3.5f) return 2.0f * mag;
    if (raw < 7.5f) return 5.0f * mag;
    return 10.0f * mag;
}

static void draw_grid(void)
{
    float step_x = nice_step(xmax - xmin);
    float step_y = nice_step(ymax - ymin);
    float v;
    char buf[12];
    int label_row = (int)screen_y(0.0f) + 2;
    int label_col = (int)screen_x(0.0f) + 3;

    if (label_row < GY + 1) label_row = GY + 1;
    if (label_row > SB_Y - 10) label_row = SB_Y - 10;
    if (label_col < 0) label_col = 0;
    if (label_col > SCREEN_W - 40) label_col = SCREEN_W - 40;

    gfx_SetColor(GRID);
    for (v = ceilf(xmin / step_x) * step_x; v <= xmax; v += step_x) {
        gfx_VertLine((int)screen_x(v), GY, GH);
    }
    for (v = ceilf(ymin / step_y) * step_y; v <= ymax; v += step_y) {
        gfx_HorizLine(0, (int)screen_y(v), SCREEN_W);
    }

    gfx_SetColor(BLACK);
    if (xmin < 0 && xmax > 0) {
        gfx_VertLine((int)screen_x(0.0f), GY, GH);
    }
    if (ymin < 0 && ymax > 0) {
        gfx_HorizLine(0, (int)screen_y(0.0f), SCREEN_W);
    }

    gfx_SetTextFGColor(DKGRAY);
    for (v = ceilf(xmin / step_x) * step_x; v <= xmax; v += step_x) {
        if (fabsf(v) > step_x * 0.01f) {
            fmt_num(buf, v);
            gfx_PrintStringXY(buf, (int)screen_x(v) + 2, label_row);
        }
    }
    for (v = ceilf(ymin / step_y) * step_y; v <= ymax; v += step_y) {
        if (fabsf(v) > step_y * 0.01f) {
            fmt_num(buf, v);
            gfx_PrintStringXY(buf, label_col, (int)screen_y(v) - 9);
        }
    }
}

static void draw_curve(int i)
{
    int prev_y = 0;
    bool prev_ok = false;
    int px;

    gfx_SetColor(eq_color[i]);
    if (eq_kind[i] == R_SOLVE) {
        int k;
        for (k = 0; k < npts; k++) {
            if (pts[k].kind == K_SOL && pts[k].eq == i) {
                gfx_VertLine(pts[k].sx, GY, GH);
                gfx_VertLine(pts[k].sx + 1, GY, GH);
            }
        }
        return;
    }
    if (eq_kind[i] == R_IMPLICIT) {
        int k;
        for (k = 0; k < nsegs[i]; k++) {
            const struct seg *s = &segs[i][k];
            gfx_Line(s->x0, s->y0, s->x1, s->y1);
            gfx_Line(s->x0, s->y0 + 1, s->x1, s->y1 + 1);
        }
        return;
    }
    for (px = 0; px < SCREEN_W; px++) {
        float y = ycache[i][px];
        float sy = screen_y(y);
        if (isfinite(y) && fabsf(sy) < 2000.0f) {
            int iy = (int)sy;
            if (prev_ok && abs(iy - prev_y) < 250) {
                gfx_Line(px - 1, prev_y, px, iy);
                gfx_Line(px - 1, prev_y + 1, px, iy + 1);
            } else {
                gfx_SetPixel(px, iy);
            }
            prev_y = iy;
            prev_ok = true;
        } else {
            prev_ok = false;
        }
    }
}

static void draw_points(void)
{
    int k;
    char buf[8];

    for (k = 0; k < npts; k++) {
        uint8_t color = pts[k].kind == K_CROSS ? BLACK : eq_color[pts[k].eq];
        gfx_SetColor(BLACK);
        gfx_FillCircle(pts[k].sx, pts[k].sy, 4);
        gfx_SetColor(color);
        gfx_FillCircle(pts[k].sx, pts[k].sy, 2);
        if (mode == M_POINTS && k == psel) {
            gfx_SetColor(BLACK);
            gfx_Circle(pts[k].sx, pts[k].sy, 8);
            gfx_Circle(pts[k].sx, pts[k].sy, 9);
        }
        sprintf(buf, "%d", k + 1);
        gfx_SetTextFGColor(BLACK);
        gfx_PrintStringXY(buf, pts[k].sx + 5, pts[k].sy - 11);
    }
}

static void put_char(char c, int x, int y)
{
    char b[2];
    b[0] = c;
    b[1] = 0;
    gfx_PrintStringXY(b, x, y);
}

static void put_range(const char *s, int from, int to, int x, int y)
{
    int k;
    for (k = from; k < to; k++) {
        put_char(s[k], x + (k - from) * 8, y);
    }
}

/* Draw one equation; [num|den] is drawn as a stacked fraction. */
static void render_row(const char *s, int x, int top, int c, bool show_cursor)
{
    int i = 0;
    int n = (int)strlen(s);

    gfx_SetTextFGColor(BLACK);
    for (;;) {
        if (show_cursor && c == i) {
            gfx_SetColor(BLACK);
            gfx_VertLine(x - 1, top + 8, 11);
        }
        if (i >= n) {
            break;
        }
        if (s[i] == '[') {
            int bar = -1, end = -1, k;
            for (k = i + 1; k < n; k++) {
                if (s[k] == '|' && bar < 0) {
                    bar = k;
                } else if (s[k] == ']') {
                    end = k;
                    break;
                }
            }
            if (bar > 0 && end > bar) {
                int nl = bar - i - 1;
                int dl = end - bar - 1;
                int nw = nl ? nl * 8 : 8;
                int dw = dl ? dl * 8 : 8;
                int fw = (nw > dw ? nw : dw) + 6;
                int nx = x + (fw - nw) / 2;
                int dx = x + (fw - dw) / 2;

                gfx_SetColor(BLACK);
                gfx_HorizLine(x + 1, top + 13, fw - 2);
                put_range(s, i + 1, bar, nx, top + 3);
                put_range(s, bar + 1, end, dx, top + 15);
                gfx_SetColor(DKGRAY);
                if (!nl) gfx_Rectangle(nx, top + 3, 8, 9);
                if (!dl) gfx_Rectangle(dx, top + 15, 8, 9);
                if (show_cursor) {
                    gfx_SetColor(BLACK);
                    if (c > i && c <= bar) {
                        gfx_VertLine(nx + 8 * (c - (i + 1)) - 1, top + 2, 11);
                    } else if (c > bar && c <= end) {
                        gfx_VertLine(dx + 8 * (c - (bar + 1)) - 1, top + 14, 11);
                    }
                }
                x += fw;
                i = end + 1;
                continue;
            }
        }
        put_char(s[i], x, top + 9);
        x += 8;
        i++;
    }
}

static void draw_editor(void)
{
    int first = sel > 0 ? sel - 1 : 0;
    int r;
    char buf[12];

    for (r = 0; r < 2 && first + r < nrows; r++) {
        int row = first + r;
        int top = r * ROW_H;
        if (row == sel) {
            gfx_SetColor(SELBG);
            gfx_FillRectangle(0, top, SCREEN_W, ROW_H);
        }
        gfx_SetColor(BLACK);
        gfx_FillCircle(10, top + 13, 7);
        gfx_SetColor(eq_color[row]);
        gfx_FillCircle(10, top + 13, 5);
        render_row(eq[row], 24, top, cur, row == sel);
        if (eq[row][0] && eq_kind[row] == R_BAD) {
            gfx_SetTextFGColor(RED);
            gfx_PrintStringXY("?", SCREEN_W - 10, top + 9);
        }
    }
    sprintf(buf, "%d/%d", sel + 1, nrows);
    gfx_SetTextFGColor(DKGRAY);
    gfx_PrintStringXY(buf, SCREEN_W - 40, 1);
}

static void draw_status(void)
{
    char buf[48];
    char xs[12], ys[12];

    gfx_SetTextFGColor(BLACK);
    if (notice) {
        sprintf(buf, "%s", notice);
        notice = NULL;
    } else if (mode == M_SLIDER) {
        char vs[12], ss[12];
        fmt_num(vs, param[strchr(PARAMS, eq[sel][0]) - PARAMS]);
        fmt_num(ss, slider_step);
        sprintf(buf, "%c = %s  step %s  Enter:step", eq[sel][0], vs, ss);
        gfx_SetTextFGColor(eq_color[sel]);
    } else if (mode == M_POINTS && npts) {
        gfx_SetTextFGColor(pts[psel].kind == K_CROSS ? BLACK : eq_color[pts[psel].eq]);
        fmt_num(xs, pts[psel].x);
        fmt_num(ys, pts[psel].y);
        if (pts[psel].kind == K_SOL) {
            sprintf(buf, "%d sol x = %s", psel + 1, xs);
        } else {
            sprintf(buf, "%d %s (%s, %s)", psel + 1, kind_name[pts[psel].kind], xs, ys);
        }
    } else if (equation_info(sel, buf)) {
        gfx_SetTextFGColor(eq_color[sel]);
    } else {
        sprintf(buf, "Graph:sliders Stat:table Trace");
        gfx_SetTextFGColor(DKGRAY);
    }
    gfx_PrintStringXY(buf, 4, SB_Y + 3);
    if (alt || alpha) {
        gfx_SetTextFGColor(alpha ? GREEN : RED);
        gfx_PrintStringXY(alpha ? "ALPHA" : "2nd", SCREEN_W - (alpha ? 48 : 28), SB_Y + 3);
    }
}

static void redraw(void)
{
    int i;
    gfx_FillScreen(WHITE);
    draw_grid();
    for (i = 0; i < NUM_EQ; i++) {
        if (curve_ok[i]) {
            draw_curve(i);
        }
    }
    draw_points();

    gfx_SetColor(WHITE);
    gfx_FillRectangle(0, 0, SCREEN_W, GY);
    gfx_FillRectangle(0, SB_Y, SCREEN_W, SCREEN_H - SB_Y);
    gfx_SetColor(BLACK);
    gfx_HorizLine(0, GY - 1, SCREEN_W);
    gfx_HorizLine(0, SB_Y - 1, SCREEN_W);
    draw_editor();
    draw_status();
    gfx_SwapDraw();
}

/* ---------- pan and zoom ---------- */

static void wait_for_release(void)
{
    while (kb_AnyKey()) {
    }
    while (os_GetCSC()) {
    }
}

static void view_loop(void)
{
    bool dirty = true;

    wait_for_release();
    for (;;) {
        float w = xmax - xmin;
        float h = ymax - ymin;

        kb_Scan();
        if ((kb_Data[6] & kb_Clear) || (kb_Data[1] & kb_Window)) {
            break;
        }
        if (kb_Data[1] & kb_Zoom) {
            set_view(0.0f, 0.0f, 20.0f);
            dirty = true;
        }
        if (kb_Data[6] & (kb_Add | kb_Sub)) {
            float f = (kb_Data[6] & kb_Add) ? 0.8f : 1.25f;
            if (w * f > 0.02f && w * f < 5000.0f) {
                set_view((xmin + xmax) / 2.0f, (ymin + ymax) / 2.0f, w * f);
                dirty = true;
            }
        }
        if (kb_Data[7] & kb_Right) { xmin += w * 0.1f; xmax += w * 0.1f; dirty = true; }
        if (kb_Data[7] & kb_Left)  { xmin -= w * 0.1f; xmax -= w * 0.1f; dirty = true; }
        if (kb_Data[7] & kb_Up)    { ymin += h * 0.1f; ymax += h * 0.1f; dirty = true; }
        if (kb_Data[7] & kb_Down)  { ymin -= h * 0.1f; ymax -= h * 0.1f; dirty = true; }

        if (dirty) {
            compute_all();
            redraw();
            dirty = false;
        } else {
            delay(30);
        }
    }
    wait_for_release();
}

/* ---------- sliders and tables ---------- */

static void slider_mode(void)
{
    static const float steps[4] = {0.01f, 0.1f, 1.0f, 10.0f};
    int si = 1;
    unsigned long used = 0, defined = 0;
    int i, idx;

    for (i = 0; i < nrows; i++) {
        if (eq_kind[i] == R_PARAM) {
            defined |= 1UL << (strchr(PARAMS, eq[i][0]) - PARAMS);
        } else {
            used |= eq_uses[i] & PARAM_MASK;
        }
    }
    for (idx = 0; idx < NUM_PARAMS; idx++) {
        if ((used & (1UL << idx)) && !(defined & (1UL << idx)) && nrows < NUM_EQ) {
            sprintf(eq[nrows], "%c=1", PARAMS[idx]);
            nrows++;
        }
    }
    compute_all();
    for (i = 0; i < nrows && eq_kind[i] != R_PARAM; i++) {
    }
    if (i == nrows) {
        notice = "No sliders: type a=1, then use a";
        return;
    }

    sel = i;
    cur = (int)strlen(eq[sel]);
    mode = M_SLIDER;
    slider_step = steps[si];
    wait_for_release();
    redraw();
    for (;;) {
        bool changed = false;
        int dir = 0;

        kb_Scan();
        if ((kb_Data[6] & kb_Clear) || (kb_Data[1] & kb_Graph)) {
            break;
        }
        if (kb_Data[7] & kb_Right) {
            dir = 1;
        } else if (kb_Data[7] & kb_Left) {
            dir = -1;
        }

        if (dir) {
            char letter = eq[sel][0];
            char num[12];
            float v = param[strchr(PARAMS, letter) - PARAMS] + dir * slider_step;
            if (fabsf(v) < 10000.0f) {
                fmt_num(num, v);
                sprintf(eq[sel], "%c=%s", letter, num);
                cur = (int)strlen(eq[sel]);
                compute_curve(sel);
                compute_dependent();
                changed = true;
            }
        } else if (kb_Data[7] & (kb_Up | kb_Down)) {
            int dstep = (kb_Data[7] & kb_Down) ? 1 : -1;
            int j = sel;
            do {
                j = (j + dstep + nrows) % nrows;
            } while (eq_kind[j] != R_PARAM);
            sel = j;
            cur = (int)strlen(eq[sel]);
            changed = true;
        } else if (kb_Data[6] & kb_Enter) {
            si = (si + 1) & 3;
            slider_step = steps[si];
            changed = true;
            while (kb_AnyKey()) {
            }
        }

        if (changed) {
            redraw();
            delay(110);
        } else {
            delay(30);
        }
    }
    mode = M_EDIT;
    wait_for_release();
}

static void table_mode(void)
{
    float start = 0.0f, step = 1.0f;
    int cols[4];
    int ncols = 0;
    int i;

    for (i = 0; i < nrows && ncols < 4; i++) {
        if (eq_kind[i] == R_FUNC || eq_kind[i] == R_Y) {
            cols[ncols++] = i;
        }
    }
    if (!ncols) {
        notice = "Table needs a function like y=x^2";
        return;
    }

    for (;;) {
        char buf[24];
        char line[48];
        int r, c;
        uint8_t k;

        gfx_FillScreen(WHITE);
        gfx_SetTextFGColor(BLACK);
        gfx_PrintStringXY("x", 8, 4);
        for (c = 0; c < ncols; c++) {
            gfx_SetTextFGColor(eq_color[cols[c]]);
            sprintf(buf, "Y%d", cols[c] + 1);
            gfx_PrintStringXY(buf, 70 + c * 64, 4);
        }
        gfx_SetColor(BLACK);
        gfx_HorizLine(0, 16, SCREEN_W);
        for (r = 0; r < 11; r++) {
            float x = start + r * step;
            int y = 22 + r * 16;
            gfx_SetTextFGColor(BLACK);
            fmt_num(buf, x);
            gfx_PrintStringXY(buf, 8, y);
            for (c = 0; c < ncols; c++) {
                float v = row_eval(cols[c], x);
                gfx_SetTextFGColor(eq_color[cols[c]]);
                if (isfinite(v)) {
                    fmt_num(buf, v);
                } else {
                    strcpy(buf, "undef");
                }
                gfx_PrintStringXY(buf, 70 + c * 64, y);
            }
        }
        gfx_SetColor(BLACK);
        gfx_HorizLine(0, 202, SCREEN_W);
        gfx_SetTextFGColor(DKGRAY);
        fmt_num(buf, step);
        sprintf(line, "Up/Down: scroll   Left/Right: step %s", buf);
        gfx_PrintStringXY(line, 4, 208);
        gfx_PrintStringXY("Zoom: reset   Clear: back", 4, 220);
        gfx_SwapDraw();

        do {
            k = os_GetCSC();
        } while (!k);
        if (k == sk_Clear || k == sk_Stat || k == sk_Mode) {
            break;
        } else if (k == sk_Up) {
            start -= step;
        } else if (k == sk_Down) {
            start += step;
        } else if (k == sk_Left && step > 0.002f) {
            step /= 2.0f;
        } else if (k == sk_Right && step < 1000.0f) {
            step *= 2.0f;
        } else if (k == sk_Zoom) {
            start = 0.0f;
            step = 1.0f;
        }
    }
}

/* ---------- main ---------- */

int main(void)
{
    bool quit = false;

    gfx_Begin();
    set_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(WHITE);
    gfx_SetTextTransparentColor(WHITE);
    set_view(0.0f, 0.0f, 20.0f);
    cur = (int)strlen(eq[0]);
    compute_all();

    while (!quit) {
        uint8_t k;

        redraw();
        do {
            k = os_GetCSC();
        } while (!k);

        if (k == sk_Mode) {
            quit = true;
        } else if (k == sk_Zoom) {
            set_view(0.0f, 0.0f, 20.0f);
            compute_all();
        } else if (mode == M_POINTS) {
            if ((k == sk_Right || k == sk_Down) && npts) {
                psel = (psel + 1) % npts;
            } else if ((k == sk_Left || k == sk_Up) && npts) {
                psel = (psel + npts - 1) % npts;
            } else if (k == sk_Clear || k == sk_Trace) {
                mode = M_EDIT;
            }
        } else if (k == sk_Alpha) {
            alpha = !alpha;
            alt = false;
        } else if (k == sk_2nd) {
            alt = !alt;
            alpha = false;
        } else if (k == sk_Up) {
            if (sel > 0) {
                sel--;
                cur = (int)strlen(eq[sel]);
            }
        } else if (k == sk_Down) {
            if (sel < nrows - 1) {
                sel++;
                cur = (int)strlen(eq[sel]);
            } else if (eq[sel][0] && nrows < NUM_EQ) {
                insert_row_after(sel);
                sel++;
                cur = 0;
                compute_all();
            }
        } else if (k == sk_Left) {
            if (cur > 0) cur--;
        } else if (k == sk_Right) {
            if (cur < (int)strlen(eq[sel])) cur++;
        } else if (k == sk_Enter) {
            if (!strchr(eq[sel], '=')) {
                insert_text(eq[sel], &cur, "=");
                equation_changed();
            }
        } else if (k == sk_Clear) {
            if (eq[sel][0]) {
                eq[sel][0] = 0;
                cur = 0;
                equation_changed();
            } else if (nrows > 1) {
                delete_row(sel);
                if (sel >= nrows) sel = nrows - 1;
                cur = (int)strlen(eq[sel]);
                compute_all();
            }
        } else if (k == sk_Del) {
            backspace(eq[sel], &cur);
            equation_changed();
        } else if (k == sk_Graph) {
            alt = false;
            slider_mode();
        } else if (k == sk_Stat) {
            table_mode();
        } else if (k == sk_Window) {
            view_loop();
        } else if (k == sk_Trace) {
            if (npts) {
                mode = M_POINTS;
                psel = 0;
            }
        } else if (k == sk_Div && !alpha) {
            make_fraction(eq[sel], &cur);
            equation_changed();
        } else if (k == sk_RParen && !alpha) {
            if (eq[sel][cur] == ')') {
                cur++;
            } else {
                insert_text(eq[sel], &cur, ")");
                equation_changed();
            }
        } else {
            const char *t = alpha ? alpha_text(k) : key_text(k);
            if (*t) {
                insert_text(eq[sel], &cur, t);
                equation_changed();
            }
            alt = false;
            alpha = false;
        }
    }

    gfx_End();
    return 0;
}
