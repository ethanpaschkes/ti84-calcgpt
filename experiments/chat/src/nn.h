#ifndef NN_H
#define NN_H

#include <stdbool.h>
#include <stdint.h>

#define NN_WORD_LEN 16

/* A tiny neural network that reads a question and says which answer it is
 * asking for. It is a "bag of words" network: each known word adds a learned
 * vector to a hidden layer, a ReLU, then one output per answer and a softmax.
 * The weights are 8-bit numbers stored in a blob (an appvar on the calculator)
 * that is read in place, so it uses almost no RAM.
 *
 * Blob layout (little endian): "CGNN", V, H, N as 16-bit numbers, then float
 * s1, s2, temperature; V words of 16 bytes (sorted); W1 (V x H, int8); b1 (H);
 * W2 (H x N, int8); b2 (N). */

struct nn_result {
    int top[3];     /* best three answers (slot numbers) */
    int conf[3];    /* their confidence, percent */
};

/* Use a blob; false if it isn't valid. */
bool nn_init(const uint8_t *blob, unsigned len);
bool nn_ready(void);
int nn_classes(void);

/* Predict from cleaned-up words (see eliza_tokens). */
bool nn_predict(char words[][NN_WORD_LEN], int nwords, struct nn_result *out);

#endif
