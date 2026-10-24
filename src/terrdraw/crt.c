/* C runtime pieces linked into the exe: strlwr, the x87 acos and asin
   helpers, strncat, atof, splitpath, remove, fread, fwrite, _fltin and
   _mbsnbcpy. */
#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include "hook.h"
#include "igi.h"

typedef struct CrtFile {
    char *ptr;
    int cnt;
    char *base;
    int flag;
    int file;
    int charbuf;
    int bufsiz;
    char *tmpfname;
} CrtFile;

typedef struct CrtFltIn {
    int flags;
    int nbytes;
    int lval;
    int pad;
    double dval;
} CrtFltIn;

ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));
ORIG(0x4A6A50, void, crt_free, (void *p));
ORIG(0x4A6BD2, int, crt_flush, (CrtFile *fp));
ORIG(0x4A6CA4, int, crt_filbuf, (CrtFile *fp));
ORIG(0x4A760B, int, crt_flsbuf, (int c, CrtFile *fp));
ORIG(0x4A7F2A, int, crt_isctype, (int c, int mask));
ORIG(0x4AA580, char *, crt_strcpy, (char *dst, const char *src));
ORIG(0x4AA670, void *, crt_malloc, (size_t size));
ORIG(0x4AA730, size_t, crt_strlen, (const char *s));
ORIG(0x4AA7B0, void *, crt_memcpy, (void *dst, const void *src, size_t n));
ORIG(0x4ABC46, void, crt_dosmaperr, (DWORD err));
ORIG(0x4ABEC7, int, crt_write, (int fh, const void *buf, unsigned int n));
ORIG(0x4AC12D, int, crt_read, (int fh, void *buf, unsigned int n));
ORIG(0x4ACAEC, int, crt_ld12tod, (void *ld12, double *d));
ORIG(0x4AD4D3, int, crt_LCMapStringA, (LCID lcid, DWORD flags, const char *src, int srclen, char *dst, int dstlen,
                                       int codepage, int error));
ORIG(0x4ADBF9, unsigned int, crt_strgtold12, (void *ld12, const char **end, const char *s, int mult12, int scale,
                                               int decpt, int implicit_e));

extern const unsigned short *gPctype;
extern int gMbCurMax;
extern CrtFltIn *gFltIn;
extern LCID gLcHandleCtype;
extern int gIsMbCodePage;
extern const unsigned char gMbCtype[];

#define IOREAD_ANY 0x10C
#define IOWRITE_ANY 0x108
#define IOEOF 0x10
#define IOERR 0x20

char *crt_strlwr(char *s)
{
    char *buf = NULL, *p;
    int n;

    if (!gLcHandleCtype) {
        for (p = s; *p; p++) {
            if (*p >= 'A' && *p <= 'Z')
                *p += 'a' - 'A';
        }
        return s;
    }
    n = crt_LCMapStringA(gLcHandleCtype, LCMAP_LOWERCASE, s, -1, NULL, 0, 0, 1);
    if (n && (buf = crt_malloc(n)) && crt_LCMapStringA(gLcHandleCtype, LCMAP_LOWERCASE, s, -1, buf, n, 0, 1))
        crt_strcpy(s, buf);
    crt_free(buf);
    return s;
}
HOOK(0x516770, crt_strlwr);

/* _CIacos / _CIasin take their argument in st(0) and pass state to the
   shared x87 helpers of the runtime in flags, eax and the caller's frame,
   so they stay in assembly. */
void crt_acos87(void);
void crt_acos87_core(void);
void crt_asin87(void);
void crt_asin87_core(void);

__asm__(".section .rdata,\"dr\"\n"
        "crt87_split:    .long 0x4A9D28\n"
        "crt87_setcw:    .long 0x4A9CB5\n"
        "crt87_nan:      .long 0x4A9CCC\n"
        "crt87_fastexit: .long 0x4A9D3E\n"
        "crt87_exit:     .long 0x4A9D4B\n"
        "crt87_error:    .long 0x4A9C57\n"
        ".text\n");

__attribute__((naked)) void crt_acos87(void)
{
    __asm__("subl $8, %esp\n\t"
            "fstl (%esp)\n\t"
            "call *crt87_split\n\t"
            "call _crt_acos87_core\n\t"
            "addl $8, %esp\n\t"
            "ret");
}
HOOK(0x516810, crt_acos87);

/* in: st(0) = x, [esp+4] = x as a double, eax/ZF from the split helper */
__attribute__((naked)) void crt_acos87_core(void)
{
    __asm__("pushl %edx\n\t"
            "fstcw (%esp)\n\t"
            "je 4f\n\t"
            "cmpw $0x27F, (%esp)\n\t"
            "je 1f\n\t"
            "call *crt87_setcw\n"
            "1:\n\t"
            "cmpl $0x3FF00000, %eax\n\t"
            "jae 3f\n\t"
            /* atan2(sqrt((1 + x) * (1 - x)), x) */
            "fld1\n\t"
            "fadd %st(1), %st\n\t"
            "fld1\n\t"
            "fsub %st(2), %st\n\t"
            "fmulp %st, %st(1)\n\t"
            "fsqrt\n\t"
            "fxch %st(1)\n\t"
            "fpatan\n"
            "2:\n\t"
            "cmpl $0, 0x936058\n\t"
            "jne 8f\n\t"
            "movl $0xD, %edx\n\t"
            "movl $0x54EC20, %ecx\n\t"
            "jmp *crt87_exit\n"
            "3:\n\t"
            "ja 6f\n\t"
            "movl 12(%esp), %eax\n\t"
            "movl %eax, %ecx\n\t"
            "andl $0xFFFFF, %eax\n\t"
            "orl 8(%esp), %eax\n\t"
            "jne 6f\n\t"
            /* x is exactly +1 or -1 */
            "andl $0x80000000, %ecx\n\t"
            "fstp %st(0)\n\t"
            "je 9f\n\t"
            "fldpi\n\t"
            "jmp 2b\n"
            "9:\n\t"
            "fldz\n\t"
            "jmp 2b\n"
            "4:\n\t"
            "testl $0xFFFFF, %eax\n\t"
            "jne 5f\n\t"
            "cmpl $0, 8(%esp)\n\t"
            "je 6f\n"
            "5:\n\t"
            "call *crt87_nan\n\t"
            "jmp 7f\n"
            "6:\n\t"
            "fstp %st(0)\n\t"
            "fldt 0x5443B0\n\t"
            "movl $1, %eax\n"
            "7:\n\t"
            "cmpl $0, 0x936058\n\t"
            "jne 8f\n\t"
            "movl $0xD, %edx\n\t"
            "movl $0x54EC20, %ecx\n\t"
            "call *crt87_error\n\t"
            "popl %edx\n\t"
            "ret\n"
            "8:\n\t"
            "jmp *crt87_fastexit");
}
HOOK(0x51682D, crt_acos87_core);

__attribute__((naked)) void crt_asin87(void)
{
    __asm__("subl $8, %esp\n\t"
            "fstl (%esp)\n\t"
            "call *crt87_split\n\t"
            "call _crt_asin87_core\n\t"
            "addl $8, %esp\n\t"
            "ret");
}
HOOK(0x5168E0, crt_asin87);

__attribute__((naked)) void crt_asin87_core(void)
{
    __asm__("pushl %edx\n\t"
            "fstcw (%esp)\n\t"
            "je 4f\n\t"
            "cmpw $0x27F, (%esp)\n\t"
            "je 1f\n\t"
            "call *crt87_setcw\n"
            "1:\n\t"
            "cmpl $0x3FF00000, %eax\n\t"
            "jae 3f\n\t"
            /* atan2(x, sqrt((1 + x) * (1 - x))) */
            "fld1\n\t"
            "fadd %st(1), %st\n\t"
            "fld1\n\t"
            "fsub %st(2), %st\n\t"
            "fmulp %st, %st(1)\n\t"
            "fsqrt\n\t"
            "fpatan\n"
            "2:\n\t"
            "cmpl $0, 0x936058\n\t"
            "jne 8f\n\t"
            "movl $0xE, %edx\n\t"
            "movl $0x54EC30, %ecx\n\t"
            "jmp *crt87_exit\n"
            "3:\n\t"
            "ja 6f\n\t"
            "movl 12(%esp), %eax\n\t"
            "movl %eax, %ecx\n\t"
            "andl $0xFFFFF, %eax\n\t"
            "orl 8(%esp), %eax\n\t"
            "jne 6f\n\t"
            /* x is exactly +1 or -1: +-pi/2 */
            "andl $0x80000000, %ecx\n\t"
            "fstp %st(0)\n\t"
            "fldt 0x5443BA\n\t"
            "je 2b\n\t"
            "fchs\n\t"
            "jmp 2b\n"
            "4:\n\t"
            "testl $0xFFFFF, %eax\n\t"
            "jne 5f\n\t"
            "cmpl $0, 8(%esp)\n\t"
            "je 6f\n"
            "5:\n\t"
            "call *crt87_nan\n\t"
            "jmp 7f\n"
            "6:\n\t"
            "fstp %st(0)\n\t"
            "fldt 0x5443B0\n\t"
            "movl $1, %eax\n"
            "7:\n\t"
            "cmpl $0, 0x936058\n\t"
            "jne 8f\n\t"
            "movl $0xE, %edx\n\t"
            "movl $0x54EC30, %ecx\n\t"
            "call *crt87_error\n\t"
            "popl %edx\n\t"
            "ret\n"
            "8:\n\t"
            "jmp *crt87_fastexit");
}
HOOK(0x5168FD, crt_asin87_core);

char *crt_strncat(char *dst, const char *src, size_t n)
{
    char *d = dst;

    if (!n)
        return dst;
    while (*d)
        d++;
    while (n) {
        if (!(*d = *src))
            return dst;
        d++;
        src++;
        n--;
    }
    *d = 0;
    return dst;
}
HOOK(0x5169B0, crt_strncat);

CrtFltIn *crt_fltin(const char *s, int len, int scale, int decpt);

double crt_atof(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;

    for (;;) {
        int space = gMbCurMax > 1 ? crt_isctype(*p, 8) : gPctype[*p] & 8;

        if (!space)
            break;
        p++;
    }
    return crt_fltin((const char *)p, crt_strlen((const char *)p), 0, 0)->dval;
}
HOOK(0x516AD3, crt_atof);

unsigned char *crt_mbsnbcpy(unsigned char *dst, const unsigned char *src, size_t n);

#define SPLIT_MAX 0xFF

void crt_splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
    const unsigned char *p = (const unsigned char *)path;
    const unsigned char *name = NULL, *dot = NULL, *end;
    unsigned int n;

    if (crt_strlen(path) >= 1 && p[1] == ':') {
        if (drive) {
            crt_mbsnbcpy((unsigned char *)drive, p, 2);
            drive[2] = 0;
        }
        p += 2;
    } else if (drive) {
        *drive = 0;
    }
    end = p;
    if (*p) {
        do {
            if (gMbCtype[*end + 1] & 4)
                end++;
            else if (*end == '/' || *end == '\\')
                name = end + 1;
            else if (*end == '.')
                dot = end;
            end++;
        } while (*end);
    }
    if (name) {
        if (dir) {
            n = name - p;
            if (n >= SPLIT_MAX)
                n = SPLIT_MAX;
            crt_mbsnbcpy((unsigned char *)dir, p, n);
            dir[n] = 0;
        }
    } else {
        name = p;
        if (dir)
            *dir = 0;
    }
    if (dot && dot >= name) {
        if (fname) {
            n = dot - name;
            if (n >= SPLIT_MAX)
                n = SPLIT_MAX;
            crt_mbsnbcpy((unsigned char *)fname, name, n);
            fname[n] = 0;
        }
        if (ext) {
            n = end - dot;
            if (n >= SPLIT_MAX)
                n = SPLIT_MAX;
            crt_mbsnbcpy((unsigned char *)ext, dot, n);
            ext[n] = 0;
        }
    } else {
        if (fname) {
            n = end - name;
            if (n >= SPLIT_MAX)
                n = SPLIT_MAX;
            crt_mbsnbcpy((unsigned char *)fname, name, n);
            fname[n] = 0;
        }
        if (ext)
            *ext = 0;
    }
}
HOOK(0x516B20, crt_splitpath);

int crt_remove(const char *path)
{
    DWORD err = DeleteFileA(path) ? 0 : GetLastError();

    if (err) {
        crt_dosmaperr(err);
        return -1;
    }
    return 0;
}
HOOK(0x516C67, crt_remove);

size_t crt_fread(void *buf, size_t size, size_t count, CrtFile *fp)
{
    char *d = buf;
    unsigned int total = size * count, left = total, bufsize, n;
    int r;

    if (!total)
        return 0;
    bufsize = (fp->flag & IOREAD_ANY) ? (unsigned int)fp->bufsiz : 0x1000;
    while (left) {
        if ((fp->flag & IOREAD_ANY) && fp->cnt) {
            n = left < (unsigned int)fp->cnt ? left : (unsigned int)fp->cnt;
            crt_memcpy(d, fp->ptr, n);
            left -= n;
            fp->cnt -= n;
            fp->ptr += n;
            d += n;
        } else if (left >= bufsize) {
            n = bufsize ? left - left % bufsize : left;
            r = crt_read(fp->file, d, n);
            if (r == 0) {
                fp->flag |= IOEOF;
                return (total - left) / size;
            }
            if (r == -1) {
                fp->flag |= IOERR;
                return (total - left) / size;
            }
            left -= r;
            d += r;
        } else {
            r = crt_filbuf(fp);
            if (r == -1)
                return (total - left) / size;
            *d++ = (char)r;
            left--;
            bufsize = fp->bufsiz;
        }
    }
    return count;
}
HOOK(0x516C91, crt_fread);

size_t crt_fwrite(const void *buf, size_t size, size_t count, CrtFile *fp)
{
    const char *s = buf;
    unsigned int total = size * count, left = total, bufsize, n;
    int r;

    if (!total)
        return 0;
    bufsize = (fp->flag & IOREAD_ANY) ? (unsigned int)fp->bufsiz : 0x1000;
    do {
        if ((fp->flag & IOWRITE_ANY) && fp->cnt) {
            n = left < (unsigned int)fp->cnt ? left : (unsigned int)fp->cnt;
            crt_memcpy(fp->ptr, s, n);
            fp->cnt -= n;
            fp->ptr += n;
            left -= n;
            s += n;
        } else if (left >= bufsize) {
            if ((fp->flag & IOWRITE_ANY) && crt_flush(fp))
                return (total - left) / size;
            n = bufsize ? left - left % bufsize : left;
            r = crt_write(fp->file, s, n);
            if (r == -1) {
                fp->flag |= IOERR;
                return (total - left) / size;
            }
            s += r;
            left -= r;
            if ((unsigned int)r < n) {
                fp->flag |= IOERR;
                return (total - left) / size;
            }
        } else {
            if (crt_flsbuf(*s, fp) == -1)
                return (total - left) / size;
            s++;
            left--;
            bufsize = fp->bufsiz;
            if ((int)bufsize <= 0)
                bufsize = 1;
        }
    } while (left);
    return count;
}
HOOK(0x516D79, crt_fwrite);

CrtFltIn *crt_fltin(const char *s, int len, int scale, int decpt)
{
    char ld12[12];
    const char *end;
    double d;
    unsigned int r;
    int flags = 0, st;

    r = crt_strgtold12(ld12, &end, s, 0, 0, 0, 0);
    if (r & 4) {
        flags = 0x200;
        d = 0;
    } else {
        st = crt_ld12tod(ld12, &d);
        if ((r & 2) || st == 1)
            flags = 0x80;
        if ((r & 1) || st == 2)
            flags |= 0x100;
    }
    gFltIn->flags = flags;
    gFltIn->nbytes = end - s;
    gFltIn->dval = d;
    return gFltIn;
}
HOOK(0x516E83, crt_fltin);

unsigned char *crt_mbsnbcpy(unsigned char *dst, const unsigned char *src, size_t n)
{
    unsigned char *d = dst, c;

    if (!gIsMbCodePage)
        return (unsigned char *)crt_strncpy((char *)dst, (const char *)src, n);
    while (n) {
        c = *src++;
        n--;
        *d++ = c;
        if (gMbCtype[c + 1] & 4) {
            if (!n) {
                d[-1] = 0;
                break;
            }
            c = *src++;
            n--;
            *d++ = c;
            if (!c) {
                d[-2] = 0;
                break;
            }
        } else if (!c) {
            break;
        }
    }
    while (n--)
        *d++ = 0;
    return dst;
}
HOOK(0x516F14, crt_mbsnbcpy);
