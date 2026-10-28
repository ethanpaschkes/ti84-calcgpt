#include <stdint.h>
#define TIMER_32K 0
#define TIMER_NOINT 0
#define TIMER_UP 0
void timer_Enable(int t, int a, int b, int c);
void timer_Set(int t, uint32_t v);
uint32_t timer_Get(int t);
