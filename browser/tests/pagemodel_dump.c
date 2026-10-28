/* Feeds a recorded stream of Mac messages through the page model in pieces of random size, then prints what it holds.
 * Usage: pagemodel_dump stream.bin [chunk]   (chunk 1 = byte by byte; default 64) */
#include <stdio.h>
#include <stdlib.h>
#include "../src/pagemodel.h"

static struct pm pm;

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    int chunk = argc > 2 ? atoi(argv[2]) : 64, i, n;
    static uint8_t data[1 << 20];
    if (!f) return 2;
    n = (int)fread(data, 1, sizeof(data), f);
    srand(99);
    pm_init(&pm);
    for (i = 0; i < n;) {
        int k = 1 + rand() % chunk;
        if (k > n - i) k = n - i;
        pm_feed(&pm, data + i, k);
        i += k;
    }
    printf("SCREEN %d\nTITLE %s\nLOADING %d\nSTATUS %s\nBOOKMARKS %d", pm.screen, pm.title, pm.loading, pm.status, pm.nbookmarks);
    for (i = 0; i < pm.nbookmarks; i++) printf("|%s", pm.bookmarks[i]);
    printf("\nROWS %d\n", pm.nrows);
    for (i = 0; i < pm.nrows; i++) {
        int s;
        printf("ROW %d %s", pm.rows[i].style, pm.rows[i].text);
        for (s = 0; s < pm.rows[i].nspans; s++) printf(" [%d %d %d]", pm.rows[i].spans[s].id, pm.rows[i].spans[s].start, pm.rows[i].spans[s].end);
        printf("\n");
    }
    printf("SELECTED %d\n", pm_selected_id(&pm));
    return 0;
}
