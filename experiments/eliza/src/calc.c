/* A small calculator for chat messages: symbols (+ - * / ^ ( ) %), number
 * words ("twenty five"), and operator words ("times", "divided by", "squared",
 * "square root of", "percent of"). Uses double for accuracy. */
#include "calc.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define MAX_TOK 48

enum { T_NUM, T_PLUS, T_MINUS, T_MUL, T_DIV, T_POW, T_LP, T_RP, T_SQRT, T_FUNC, T_MOD };

#define PI_D 3.14159265358979323846

static double last_ans;
static bool have_ans;
static bool use_rad;

struct tok {
    uint8_t type;
    double val;
};

static struct tok toks[MAX_TOK];
static int ntok;
static int pos;
static bool bad;
static bool div_zero;

static bool add_tok(int type, double val)
{
    if (ntok >= MAX_TOK) {
        return false;
    }
    toks[ntok].type = (uint8_t)type;
    toks[ntok].val = val;
    ntok++;
    return true;
}

static const char *const small_words[] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
    "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
};
static const char *const tens_words[] = {
    "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety",
};
struct func_name {
    const char *name;
    uint8_t id;
};
static const struct func_name func_names[] = {
    {"sin", 0}, {"sine", 0}, {"cos", 1}, {"cosine", 1}, {"tan", 2}, {"tangent", 2},
    {"asin", 3}, {"arcsin", 3}, {"acos", 4}, {"arccos", 4}, {"atan", 5}, {"arctan", 5},
    {"ln", 6}, {"log", 7}, {"abs", 8},
};
static const char *const filler[] = {
    "in", "degrees", "degree", "deg", "radians", "radian", "rad",
    "what", "whats", "is", "calculate", "calc", "compute", "solve", "equals", "equal", "the",
    "tell", "me", "please", "answer", "can", "you", "give", "do", "does", "how", "much", "by",
    "to", "whats", "and", "square",
};

static bool in_list(const char *const *list, int n, const char *w)
{
    int i;
    for (i = 0; i < n; i++) {
        if (!strcmp(list[i], w)) {
            return true;
        }
    }
    return false;
}

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Value of a number word, or -1. Sets *kind: 0 small, 1 tens, 2 hundred, 3 thousand, 4 million. */
static double number_word(const char *w, int *kind)
{
    int i;
    for (i = 0; i < N(small_words); i++) {
        if (!strcmp(small_words[i], w)) {
            *kind = 0;
            return i;
        }
    }
    for (i = 0; i < N(tens_words); i++) {
        if (!strcmp(tens_words[i], w)) {
            *kind = 1;
            return (i + 2) * 10;
        }
    }
    if (!strcmp(w, "hundred")) { *kind = 2; return 100; }
    if (!strcmp(w, "thousand")) { *kind = 3; return 1000; }
    if (!strcmp(w, "million")) { *kind = 4; return 1000000; }
    return -1;
}

static int func_id(const char *w)
{
    int i;
    for (i = 0; i < N(func_names); i++) {
        if (!strcmp(func_names[i].name, w)) {
            return func_names[i].id;
        }
    }
    return -1;
}

/* Turn the text into tokens. False if anything is not arithmetic. */
static bool tokenize(const char *s)
{
    bool after_percent = false;
    bool in_words = false;
    double total = 0, current = 0;

    ntok = 0;
    use_rad = false;
    while (*s) {
        char c = *s;
        if ((c >= '0' && c <= '9') || (c == '.' && s[1] >= '0' && s[1] <= '9')) {
            double v = 0, scale = 0.1;
            while (*s >= '0' && *s <= '9') {
                v = v * 10 + (*s - '0');
                s++;
            }
            if (*s == '.') {
                s++;
                while (*s >= '0' && *s <= '9') {
                    v += (*s - '0') * scale;
                    scale /= 10;
                    s++;
                }
            }
            if (in_words) { add_tok(T_NUM, total + current); in_words = false; total = current = 0; }
            if (!add_tok(T_NUM, v)) return false;
            continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            char w[16];
            int n = 0, kind;
            double nv;
            while (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '\'') && n < 15) {
                if (*s != '\'') {
                    w[n++] = (*s >= 'A' && *s <= 'Z') ? (char)(*s + 32) : *s;
                }
                s++;
            }
            while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '\'') {
                s++;
            }
            w[n] = 0;

            nv = number_word(w, &kind);
            if (nv >= 0) {
                if (kind <= 1) current += nv;
                else if (kind == 2) current = (current == 0 ? 1 : current) * 100;
                else { total += (current == 0 ? 1 : current) * nv; current = 0; }
                in_words = true;
                continue;
            }
            if (in_words && !strcmp(w, "and")) {
                continue;
            }
            if (!strcmp(w, "radians") || !strcmp(w, "radian") || !strcmp(w, "rad")) {
                use_rad = true;
            }
            if (in_words) {
                if (!add_tok(T_NUM, total + current)) return false;
                in_words = false;
                total = current = 0;
            }
            if (!strcmp(w, "ans")) { if (!have_ans || !add_tok(T_NUM, last_ans)) return false; }
            else if (!strcmp(w, "pi")) { if (!add_tok(T_NUM, PI_D)) return false; }
            else if (!strcmp(w, "e")) { if (!add_tok(T_NUM, 2.718281828459045)) return false; }
            else if (!strcmp(w, "mod")) { if (!add_tok(T_MOD, 0)) return false; }
            else if (func_id(w) >= 0) { if (!add_tok(T_FUNC, func_id(w))) return false; }
            else if (!strcmp(w, "plus")) { if (!add_tok(T_PLUS, 0)) return false; }
            else if (!strcmp(w, "minus")) { if (!add_tok(T_MINUS, 0)) return false; }
            else if (!strcmp(w, "times") || !strcmp(w, "multiplied") || !strcmp(w, "x")) { if (!add_tok(T_MUL, 0)) return false; }
            else if (!strcmp(w, "divided") || !strcmp(w, "over")) { if (!add_tok(T_DIV, 0)) return false; }
            else if (!strcmp(w, "power")) { if (!add_tok(T_POW, 0)) return false; }
            else if (!strcmp(w, "squared")) { if (!add_tok(T_POW, 0) || !add_tok(T_NUM, 2)) return false; }
            else if (!strcmp(w, "cubed")) { if (!add_tok(T_POW, 0) || !add_tok(T_NUM, 3)) return false; }
            else if (!strcmp(w, "root") || !strcmp(w, "sqrt")) { if (!add_tok(T_SQRT, 0)) return false; }
            else if (!strcmp(w, "percent")) { if (!add_tok(T_DIV, 0) || !add_tok(T_NUM, 100)) return false; after_percent = true; }
            else if (!strcmp(w, "of")) { if (after_percent) { if (!add_tok(T_MUL, 0)) return false; after_percent = false; } }
            else if (!in_list(filler, N(filler), w)) return false;
            continue;
        }
        if (in_words && (c == ' ' || c == ',' || (c == '-' && ((s[1] >= 'a' && s[1] <= 'z') || (s[1] >= 'A' && s[1] <= 'Z'))))) {
            s++;
            continue;
        }
        if (in_words) {
            if (!add_tok(T_NUM, total + current)) return false;
            in_words = false;
            total = current = 0;
        }
        switch (c) {
        case '+': if (!add_tok(T_PLUS, 0)) return false; break;
        case '-': if (!add_tok(T_MINUS, 0)) return false; break;
        case '*': if (!add_tok(T_MUL, 0)) return false; break;
        case '/': if (!add_tok(T_DIV, 0)) return false; break;
        case '^': if (!add_tok(T_POW, 0)) return false; break;
        case '(': if (!add_tok(T_LP, 0)) return false; break;
        case ')': if (!add_tok(T_RP, 0)) return false; break;
        case '%': if (!add_tok(T_DIV, 0) || !add_tok(T_NUM, 100)) return false; after_percent = true; break;
        default: break; /* spaces, ?, !, =, ",", "." are ignored */
        }
        s++;
    }
    if (in_words) {
        if (!add_tok(T_NUM, total + current)) return false;
    }
    return true;
}

/* ---------- parsing: expr = term {+|- term}; term = unary {*|/ unary} ---------- */

static double parse_expr(void);

static double parse_atom(void)
{
    if (pos >= ntok) { bad = true; return 0; }
    if (toks[pos].type == T_NUM) return toks[pos++].val;
    if (toks[pos].type == T_LP) {
        double v;
        pos++;
        v = parse_expr();
        if (pos < ntok && toks[pos].type == T_RP) pos++;
        return v;
    }
    if (toks[pos].type == T_FUNC) {
        int id = (int)toks[pos++].val;
        double v, r;
        bool neg = false;
        if (pos < ntok && toks[pos].type == T_MINUS) {
            neg = true;
            pos++;
        }
        v = parse_atom();
        if (neg) v = -v;
        if (id <= 2) {
            double a = use_rad ? v : v * PI_D / 180.0;
            r = id == 0 ? sin(a) : id == 1 ? cos(a) : tan(a);
            if (id == 2 && fabs(cos(a)) < 1e-12) { bad = true; return 0; }
        } else if (id <= 5) {
            if (id <= 4 && (v > 1 || v < -1)) { bad = true; return 0; }
            r = id == 3 ? asin(v) : id == 4 ? acos(v) : atan(v);
            if (!use_rad) r = r * 180.0 / PI_D;
        } else if (id == 6 || id == 7) {
            if (v <= 0) { bad = true; return 0; }
            r = id == 6 ? log(v) : log10(v);
        } else {
            r = fabs(v);
        }
        return r;
    }
    if (toks[pos].type == T_SQRT) {
        double v;
        pos++;
        v = parse_atom();
        if (v < 0) { bad = true; return 0; }
        return sqrt(v);
    }
    bad = true;
    return 0;
}

static double power(double b, double e)
{
    int n = (int)e;
    if (e == (double)n && n >= 0 && n <= 64) {
        double r = 1;
        while (n--) r *= b;
        return r;
    }
    if (b < 0) { bad = true; return 0; }
    return pow(b, e);
}

static double parse_unary(void);

static double parse_pow(void)
{
    double b = parse_atom();
    if (pos < ntok && toks[pos].type == T_POW) {
        pos++;
        return power(b, parse_unary());
    }
    return b;
}

static double parse_unary(void)
{
    if (pos < ntok && toks[pos].type == T_MINUS) {
        pos++;
        return -parse_unary();
    }
    if (pos < ntok && toks[pos].type == T_PLUS) {
        pos++;
    }
    return parse_pow();
}

static double parse_term(void)
{
    double v = parse_unary();
    while (!bad && pos < ntok) {
        if (toks[pos].type == T_MUL) {
            pos++;
            v *= parse_unary();
        } else if (toks[pos].type == T_DIV) {
            double d;
            pos++;
            d = parse_unary();
            if (d == 0) { div_zero = true; return 0; }
            v /= d;
        } else if (toks[pos].type == T_MOD) {
            double d;
            pos++;
            d = parse_unary();
            if (d == 0) { div_zero = true; return 0; }
            v = fmod(v, d);
        } else if (toks[pos].type == T_LP || toks[pos].type == T_FUNC || toks[pos].type == T_SQRT) {
            v *= parse_pow();
        } else {
            break;
        }
    }
    return v;
}

static double parse_expr(void)
{
    double v = parse_term();
    while (!bad && !div_zero && pos < ntok) {
        if (toks[pos].type == T_PLUS) { pos++; v += parse_term(); }
        else if (toks[pos].type == T_MINUS) { pos++; v -= parse_term(); }
        else break;
    }
    return v;
}

/* ---------- formatting ---------- */

static void put_uint(char **o, unsigned long long v)
{
    char tmp[24];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while (v);
    while (n) *(*o)++ = tmp[--n];
}

static void format_number(double v, char *out)
{
    char *o = out;
    unsigned long long ip;
    long frac;

    if (v != v || v > 1e15 || v < -1e15) {
        strcpy(out, "too big for me");
        return;
    }
    if (v < 0) {
        *o++ = '-';
        v = -v;
    }
    ip = (unsigned long long)v;
    frac = (long)((v - (double)ip) * 1000000.0 + 0.5);
    if (frac >= 1000000) {
        ip++;
        frac -= 1000000;
    }
    if (o != out && ip == 0 && frac == 0) {
        o = out;
    }
    put_uint(&o, ip);
    if (frac) {
        char digits[7];
        int i, last = 5;
        for (i = 5; i >= 0; i--) {
            digits[i] = (char)('0' + frac % 10);
            frac /= 10;
        }
        while (last > 0 && digits[last] == '0') last--;
        *o++ = '.';
        for (i = 0; i <= last; i++) *o++ = digits[i];
    }
    *o = 0;
}


/* ---------- unit conversion: "100 knots to mph", "20 c to f" ---------- */

struct unit {
    const char *names;
    uint8_t kind; /* 0 length (m), 1 speed (m/s), 2 mass (kg), 3 temperature */
    double factor;
    const char *label;
};

static const struct unit units[] = {
    {" m meter meters metre metres ", 0, 1.0, "m"},
    {" km kilometer kilometers kilometre kilometres ", 0, 1000.0, "km"},
    {" cm centimeter centimeters ", 0, 0.01, "cm"},
    {" mm millimeter millimeters ", 0, 0.001, "mm"},
    {" mi mile miles ", 0, 1609.344, "mi"},
    {" ft foot feet ", 0, 0.3048, "ft"},
    {" inch inches ", 0, 0.0254, "in"},
    {" yd yard yards ", 0, 0.9144, "yd"},
    {" nmi ", 0, 1852.0, "nmi"},
    {" kt kts knot knots ", 1, 0.514444444, "kt"},
    {" mph ", 1, 0.44704, "mph"},
    {" kph kmh kmph ", 1, 0.277777778, "km/h"},
    {" mps ", 1, 1.0, "m/s"},
    {" fps ", 1, 0.3048, "ft/s"},
    {" kg kilogram kilograms kilo kilos ", 2, 1.0, "kg"},
    {" g gram grams ", 2, 0.001, "g"},
    {" lb lbs pound pounds ", 2, 0.45359237, "lb"},
    {" oz ounce ounces ", 2, 0.0283495231, "oz"},
    {" c celsius ", 3, 0, "C"},
    {" f fahrenheit ", 3, 1, "F"},
    {" k kelvin ", 3, 2, "K"},
};

static const struct unit *find_unit(const char *w)
{
    char pat[20];
    int i;
    if (strlen(w) > 14) {
        return NULL;
    }
    strcpy(pat, " ");
    strcat(pat, w);
    strcat(pat, " ");
    for (i = 0; i < N(units); i++) {
        if (strstr(units[i].names, pat)) {
            return &units[i];
        }
    }
    return NULL;
}

static bool is_conv_filler(const char *w)
{
    static const char *const f[] = {
        "convert", "what", "whats", "is", "how", "many", "much", "the", "a", "about", "equal", "equals",
        "are", "does", "do", "please", "tell", "me", "give", "in", "into", "to", "as",
    };
    return in_list(f, N(f), w);
}

static bool is_separator(const char *w)
{
    return !strcmp(w, "to") || !strcmp(w, "in") || !strcmp(w, "into") || !strcmp(w, "as");
}

static bool convert_try(const char *s, char *result, size_t cap)
{
    int state = 0;
    double v = 0;
    const struct unit *from = NULL, *to = NULL;
    char num[32];

    while (*s) {
        char c = *s;
        if ((c >= '0' && c <= '9') || (c == '-' && s[1] >= '0' && s[1] <= '9' && state == 0)
            || (c == '.' && s[1] >= '0' && s[1] <= '9')) {
            double scale = 0.1;
            bool neg = false;
            if (state != 0) return false;
            if (c == '-') { neg = true; s++; }
            v = 0;
            while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
            if (*s == '.') {
                s++;
                while (*s >= '0' && *s <= '9') { v += (*s - '0') * scale; scale /= 10; s++; }
            }
            if (neg) v = -v;
            state = 1;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            char w[16];
            int n = 0;
            while (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) && n < 15) {
                w[n++] = (*s >= 'A' && *s <= 'Z') ? (char)(*s + 32) : *s;
                s++;
            }
            w[n] = 0;
            if (*s >= 'a' && *s <= 'z') return false;
            if (state == 1 && !(is_separator(w) && !find_unit(w))) {
                from = find_unit(w);
                if (!from) return false;
                state = 2;
            } else if (state == 2 && is_separator(w)) {
                state = 3;
            } else if (state == 3 && !is_conv_filler(w)) {
                to = find_unit(w);
                if (!to) return false;
                state = 4;
            } else if (state == 3) {
                /* "to the" and similar: keep waiting */
            } else if (!is_conv_filler(w)) {
                return false;
            }
        } else {
            s++;
        }
    }
    if (state != 4 || !from || !to) {
        return false;
    }
    if (from->kind != to->kind) {
        strncpy(result, "Those units don't match.", cap - 1);
        result[cap - 1] = 0;
        return true;
    }
    if (from->kind == 3) {
        double k = from->factor == 0 ? v + 273.15 : from->factor == 1 ? (v - 32) * 5.0 / 9.0 + 273.15 : v;
        v = to->factor == 0 ? k - 273.15 : to->factor == 1 ? (k - 273.15) * 9.0 / 5.0 + 32 : k;
    } else {
        v = v * from->factor / to->factor;
    }
    last_ans = v;
    have_ans = true;
    format_number(v, num);
    strncpy(result, "= ", cap - 1);
    result[cap - 1] = 0;
    strncat(result, num, cap - strlen(result) - 1);
    strncat(result, " ", cap - strlen(result) - 1);
    strncat(result, to->label, cap - strlen(result) - 1);
    return true;
}

bool calc_try(const char *input, char *result, size_t cap)
{
    int i, ops = 0, nums = 0;
    double v;
    char num[32];

    if (convert_try(input, result, cap)) {
        return true;
    }
    if (!tokenize(input) || ntok < 2) {
        return false;
    }
    for (i = 0; i < ntok; i++) {
        if (toks[i].type == T_NUM) nums++;
        else if (toks[i].type != T_LP && toks[i].type != T_RP) ops++;
    }
    if (!nums || !ops) {
        return false;
    }
    if (have_ans && ntok < MAX_TOK
        && (toks[0].type == T_MUL || toks[0].type == T_DIV || toks[0].type == T_POW || toks[0].type == T_MOD)) {
        memmove(&toks[1], &toks[0], ntok * sizeof(toks[0]));
        toks[0].type = T_NUM;
        toks[0].val = last_ans;
        ntok++;
    }
    pos = 0;
    bad = false;
    div_zero = false;
    v = parse_expr();
    if (div_zero) {
        strncpy(result, "I can't divide by zero.", cap - 1);
        result[cap - 1] = 0;
        return true;
    }
    if (bad || pos != ntok) {
        return false;
    }
    last_ans = v;
    have_ans = true;
    format_number(v, num);
    strncpy(result, "= ", cap - 1);
    result[cap - 1] = 0;
    strncat(result, num, cap - strlen(result) - 1);
    return true;
}
