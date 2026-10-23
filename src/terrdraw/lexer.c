/* Token reading and number parsing for the script and config readers. */
#include "hook.h"
#include "igi.h"

ORIG(0x4A5490, int, crt_atoi, (const char *s));

double crt_atof(const char *s);

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int hex_value(signed char c)
{
    return c <= '9' ? (c & 0xF) : (c & 0xF) + 9;
}

/* Reads a quoted string (with \b \n \r \t \xNN escapes), a number (decimal,
   0x hex, $ hex, with fraction and exponent) or a name into out. Returns 1
   when something was read. Running out of room returns 0 without
   terminating out or setting end. */
int Lex_ReadToken(const char *s, const char **end, char *out, int size)
{
    int i = 0, n = 0, found = 0, hex = 0, number = 0, exp = 0;
    char c;

    if (!size)
        return 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')
        i++;
    if (s[i] == '"') {
        i++;
        while ((c = s[i]) != '"') {
            if (!c)
                return 0;
            if (c == '\\') {
                c = s[++i];
                switch (c) {
                case 'X':
                case 'x':
                    c = s[++i];
                    c = hex_value(c) << 4;
                    c |= hex_value(s[++i]);
                    break;
                case 'b':
                    c = '\b';
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 't':
                    c = '\t';
                    break;
                }
            }
            out[n++] = c;
            i++;
        }
        i++;
        found = 1;
    } else {
        c = s[i];
        if (is_digit(c)) {
            number = 1;
            if (c == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                i++;
                hex = 1;
                out[0] = '0';
                if (!--size)
                    return 0;
                out[1] = s[i++];
                n = 2;
                if (!--size)
                    return 0;
            }
        } else if (c == '$') {
            n = 1;
            i++;
            number = 1;
            hex = 1;
            out[0] = '$';
            if (!--size)
                return 0;
        }
        for (;;) {
            c = s[i];
            if (hex) {
                if (!is_digit(c) && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F'))
                    break;
            } else if (number) {
                if (c == '.') {
                    out[n++] = '.';
                    i++;
                    if (!--size)
                        return 0;
                    c = s[i];
                }
                if (c == 'e' || c == 'E') {
                    if (exp)
                        break;
                    out[n++] = c;
                    i++;
                    if (!--size)
                        return 0;
                    c = s[i];
                    if (c == '+' || c == '-') {
                        out[n++] = c;
                        i++;
                        if (!--size)
                            return 0;
                        c = s[i];
                    }
                    exp = 1;
                }
                if (!is_digit(c))
                    break;
            } else {
                if (!is_digit(c) && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c != '_' && c != '.')
                    break;
            }
            out[n++] = c;
            i++;
            if (!--size)
                return 0;
            found = 1;
        }
    }
    out[n] = 0;
    if (end)
        *end = s + i;
    return found;
}
HOOK(0x517260, Lex_ReadToken);

/* Decimal, or hex after "$" or "0x". */
int Str_ToInt(const char *s)
{
    int v = 0;

    if (*s != '$') {
        if (*s != '0' || (s[1] != 'x' && s[1] != 'X'))
            return crt_atoi(s);
        s++;
    }
    for (s++; *s; s++)
        v = v * 16 + hex_value(*s);
    return v;
}
HOOK(0x517510, Str_ToInt);

double Str_ToFloat(const char *s)
{
    return crt_atof(s);
}
HOOK(0x517570, Str_ToFloat);

void Str_ToUpper(char *s)
{
    for (; *s; s++) {
        if (*s >= 'a' && *s <= 'z')
            *s -= 'a' - 'A';
    }
}
HOOK(0x517580, Str_ToUpper);
