#include "nn.h"
#include <math.h>
#include <string.h>

#define MAX_HIDDEN 64
#define MAX_CLASSES 320

static const uint8_t *blob;
static unsigned V, H, N;
static float s1, s2, temp;
static const uint8_t *vocab, *w1, *b1, *w2, *b2;

static unsigned rd16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static float rdf(const uint8_t *p)
{
    float f;
    memcpy(&f, p, sizeof(f));
    return f;
}

bool nn_init(const uint8_t *data, unsigned len)
{
    const unsigned header = 4 + 6 + 12;
    unsigned need;

    blob = NULL;
    if (!data || len < header || memcmp(data, "CGNN", 4) != 0) {
        return false;
    }
    V = rd16(data + 4);
    H = rd16(data + 6);
    N = rd16(data + 8);
    if (!V || !H || !N || H > MAX_HIDDEN || N > MAX_CLASSES) {
        return false;
    }
    need = header + V * NN_WORD_LEN + V * H + H + H * N + N;
    if (len < need) {
        return false;
    }
    s1 = rdf(data + 10);
    s2 = rdf(data + 14);
    temp = rdf(data + 18);
    vocab = data + header;
    w1 = vocab + V * NN_WORD_LEN;
    b1 = w1 + V * H;
    w2 = b1 + H;
    b2 = w2 + H * N;
    blob = data;
    return true;
}

bool nn_ready(void)
{
    return blob != NULL;
}

int nn_classes(void)
{
    return (int)N;
}

/* binary search of the sorted word list */
static int find_word(const char *w)
{
    int lo = 0, hi = (int)V - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strncmp(w, (const char *)vocab + mid * NN_WORD_LEN, NN_WORD_LEN);
        if (c == 0) {
            return mid;
        }
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -1;
}

bool nn_predict(char words[][NN_WORD_LEN], int nwords, struct nn_result *out)
{
    int16_t hid[MAX_HIDDEN];
    static float logit[MAX_CLASSES];
    uint8_t seen[(1024 + 7) / 8];
    int i, j, known = 0;
    float best, sum;
    int top[3] = {-1, -1, -1};

    if (!blob) {
        return false;
    }
    for (j = 0; j < (int)H; j++) {
        hid[j] = (int8_t)b1[j];
    }
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < nwords; i++) {
        int id = find_word(words[i]);
        if (id < 0 || id >= 1024 || (seen[id >> 3] & (1 << (id & 7)))) {
            continue;
        }
        seen[id >> 3] |= (uint8_t)(1 << (id & 7));
        known++;
        for (j = 0; j < (int)H; j++) {
            hid[j] = (int16_t)(hid[j] + (int8_t)w1[id * H + j]);
        }
    }
    if (!known) {
        return false;
    }
    for (j = 0; j < (int)H; j++) {
        if (hid[j] < 0) hid[j] = 0;
    }
    best = -1e30f;
    for (i = 0; i < (int)N; i++) {
        long acc = 0;
        for (j = 0; j < (int)H; j++) {
            acc += (long)hid[j] * (int8_t)w2[j * N + i];
        }
        logit[i] = ((float)acc * s1 + 0.0f) * s2 + (float)(int8_t)b2[i] * s2;
        if (logit[i] > best) {
            best = logit[i];
        }
    }
    sum = 0.0f;
    for (i = 0; i < (int)N; i++) {
        logit[i] = expf((logit[i] - best) / temp);
        sum += logit[i];
        /* keep the top three */
        for (j = 0; j < 3; j++) {
            if (top[j] < 0 || logit[i] > logit[top[j]]) {
                int k;
                for (k = 2; k > j; k--) top[k] = top[k - 1];
                top[j] = i;
                break;
            }
        }
    }
    for (j = 0; j < 3; j++) {
        out->top[j] = top[j];
        out->conf[j] = top[j] >= 0 ? (int)(100.0f * logit[top[j]] / sum + 0.5f) : 0;
    }
    return true;
}
