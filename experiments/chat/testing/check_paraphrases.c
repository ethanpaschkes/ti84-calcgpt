/* Feed "slot<TAB>question" lines through the app's answer matching and report
 * the ones that don't reach the intended answer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "kb.h"
#include "tri.h"

int main(int argc, char **argv)
{
    char line[300], reply[900];
    int total = 0, ok = 0, other = 0, miss = 0;
    FILE *f;

    if (argc < 2 || !(f = fopen(argv[1], "r"))) {
        fprintf(stderr, "usage: %s paraphrases.tsv\n", argv[0]);
        return 2;
    }
    while (fgets(line, sizeof line, f)) {
        char *tab = strchr(line, '\t');
        int slot, got;
        char *q;
        if (!tab) continue;
        *tab = 0;
        q = tab + 1;
        q[strcspn(q, "\r\n")] = 0;
        slot = atoi(line);
        total++;
        if (tri_try(q, reply, sizeof reply) || calc_try(q, reply, sizeof reply)) {
            printf("MATH   [%d] %s\n", slot, q);
            other++;
            continue;
        }
        if (!kb_answer(q, false, reply, sizeof reply)) {
            printf("MISS   [%d] %s\n", slot, q);
            miss++;
            continue;
        }
        got = kb_last_slot();
        if (got == slot) {
            ok++;
        } else {
            printf("OTHER  [%d] %-34s -> [%d] %.60s\n", slot, q, got, reply);
            other++;
        }
    }
    printf("\n%d questions: %d reached the intended answer, %d went to a different answer, %d had no answer\n",
           total, ok, other, miss);
    return 0;
}
