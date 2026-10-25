/* QCube_Walk: the per frame octree walk that collects visible objects and terrain cells. */
#pragma GCC optimize("excess-precision=standard")
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "core/pool.h"
#include "transctx/transctx.h"
#include "terrain/terraintypes.h"
#include "lod/lodtypes.h"
#include "coll/colltypes.h"
#include "glowmesh/glowmeshtypes.h"
#include "task/taskvars.h"
#include "coll/collvars.h"
#include "bonemesh/bonemeshvars.h"
#include "glowmesh/glowmeshvars.h"
#include "editcam/editcamvars.h"
#include "magicobj/magicobjvars.h"
#include "lod/lodvars.h"
#include "nodereg/noderegvars.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4C5810, void, Coll_ResetMods, (void));

typedef struct WStack {
    CollNode *cell;
    int flags; /* 1 visited, 2 has cell, 4..6 sub order, 7..9 octant, 10.. trees */
} WStack;

typedef void (*WHandler)(DynCubeObj *task, void *arg);

extern signed char gWalkTreeIndex[];
extern DynCubeNode gWalkSavedRoots[];
extern DynCubeNode gWalkFakeRoots[];
extern int gWalkOverflow;
extern int gCollBumpTop;

F32(kSlope, 0x3fddb574);     /* 1.7321 */
F32(kFixedOne, 0x4e800000);  /* 2^30 */
F32(kHalfF, 0x3f000000);
F32(kMaxSize, 0x46160000);   /* 9600 */
static const double kNearLimit = 64.0;

#define SAR(x, n) ((x) >> ((n) & 31))

static inline long long walk_round(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return r;
}

static long double walk_cos_atan(long double x)
{
    long double r;

    __asm__("fpatan\n\tfcos" : "=t"(r) : "0"(1.0L), "u"(x) : "st(1)");
    return r;
}

static inline int walk_abs(int x)
{
    return x < 0 ? (int)(0u - (unsigned)x) : x;
}

static void walk_add(DynCubeObj *t, uint8_t slot)
{
    gQCubeOut->task = t;
    gQCubeOut->slot = slot;
    gQCubeOut++;
    gQCubeCount++;
}

static void walk_release(Pool *p, char *elem)
{
    int slot = *(int *)(elem + p->size);
    int last = --p->used;

    if (slot != last) {
        char *le = p->data + p->free[last] * p->stride;
        int tmp = p->free[slot];
        int other = *(int *)(le + p->size);

        p->free[slot] = p->free[other];
        p->free[other] = tmp;
        *(int *)(le + p->size) = slot;
    }
}

/*
 * Walks the terrain cell tree and up to five object octrees at once with an
 * explicit stack. Positions are fixed point (the root cube spans +-2^30);
 * children are tested against the view frustum in view space. Visible
 * objects go on the draw list, visible terrain cells at depth 7 and below
 * (or small enough) on the terrain cell list, with per cell data cached
 * across frames by position.
 */
void QCube_Walk(Terrain *w)
{
    double *nearp = gCurrentWorld->waterLevel;
    int nearZ = nearp ? (int)walk_round(*nearp) : (int)0xE0000000;
    TerrainCell *out = gTerrainCells;
    TerrainMesh **meshes = gTerrainMeshes;
    int cells = 0, nmeshes = 0;
    int cur = gDisplayFrame & 0xFFFF, prev = (gDisplayFrame - 1) & 0xFFFF;
    CollNode *terrain = w->ctr;
    int minSize = gQCubeMinSize;
    float fovx, fovy, sx, sy, cy, cz, m[10];
    uint8_t order[64], perm[64];
    int camx, camy, camz, ox, oy, oz, vx, vy, vz;
    int corner[8][3], rcorner[8][3];
    DynCubeNode *ext[5];
    WStack stack[128], *top;
    CollNode *cell = NULL, *cc = NULL;
    int depth, i, j, k;

    gWalkOverflow = 0;
    Coll_ResetMods();
    fovx = gTransContext.fov;
    fovy = gTransContext.fovY;
    memcpy(order, gQCubeChildOrder, sizeof(order));
    sx = kSlope.f / walk_cos_atan(fovx) * kFixedOne.f;
    sy = kSlope.f / walk_cos_atan(fovy) * kFixedOne.f;
    for (i = 0; i < 64; i++)
        perm[i] = gCollChildTable[i];
    cy = gTransContext.pos.y;
    cz = gTransContext.pos.z;
    memcpy(m, gTransContext.matrix.m, sizeof(m));
    camx = walk_round(gTransContext.pos.x);
    ox = -(camx + gQCubeOrigin[0]);
    camy = walk_round(cy);
    oy = -(gQCubeOrigin[1] + camy);
    camz = walk_round(cz);
    oz = -(camz + gQCubeOrigin[2]);
    {
        long double a = ox, b = oy, c = oz;
        float t0 = m[2] * c + m[1] * b + m[0] * a;
        float t1 = m[5] * c + m[4] * b + m[3] * a;
        long double t2 = m[8] * c + m[7] * b + m[6] * a;

        vx = walk_round(t0);
        vy = walk_round(t1);
        vz = walk_round(t2);
    }
    for (i = 0; i < 8; i++) {
        long double x, y, z;
        float t0, t1;
        long double t2;

        corner[i][0] = i & 1 ? 0x40000000 : (int)0xC0000000;
        corner[i][1] = i & 2 ? 0x40000000 : (int)0xC0000000;
        corner[i][2] = i & 4 ? 0x40000000 : (int)0xC0000000;
        x = corner[i][0];
        y = corner[i][1];
        z = corner[i][2];
        t0 = z * m[2] + y * m[1] + x * m[0];
        t1 = z * m[5] + y * m[4] + x * m[3];
        t2 = z * m[8] + y * m[7] + x * m[6];
        rcorner[i][0] = walk_round(t0);
        rcorner[i][1] = walk_round(t1);
        rcorner[i][2] = walk_round(t2);
    }

    stack[0].flags = 0;
    if (terrain) {
        stack[0].cell = &terrain[1];
        terrain[1].mask &= 0xF0;
        stack[0].flags = 2;
    }
    /* the object trees are walked mirrored: children and their bits reversed */
    for (k = 10; k < 15; k++) {
        DynCubeNode *root = w->roots[gWalkTreeIndex[k - 10]];
        DynCubeNode *saved = &gWalkSavedRoots[k - 10];
        uint8_t b, r;

        if (!root)
            continue;
        *saved = *root;
        for (j = 0; j < 8; j++)
            root->children[j] = saved->children[7 - j];
        b = root->child_mask;
        r = 0;
        for (j = 0; j < 8; j++)
            r = r * 2 + (b >> j & 1);
        root->child_mask = r;
        gWalkFakeRoots[k - 10].children[0] = root;
        stack[0].flags |= 1 << k;
        ext[k - 10] = &gWalkFakeRoots[k - 10];
    }
    vx -= rcorner[0][0];
    vy -= rcorner[0][1];
    vz -= rcorner[0][2];
    ox -= corner[0][0];
    oy -= corner[0][1];
    oz -= corner[0][2];

    depth = -1;
    top = stack;
    do {
        int fl = top->flags;
        int oct = fl >> 7 & 7;
        int trees = fl >> 10;
        int sel = 0, cellmask = 0, extmask = 0;
        const uint8_t *pp, *ord;

        if (fl & 1) {
            int bit = 1;

            if (trees) {
                for (j = 0; j < 5; j++, bit <<= 1) {
                    DynCubeNode *n;
                    DynCubeObj *t;

                    if (!(trees & bit))
                        continue;
                    n = ext[j];
                    ext[j] = n->parent;
                    if (n->mask & 0x100) {
                        t = n->objs;
                        do {
                            if (t->mask & 0x100)
                                gTaskHandlers[16][t->task.type](t, (void *)t->unk48);
                            t = t->next;
                        } while (t);
                    }
                    if (n->mask & 2) {
                        t = n->objs;
                        do {
                            if ((t->mask & 2) && !t->unk60)
                                walk_add(t, 5);
                            t = t->next;
                        } while (t);
                    }
                }
            }
            vx -= SAR(rcorner[oct][0], depth);
            vy -= SAR(rcorner[oct][1], depth);
            sx = sx + sx;
            sy = sy + sy;
            vz -= SAR(rcorner[oct][2], depth);
            ox -= SAR(corner[oct][0], depth);
            oy -= SAR(corner[oct][1], depth);
            oz -= SAR(corner[oct][2], depth);
            depth--;
            top--;
            continue;
        }

        top->flags = fl | 1;
        if (fl & 2) {
            cell = top->cell;
            sel = (fl | 1) >> 4 & 7;
            cellmask = cell->mask;
        }
        gQCubeSkipCells = 0;
        if (trees) {
            int bit = 1;

            for (j = 0; j < 5; j++, bit <<= 1) {
                DynCubeNode *c;
                DynCubeObj *t;

                if (!(trees & bit))
                    continue;
                c = ext[j]->children[oct];
                ext[j] = c;
                extmask |= c->child_mask;
                if (c->mask & 0x80) {
                    t = c->objs;
                    do {
                        if (t->mask & 0x80)
                            gTaskHandlers[15][t->task.type](t, (void *)t->unk48);
                        t = t->next;
                    } while (t);
                }
                if (c->mask & 1) {
                    t = c->objs;
                    do {
                        if ((t->mask & 1) && !t->unk60)
                            walk_add(t, 4);
                        t = t->next;
                    } while (t);
                }
            }
        }
        depth++;
        vx += SAR(rcorner[oct][0], depth);
        sx *= kHalfF.f;
        vy += SAR(rcorner[oct][1], depth);
        vz += SAR(rcorner[oct][2], depth);
        ox += SAR(corner[oct][0], depth);
        sy *= kHalfF.f;
        oy += SAR(corner[oct][1], depth);
        oz += SAR(corner[oct][2], depth);
        pp = &perm[sel * 8];
        ord = &order[sel * 8];
        if (gQCubeSkipCells)
            cellmask = 0;

        for (i = 0; i < 8; i++) {
            int c = (signed char)pp[i];
            int cbit = 1 << (c & 31);
            int hasExt = 1 << i & extmask;
            int sh = depth + 1;
            int z, cx, cyi, czi, half, mx, size, newflags, push, sub;
            float zf;
            long double xx, yy, wx, wy;

            if (!hasExt && !(cellmask & cbit))
                continue;
            z = SAR(rcorner[i][2], sh) + vz;
            zf = z;
            if (kNearLimit - sy >= z)
                continue;
            xx = SAR(rcorner[i][0], sh) + vx;
            wx = zf * fovx + sx;
            if (xx >= wx || !(xx > -wx))
                continue;
            yy = SAR(rcorner[i][1], sh) + vy;
            wy = zf * fovy + sy;
            if (yy >= wy || !(yy > -wy))
                continue;
            czi = SAR(corner[i][2], sh) + oz;
            half = SAR(0x40000000, sh);
            if (half + czi + camz < nearZ)
                continue;
            cx = SAR(corner[i][0], sh) + ox;
            cyi = SAR(corner[i][1], sh) + oy;
            mx = walk_abs(cx);
            if (walk_abs(cyi) > mx)
                mx = walk_abs(cyi);
            if (walk_abs(czi) > mx)
                mx = walk_abs(czi);
            newflags = 0;
            push = 0;
            size = SAR(mx - half, 0x18 - depth);
            if (hasExt && !((long double)size * gTransContext.fov >= kMaxSize.f)) {
                int bit = 1;

                for (j = 0; j < 5; j++, bit <<= 1) {
                    DynCubeNode *n;

                    if (!(trees & bit) || !(ext[j]->child_mask & (uint8_t)(1 << i)))
                        continue;
                    n = ext[j]->children[i];
                    if ((n->sub_mask | n->mask) & 0x183) {
                        newflags |= bit << 10;
                        push = 1;
                    }
                }
            }
            if (cellmask & cbit) {
                cc = &terrain[cell->child[c]];
                sub = (signed char)ord[cell->code[c]];
                if (size < minSize && depth + 1 < 16) {
                    newflags |= sub << 4 | 2;
                    push = 1;
                } else if (depth + 1 > 6) {
                    int X = cx + camx, Z = czi + camz, Y = cyi + camy;
                    unsigned h = ((((((Z >> 2) & 0x1FFC000) ^ (X & 0x1FFC000)) >> 3 ^ (Y & 0x3FF800)) >> 5) ^
                                  ((Z ^ Y ^ X) & 0x1FFC0)) >> 6;
                    QCacheEntry *ce;
                    int fresh = 1;

                    for (;;) {
                        ce = &gQCubeCache[h];
                        if (ce->frame == -1)
                            break;
                        if (prev == ce->frame && ce->cell[0] == X && ce->cell[1] == Y && ce->cell[2] == Z)
                            break;
                        h = (h + 1) & 0x7FF;
                    }
                    if (prev == ce->frame) {
                        ce->frame = cur;
                        fresh = 0;
                        if (!ce->data) {
                            Error_Show("Should have allocated data for cube existing previous frame");
                            for (;;)
                                ;
                        }
                        *meshes++ = (TerrainMesh *)ce->data;
                        nmeshes++;
                    } else {
                        if (ce->data) {
                            Error_Show("Should't have allocate hash data! ERROR!");
                            for (;;)
                                ;
                        }
                        ce->kind = 3;
                        ce->cell[3] = size;
                        ce->cell[0] = X;
                        ce->cell[1] = Y;
                        ce->cell[2] = Z;
                        ce->frame = cur;
                    }
                    if (cells > 0xFFF) {
                        gWalkOverflow++;
                    } else if (fresh) {
                        out->cache = ce;
                        out->materials = gCollHoleBits;
                        if (gVisibleLightMapCount)
                            out->maps = gVisibleLightMaps[gVisibleLightMapCount - 1] | gCollBumpTop << 16;
                        else
                            out->maps = gCollBumpTop << 16 | 0xFFFF;
                        out->shape = cc->leaf;
                        out->flags = sub;
                        out->level = depth + 1;
                        out->pos[2] = Z;
                        out->pos[1] = Y;
                        out->pos[0] = X;
                        out++;
                        cells++;
                    }
                }
            }
            if (push) {
                top++;
                top->flags = i << 7 | newflags;
                top->cell = cc;
            }
        }
    } while (top >= stack);

    for (j = 0; j < 5; j++) {
        DynCubeNode *root = w->roots[gWalkTreeIndex[j]];

        if (root)
            *root = gWalkSavedRoots[j];
    }
    gTerrainCellCount = cells;
    gTerrainMeshCount = nmeshes;
    for (i = 0; i < 0x800; i++) {
        QCacheEntry *c = &gQCubeCache[i];
        int f = c->frame;

        if (((f & 0xFFFF0000) == 0 && (f & 0xFFFF) != cur) || (f & 0xFFFF) + 1 < cur) {
            if (c->data) {
                if (c->kind == 0) {
                    walk_release(&gQCubePool0, c->data);
                } else if (c->kind == 1) {
                    walk_release(&gQCubePool1, c->data);
                } else if (c->kind == 2) {
                    walk_release(&gQCubePool2, c->data);
                } else {
                    Error_Show("Illegal alloc info");
                    for (;;)
                        ;
                }
                c->data = NULL;
            }
            c->frame = -1;
        }
    }
}
HOOK(0x4D14E0, QCube_Walk);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 548444, 1) signed char store_548444[8] INIT({0, 1, 2, 4, 5});
ALIAS(gWalkTreeIndex, store_548444, 0);
PAD(igb, A4EF38, A4EF38, 0x20C4);
ALIAS(gWalkSavedRoots, pad_A4EF38, 0);
PAD(igb, A53740, A53740, 0x18C);
ALIAS(gWalkFakeRoots, pad_A53740, 0);
BSS(B12B80) int gWalkOverflow;
BSS(B12BB8) int gQCubeSkipCells;
PAD(igb, B12BB8, B12BBC, 0x4);
STORE(igb, B12BB8, 4) int store_B12BC0[3];
ALIAS(gQCubeOrigin, store_B12BC0, 0);
STORE(igb, B12BE0, 1) uint8_t store_B12BE0[8][8];
ALIAS(gQCubeChildOrder, store_B12BE0, 0);
BSS(B12C58) QEntry *gQCubeOut;
STORE(igb, B16CE0, 8) __typeof__(gTerrainCells[0]) store_B16CE0[4096];
ALIAS(gTerrainCells, store_B16CE0, 0);
STORE(igb, B97B20, 4) int store_B97B20[64];
ALIAS(gVisibleLightMaps, store_B97B20, 0);
BSS(C10380) int gDisplayFrame;
