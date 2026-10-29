#ifndef KEYS_H
#define KEYS_H

/* The calculator's keys as one number each: group * 8 + bit, the same numbering as typing_tables.h. The groups and bits are
 * CEdev's (keypadc.h: kb_Data[group] & (1 << bit)); calcgpt/tests/keys_test.c checks every number here against that header.
 * The number 14 is [mode] (group 1, bit 6): the key that seemed dead on 2026-10-29 until a stale .8xp was replaced. */

#define K(g, b) ((g) * 8 + (b))
#define K_GROUP(k) ((k) / 8)
#define K_MASK(k) (1 << ((k) % 8))

#define K_GRAPH K(1, 0)
#define K_TRACE K(1, 1)
#define K_ZOOM K(1, 2)
#define K_WINDOW K(1, 3)
#define K_YEQU K(1, 4)
#define K_2ND K(1, 5)
#define K_MODE K(1, 6)
#define K_DEL K(1, 7)
#define K_ALPHA K(2, 7)
#define K_ENTER K(6, 0)
#define K_CLEAR K(6, 6)
#define K_DOWN K(7, 0)
#define K_LEFT K(7, 1)
#define K_RIGHT K(7, 2)
#define K_UP K(7, 3)

#endif
