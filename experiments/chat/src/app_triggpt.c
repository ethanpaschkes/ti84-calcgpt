/* TrigGPT: general chat plus trig identities, proofs and triangle solving. */
#include "kb.h"

const struct kb_table app_tables[] = {
    {kb_general, 0},
    {kb_trig, kb_trig_follow},
};
const int app_table_count = 2;

const char *const app_chips[] = {
    "trig identities", "proof 1", "law of sines", "ambiguous case", "sa 6 sb 8 sc 11", "r u ai?",
};
const int app_chip_count = 6;

const int app_solver_gas = 0;
const int app_solver_trig = 1;

const char *const app_default_next = "trig identities|list proofs|law of sines";
