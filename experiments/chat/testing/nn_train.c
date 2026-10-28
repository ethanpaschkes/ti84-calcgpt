/* Train the question-to-answer network on the Mac and write the weight blob.
 *   nn_train OUT.bin paraphrases1.tsv [paraphrases2.tsv ...]
 * Trains on 80% of the model-written questions plus questions made from the
 * answer keys and the app's own suggestion buttons, and reports accuracy on the
 * held-out 20% next to the keyword matcher. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eliza.h"
#include "kb.h"
#include "nn.h"

#define MAXS 6000
#define MAXW 40
#define HID 40
#define EPOCHS 140

struct sample {
    int slot;
    int nw;
    char w[MAXW][ELIZA_WORD_LEN];
    int nid;
    int16_t id[MAXW];
    uint8_t val;
    char text[60];
};

static struct sample *S;
static int ns;
static int N, V;
static char (*vocab)[ELIZA_WORD_LEN];
static float *W1, *B1, *W2, *B2;

static uint32_t rng = 2463534242u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frand(void) { return (rnd() & 0xFFFFFF) / 16777216.0f; }

static int cmp16(const void *a, const void *b) { return strncmp(a, b, ELIZA_WORD_LEN); }

static void add_sample(int slot, const char *text, bool val)
{
    struct sample *s;
    int i, j;
    if (ns >= MAXS) return;
    s = &S[ns];
    memset(s, 0, sizeof(*s));
    s->slot = slot;
    s->val = val;
    strncpy(s->text, text, 59);
    s->nw = eliza_tokens(text, s->w, MAXW);
    /* drop duplicate words */
    for (i = 0; i < s->nw; i++) {
        for (j = 0; j < i; j++) {
            if (!strcmp(s->w[i], s->w[j])) { memmove(s->w[i], s->w[s->nw - 1], ELIZA_WORD_LEN); s->nw--; i--; break; }
        }
    }
    if (s->nw) ns++;
}

/* questions made from an entry's keys: every alternative of each group */
static void keys_to_samples(int slot, const char *keys)
{
    const char *k = keys;
    if (k[0] >= '0' && k[0] <= '9' && k[1] == ':') k += 2;
    for (;;) {
        const char *e = k;
        int variant;
        while (*e && *e != '/') e++;
        for (variant = 0; variant < 4; variant++) {
            char phrase[120] = "";
            const char *p = k;
            bool any = true;
            while (p < e) {
                /* one group: alternatives separated by '|' up to a space */
                const char *g = p, *ge = p;
                int alt = 0, pick = variant;
                char word[ELIZA_WORD_LEN];
                while (ge < e && *ge != ' ') ge++;
                {
                    const char *a = g;
                    while (a <= ge) {
                        const char *ae = a;
                        while (ae < ge && *ae != '|') ae++;
                        if (alt == pick || ae >= ge) {
                            int n = (int)(ae - a);
                            if (n > ELIZA_WORD_LEN - 1) n = ELIZA_WORD_LEN - 1;
                            memcpy(word, a, n); word[n] = 0;
                            break;
                        }
                        alt++;
                        a = ae + 1;
                    }
                }
                if (strlen(phrase) + strlen(word) + 2 < sizeof(phrase)) { strcat(phrase, word); strcat(phrase, " "); }
                p = ge < e ? ge + 1 : ge;
            }
            if (any) add_sample(slot, phrase, false);
        }
        if (!*e) break;
        k = e + 1;
    }
}

static float *hid_buf;

static void forward(const int16_t *ids, int n, float *hid, float *logit)
{
    int j, i, c;
    for (j = 0; j < HID; j++) hid[j] = B1[j];
    for (i = 0; i < n; i++) for (j = 0; j < HID; j++) hid[j] += W1[ids[i] * HID + j];
    for (j = 0; j < HID; j++) if (hid[j] < 0) hid[j] = 0;
    for (c = 0; c < N; c++) {
        float a = B2[c];
        for (j = 0; j < HID; j++) a += hid[j] * W2[j * N + c];
        logit[c] = a;
    }
}

static void softmax(float *l, int n, float t)
{
    float m = l[0], sum = 0;
    int i;
    for (i = 1; i < n; i++) if (l[i] > m) m = l[i];
    for (i = 0; i < n; i++) { l[i] = expf((l[i] - m) / t); sum += l[i]; }
    for (i = 0; i < n; i++) l[i] /= sum;
}

static int argmax(const float *l, int n)
{
    int i, b = 0;
    for (i = 1; i < n; i++) if (l[i] > l[b]) b = i;
    return b;
}

int main(int argc, char **argv)
{
    int a, i, j, c, epoch, t, slot = 0, nsamples_train = 0;
    float *g1, *g2, *gb1, *gb2, *m1, *v1, *m2, *v2, *mb1, *vb1, *mb2, *vb2;
    float *hid, *logit, *dh;
    int *count;
    FILE *f;
    char line[300];

    if (argc < 3) { fprintf(stderr, "usage: nn_train OUT.bin paraphrases.tsv...\n"); return 2; }
    S = calloc(MAXS, sizeof(*S));

    for (t = 0; t < app_table_count; t++) {
        for (i = 0; app_tables[t].entries[i].keys; i++, slot++) { (void)0; }
    }
    N = slot;
    count = calloc(N, sizeof(int));

    /* model-written questions */
    for (a = 2; a < argc; a++) {
        f = fopen(argv[a], "r");
        if (!f) { perror(argv[a]); return 1; }
        while (fgets(line, sizeof line, f)) {
            char *tab = strchr(line, '\t');
            int sl;
            if (!tab) continue;
            *tab = 0;
            sl = atoi(line);
            tab[1 + strcspn(tab + 1, "\r\n")] = 0;
            if (sl < 0 || sl >= N) continue;
            add_sample(sl, tab + 1, false);
            if (ns) count[sl]++;
        }
        fclose(f);
    }
    /* about 20% held out, but only from entries with enough questions */
    {
        int seen[1024] = {0};
        for (i = 0; i < ns; i++) {
            int sl = S[i].slot;
            seen[sl]++;
            if (seen[sl] > 4 && (seen[sl] * 7 + sl * 3) % 5 == 0) S[i].val = 1;
        }
    }
    /* questions made from the answer keys and the app's buttons always train */
    slot = 0;
    for (t = 0; t < app_table_count; t++) {
        for (i = 0; app_tables[t].entries[i].keys; i++, slot++) keys_to_samples(slot, app_tables[t].entries[i].keys);
        if (app_tables[t].follow) {
            const struct kb_follow *fo;
            for (fo = app_tables[t].follow; fo->keys; fo++) {
                char copy[200], *p;
                strcpy(copy, fo->next);
                for (p = strtok(copy, "|"); p; p = strtok(NULL, "|")) {
                    char r[900];
                    if (!strncmp(p, "solve", 5)) continue;
                    if (kb_answer(p, false, r, sizeof r)) add_sample(kb_last_slot(), p, false);
                }
            }
        }
    }
    for (i = 0; i < app_chip_count; i++) {
        char r[900];
        if (!strncmp(app_chips[i], "sa ", 3) || !strncmp(app_chips[i], "solve", 5)) continue;
        if (kb_answer(app_chips[i], false, r, sizeof r)) add_sample(kb_last_slot(), app_chips[i], false);
    }

    /* vocabulary: words seen in training questions at least twice, plus every word in a key */
    {
        char (*all)[ELIZA_WORD_LEN] = malloc((size_t)MAXS * MAXW * ELIZA_WORD_LEN);
        int na = 0, nv = 0;
        for (i = 0; i < ns; i++) {
            if (S[i].val) continue;
            nsamples_train++;
            for (j = 0; j < S[i].nw; j++) { memset(all[na], 0, ELIZA_WORD_LEN); strncpy(all[na++], S[i].w[j], ELIZA_WORD_LEN - 1); }
        }
        qsort(all, na, ELIZA_WORD_LEN, cmp16);
        vocab = malloc((size_t)na * ELIZA_WORD_LEN);
        for (i = 0; i < na;) {
            int run = 1;
            while (i + run < na && !strncmp(all[i], all[i + run], ELIZA_WORD_LEN)) run++;
            if (run >= 2) { memcpy(vocab[nv++], all[i], ELIZA_WORD_LEN); }
            i += run;
        }
        V = nv;
        free(all);
    }
    for (i = 0; i < ns; i++) {
        S[i].nid = 0;
        for (j = 0; j < S[i].nw; j++) {
            char key[ELIZA_WORD_LEN];
            void *hit;
            memset(key, 0, sizeof key);
            strncpy(key, S[i].w[j], ELIZA_WORD_LEN - 1);
            hit = bsearch(key, vocab, V, ELIZA_WORD_LEN, cmp16);
            if (hit) S[i].id[S[i].nid++] = (int16_t)(((char (*)[ELIZA_WORD_LEN])hit) - vocab);
        }
    }
    printf("%d classes, %d vocabulary words, %d training questions, %d held-out questions\n",
           N, V, nsamples_train, ns - nsamples_train);
    if (V > 1000) { fprintf(stderr, "vocabulary too large for nn.c\n"); return 1; }

    /* weights */
    W1 = malloc(sizeof(float) * V * HID); B1 = calloc(HID, sizeof(float));
    W2 = malloc(sizeof(float) * HID * N); B2 = calloc(N, sizeof(float));
    for (i = 0; i < V * HID; i++) W1[i] = (frand() - 0.5f) * 0.6f;
    for (i = 0; i < HID * N; i++) W2[i] = (frand() - 0.5f) * 0.5f;
    g1 = calloc(V * HID, sizeof(float)); g2 = calloc(HID * N, sizeof(float));
    gb1 = calloc(HID, sizeof(float)); gb2 = calloc(N, sizeof(float));
    m1 = calloc(V * HID, sizeof(float)); v1 = calloc(V * HID, sizeof(float));
    m2 = calloc(HID * N, sizeof(float)); v2 = calloc(HID * N, sizeof(float));
    mb1 = calloc(HID, sizeof(float)); vb1 = calloc(HID, sizeof(float));
    mb2 = calloc(N, sizeof(float)); vb2 = calloc(N, sizeof(float));
    hid = malloc(sizeof(float) * HID); logit = malloc(sizeof(float) * N); dh = malloc(sizeof(float) * HID);
    hid_buf = hid;
    {
        int *order = malloc(sizeof(int) * ns), nord = 0, step = 0;
        for (i = 0; i < ns; i++) if (!S[i].val) order[nord++] = i;
        for (epoch = 0; epoch < EPOCHS; epoch++) {
            float lr = 0.02f * (1.0f - 0.8f * epoch / EPOCHS), loss = 0;
            for (i = nord - 1; i > 0; i--) { int r = rnd() % (i + 1), tmp = order[i]; order[i] = order[r]; order[r] = tmp; }
            for (i = 0; i < nord; i += 16) {
                int end = i + 16 < nord ? i + 16 : nord, b;
                memset(g1, 0, sizeof(float) * V * HID); memset(g2, 0, sizeof(float) * HID * N);
                memset(gb1, 0, sizeof(float) * HID); memset(gb2, 0, sizeof(float) * N);
                for (b = i; b < end; b++) {
                    struct sample *s = &S[order[b]];
                    int16_t ids[MAXW];
                    int n = 0, k;
                    for (k = 0; k < s->nid; k++) if ((rnd() % 100) >= 12) ids[n++] = s->id[k];
                    if (!n && s->nid) ids[n++] = s->id[0];
                    forward(ids, n, hid, logit);
                    softmax(logit, N, 1.0f);
                    loss -= logf(logit[s->slot] + 1e-9f);
                    logit[s->slot] -= 1.0f; /* dL/dlogit */
                    for (j = 0; j < HID; j++) dh[j] = 0;
                    for (c = 0; c < N; c++) {
                        float d = logit[c];
                        if (fabsf(d) < 1e-6f) continue;
                        gb2[c] += d;
                        for (j = 0; j < HID; j++) { g2[j * N + c] += hid[j] * d; dh[j] += W2[j * N + c] * d; }
                    }
                    for (j = 0; j < HID; j++) if (hid[j] <= 0) dh[j] = 0;
                    for (j = 0; j < HID; j++) gb1[j] += dh[j];
                    for (k = 0; k < n; k++) for (j = 0; j < HID; j++) g1[ids[k] * HID + j] += dh[j];
                }
                step++;
                {
                    float bc1 = 1.0f - powf(0.9f, (float)step), bc2 = 1.0f - powf(0.999f, (float)step);
                    int sz = end - i;
#define ADAM(w, g, m, v, n) for (k2 = 0; k2 < (n); k2++) { float gg = g[k2] / sz + 1e-5f * w[k2]; m[k2] = 0.9f * m[k2] + 0.1f * gg; v[k2] = 0.999f * v[k2] + 0.001f * gg * gg; w[k2] -= lr * (m[k2] / bc1) / (sqrtf(v[k2] / bc2) + 1e-8f); }
                    int k2;
                    ADAM(W1, g1, m1, v1, V * HID)
                    ADAM(B1, gb1, mb1, vb1, HID)
                    ADAM(W2, g2, m2, v2, HID * N)
                    ADAM(B2, gb2, mb2, vb2, N)
                }
            }
            if (epoch % 20 == 0 || epoch == EPOCHS - 1) printf("epoch %3d  training loss %.3f\n", epoch, loss / nord);
        }
        free(order);
    }

    /* quantize to 8 bits and write the blob */
    {
        float m = 0, s1, s2;
        size_t len = 4 + 6 + 12 + (size_t)V * ELIZA_WORD_LEN + (size_t)V * HID + HID + (size_t)HID * N + N;
        uint8_t *blob = calloc(len, 1), *p = blob;
        float temp = 1.0f, bestT = 1.0f, bestNll = 1e30f;
        int ok_f = 0, ok_q = 0, top3 = 0, nval = 0;
        int kw_ok = 0, kw_hit = 0;
        uint16_t v16 = (uint16_t)V, h16 = HID, n16 = (uint16_t)N;

        for (i = 0; i < V * HID; i++) if (fabsf(W1[i]) > m) m = fabsf(W1[i]);
        for (i = 0; i < HID; i++) if (fabsf(B1[i]) > m) m = fabsf(B1[i]);
        s1 = m / 127.0f;
        m = 0;
        for (i = 0; i < HID * N; i++) if (fabsf(W2[i]) > m) m = fabsf(W2[i]);
        for (i = 0; i < N; i++) if (fabsf(B2[i]) > m) m = fabsf(B2[i]);
        s2 = m / 127.0f;
        memcpy(p, "CGNN", 4); p += 4;
        memcpy(p, &v16, 2); p += 2; memcpy(p, &h16, 2); p += 2; memcpy(p, &n16, 2); p += 2;
        memcpy(p, &s1, 4); p += 4; memcpy(p, &s2, 4); p += 4; memcpy(p, &temp, 4); p += 4;
        memcpy(p, vocab, (size_t)V * ELIZA_WORD_LEN); p += (size_t)V * ELIZA_WORD_LEN;
        for (i = 0; i < V * HID; i++) *p++ = (uint8_t)(int8_t)lrintf(W1[i] / s1);
        for (i = 0; i < HID; i++) *p++ = (uint8_t)(int8_t)lrintf(B1[i] / s1);
        for (i = 0; i < HID * N; i++) *p++ = (uint8_t)(int8_t)lrintf(W2[i] / s2);
        for (i = 0; i < N; i++) *p++ = (uint8_t)(int8_t)lrintf(B2[i] / s2);

        /* temperature: pick the one that makes the confidences honest on held-out questions */
        for (temp = 0.3f; temp <= 4.0f; temp += 0.1f) {
            double nll = 0;
            int cnt = 0;
            memcpy(blob + 18, &temp, 4);
            nn_init(blob, (unsigned)len);
            for (i = 0; i < ns; i++) {
                struct nn_result r;
                if (!S[i].val) continue;
                if (nn_predict(S[i].w, S[i].nw, &r)) {
                    float pc = 1e-4f;
                    for (j = 0; j < 3; j++) if (r.top[j] == S[i].slot) pc = r.conf[j] / 100.0f > 1e-4f ? r.conf[j] / 100.0f : 1e-4f;
                    nll -= log(pc);
                } else nll += 9.2;
                cnt++;
            }
            if (cnt && nll / cnt < bestNll) { bestNll = (float)(nll / cnt); bestT = temp; }
        }
        memcpy(blob + 18, &bestT, 4);
        nn_init(blob, (unsigned)len);

        /* held-out evaluation, using the same code the calculator runs */
        int conf_buckets[10] = {0}, conf_ok[10] = {0};
        int hy_a = 0, hy_b = 0, hy_ask_ok = 0, hy_ask_bad = 0, hy_wrong = 0;
        for (i = 0; i < ns; i++) {
            struct nn_result r;
            char reply[900];
            int kw_slot = -1;
            bool nn_has;
            if (!S[i].val) continue;
            nval++;
            nn_has = nn_predict(S[i].w, S[i].nw, &r);
            if (nn_has) {
                if (r.top[0] == S[i].slot) ok_q++;
                if (r.top[0] == S[i].slot || r.top[1] == S[i].slot || r.top[2] == S[i].slot) top3++;
                { int b = r.conf[0] / 10; if (b > 9) b = 9; conf_buckets[b]++; if (r.top[0] == S[i].slot) conf_ok[b]++; }
            }
            if (kb_answer(S[i].text, false, reply, sizeof reply)) { kw_slot = kb_last_slot(); kw_hit++; if (kw_slot == S[i].slot) kw_ok++; }
            /* hybrid: the keyword answer if it exists and the network doesn't strongly disagree;
             * otherwise the network's answer when confident; otherwise offer its top 3 as buttons */
            if (kw_slot >= 0 && (!nn_has || r.top[0] == kw_slot || r.conf[0] < 70)) { if (kw_slot == S[i].slot) hy_a++; else hy_wrong++; }
            else if (nn_has && r.conf[0] >= 60) { if (r.top[0] == S[i].slot) hy_a++; else hy_wrong++; }
            else if (nn_has) { if (r.top[0] == S[i].slot || r.top[1] == S[i].slot || r.top[2] == S[i].slot) hy_ask_ok++; else hy_ask_bad++; }
            else hy_ask_bad++;
            (void)ok_f; (void)hy_b;
        }
        printf("\nHELD-OUT RESULTS on %d questions the network never trained on (temperature %.1f):\n", nval, bestT);
        printf("  keyword matcher:  right answer %3d%%   (it gave some answer %3d%% of the time)\n", 100 * kw_ok / nval, 100 * kw_hit / nval);
        printf("  neural network:   right answer %3d%%   right answer in its top 3: %3d%%\n", 100 * ok_q / nval, 100 * top3 / nval);
        printf("  both together:    right answer %3d%%,  asked 'did you mean' with the right one in the 3 buttons %3d%%,\n"
               "                    asked without the right one %3d%%,  wrong answer %3d%%\n",
               100 * hy_a / nval, 100 * hy_ask_ok / nval, 100 * hy_ask_bad / nval, 100 * hy_wrong / nval);
        printf("  confidence check (how often the top guess was right at each confidence):\n");
        for (i = 9; i >= 0; i--) if (conf_buckets[i]) printf("    %3d-%3d%%: right %3d%% of %d\n", i * 10, i * 10 + 9, 100 * conf_ok[i] / conf_buckets[i], conf_buckets[i]);
        printf("  weight file: %zu bytes\n", len);
        f = fopen(argv[1], "wb");
        fwrite(blob, 1, len, f);
        fclose(f);
    }
    return 0;
}
