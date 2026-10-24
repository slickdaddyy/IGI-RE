#ifndef MATH_X87_H
#define MATH_X87_H

/* Single x87 instructions, so results match the original bit for bit. */

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

/* fchs, so gcc cannot move the negation after a rounding or turn -(a - b) into b - a */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

static inline long double x87_sin(long double x)
{
    __asm__("fsin" : "+t"(x));
    return x;
}

static inline long double x87_cos(long double x)
{
    __asm__("fcos" : "+t"(x));
    return x;
}

static inline long double x87_atan2(long double y, long double x)
{
    long double r;

    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

#endif
