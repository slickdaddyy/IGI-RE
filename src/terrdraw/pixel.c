/* Pixel conversion to 32 bit BGRA (bytes b, g, r, a) for texture uploads. */
#include <stdint.h>

#include "hook.h"
#include "igi.h"

/* Optional source rectangle: x, y, w, h. */
typedef struct PixRect {
    int x, y, w, h;
} PixRect;

/* Halves a 32 bit image: each output pixel is the rounded average of a
   2x2 block. */
void Pixel_Halve32(uint32_t *dst, const uint32_t *src, int width, int height)
{
    int half = width >> 1, rows = height >> 1;
    const uint32_t *r0 = src, *r1 = src + width;
    uint32_t *end;

    if (rows <= 0)
        return;
    do {
        const uint32_t *a = r0, *b = r1;

        for (end = dst + half; dst < end; dst++, a += 2, b += 2) {
            uint32_t p = a[0], q = a[1], s = b[0], t = b[1];
            uint32_t g = ((s & 0xFF00) + (q & 0xFF00) + (p & 0xFF00) + (t & 0xFF00) + 2) & 0x3FC00;
            uint32_t r = ((t & 0xFF0000) + (s & 0xFF0000) + (q & 0xFF0000) + (p & 0xFF0000) + 2) & 0x3FC0000;
            uint32_t bl = (t & 0xFF) + (s & 0xFF) + (q & 0xFF) + (p & 0xFF) + 2;
            uint32_t al = ((t >> 2 & 0x3FC00000) + (s >> 2 & 0x3FC00000) + (q >> 2 & 0x3FC00000) +
                           (p >> 2 & 0x3FC00000) + 0x800000) & 0xFF000000;

            *dst = (g | r | bl) >> 2 | al;
        }
        dst = end;
        r0 += width * 2;
        r1 += width * 2;
    } while (--rows);
}
HOOK(0x517B00, Pixel_Halve32);

static void clip(const PixRect *rect, int pitch, int rows, int *x, int *y, int *w, int *h)
{
    if (rect) {
        *x = rect->x;
        *y = rect->y;
        *w = rect->w < pitch - rect->x ? rect->w : pitch - rect->x;
        *h = rect->h < rows - rect->y ? rect->h : rows - rect->y;
    } else {
        *x = 0;
        *y = 0;
        *w = pitch;
        *h = rows;
    }
}

/* src is pitch pixels wide and rows high; pixels outside it (or outside
   rect) become 0. */
void Pixel_1555To32(uint8_t *dst, int width, int height, const uint16_t *src, int pitch, int rows, const PixRect *rect)
{
    int x0, y0, w, h, x, y;
    const uint16_t *line;

    clip(rect, pitch, rows, &x0, &y0, &w, &h);
    line = src + y0 * pitch + x0;
    for (y = 0; y < height; y++, line += pitch) {
        const uint16_t *s = line;

        for (x = 0; x < width; x++, s++, dst += 4) {
            if (x < w && y < h) {
                unsigned int p = *s;

                dst[0] = (p & 0x1F) << 3;
                dst[1] = (p >> 2) & 0xF8;
                dst[2] = (p >> 7) & 0xF8;
                dst[3] = (p & 0x8000) ? 0xFF : 0;
            } else {
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            }
        }
    }
}
HOOK(0x517C90, Pixel_1555To32);

void Pixel_565To32(uint8_t *dst, int width, int height, const uint16_t *src, int pitch, int rows, const PixRect *rect)
{
    int x0, y0, w, h, x, y;
    const uint16_t *line;

    clip(rect, pitch, rows, &x0, &y0, &w, &h);
    line = src + y0 * pitch + x0;
    for (y = 0; y < height; y++, line += pitch) {
        const uint16_t *s = line;

        for (x = 0; x < width; x++, s++, dst += 4) {
            if (x < w && y < h) {
                unsigned int p = *s;

                dst[0] = (p & 0x1F) << 3;
                dst[1] = (p >> 3) & 0xFC;
                dst[2] = (p >> 8) & 0xF8;
                dst[3] = 0xFF;
            } else {
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            }
        }
    }
}
HOOK(0x517DD0, Pixel_565To32);

/* The start offset is scaled by 16 bytes per pixel while rows step by 4. */
void Pixel_Copy32(uint32_t *dst, int width, int height, const char *src, int pitch, int rows, const PixRect *rect)
{
    int x0, y0, w, h, x, y;
    const uint32_t *line;

    clip(rect, pitch, rows, &x0, &y0, &w, &h);
    line = (const uint32_t *)(src + (y0 * pitch + x0) * 16);
    for (y = 0; y < height; y++, line += pitch, dst += width) {
        for (x = 0; x < width; x++)
            dst[x] = x < w && y < h ? line[x] : 0;
    }
}
HOOK(0x517F00, Pixel_Copy32);

/* 8 bit palettised: alpha from the alpha map, or 0xFF for any index but 0. */
void Pixel_8To32(uint8_t *dst, int width, int height, const uint8_t *alpha, const uint8_t *src, const uint8_t *palette,
                 int pitch, int rows, const PixRect *rect)
{
    int x0, y0, w, h, x, y;
    const uint8_t *line, *aline = NULL;

    clip(rect, pitch, rows, &x0, &y0, &w, &h);
    if (alpha)
        aline = alpha + y0 * pitch + x0;
    line = src + y0 * pitch + x0;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++, dst += 4) {
            if (x < w && y < h) {
                unsigned int i = line[x];

                dst[3] = alpha ? aline[x] : (i ? 0xFF : 0);
                dst[2] = palette[i * 4 + 2];
                dst[1] = palette[i * 4 + 1];
                dst[0] = palette[i * 4];
            } else {
                dst[3] = 0;
                dst[2] = 0;
                dst[1] = 0;
                dst[0] = 0;
            }
        }
        if (alpha)
            aline += pitch;
        line += pitch;
    }
}
HOOK(0x517FD0, Pixel_8To32);
