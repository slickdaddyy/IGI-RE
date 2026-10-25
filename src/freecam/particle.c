#pragma GCC optimize("excess-precision=standard")

#include "freecam/freecam.h"
#include "transctx/transctxtypes.h"
#include "bonemesh/bonemeshvars.h"
#include "math/mathvars.h"
#include "sound/soundvars.h"

/* Particle systems that fill a cube of cells around the camera (rain, snow,
   dust); spawn() is called for every visible cell corner each frame. */

typedef struct ParticleSystem {
    Task task;
    char unk20[0x4C];
    int radius;
    uint32_t cellMask;
    int unk74;
    int cellSize;
    void *buffer;
    void *world;
    float cullRadius;
    Vec3d lastCam;
    float move[3];
    void (*onMove)(struct ParticleSystem *ps);
    void (*spawn)(struct ParticleSystem *ps, Vec3d *pos);
    char reset;
    char unkB5[3];
} ParticleSystem;

_Static_assert(sizeof(ParticleSystem) == 0xB8, "ParticleSystem");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));

extern uint16_t gParticleSystemType;
extern uint16_t gSpriteParticleSystemType;
extern int gSpriteParticleSystemReady;
extern char gParticleRandom[];

void ParticleSystem_Draw(ParticleSystem *ps);
void ParticleSystem_Delete(ParticleSystem *ps);
int ParticleSystem_IsVisible(const Vec3d *p, float radius);

void ParticleSystem_Init(void)
{
    gParticleSystemType = TaskType_New("ParticleSystemDynCubeObj", sizeof(ParticleSystem), gDynCubeObjType, 0, 0);
    TaskType_SetHandler(gParticleSystemType, 4, ParticleSystem_Draw);
    TaskType_SetHandler2(gParticleSystemType, 2, ParticleSystem_Delete);
    Random_Init(gParticleRandom);
}
HOOK(0x530C80, ParticleSystem_Init);

void ParticleSystem_Draw(ParticleSystem *ps)
{
    void (*onMove)(ParticleSystem *);
    uint32_t mask;
    int ix, iy, iz, cell, count, off, x, y, z, a, b, c;

    if (ps->reset) {
        memcpy(&ps->lastCam, &gTransContext.pos, sizeof(Vec3d));
        ps->reset = 0;
    }
    ps->move[0] = gTransContext.pos.x - ps->lastCam.x;
    onMove = ps->onMove;
    ps->move[1] = gTransContext.pos.y - ps->lastCam.y;
    ps->move[2] = gTransContext.pos.z - ps->lastCam.z;
    memcpy(&ps->lastCam, &gTransContext.pos, sizeof(Vec3d));
    if (onMove)
        onMove(ps);
    mask = ~ps->cellMask;
    ix = fc_fistp(gTransContext.pos.x) & mask;
    iy = fc_fistp(gTransContext.pos.y) & mask;
    iz = fc_fistp(gTransContext.pos.z) & mask;
    cell = ps->cellSize;
    count = ps->radius * 2 - 1;
    if (count <= 0)
        return;
    off = ps->radius * cell;
    x = ix + off;
    for (a = count; a; a--) {
        double px = x;

        y = off + iy;
        for (b = count; b; b--) {
            double py = y;

            z = cell / 2 + off + iz;
            for (c = count; c; c--) {
                Vec3d p, q;

                p.x = px;
                p.y = py;
                p.z = z;
                if (ParticleSystem_IsVisible(&p, ps->cullRadius)) {
                    q.x = px;
                    q.y = py;
                    q.z = z;
                    ps->spawn(ps, &q);
                }
                z -= cell;
            }
            y -= cell;
        }
        x -= cell;
    }
}
HOOK(0x530CD0, ParticleSystem_Draw);

/* the sphere at p against the camera view pyramid */
int ParticleSystem_IsVisible(const Vec3d *p, float radius)
{
    const float *m = gTransContext.matrix.m;
    long double dx = p->x - gTransContext.pos.x, ax, cz;
    double dy = p->y - gTransContext.pos.y;
    double dz = p->z - gTransContext.pos.z;
    float sx, sy, sz, by;

    sx = m[2] * dz + m[1] * dy + m[0] * dx;
    sy = m[5] * dz + m[4] * dy + m[3] * dx;
    sz = m[8] * dz + m[7] * dy + m[6] * dx;
    ax = (!(sx >= kZero) ? -(long double)sx : sx) - radius;
    by = (!(sy >= kZero) ? -(long double)sy : sy) - radius;
    cz = (long double)sz + radius;
    sz = cz;
    if (!(cz >= kZero))
        return 0;
    if (ax >= kZero && ax > gTransContext.fov * sz)
        return 0;
    if (by >= kZero && !(gTransContext.fovY * sz >= by))
        return 0;
    return 1;
}
HOOK(0x530F20, ParticleSystem_IsVisible);

void ParticleSystem_Delete(ParticleSystem *ps)
{
    DynCube_ListRemove(ps->world, ps, 0);
    Mem_Free(ps->buffer);
    ps->buffer = NULL;
}
HOOK(0x531090, ParticleSystem_Delete);

void ParticleSystem_Shutdown(void)
{
    TaskType_Free(gParticleSystemType);
}
HOOK(0x5310C0, ParticleSystem_Shutdown);

uint16_t ParticleSystem_GetType(void)
{
    return gParticleSystemType;
}
HOOK(0x5310D0, ParticleSystem_GetType);

void SpriteParticleSystem_Init(void)
{
    gSpriteParticleSystemType = TaskType_New("SpriteParticleSystemDynCubeObj", 0xD8, ParticleSystem_GetType(), 0, 0);
    gSpriteParticleSystemReady = 1;
}
HOOK(0x531190, SpriteParticleSystem_Init);

void SpriteParticleSystem_Shutdown(void)
{
    TaskType_Free(gSpriteParticleSystemType);
    gSpriteParticleSystemReady = 0;
}
HOOK(0x5311C0, SpriteParticleSystem_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54F7B0) uint16_t gParticleSystemType INIT(385);
PAD(igd, 54F7B0, 54F7B2, 0x2);
STORE(igb, A845F8, 1) char store_A845F8[16];
ALIAS(gParticleRandom, store_A845F8, 0);
BSS(A845F8) uint16_t gSpriteParticleSystemType;
PAD(igb, A845F8, A8460A, 0x6);
BSS(A845F8) int gSpriteParticleSystemReady;
