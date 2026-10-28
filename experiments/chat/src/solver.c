/* Guided solver for the gas laws and triangle laws: asks for each value, takes
 * ? for the unknown, then shows the working. Pure C so it can be tested on a
 * computer. */
#include "solver.h"
#include "eliza.h"
#include "tri.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAXV 6
#define PI_D 3.14159265358979323846

enum { K_POS, K_NONNEG, K_PRESS, K_VOL, K_TEMP, K_MOLES, K_ANGLE };
enum { T_PRODUCT, T_DALTON, T_SINES, T_COSINES, T_TRIANGLE };
enum { A_GAS, A_TRIG };
enum { S_IDLE, S_CHOOSE, S_TUNIT, S_PUNIT, S_VALUE };

struct var {
    const char *name;
    const char *prompt;
    uint8_t kind;
};

struct law {
    const char *id;
    const char *title;
    const char *formula;
    uint8_t app, type, nvars;
    bool has_t;  /* asks whether temperatures are in C or K */
    bool has_r;  /* asks for a pressure unit and uses R */
    struct var v[MAXV];
    int8_t e[MAXV + 1]; /* exponents in "product = 1" form; e[nvars] is R */
    const char *solve_for[MAXV];
    const char *note;
    const char *after;
};

static const struct law laws[] = {
    {"boyle", "Boyle's law", "P1 x V1 = P2 x V2", A_GAS, T_PRODUCT, 4, false, false,
     {{"P1", "pressure", K_PRESS}, {"V1", "volume", K_VOL}, {"P2", "pressure", K_PRESS}, {"V2", "volume", K_VOL}},
     {1, 1, -1, -1, 0, 0, 0},
     {"P2 x V2 / V1", "P2 x V2 / P1", "P1 x V1 / V2", "P1 x V1 / P2"},
     "Use the same unit for both pressures and for both volumes.",
     "solve boyle|boyle example|charles's law"},
    {"charles", "Charles's law", "V1 / T1 = V2 / T2", A_GAS, T_PRODUCT, 4, true, false,
     {{"V1", "volume", K_VOL}, {"T1", "temperature", K_TEMP}, {"V2", "volume", K_VOL}, {"T2", "temperature", K_TEMP}},
     {1, -1, -1, 1, 0, 0, 0},
     {"V2 x T1 / T2", "V1 x T2 / V2", "V1 x T2 / T1", "V2 x T1 / V1"},
     "Use the same unit for both volumes.",
     "solve charles|charles example|gay-lussac's law"},
    {"gay-lussac", "Gay-Lussac's law", "P1 / T1 = P2 / T2", A_GAS, T_PRODUCT, 4, true, false,
     {{"P1", "pressure", K_PRESS}, {"T1", "temperature", K_TEMP}, {"P2", "pressure", K_PRESS}, {"T2", "temperature", K_TEMP}},
     {1, -1, -1, 1, 0, 0, 0},
     {"P2 x T1 / T2", "P1 x T2 / P2", "P1 x T2 / T1", "P2 x T1 / P1"},
     "Use the same unit for both pressures.",
     "solve gay-lussac|gay-lussac example|combined gas law"},
    {"combined", "Combined gas law", "P1 x V1 / T1 = P2 x V2 / T2", A_GAS, T_PRODUCT, 6, true, false,
     {{"P1", "pressure", K_PRESS}, {"V1", "volume", K_VOL}, {"T1", "temperature", K_TEMP},
      {"P2", "pressure", K_PRESS}, {"V2", "volume", K_VOL}, {"T2", "temperature", K_TEMP}},
     {1, 1, -1, -1, -1, 1, 0},
     {"P2 x V2 x T1 / ( V1 x T2 )", "P2 x V2 x T1 / ( P1 x T2 )", "P1 x V1 x T2 / ( P2 x V2 )",
      "P1 x V1 x T2 / ( V2 x T1 )", "P1 x V1 x T2 / ( P2 x T1 )", "P2 x V2 x T1 / ( P1 x V1 )"},
     "Use the same unit for both pressures and for both volumes.",
     "solve combined|combined example|ideal gas law"},
    {"avogadro", "Avogadro's law", "V1 / n1 = V2 / n2", A_GAS, T_PRODUCT, 4, false, false,
     {{"V1", "volume", K_VOL}, {"n1", "moles", K_MOLES}, {"V2", "volume", K_VOL}, {"n2", "moles", K_MOLES}},
     {1, -1, -1, 1, 0, 0, 0},
     {"V2 x n1 / n2", "V1 x n2 / V2", "V1 x n2 / n1", "V2 x n1 / V1"},
     "Same temperature and pressure. Use the same unit for both volumes.",
     "solve avogadro|avogadro example|molar volume"},
    {"ideal", "Ideal gas law", "P x V = n x R x T", A_GAS, T_PRODUCT, 4, true, true,
     {{"P", "pressure", K_PRESS}, {"V", "volume in liters", K_VOL}, {"n", "moles", K_MOLES}, {"T", "temperature", K_TEMP}},
     {1, 1, -1, -1, -1, 0, 0},
     {"n x R x T / V", "n x R x T / P", "P x V / ( R x T )", "P x V / ( n x R )"},
     "Volume must be in liters and moles in mol.",
     "solve ideal gas|ideal gas example|gas constant"},
    {"dalton", "Dalton's law", "Ptotal = P1 + P2 + P3", A_GAS, T_DALTON, 4, false, false,
     {{"Ptotal", "total pressure", K_NONNEG}, {"P1", "partial pressure of gas 1", K_NONNEG},
      {"P2", "partial pressure of gas 2 (0 if none)", K_NONNEG}, {"P3", "partial pressure of gas 3 (0 if none)", K_NONNEG}},
     {0, 0, 0, 0, 0, 0, 0},
     {"P1 + P2 + P3", "Ptotal - P2 - P3", "Ptotal - P1 - P3", "Ptotal - P1 - P2"},
     "Use the same pressure unit for every value.",
     "solve dalton|dalton example|mole fraction"},
    {"graham", "Graham's law", "rate1 / rate2 = sqrt( M2 / M1 )", A_GAS, T_PRODUCT, 4, false, false,
     {{"rate1", "rate of gas 1", K_POS}, {"rate2", "rate of gas 2", K_POS},
      {"M1", "molar mass of gas 1", K_POS}, {"M2", "molar mass of gas 2", K_POS}},
     {2, -2, 1, -1, 0, 0, 0},
     {"rate2 x sqrt( M2 / M1 )", "rate1 x sqrt( M1 / M2 )", "M2 x ( rate2 / rate1 )^2", "M1 x ( rate1 / rate2 )^2"},
     "Use the same unit for both rates and both molar masses.",
     "solve graham|graham example|effusion"},
    {"law of sines", "Law of Sines", "a / sin A = b / sin B", A_TRIG, T_SINES, 4, false, false,
     {{"a", "side a", K_POS}, {"A", "angle A in degrees", K_ANGLE}, {"b", "side b", K_POS}, {"B", "angle B in degrees", K_ANGLE}},
     {0, 0, 0, 0, 0, 0, 0},
     {"b x sin A / sin B", "arcsin( a x sin B / b )", "a x sin B / sin A", "arcsin( b x sin A / a )"},
     "Each side is opposite the angle with the same letter.",
     "solve law of sines|law of cosines|solve triangle"},
    {"law of cosines", "Law of Cosines", "c^2 = a^2 + b^2 - 2 x a x b x cos C", A_TRIG, T_COSINES, 4, false, false,
     {{"a", "side a", K_POS}, {"b", "side b", K_POS}, {"C", "angle C in degrees (between a and b)", K_ANGLE}, {"c", "side c (opposite C)", K_POS}},
     {0, 0, 0, 0, 0, 0, 0},
     {"", "", "arccos( ( a^2 + b^2 - c^2 ) / ( 2 x a x b ) )", "sqrt( a^2 + b^2 - 2 x a x b x cos C )"},
     "I can find c or C. For a or b, relabel the triangle so the unknown side is c.",
     "solve law of cosines|law of sines|solve triangle"},
    {"triangle", "Triangle solver", "sides a b c, angles A B C", A_TRIG, T_TRIANGLE, 6, false, false,
     {{"a", "side a", K_POS}, {"b", "side b", K_POS}, {"c", "side c", K_POS},
      {"A", "angle A in degrees", K_ANGLE}, {"B", "angle B in degrees", K_ANGLE}, {"C", "angle C in degrees", K_ANGLE}},
     {0, 0, 0, 0, 0, 0, 0},
     {"", "", "", "", "", ""},
     "Enter exactly three values (at least one a side) and put ? for the other three.",
     "solve triangle|law of sines|law of cosines"},
};

#define NUM_LAWS ((int)(sizeof(laws) / sizeof(laws[0])))

static struct {
    int state;
    const struct law *law;
    int step;
    bool kelvin;
    int punit; /* 0 atm, 1 kPa, 2 mmHg */
    char typed[MAXV][16];
    double val[MAXV];
    bool unk[MAXV];
} S;

static const char *next_prompts;

/* ---------- small helpers ---------- */

static void put(char **o, char *end, const char *s)
{
    while (*s && *o < end) {
        *(*o)++ = *s++;
    }
}

/* Append v with about 4 significant digits, dropping trailing zeros. */
static void put_sig(char **o, char *end, double v)
{
    char buf[40], *b = buf;
    unsigned long long ip;
    double scale = 1.0, a;
    long frac;
    int places, i, digits;

    if (v == 0) {
        put(o, end, "0");
        return;
    }
    if (v < 0) {
        *b++ = '-';
        v = -v;
    }
    a = v;
    digits = (int)floor(log10(a)) + 1;
    places = 4 - digits;
    if (places < 0) places = 0;
    if (places > 6) places = 6;
    for (i = 0; i < places; i++) scale *= 10.0;
    v = floor(v * scale + 0.5) / scale;
    ip = (unsigned long long)v;
    frac = (long)((v - (double)ip) * scale + 0.5);
    {
        char tmp[24];
        int n = 0;
        unsigned long long t = ip;
        do { tmp[n++] = (char)('0' + (int)(t % 10)); t /= 10; } while (t);
        while (n) *b++ = tmp[--n];
    }
    if (frac && places > 0) {
        char d[8];
        int last = places - 1;
        for (i = places - 1; i >= 0; i--) { d[i] = (char)('0' + frac % 10); frac /= 10; }
        while (last > 0 && d[last] == '0') last--;
        *b++ = '.';
        for (i = 0; i <= last; i++) *b++ = d[i];
    }
    *b = 0;
    put(o, end, buf);
}

static bool parse_num(const char *s, double *out)
{
    double v = 0, scale = 0.1;
    bool neg = false, digits = false;

    while (*s == ' ') s++;
    if (*s == '-') { neg = true; s++; } else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; digits = true; }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { v += (*s - '0') * scale; scale /= 10; s++; digits = true; }
    }
    while (*s == ' ') s++;
    if (!digits || *s) return false;
    *out = neg ? -v : v;
    return true;
}

static bool is_unknown(const char *s)
{
    while (*s == ' ') s++;
    return (s[0] == '?' || s[0] == 'x' || s[0] == 'X') && (s[1] == 0 || s[1] == ' ');
}

static bool has_word(char w[][ELIZA_WORD_LEN], int n, const char *x)
{
    int i;
    for (i = 0; i < n; i++) {
        if (!strcmp(w[i], x)) return true;
    }
    return false;
}

static bool allowed(const struct law *l)
{
    return (l->app == A_GAS && app_solver_gas) || (l->app == A_TRIG && app_solver_trig);
}

static const struct law *find_law(char w[][ELIZA_WORD_LEN], int n)
{
    static const struct { const char *word; const char *id; } map[] = {
        {"boyle", "boyle"}, {"charles", "charles"}, {"gay", "gay-lussac"}, {"lussac", "gay-lussac"},
        {"gaylussac", "gay-lussac"}, {"combined", "combined"}, {"avogadro", "avogadro"},
        {"ideal", "ideal"}, {"nrt", "ideal"}, {"pvnrt", "ideal"}, {"dalton", "dalton"},
        {"graham", "graham"}, {"cosines", "law of cosines"}, {"cosine", "law of cosines"},
        {"sines", "law of sines"}, {"sine", "law of sines"}, {"triangle", "triangle"},
        {"triangles", "triangle"},
    };
    int i, j;
    for (i = 0; i < (int)(sizeof(map) / sizeof(map[0])); i++) {
        if (!has_word(w, n, map[i].word)) continue;
        for (j = 0; j < NUM_LAWS; j++) {
            if (!strcmp(laws[j].id, map[i].id) && allowed(&laws[j])) return &laws[j];
        }
    }
    return NULL;
}

/* ---------- conversation ---------- */

static const char *list_prompts(void)
{
    return app_solver_gas ? "solve boyle|solve charles|solve ideal gas"
                          : "solve law of sines|solve law of cosines|solve triangle";
}

static void ask_value(char **o, char *end)
{
    const struct var *v = &S.law->v[S.step];
    put(o, end, v->name);
    put(o, end, " (");
    put(o, end, v->prompt);
    if (v->kind == K_TEMP) put(o, end, S.kelvin ? ", in K" : ", in C");
    put(o, end, ")? ");
    if (S.step == 0) put(o, end, "Type ? for the unknown.");
}

static void start_values(char **o, char *end)
{
    int i;
    S.state = S_VALUE;
    S.step = 0;
    for (i = 0; i < MAXV; i++) { S.unk[i] = false; S.typed[i][0] = 0; }
    ask_value(o, end);
}

static void begin(const struct law *l, char **o, char *end)
{
    S.law = l;
    S.kelvin = true;
    S.punit = 0;
    put(o, end, l->title);
    put(o, end, ": ");
    put(o, end, l->formula);
    put(o, end, "\n");
    put(o, end, l->note);
    put(o, end, "\n");
    if (l->has_t) {
        S.state = S_TUNIT;
        put(o, end, "Are your temperatures in c or k?");
    } else if (l->has_r) {
        S.state = S_PUNIT;
        put(o, end, "Pressure unit: atm, kpa or mmhg?");
    } else {
        start_values(o, end);
    }
    next_prompts = NULL;
}

static const char *const r_text[3] = {"0.0821", "8.314", "62.4"};
static const double r_val[3] = {0.0821, 8.314, 62.4};

/* the value used in the math: temperatures are converted to kelvin */
static double math_val(int i)
{
    if (S.law->v[i].kind == K_TEMP && !S.kelvin) return S.val[i] + 273.0;
    return S.val[i];
}

static void substitute(char **o, char *end, const char *rhs)
{
    char tok[40];
    const char *p = rhs;
    int i;

    while (*p) {
        int n = 0;
        bool found = false;
        char base[40];
        const char *suffix;
        while (*p && *p != ' ' && n < 38) tok[n++] = *p++;
        tok[n] = 0;
        while (*p == ' ') p++;
        /* a token like "a^2" is the variable a followed by "^2" */
        strcpy(base, tok);
        suffix = "";
        {
            char *caret = strchr(base, '^');
            if (caret && caret != base) {
                suffix = tok + (caret - base);
                *caret = 0;
            }
        }
        for (i = 0; i < S.law->nvars; i++) {
            if (!strcmp(base, S.law->v[i].name)) {
                put(o, end, "(");
                if (S.law->v[i].kind == K_TEMP && !S.kelvin) put_sig(o, end, S.val[i] + 273.0);
                else put(o, end, S.typed[i]);
                put(o, end, ")");
                put(o, end, suffix);
                found = true;
                break;
            }
        }
        if (!found && !strcmp(tok, "R")) {
            put(o, end, "(");
            put(o, end, r_text[S.punit]);
            put(o, end, ")");
            found = true;
        }
        if (!found) put(o, end, tok);
        if (*p) put(o, end, " ");
    }
}

static double to_rad(double d) { return d * PI_D / 180.0; }
static double to_deg(double r) { return r * 180.0 / PI_D; }

static void finish_triangle(char **o, char *end)
{
    char query[120], result[600];
    char *q = query;
    int i, ns = 0, na = 0;
    static const char *const sn[3] = {"sa ", "sb ", "sc "};
    static const char *const an[3] = {"aa ", "ab ", "ac "};

    for (i = 0; i < 6; i++) {
        if (!S.unk[i]) {
            if (i < 3) ns++; else na++;
        }
    }
    if (ns + na != 3 || ns == 0) {
        put(o, end, "I need exactly three known values, at least one a side. Let's try again.\n");
        start_values(o, end);
        return;
    }
    for (i = 0; i < 6; i++) {
        if (!S.unk[i]) {
            const char *tag = i < 3 ? sn[i] : an[i - 3];
            char tmp[20];
            while (*tag) *q++ = *tag++;
            strcpy(tmp, S.typed[i]);
            { const char *t = tmp; while (*t) *q++ = *t++; }
            *q++ = ' ';
        }
    }
    *q = 0;
    tri_try(query, result, sizeof(result));
    put(o, end, "Method: ");
    if (ns == 3) put(o, end, "SSS, so Law of Cosines for each angle.");
    else if (ns == 1) put(o, end, "two angles and a side: third angle = 180 - the other two, then Law of Sines.");
    else {
        /* two sides and an angle: SAS if the angle is between them */
        int ai = -1;
        for (i = 3; i < 6; i++) if (!S.unk[i]) ai = i - 3;
        if (!S.unk[(ai + 1) % 3] && !S.unk[(ai + 2) % 3]) put(o, end, "SAS, so Law of Cosines for the third side, then Law of Sines.");
        else put(o, end, "SSA (ambiguous case): Law of Sines, then check for 0, 1 or 2 triangles.");
    }
    put(o, end, "\n");
    put(o, end, result);
}

static void solve_and_show(char **o, char *end)
{
    const struct law *l = &S.law[0];
    int u = -1, i, count = 0;
    double result = 0;
    bool temp_unknown;

    for (i = 0; i < l->nvars; i++) {
        if (S.unk[i]) { u = i; count++; }
    }
    S.state = S_IDLE;
    next_prompts = l->after;
    if (l->type == T_TRIANGLE) {
        finish_triangle(o, end);
        if (S.state == S_VALUE) next_prompts = NULL;
        return;
    }
    if (count != 1) {
        put(o, end, "I need exactly one ? to solve for. Let's try again.\n");
        start_values(o, end);
        next_prompts = NULL;
        return;
    }

    /* the law and any temperature conversions */
    put(o, end, l->title);
    put(o, end, ": ");
    put(o, end, l->formula);
    put(o, end, "\n");
    for (i = 0; i < l->nvars; i++) {
        if (l->v[i].kind == K_TEMP && !S.kelvin && !S.unk[i]) {
            put(o, end, l->v[i].name);
            put(o, end, " = ");
            put(o, end, S.typed[i]);
            put(o, end, " C + 273 = ");
            put_sig(o, end, S.val[i] + 273.0);
            put(o, end, " K\n");
        }
    }
    put(o, end, "Find ");
    put(o, end, l->v[u].name);
    put(o, end, ": ");
    put(o, end, l->v[u].name);
    put(o, end, " = ");
    put(o, end, l->solve_for[u]);
    put(o, end, "\n");

    /* compute */
    if (l->type == T_PRODUCT) {
        double prod = 1.0;
        for (i = 0; i < l->nvars; i++) {
            if (i != u) prod *= pow(math_val(i), -l->e[i]);
        }
        if (l->has_r) prod *= pow(r_val[S.punit], -l->e[l->nvars]);
        result = pow(prod, 1.0 / l->e[u]);
    } else if (l->type == T_DALTON) {
        if (u == 0) {
            result = S.val[1] + S.val[2] + S.val[3];
        } else {
            result = S.val[0];
            for (i = 1; i < 4; i++) if (i != u) result -= S.val[i];
        }
    } else if (l->type == T_SINES) {
        /* vars: a, A, b, B */
        double a = S.val[0], A = S.val[1], b = S.val[2], B = S.val[3];
        if (u == 0) result = b * sin(to_rad(A)) / sin(to_rad(B));
        else if (u == 2) result = a * sin(to_rad(B)) / sin(to_rad(A));
        else {
            double s = u == 1 ? a * sin(to_rad(B)) / b : b * sin(to_rad(A)) / a;
            if (s > 1.0) {
                put(o, end, "That sine would be more than 1, so no triangle fits these numbers.");
                return;
            }
            result = to_deg(asin(s));
        }
    } else {
        /* law of cosines: vars a, b, C, c */
        double a = S.val[0], b = S.val[1], C = S.val[2], c = S.val[3];
        if (u == 3) {
            double q = a * a + b * b - 2 * a * b * cos(to_rad(C));
            result = sqrt(q);
        } else if (u == 2) {
            double q = (a * a + b * b - c * c) / (2 * a * b);
            if (q > 1.0 || q < -1.0) {
                put(o, end, "No triangle has those side lengths.");
                return;
            }
            result = to_deg(acos(q));
        } else {
            put(o, end, "I can find c or C here. For a or b, relabel the triangle so the unknown side is c, or use the Law of Sines.");
            return;
        }
    }

    /* the substitution and the answer */
    put(o, end, l->v[u].name);
    put(o, end, " = ");
    substitute(o, end, l->solve_for[u]);
    put(o, end, "\n");
    temp_unknown = l->v[u].kind == K_TEMP;
    put(o, end, "Answer: ");
    put(o, end, l->v[u].name);
    put(o, end, " = ");
    put_sig(o, end, result);
    if (temp_unknown) put(o, end, " K");
    else if (l->v[u].kind == K_ANGLE) put(o, end, " degrees");
    if (temp_unknown) {
        put(o, end, " = ");
        put_sig(o, end, result - 273.0);
        put(o, end, " C");
    }
    if (l->type == T_SINES && (u == 1 || u == 3)) {
        put(o, end, "\nAmbiguous case: 180 - ");
        put_sig(o, end, result);
        put(o, end, " = ");
        put_sig(o, end, 180.0 - result);
        put(o, end, " could also work if the angles still add to less than 180.");
    } else if (l->type == T_DALTON && result < 0) {
        put(o, end, "\nThat's negative, so check your numbers.");
    } else if (l->type == T_PRODUCT || l->type == T_DALTON) {
        const struct var *a = &l->v[u];
        if (a->kind == K_PRESS || a->kind == K_VOL) put(o, end, " (same unit you used)");
    }
}

bool solver_active(void)
{
    return S.state != S_IDLE;
}

const char *solver_next(void)
{
    return next_prompts;
}

void solver_cancel(void)
{
    S.state = S_IDLE;
    next_prompts = NULL;
}

static bool validate(const struct var *v, double x, bool kelvin, char **o, char *end)
{
    bool ok = true;
    switch (v->kind) {
    case K_TEMP: ok = kelvin ? x > 0 : x > -273.0; break;
    case K_NONNEG: ok = x >= 0; break;
    case K_ANGLE: ok = x > 0 && x < 180; break;
    default: ok = x > 0; break;
    }
    if (!ok) {
        put(o, end, v->kind == K_ANGLE ? "An angle must be between 0 and 180. "
                    : v->kind == K_TEMP ? "That temperature is not possible. "
                    : "That must be positive. ");
    }
    return ok;
}

bool solver_handle(const char *input, char *out, size_t cap)
{
    char words[40][ELIZA_WORD_LEN];
    int nwords;
    char *o = out, *end = out + cap - 1;
    const struct law *found;

    next_prompts = NULL;
    nwords = eliza_tokens(input, words, 40);

    if (S.state == S_IDLE) {
        if (!has_word(words, nwords, "solve")) return false;
        found = find_law(words, nwords);
        if (found) {
            begin(found, &o, end);
            *o = 0;
            return true;
        }
        /* "solve" with no law named */
        {
            int i;
            bool any = false;
            for (i = 0; i < NUM_LAWS; i++) if (allowed(&laws[i])) any = true;
            if (!any) return false;
        }
        S.state = S_CHOOSE;
        put(&o, end, app_solver_gas
            ? "Which law? boyle, charles, gay-lussac, combined, avogadro, ideal gas, dalton or graham."
            : "What should I solve? law of sines, law of cosines or triangle.");
        next_prompts = list_prompts();
        *o = 0;
        return true;
    }

    if (has_word(words, nwords, "stop") || has_word(words, nwords, "cancel") ||
        has_word(words, nwords, "quit") || has_word(words, nwords, "exit")) {
        S.state = S_IDLE;
        put(&o, end, "Okay, stopped. Ask me anything else.");
        *o = 0;
        return true;
    }

    if (S.state == S_CHOOSE) {
        found = find_law(words, nwords);
        if (found) {
            begin(found, &o, end);
        } else {
            put(&o, end, app_solver_gas ? "I didn't catch that. Try: boyle, charles, gay-lussac, combined, avogadro, ideal gas, dalton or graham."
                                        : "I didn't catch that. Try: law of sines, law of cosines or triangle.");
            next_prompts = list_prompts();
        }
        *o = 0;
        return true;
    }

    if (S.state == S_TUNIT) {
        if (has_word(words, nwords, "c") || has_word(words, nwords, "celsius")) S.kelvin = false;
        else if (has_word(words, nwords, "k") || has_word(words, nwords, "kelvin") || has_word(words, nwords, "yes")) S.kelvin = true;
        else {
            put(&o, end, "Please type c for Celsius or k for Kelvin.");
            *o = 0;
            return true;
        }
        /* "k" is cleaned to "yes" by the input cleanup, which is why yes counts as k */
        if (S.law->has_r) {
            S.state = S_PUNIT;
            put(&o, end, "Pressure unit: atm, kpa or mmhg?");
        } else {
            start_values(&o, end);
        }
        *o = 0;
        return true;
    }

    if (S.state == S_PUNIT) {
        if (has_word(words, nwords, "atm")) S.punit = 0;
        else if (has_word(words, nwords, "kpa")) S.punit = 1;
        else if (has_word(words, nwords, "mmhg") || has_word(words, nwords, "torr")) S.punit = 2;
        else {
            put(&o, end, "Please type atm, kpa or mmhg.");
            *o = 0;
            return true;
        }
        put(&o, end, "Using R = ");
        put(&o, end, r_text[S.punit]);
        put(&o, end, ". ");
        start_values(&o, end);
        *o = 0;
        return true;
    }

    /* S_VALUE: one value per message */
    {
        const struct var *v = &S.law->v[S.step];
        double x = 0;
        bool unknown = is_unknown(input);

        if (!unknown) {
            if (!parse_num(input, &x)) {
                put(&o, end, "Please type a number, or ? if this is the unknown. ");
                ask_value(&o, end);
                *o = 0;
                return true;
            }
            if (!validate(v, x, S.kelvin, &o, end)) {
                ask_value(&o, end);
                *o = 0;
                return true;
            }
            S.val[S.step] = x;
            S.unk[S.step] = false;
            strncpy(S.typed[S.step], input, 15);
            S.typed[S.step][15] = 0;
            {   /* trim spaces */
                char *t = S.typed[S.step];
                while (*t == ' ') memmove(t, t + 1, strlen(t));
                { int n = (int)strlen(t); while (n > 0 && t[n - 1] == ' ') t[--n] = 0; }
            }
        } else {
            S.unk[S.step] = true;
            strcpy(S.typed[S.step], "?");
        }
        S.step++;
        if (S.step < S.law->nvars) {
            put(&o, end, "Got it. ");
            ask_value(&o, end);
        } else {
            solve_and_show(&o, end);
        }
    }
    *o = 0;
    return true;
}
