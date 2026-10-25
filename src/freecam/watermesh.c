#pragma GCC optimize("excess-precision=standard")

#include "freecam/freecam.h"
#include "terredit/terredittypes.h"
#include "math/mathvars.h"
#include "terredit/terreditvars.h"
#include "freecam/freecamvars.h"

/* Quad tree of the water surface around the camera, split finer near the
   viewer. The top level is an 8x8 grid; the 2x2 children of every split
   node of level n go into the 4x4 grid n. */

ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B4010, void, Matrix_SetDirection, (float *m, const float *dir));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4B38E0, void, Matrix_SetEuler, (float *m, float ax, float ay, float az));
ORIG(0x4B2900, void, Vec2_Normalize, (float *v));
ORIG(0x4B2760, long double, Math_ACosF, (float x));
ORIG(0x4B2B30, long double, Vec3_Angle, (const float *a, const float *b));
ORIG(0x52DAE0, void, WaterMesh_GetVertex, (WaterMesh *w, WaterVertex *out, const Vec3d *pos));
ORIG(0x52E180, void, WaterMesh_Light, (WaterMesh *w, float *color, const float *normal));
ORIG(0x5116D0, void, TerrainMesh_DrawTriangle, (WaterTri *t));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));

extern double kEnvLift;
extern float kEnvAngleScale;
extern float kEnvDistScale;
extern float kViewAngle;
extern double kSplitMin;
extern float kSplitDist;

void WaterMesh_CalcEnvUV(WaterMesh *w, float *uv, const Vec3d *pos, const float *normal)
{
    Vec3d d;
    float dir[3], a[10], b[10], c[10], v[2];
    float ax, ay, az, bx, by, bz, rx, ry, rz, nx, len;
    long double ny, inv;

    if (!w->envMap)
        return;
    d.x = pos->x - w->cam.x;
    d.y = pos->y - w->cam.y;
    d.z = pos->z - w->cam.z + kEnvLift;
    Vec3d_Normalize(&d);
    dir[0] = d.x;
    dir[1] = d.y;
    dir[2] = d.z;
    Matrix_SetDirection(a, normal);
    Matrix_SetDirection(b, dir);
    Matrix_GetEuler(a, &ax, &ay, &az);
    Matrix_GetEuler(b, &bx, &by, &bz);
    ax -= bx;
    ay -= by;
    az -= bz;
    Matrix_SetEuler(c, ax, ay, az);
    rx = c[0] * normal[0] + c[6] * normal[2] + c[3] * normal[1];
    ry = c[1] * normal[0] + c[7] * normal[2] + c[4] * normal[1];
    rz = c[2] * normal[0] + c[8] * normal[2] + c[5] * normal[1];
    nx = rx;
    if (fc_fne(rx, kZero) || fc_fne(ry, kZero) || fc_fne(rz, kZero)) {
        inv = kOneD / __builtin_sqrtl(rx * rx + ry * ry + rz * rz);
        nx = rx * inv;
        ny = inv * ry;
    } else {
        ny = ry;
    }
    v[1] = ny;
    len = __builtin_sqrtl(ny * ny + nx * nx);
    v[0] = nx;
    Vec2_Normalize(v);
    uv[0] = __builtin_fabsl(Math_ACosF(v[0])) * kEnvAngleScale;
    uv[1] = len * kEnvDistScale;
}
HOOK(0x52F3D0, WaterMesh_CalcEnvUV);

/* Splits the edge of t that borders finer nodes into count pieces and draws
   the fan of triangles; base is the index of the first piece. */
void WaterMesh_DrawSplitTri(WaterMesh *w, WaterNode *node, WaterTri *t, int base, int count, int recalc)
{
    Vec3d pos[32];
    float color[32][3], uv[32][2];
    WaterVertex v;
    Vec3d *a = NULL, *b = NULL;
    float *ca = NULL, *cb = NULL, *ua = NULL, *ub = NULL;
    uint32_t edges = node->edges;
    long double d, n;
    int i;

    if (edges & 0xA) {
        if (edges & 2) {
            a = &t->pos[0], b = &t->pos[1];
            ca = t->color[0], cb = t->color[1];
            ua = t->uv[0], ub = t->uv[1];
            if (count > 1) {
                d = b->x - a->x;
                n = base + count;
                for (i = 1; i < count; i++) {
                    pos[i].x = (base + i) * d / n + a->x;
                    memcpy(&pos[i].y, &a->y, 8);
                    pos[i].z = w->edgeZ;
                }
            }
        }
        if (edges & 8) {
            a = &t->pos[1], b = &t->pos[2];
            ca = t->color[1], cb = t->color[2];
            ua = t->uv[1], ub = t->uv[2];
            if (count > 1) {
                d = b->y - a->y;
                n = base + count;
                for (i = 1; i < count; i++) {
                    memcpy(&pos[i].x, &a->x, 8);
                    pos[i].y = (base + i) * d / n + a->y;
                    pos[i].z = w->edgeZ;
                }
            }
        }
    } else if (edges & 0x14) {
        if (edges & 4) {
            a = &t->pos[0], b = &t->pos[1];
            ca = t->color[0], cb = t->color[1];
            ua = t->uv[0], ub = t->uv[1];
            if (count > 1) {
                d = b->x - a->x;
                n = base + count;
                for (i = 1; i < count; i++) {
                    pos[i].x = (base + i) * d / n + a->x;
                    memcpy(&pos[i].y, &a->y, 8);
                    pos[i].z = w->edgeZ;
                }
            }
        }
        if (edges & 0x10) {
            a = &t->pos[0], b = &t->pos[2];
            ca = t->color[0], cb = t->color[2];
            ua = t->uv[0], ub = t->uv[2];
            if (count > 1) {
                d = b->y - a->y;
                n = base + count;
                for (i = 1; i < count; i++) {
                    memcpy(&pos[i].x, &a->x, 8);
                    pos[i].y = (base + i) * d / n + a->y;
                    pos[i].z = w->edgeZ;
                }
            }
        }
    } else {
        return;
    }
    memcpy(&pos[0], a, sizeof(Vec3d));
    memcpy(color[0], ca, 12);
    memcpy(uv[0], ua, 8);
    memcpy(&pos[count], b, sizeof(Vec3d));
    memcpy(color[count], cb, 12);
    memcpy(uv[count], ub, 8);
    if (recalc && count > 1) {
        for (i = 1; i < count; i++) {
            WaterMesh_GetVertex(w, &v, &pos[i]);
            memcpy(&pos[i].z, &v.pos.z, 8);
            WaterMesh_Light(w, color[i], v.normal);
            if (t->envMap == 1)
                WaterMesh_CalcEnvUV(w, uv[i], &v.pos, v.normal);
        }
    }
    for (i = 1; i <= count; i++) {
        memcpy(a, &pos[i - 1], sizeof(Vec3d));
        memcpy(b, &pos[i], sizeof(Vec3d));
        if (recalc) {
            memcpy(ca, color[i - 1], 12);
            memcpy(cb, color[i], 12);
            memcpy(ua, uv[i - 1], 8);
            memcpy(ub, uv[i], 8);
        }
        if (w->debugColors) {
            /* alternate blue and black pieces */
            int k;

            for (k = 0; k < 3; k++) {
                t->color[k][0] = 0;
                t->color[k][1] = 0;
                t->color[k][2] = i % 2 ? 1.0f : 0;
            }
        }
        TerrainMesh_DrawTriangle(t);
    }
}
HOOK(0x52F630, WaterMesh_DrawSplitTri);

/* Sets the edge flags of grid node (level, x, y) from the split neighbours
   whose children touch it. */
void WaterMesh_SetEdgeFlags(WaterMesh *w, int level, int x, int y)
{
    uint32_t f = 0;
    WaterNode *nb, *c;
    int k;

    if ((uint32_t)(level + 1) >= w->count) {
        w->grid[level][x][y].edges = 0;
        return;
    }
    if (x > 0) {
        nb = &w->grid[level][x - 1][y];
        if (nb->active) {
            f = 0x10;
            for (k = 0; k < 4; k++) {
                c = &w->grid[level + 1][3][k];
                if (c->active && c->y >= nb->y && (long double)nb->size + nb->y > c->y)
                    f |= k % 2 ? 0x100 : 0x20;
            }
        }
    }
    if (y > 0) {
        nb = &w->grid[level][x][y - 1];
        if (nb->active) {
            f |= 4;
            for (k = 0; k < 4; k++) {
                c = &w->grid[level + 1][k][3];
                if (c->active && c->x >= nb->x && (long double)nb->size + nb->x > c->x)
                    f |= k % 2 ? 0x100 : 0x80;
            }
        }
    }
    if (x < 3) {
        nb = &w->grid[level][x + 1][y];
        if (nb->active) {
            f |= 8;
            for (k = 0; k < 4; k++) {
                c = &w->grid[level + 1][0][k];
                if (c->active && c->y >= nb->y && (long double)nb->size + nb->y > c->y)
                    f |= k % 2 ? 0x80 : 0x40;
            }
        }
    }
    if (y < 3) {
        nb = &w->grid[level][x][y + 1];
        if (nb->active) {
            f |= 2;
            for (k = 0; k < 4; k++) {
                c = &w->grid[level + 1][k][0];
                if (c->active && c->x >= nb->x && (long double)nb->size + nb->x > c->x)
                    f |= k % 2 ? 0x20 : 0x40;
            }
        }
    }
    w->grid[level][x][y].edges = f;
}
HOOK(0x52FBA0, WaterMesh_SetEdgeFlags);

void WaterMesh_CheckVisible(WaterMesh *w, WaterNode *n)
{
    long double h = n->size >> 1;
    long double cx = h + n->x;
    double cy = h + n->y;
    double cz = w->level;
    long double inv;
    float d[3];

    d[0] = cx - w->cam.x;
    d[1] = cy - w->cam.y;
    d[2] = cz - w->cam.z;
    if (!(__builtin_fabsl(d[0]) >= h) && !(__builtin_fabsl(d[1]) >= h)) {
        n->visible = 1;
        return;
    }
    if (fc_fne(d[0], kZero) || fc_fne(d[1], kZero) || fc_fne(d[2], kZero)) {
        inv = kOneD / __builtin_sqrtl(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        d[0] = d[0] * inv;
        d[1] = d[1] * inv;
        d[2] = d[2] * inv;
    }
    n->visible = !(__builtin_fabsl(Vec3_Angle(d, w->camDir)) > kViewAngle);
}
HOOK(0x52FE60, WaterMesh_CheckVisible);

void WaterMesh_Subdivide(WaterMesh *w, WaterNode *n)
{
    uint32_t size = n->size, half = size >> 1, c;
    long double h = half;
    double cx = h + n->x;
    long double cy = h + n->y;
    uint32_t i, j;
    int ox = 0, oy = 0;

    if (fc_fistp(__builtin_fabsl(cx - w->cam.x)) > size || fc_fistp(__builtin_fabsl(cy - w->cam.y)) > size
        || !((long double)size > kSplitMin) || !((long double)size * kSplitDist > w->camHeight)) {
        if (n->level > 0) {
            WaterMesh_CheckVisible(w, n);
            n->active = 0;
            return;
        }
    }
    n->active = 1;
    c = gWaterSplits[n->level];
    if (c == 1) {
        oy = 2;
    } else if (c == 2) {
        ox = 2;
    } else if (c == 3) {
        ox = 2;
        oy = 2;
    }
    gWaterSplits[n->level] = c + 1;
    for (i = 0; i < 2; i++) {
        double offX = (uint32_t)(i * half);

        for (j = 0; j < 2; j++) {
            WaterNode *ch = &w->grid[n->level][i + ox][j + oy];

            ch->parent = n;
            ch->size = half;
            ch->level = n->level + 1;
            ch->flags = 0;
            ch->x = offX + n->x;
            ch->y = (long double)(uint32_t)(j * half) + n->y;
            if (w->count <= ch->level)
                w->count = ch->level;
            ch->flags |= i ? 8 : 0x10;
            ch->flags |= j ? 2 : 4;
            if ((long double)ch->size > kSplitMin) {
                WaterMesh_Subdivide(w, ch);
            } else {
                WaterMesh_CheckVisible(w, ch);
                ch->active = 0;
            }
        }
    }
}
HOOK(0x52FFA0, WaterMesh_Subdivide);

void WaterMesh_Close(void)
{
    Primitive_Unregister(gWaterPrimitive);
}
HOOK(0x530210, WaterMesh_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5336D4) float kEnvDistScale INIT(255.0f);
ALIAS(k255, kEnvDistScale, 0);
DATA(534CE0) float kEnvAngleScale INIT(81.16902f);
PAD(igd, 534CE0, 534CE4, 0x4);
DATA(534CE0) double kEnvLift INIT(819200.0);
DATA(534CE0) float kViewAngle INIT(1.7278761f);
PAD(igd, 534CE0, 534CF4, 0x4);
DATA(534CE0) double kSplitMin INIT(131072.0);
STORE(igb, A84584, 4) uint32_t store_A84584[21];
ALIAS(gWaterSplits, store_A84584, 0);
