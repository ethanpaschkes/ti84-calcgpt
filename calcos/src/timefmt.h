#ifndef TIMEFMT_H
#define TIMEFMT_H

#include <stdint.h>

/* Clock, date and stopwatch text. Pure functions, tested on a computer. */

/* "3:07 PM" (use24 = 0) or "15:07" (use24 = 1); out needs 9 bytes */
void fmt_clock(int hours, int minutes, int use24, char *out);

/* 0 = Sunday ... 6 = Saturday */
int weekday(int day, int month, int year);

/* "Sat Oct 24 2026"; out needs 16 bytes */
void fmt_date(int day, int month, int year, char *out);

/* ticks of a 32768 Hz counter as "MM:SS.t" (minutes, seconds, tenths); out needs 9 bytes. Minutes wrap at 100. */
void fmt_stopwatch(uint32_t ticks, char *out);

/* a number of seconds as "MM:SS"; out needs 7 bytes */
void fmt_countdown(int seconds, char *out);

#endif
