#include "freecam/freecam.h"

/* The C runtime qsort, with its insertion sort for short runs. */

#define QSORT_CUTOFF 8

void crt_shortsort(char *lo, char *hi, unsigned width, int (*comp)(const void *, const void *));
void crt_swap(char *a, char *b, unsigned width);

void crt_qsort(void *base, unsigned num, unsigned width, int (*comp)(const void *, const void *))
{
    char *lo, *hi, *mid, *loguy, *higuy;
    char *lostk[30], *histk[30];
    unsigned size;
    int stkptr;

    if (num < 2 || width == 0)
        return;
    stkptr = 0;
    lo = base;
    hi = (char *)base + width * (num - 1);
recurse:
    size = (hi - lo) / width + 1;
    if (size <= QSORT_CUTOFF) {
        crt_shortsort(lo, hi, width, comp);
    } else {
        mid = lo + (size / 2) * width;
        crt_swap(mid, lo, width);
        loguy = lo;
        higuy = hi + width;
        for (;;) {
            do
                loguy += width;
            while (loguy <= hi && comp(loguy, lo) <= 0);
            do
                higuy -= width;
            while (higuy > lo && comp(higuy, lo) >= 0);
            if (higuy < loguy)
                break;
            crt_swap(loguy, higuy, width);
        }
        crt_swap(lo, higuy, width);
        if (higuy - 1 - lo >= hi - loguy) {
            if (lo + width < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy - width;
                ++stkptr;
            }
            if (loguy < hi) {
                lo = loguy;
                goto recurse;
            }
        } else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                ++stkptr;
            }
            if (lo + width < higuy) {
                hi = higuy - width;
                goto recurse;
            }
        }
    }
    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;
    }
}
HOOK(0x5321F0, crt_qsort);

void crt_shortsort(char *lo, char *hi, unsigned width, int (*comp)(const void *, const void *))
{
    char *p, *max;

    while (hi > lo) {
        max = lo;
        for (p = lo + width; p <= hi; p += width)
            if (comp(p, max) > 0)
                max = p;
        crt_swap(max, hi, width);
        hi -= width;
    }
}
HOOK(0x532344, crt_shortsort);

void crt_swap(char *a, char *b, unsigned width)
{
    char tmp;

    if (a != b)
        while (width--) {
            tmp = *a;
            *a++ = *b;
            *b++ = tmp;
        }
}
HOOK(0x532392, crt_swap);
