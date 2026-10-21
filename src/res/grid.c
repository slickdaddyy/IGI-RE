#include <string.h>

#include "hook.h"
#include "igi.h"
#include "res/sprite.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));

/* The character map of a "LOOP" sprite file */
typedef struct CharMap {
    int unk0[4];
    int cols, rows;
    int unk18[3];
    struct {
        unsigned char frame;
        char unk1[15];
    } cells[];
} CharMap;

Grid *Grid_LoadCharMap(const char *data, Sprite *s)
{
    const CharMap *m = (const CharMap *)data;
    Grid *g;
    int r, c, i = 0;

    Sprite_SetFixed(s, 1);
    g = Grid_New(m->cols, m->rows, s);
    for (r = 0; r < m->rows; r++)
        for (c = 0; c < m->cols; c++, i++)
            g->cells[i] = m->cells[i].frame;
    return g;
}
HOOK(0x4B75C0, Grid_LoadCharMap);

Grid *Grid_New(int cols, int rows, Sprite *s)
{
    int n = cols * rows;
    Grid *g = Mem_Alloc(n + 0x13, 4);
    int i;

    memset(g, 0, n + 0x13);
    g->proportional = !Sprite_IsFixed(s);
    g->cols = cols;
    g->rows = rows;
    g->sprite = s;
    g->widths = Mem_Alloc(rows * 2, 4);
    for (i = 0; i < rows; i++)
        g->widths[i] = 0;
    if (n > 0)
        memset(g->cells, 0, n);
    return g;
}
HOOK(0x4B7630, Grid_New);

void Grid_Free(Grid *g)
{
    Mem_Free(g->widths);
    g->widths = NULL;
    Mem_Free(g);
}
HOOK(0x4B76D0, Grid_Free);

void Grid_Set(Grid *g, int x, int y, int c)
{
    g->cells[g->cols * y + x] = c;
}
HOOK(0x4B76F0, Grid_Set);

char Grid_Get(Grid *g, int x, int y)
{
    return g->cells[g->cols * y + x];
}
HOOK(0x4B7710, Grid_Get);

void Grid_UpdateRow(Grid *g, int row)
{
    Sprite *s = g->sprite;
    SpriteFrame *frames = s->frames;
    int cols = g->cols;
    unsigned char *p = &g->cells[cols * row];
    int x = 0, end = 0, i;

    if (g->proportional == 0) {
        int w = Sprite_Width(s);

        for (i = 0; i < cols; i++) {
            x += w;
            if (p[i])
                end = x;
        }
        g->widths[row] = end;
    } else if (g->proportional == 1) {
        int advance = Sprite_Advance(s);

        for (i = 0; i < cols; i++) {
            if (p[i]) {
                x += frames[p[i] - 1].advance;
                end = x;
            } else {
                x += advance;
            }
        }
        g->widths[row] = end;
    }
}
HOOK(0x4B7730, Grid_UpdateRow);

void Grid_Print(Grid *g, const char *text, int x, int y)
{
    unsigned char *map = g->sprite->map->cells;
    unsigned char *p = &g->cells[g->cols * y + x];

    for (; *text; text++)
        *p++ = map[(unsigned char)*text];
    Grid_UpdateRow(g, y);
}
HOOK(0x4B77F0, Grid_Print);

void Grid_Scroll(Grid *g, int n)
{
    int cols = g->cols;
    unsigned char *dst = g->cells, *src = g->cells + cols * n;
    int i = 0, k;

    if (n < 1)
        return;
    for (; i < g->rows - n; i++) {
        g->widths[i] = g->widths[i + n];
        for (k = 0; k < cols; k++)
            dst[k] = src[k];
        dst += cols;
        src += cols;
    }
    for (; i < g->rows; i++) {
        g->widths[i] = 0;
        if (cols > 0)
            memset(dst, 0, cols);
        dst += cols;
    }
}
HOOK(0x4B7850, Grid_Scroll);

void Grid_Draw(Grid *g, float x, float y, float r, float gr, float b, float alpha, float z, unsigned int flags, int unk20)
{
    Sprite *s = g->sprite;
    int cw = Sprite_Width(s);
    int ch = Sprite_Height(s);
    /* stored as floats after every step, like the original */
    volatile float cx, cy = y, step;
    int row, col, i = 0, advance;

    if (g->proportional == 0) {
        for (row = 0; row < g->rows; row++) {
            if (flags & TEXT_CENTER)
                cx = x - (float)(g->widths[row] / 2);
            else if (flags & TEXT_RIGHT)
                cx = x - (float)g->widths[row];
            else
                cx = x;
            if (g->cols > 0)
                step = (float)cw;
            for (col = 0; col < g->cols; col++, i++) {
                unsigned char c = g->cells[i];

                if (c)
                    QVertex_DrawFrame(s, cx, cy, r, gr, b, alpha, z, (unsigned char)(c - 1), flags, unk20);
                cx = step + cx;
            }
            cy = (float)ch + cy;
        }
    } else if (g->proportional == 1) {
        advance = Sprite_Advance(s);
        for (row = 0; row < g->rows; row++) {
            if (flags & TEXT_CENTER)
                cx = x - (float)(g->widths[row] / 2);
            else if (flags & TEXT_RIGHT)
                cx = x - (float)g->widths[row];
            else
                cx = x;
            for (col = 0; col < g->cols; col++, i++) {
                unsigned char c = g->cells[i];

                if (c) {
                    c--;
                    QVertex_DrawFrame(s, cx, cy, r, gr, b, alpha, z, c, flags, unk20);
                    cx = (float)s->frames[c].advance + cx;
                } else {
                    cx = (float)advance + cx;
                }
            }
            cy = (float)ch + cy;
        }
    }
}
HOOK(0x4B7930, Grid_Draw);

int Grid_Width(Grid *g)
{
    int w = 0, i;

    for (i = 0; i < g->rows; i++)
        if (g->widths[i] > w)
            w = g->widths[i];
    return w;
}
HOOK(0x4B7BF0, Grid_Width);

int Grid_Height(Grid *g)
{
    return Sprite_Height(g->sprite) * g->rows;
}
HOOK(0x4B7C20, Grid_Height);

void Grid_Clear(Grid *g)
{
    memset(g->widths, 0, g->rows * 2);
    memset(g->cells, 0, g->cols * g->rows);
}
HOOK(0x4B7C40, Grid_Clear);
