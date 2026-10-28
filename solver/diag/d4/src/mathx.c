#include "mathx.h"
#include <math.h>
#include "mreal.h"
#include <string.h>

#define PI_D 3.14159265358979323846L
#define STACK 24

bool mx_degrees = true;
bool (*mx_stop_check)(void) = 0;

enum { OP_CONST, OP_X, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_NEG, OP_FN };

enum { F_SIN, F_COS, F_TAN, F_CSC, F_SEC, F_COT, F_ASIN, F_ACOS, F_ATAN, F_SQRT, F_ABS, F_LN, F_LOG, F_EXP, NUM_FN };

struct fname {
    const char *name;
    uint8_t id;
};

/* longer names first so "asin" is tried before "sin" */
static const struct fname fnames[] = {
    {"arcsin", F_ASIN}, {"arccos", F_ACOS}, {"arctan", F_ATAN},
    {"asin", F_ASIN}, {"acos", F_ACOS}, {"atan", F_ATAN},
    {"sqrt", F_SQRT}, {"sin", F_SIN}, {"cos", F_COS}, {"tan", F_TAN},
    {"csc", F_CSC}, {"sec", F_SEC}, {"cot", F_COT}, {"abs", F_ABS},
    {"ln", F_LN}, {"log", F_LOG}, {"exp", F_EXP},
};
#define N_FNAMES ((int)(sizeof(fnames) / sizeof(fnames[0])))

static const char *p;
static struct mx_prog *cp;
static int depth;
static bool perr;

static void emit(uint8_t op, uint8_t arg)
{
    if (cp->nops >= MX_OPS) {
        perr = true;
        return;
    }
    cp->op[cp->nops] = op;
    cp->arg[cp->nops] = arg;
    cp->nops++;
    if (op == OP_CONST || op == OP_X) {
        if (++depth > STACK) perr = true;
    } else if (op >= OP_ADD && op <= OP_POW) {
        depth--;
    }
}

static void emit_const(real v)
{
    if (cp->nk >= MX_CONSTS) {
        perr = true;
        return;
    }
    cp->k[cp->nk] = v;
    emit(OP_CONST, cp->nk++);
}

static void skip(void)
{
    while (*p == ' ') p++;
}

static int match_fn(const char *s)
{
    int i;
    for (i = 0; i < N_FNAMES; i++) {
        size_t n = strlen(fnames[i].name);
        if (!strncmp(s, fnames[i].name, n)) return i;
    }
    return -1;
}

static void parse_expr(void);
static void parse_unary(void);

/* can this character start something that multiplies implicitly? */
static bool starts_factor(const char *s)
{
    char c = *s;
    if ((c >= '0' && c <= '9') || c == '.' || c == '(' || c == '[' || c == 'x') return true;
    if (c == 'p' && s[1] == 'i') return true;
    if (c == 'e' && !(s[1] >= 'a' && s[1] <= 'z')) return true;
    return match_fn(s) >= 0;
}

/* a factor that may follow a function name without brackets: sin 2x, sin x */
static void parse_simple_arg(void)
{
    skip();
    if (*p == '-') {
        p++;
        parse_simple_arg();
        emit(OP_NEG, 0);
        return;
    }
    /* first factor */
    {
        extern void mx__atom(void);
        mx__atom();
    }
    skip();
    /* implicit multiplication by plain numbers, x, constants and brackets only */
    while (!perr) {
        char c = *p;
        /* only plain numbers, x and pi continue the argument: sin 2x, but csc x (sec x) is csc(x) times (sec x) */
        bool more = (c >= '0' && c <= '9') || c == '.' || c == 'x' || (c == 'p' && p[1] == 'i');
        if (!more) break;
        {
            extern void mx__atom(void);
            mx__atom();
        }
        emit(OP_MUL, 0);
        skip();
    }
}

void mx__atom(void)
{
    skip();
    if ((*p >= '0' && *p <= '9') || *p == '.') {
        real v = 0, scale = 0.1L;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
        if (*p == '.') {
            p++;
            while (*p >= '0' && *p <= '9') { v += (*p - '0') * scale; scale /= 10; p++; }
        }
        emit_const(v);
        return;
    }
    if (*p == '(') {
        p++;
        parse_expr();
        skip();
        if (*p == ')') p++;
        return;
    }
    if (*p == '[') {
        p++;
        parse_expr();
        if (*p == '|') p++; else perr = true;
        parse_expr();
        if (*p == ']') p++; else perr = true;
        emit(OP_DIV, 0);
        return;
    }
    if (*p == 'x') { p++; emit(OP_X, 0); return; }
    if (p[0] == 'p' && p[1] == 'i') { p += 2; emit_const(PI_D); return; }
    if (*p == 'e' && !(p[1] >= 'a' && p[1] <= 'z')) { p++; emit_const(2.718281828459045L); return; }
    {
        int f = match_fn(p);
        if (f >= 0) {
            uint8_t id = fnames[f].id;
            int power = 0;
            p += strlen(fnames[f].name);
            /* sin^2(x): a small whole-number power written before the argument */
            if (*p == '^' && ((p[1] >= '0' && p[1] <= '9'))) {
                p++;
                while (*p >= '0' && *p <= '9') { power = power * 10 + (*p - '0'); p++; }
            }
            skip();
            if (*p == '(') {
                p++;
                parse_expr();
                skip();
                if (*p == ')') p++;
            } else {
                parse_simple_arg();
            }
            emit(OP_FN, id);
            if (power > 1) {
                emit_const((real)power);
                emit(OP_POW, 0);
            }
            return;
        }
    }
    perr = true;
    if (*p) p++;
}

static void parse_pow(void)
{
    mx__atom();
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
    if (*p == '+') p++;
    parse_pow();
}

static void parse_term(void)
{
    parse_unary();
    while (!perr) {
        skip();
        if (*p == '*') { p++; parse_unary(); emit(OP_MUL, 0); }
        else if (*p == '/') { p++; parse_unary(); emit(OP_DIV, 0); }
        else if (starts_factor(p)) { parse_pow(); emit(OP_MUL, 0); }
        else break;
    }
}

static void parse_expr(void)
{
    parse_term();
    while (!perr) {
        skip();
        if (*p == '+') { p++; parse_term(); emit(OP_ADD, 0); }
        else if (*p == '-') { p++; parse_term(); emit(OP_SUB, 0); }
        else break;
    }
}

bool mx_compile(const char *s, char end, struct mx_prog *out)
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

static real to_rad(real a) { return mx_degrees ? a * PI_D / 180.0L : a; }
static real from_rad(real r) { return mx_degrees ? r * 180.0L / PI_D : r; }

static real apply(int f, real v)
{
    switch (f) {
    case F_SIN: return sin(to_rad(v));
    case F_COS: return cos(to_rad(v));
    case F_TAN: { real c = cos(to_rad(v)); return fabs(c) < 1e-14L ? NAN : sin(to_rad(v)) / c; }
    case F_CSC: { real s = sin(to_rad(v)); return fabs(s) < 1e-14L ? NAN : 1.0L / s; }
    case F_SEC: { real c = cos(to_rad(v)); return fabs(c) < 1e-14L ? NAN : 1.0L / c; }
    case F_COT: { real s = sin(to_rad(v)); return fabs(s) < 1e-14L ? NAN : cos(to_rad(v)) / s; }
    case F_ASIN: return (v > 1 || v < -1) ? NAN : from_rad(asin(v));
    case F_ACOS: return (v > 1 || v < -1) ? NAN : from_rad(acos(v));
    case F_ATAN: return from_rad(atan(v));
    case F_SQRT: return v < 0 ? NAN : sqrt(v);
    case F_ABS: return fabs(v);
    case F_LN: return v <= 0 ? NAN : log(v);
    case F_LOG: return v <= 0 ? NAN : log10(v);
    default: return exp(v);
    }
}

static real power(real b, real e)
{
    int n = (int)e;
    if (e == (real)n && n >= -32 && n <= 32) {
        real r = 1.0L;
        int m = n < 0 ? -n : n;
        real base = b;
        while (m) {
            if (m & 1) r *= base;
            base *= base;
            m >>= 1;
        }
        return n < 0 ? 1.0L / r : r;
    }
    if (b < 0) return NAN;
    return pow(b, e);
}

real mx_run(const struct mx_prog *pr, real x)
{
    real st[STACK + 1];
    int sp = 0, i;

    for (i = 0; i < pr->nops; i++) {
        switch (pr->op[i]) {
        case OP_CONST: st[sp++] = pr->k[pr->arg[i]]; break;
        case OP_X: st[sp++] = x; break;
        case OP_ADD: sp--; st[sp - 1] += st[sp]; break;
        case OP_SUB: sp--; st[sp - 1] -= st[sp]; break;
        case OP_MUL: sp--; st[sp - 1] *= st[sp]; break;
        case OP_DIV: sp--; st[sp - 1] = st[sp] == 0.0L ? NAN : st[sp - 1] / st[sp]; break;
        case OP_POW: sp--; st[sp - 1] = power(st[sp - 1], st[sp]); break;
        case OP_NEG: st[sp - 1] = -st[sp - 1]; break;
        default: st[sp - 1] = apply(pr->arg[i], st[sp - 1]); break;
        }
    }
    return st[0];
}

bool mx_has_trig(const char *s)
{
    static const char *const names[] = {"sin", "cos", "tan", "csc", "sec", "cot"};
    int i;
    for (; *s; s++) {
        for (i = 0; i < 6; i++) {
            if (!strncmp(s, names[i], 3)) return true;
        }
    }
    return false;
}
