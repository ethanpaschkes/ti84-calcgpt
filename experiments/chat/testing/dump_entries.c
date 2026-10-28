/* Print every answer of an app as: slot <TAB> table <TAB> keys <TAB> summary.
 * The slot numbers match kb_last_slot(). */
#include <stdio.h>
#include <string.h>
#include "kb.h"

int main(void)
{
    int t, i, slot = 0;
    for (t = 0; t < app_table_count; t++) {
        for (i = 0; app_tables[t].entries[i].keys; i++, slot++) {
            const struct entry *e = &app_tables[t].entries[i];
            char sum[260];
            int n = 0;
            const char *p;
            for (p = e->answer; *p && *p != '~' && n < 240; p++) {
                sum[n++] = *p == '\n' ? ' ' : *p;
            }
            sum[n] = 0;
            printf("%d\t%d\t%s\t%s\n", slot, t, e->keys, sum);
        }
    }
    return 0;
}
