#include <string.h>

#include "hook.h"
#include "qsc/qsc.h"

FN_EXTERN(Reader_ReadLine);

#define gReaderLineSize VAR(int, 0xA969C4)

Reader *Reader_Init(Reader *r, const char *src, int size, int (*refill)(Reader *r))
{
    r->buf = Mem_Alloc(gReaderLineSize, 4);
    memset(r->buf, 0, gReaderLineSize);
    r->src = src;
    r->size = size;
    r->cur = r->buf;
    r->line = 0;
    r->textline = 0;
    r->pos = 0;
    r->refill = refill;
    return r;
}
HOOK(0x4BEF60, Reader_Init);

void Reader_Free(Reader *r)
{
    Mem_Free(r->buf);
    r->buf = NULL;
}
HOOK(0x4BEFC0, Reader_Free);

static int IsLineEnd(char c)
{
    return c == '\r' || c == '\n' || c == '\0';
}

int Reader_ReadLine(Reader *r)
{
    char *s;
    char c;
    int n, i;

    if (!IsLineEnd(*r->cur))
        return 1;
    if (r->pos >= r->size)
        return 0;

    for (n = 0; n < gReaderLineSize - 2 && r->pos + n < r->size; n++) {
        c = r->src[r->pos + n];
        r->buf[n] = c;
        r->textline = r->line;
        if (IsLineEnd(c)) {
            r->buf[n] = '\0';
            while (IsLineEnd(c)) {
                if (c == '\n')
                    r->line++;
                n++;
                if (r->pos + n >= r->size)
                    break;
                c = r->src[r->pos + n];
            }
            r->pos += n;
            goto strip;
        }
    }
    if (n >= gReaderLineSize - 2) {
        /* line too long: cut it at the last blank */
        while (n != 0) {
            c = r->buf[--n];
            if (c == ' ' || c == '\t') {
                r->buf[n] = '\0';
                break;
            }
        }
        if (n == 0)
            Script_Error("Token too big in line %d '%s'.", r->textline, r->buf);
    }
    r->pos += n;
    r->buf[n] = '\0';

strip:
    r->cur = s = r->buf;
    if (*s == '\0')
        return 1;
    i = 0;
    do {
        c = s[i];
        if (c == '"') {
            /* starts on the opening quote and stops there, so "//" is cut even inside strings */
            for (;;) {
                if (c == '\\') {
                    c = s[++i];
                    if (c == '\0') {
                        i--;
                        break;
                    }
                } else if (c == '"') {
                    break;
                }
                c = s[++i];
                if (c == '\0')
                    break;
            }
        }
        if (s[i] == '/' && s[i + 1] == '/') {
            s[i] = '\0';
            return 1;
        }
    } while (s[++i]);
    return 1;
}
HOOK(0x4BEFE0, Reader_ReadLine);

Reader *Reader_InitMem(Reader *r, const char *src, int size)
{
    return Reader_Init(r, src, size, (int (*)(Reader *))FN(0x4BEFE0, Reader_ReadLine));
}
HOOK(0x4BF1C0, Reader_InitMem);

int Reader_Refill(Reader *r)
{
    return r->refill(r);
}
HOOK(0x4BF1E0, Reader_Refill);
