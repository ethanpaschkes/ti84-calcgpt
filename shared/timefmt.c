#include "timefmt.h"

static void two(char *out, int v)
{
    out[0] = (char)('0' + (v / 10) % 10);
    out[1] = (char)('0' + v % 10);
}

void fmt_clock(int hours, int minutes, int use24, char *out)
{
    int h = hours, i = 0;
    if (!use24) {
        h = hours % 12;
        if (h == 0) h = 12;
    }
    if (use24 || h >= 10) out[i++] = (char)('0' + h / 10);
    out[i++] = (char)('0' + h % 10);
    out[i++] = ':';
    two(out + i, minutes);
    i += 2;
    if (!use24) {
        out[i++] = ' ';
        out[i++] = hours >= 12 ? 'P' : 'A';
        out[i++] = 'M';
    }
    out[i] = 0;
}

int weekday(int day, int month, int year)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year -= 1;
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

void fmt_date(int day, int month, int year, char *out)
{
    static const char *const days = "SunMonTueWedThuFriSat";
    static const char *const months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int w = weekday(day, month, year), i = 0, k;
    for (k = 0; k < 3; k++) out[i++] = days[w * 3 + k];
    out[i++] = ' ';
    for (k = 0; k < 3; k++) out[i++] = months[(month - 1) * 3 + k];
    out[i++] = ' ';
    if (day >= 10) out[i++] = (char)('0' + day / 10);
    out[i++] = (char)('0' + day % 10);
    out[i++] = ' ';
    out[i++] = (char)('0' + (year / 1000) % 10);
    out[i++] = (char)('0' + (year / 100) % 10);
    out[i++] = (char)('0' + (year / 10) % 10);
    out[i++] = (char)('0' + year % 10);
    out[i] = 0;
}

void fmt_stopwatch(uint32_t ticks, char *out)
{
    uint32_t seconds = ticks / 32768u, rest = ticks % 32768u;
    int tenths = (int)(rest * 10u / 32768u);
    two(out, (int)(seconds / 60 % 100));
    out[2] = ':';
    two(out + 3, (int)(seconds % 60));
    out[5] = '.';
    out[6] = (char)('0' + tenths);
    out[7] = 0;
}

void fmt_countdown(int seconds, char *out)
{
    if (seconds < 0) seconds = 0;
    two(out, seconds / 60 % 100);
    out[2] = ':';
    two(out + 3, seconds % 60);
    out[5] = 0;
}
