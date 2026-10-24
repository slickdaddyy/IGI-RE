#pragma GCC optimize("excess-precision=standard")

#include "aiaction/aiaction.h"
#include "hook.h"
#include "glass/glasstypes.h"

ORIG(0x44A630, void, GlassShatter_PlaySound, (const Vec3d *pos));
ORIG(0x44A670, void, GlassShatter_AddTri, (float *a, float *b, float *c));
ORIG(0x49A450, void *, PolyList_New, (void *world, int a, const Vec3d *pos, const Matrix *m, void *polys, int count,
                                    int b, int c, int d, int e, int f));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));

extern const float *gShatterColor;
extern float gShatterSpeed;
extern uint16_t gShatterType;
extern int gShatter244;
extern GlassShatter *gShatterTask;
extern void *gShatterPolys;
extern Vec3f gShatterHit;
extern void *gShatterShards;

extern const float gShardColor2[]; /* 0.7, 0.7, 0.7 */

extern const float kZero;
extern const float kOne;

/* like GlassShatter_Spawn, but the pieces fly away from the point from;
   force is not used */
void GlassShatter_SpawnFrom(void **model, const Matrix *matrix, const Vec3d *pos, const Vec3d *from, float force,
                            float scale)
{
    char *info = *(char **)(*(char **)((char *)model[0] + 0xC) + 0xC);
    GlassShatter *g = (GlassShatter *)Task_New(Level_GetDynamic(), gShatterType, NULL);
    const float *m = matrix->m;
    TriMesh *mesh;
    long double z, k, f, zs;
    float len, xs, ys, a0, a1, a2;
    int i;

    (void)force;
    gShatter244 = 1;
    mesh = *(TriMesh **)(info + 0x2C);
    if (!mesh->count)
        return;
    g->dir.x = pos->x - from->x;
    g->dir.y = pos->y - from->y;
    z = pos->z - from->z;
    g->dir.z = z;
    len = x87_sqrt(((long double)g->dir.x * g->dir.x + (long double)g->dir.y * g->dir.y) + z * z);
    if (kZero > len)
        k = kZero;
    else if (kOne < len)
        k = kOne;
    else
        k = len;
    k = k * scale;
    f = k / len;
    xs = f * g->dir.x;
    g->dir.x = xs;
    ys = f * g->dir.y;
    g->dir.y = ys;
    zs = f * g->dir.z;
    g->dir.z = zs;
    a0 = (xs * m[0] + zs * m[6]) + ys * m[3];
    a1 = (xs * m[1] + zs * m[7]) + ys * m[4];
    a2 = (zs * m[8] + ys * m[5]) + xs * m[2];
    g->dir.x = a0;
    g->dir.y = a1;
    g->dir.z = a2;
    g->pos = *pos;
    g->matrix = *matrix;
    g->model = model;
    g->frames = 30;

    gShatterHit.x = from->x - pos->x;
    gShatterHit.y = from->y - pos->y;
    gShatterHit.z = from->z - pos->z;
    a0 = (gShatterHit.x * m[0] + gShatterHit.z * m[6]) + gShatterHit.y * m[3];
    a1 = (gShatterHit.x * m[1] + gShatterHit.z * m[7]) + gShatterHit.y * m[4];
    a2 = (gShatterHit.x * m[2] + gShatterHit.z * m[8]) + gShatterHit.y * m[5];
    gShatterHit.x = a0;
    gShatterHit.y = a1;
    gShatterHit.z = a2;
    gShatterPolys = g->polys;
    gShatterColor = gShardColor2;
    gShatterTask = g;
    gShatterSpeed = k;
    gShatterShards = g->shards;
    g->count = 0;
    for (i = 0; i < mesh->count; i++) {
        uint16_t *t = mesh->tris[i];
        float *p0 = mesh->verts[t[0]], *p1 = mesh->verts[t[1]], *p2 = mesh->verts[t[2]];
        long double u0 = p2[0] - p0[0], u1 = p2[1] - p0[1], u2 = p2[2] - p0[2];
        float w0 = p1[0] - p0[0], w1 = p1[1] - p0[1];
        long double w2 = p1[2] - p0[2], nz;
        float nx, ny;

        nx = w2 * u1 - w1 * u2;
        ny = -(w2 * u0 - w0 * u2);
        nz = u0 * w1 - w0 * u1;
        if (!(((pos->z - from->z) * nz + (pos->y - from->y) * ny) + (pos->x - from->x) * nx >= kZero))
            GlassShatter_AddTri(p0, p1, p2);
    }
    g->mesh = PolyList_New(Game_GetWorld(), 0, pos, matrix, g->polys, g->count, 0, 1, 0, 0, 0);
    GlassShatter_PlaySound(pos);
}
HOOK(0x44BD70, GlassShatter_SpawnFrom);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 53C8F0, 4) const float store_53C8F0[12] INIT({0.7f, 0.7f, 0.7f, 1.7859847e+31f, 2.6785361e+20f, 4.5448224e+30f, 0.0f, 1.7108323e+25f, 15.276952f, 8.944435e-39f, 2.6797955e+20f, 1.54143e-43f});
ALIAS(gShardColor2, store_53C8F0, 0);
BSS(57C270) Vec3f gShatterHit;
