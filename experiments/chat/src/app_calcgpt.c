/* CalcGPT: general chat plus the 10th grade chemistry gas unit. */
#include "kb.h"

const struct kb_table app_tables[] = {
    {kb_general, 0},
    {kb_gas, kb_gas_follow},
};
const int app_table_count = 2;

const char *const app_chips[] = {
    "gas laws", "boyle's law", "ideal gas law", "gas law example", "760 mmhg to atm", "r u ai?",
};
const int app_chip_count = 6;

const int app_solver_gas = 1;
const int app_solver_trig = 0;

const char *const app_default_next = "gas laws|gas law example|how do i solve a gas problem";
