#include <windows.h>

#include "hook.h"
#include "igi.h"

/* Local time stamps as "YYYY-MM-DD-hh-mm-ss-mmmm" strings, compared as text. */

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));

typedef struct DateTime {
    int year, month, day;
    int hour, minute, second;
    int ms;
} DateTime;

void DateTime_Get(DateTime *dt)
{
    SYSTEMTIME st;

    GetLocalTime(&st);
    dt->year = st.wYear;
    dt->month = st.wMonth;
    dt->day = st.wDay;
    dt->hour = st.wHour;
    dt->minute = st.wMinute;
    dt->second = st.wSecond;
    dt->ms = st.wMilliseconds;
}
HOOK(0x52D440, DateTime_Get);

static char *str_end(char *s)
{
    while (*s)
        s++;
    return s;
}

static void str_cat(char *s, const char *add)
{
    s = str_end(s);
    while ((*s++ = *add++))
        ;
}

int DateTime_FormatDate(const DateTime *dt, char *buf, int size);
int DateTime_FormatTime(const DateTime *dt, char *buf, int size);
int DateTime_FormatMs(const DateTime *dt, char *buf, int size);

/* the time is always written with the date; time only adds the separator */
int DateTime_Format(const DateTime *dt, char *buf, int size, int date, int time, int ms)
{
    int sep = time || ms;
    int n = 0;

    *buf = 0;
    if (date) {
        n = DateTime_FormatDate(dt, buf, size);
        if (sep) {
            str_cat(buf, "-");
            n++;
        }
        buf = str_end(buf);
        n += DateTime_FormatTime(dt, buf, size);
        if (ms) {
            str_cat(buf, "-");
            n++;
        }
        buf = str_end(buf);
    }
    if (ms)
        n += DateTime_FormatMs(dt, buf, size);
    return n;
}
HOOK(0x52D4B0, DateTime_Format);

int DateTime_FormatDate(const DateTime *dt, char *buf, int size)
{
    return crt_sprintf(buf, "%04d-%02d-%02d", dt->year, dt->month, dt->day);
}
HOOK(0x52D5B0, DateTime_FormatDate);

int DateTime_FormatTime(const DateTime *dt, char *buf, int size)
{
    return crt_sprintf(buf, "%02d-%02d-%02d", dt->hour, dt->minute, dt->second);
}
HOOK(0x52D5E0, DateTime_FormatTime);

int DateTime_FormatMs(const DateTime *dt, char *buf, int size)
{
    return crt_sprintf(buf, "%04d", dt->ms);
}
HOOK(0x52D610, DateTime_FormatMs);

int DateTime_Compare(const DateTime *a, const DateTime *b)
{
    unsigned char sa[256], sb[256];
    int i;

    DateTime_Format(a, (char *)sa, sizeof(sa), 1, 1, 1);
    DateTime_Format(b, (char *)sb, sizeof(sb), 1, 1, 1);
    for (i = 0; sa[i] == sb[i]; i++)
        if (!sa[i])
            return 0;
    return sa[i] < sb[i] ? -1 : 1;
}
HOOK(0x52D630, DateTime_Compare);
