#ifndef PROOFS_H
#define PROOFS_H

/* The eight identities from the "Additional Proof Practice" sheet, with their
 * worked proofs. Matching is by value (the same functions), so any way of
 * writing the same identity is recognised. Returns the proof number 1-8 and its
 * steps, or 0. */
int mx_find_proof(const char *lhs, const char *rhs, const char **steps);

#endif
