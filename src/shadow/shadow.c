#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "shadow/shadow.h"

FN_EXTERN(SubLevel_Run);
FN_EXTERN(Task_Nothing);

ORIG(0x4012A0, Task *, Task_New, (Task *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (Task *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x49A450, Task *, PolyList_New, (void *world, void *owner, Vec3d *pos, const Matrix *m, ShadowPoly *polys,
                                      int count, int a, int b, int c, int d, int e));
ORIG(0x4B4010, void, Matrix_SetDirection, (Matrix *m, void *dir));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));

extern uint16_t gMagicObjContainerType;
extern uint16_t gShadowType;
extern float gShadowColor[];
extern Matrix gIdentity;
extern float kShadowFade;
extern float kShadowMinFade;
extern float kShadowDepth;

#define SubLevel_Run (FN(0x488700, SubLevel_Run))
#define sub_4D3710 (FN(0x4D3710, Task_Nothing))

void MagicObjContainer_Event(Task *t, void *arg);

void MagicObjContainer_Open(void)
{
    gMagicObjContainerType = TaskType_New("MagicObjContainer", 0x24, Task_GetRootType(), 0);
    TaskType_SetHandler2(gMagicObjContainerType, 1, SubLevel_Run);
    TaskType_SetHandler(gMagicObjContainerType, RigidDynCubeObj_GetEvent2(), MagicObjContainer_Event);
}
HOOK(0x4E0840, MagicObjContainer_Open);

void MagicObjContainer_Event(Task *t, void *arg)
{
    EACH_CHILD(t, RigidDynCubeObj_GetEvent2(), arg);
}
HOOK(0x4E0890, MagicObjContainer_Event);

void MagicObjContainer_Close(void)
{
    TaskType_Free(gMagicObjContainerType);
}
HOOK(0x4E0920, MagicObjContainer_Close);

Task *MagicObjContainer_New(Task *parent, void *owner)
{
    Task *t = Task_New(parent, gMagicObjContainerType, NULL);

    ((void **)t)[8] = owner;
    return t;
}
HOOK(0x4E0930, MagicObjContainer_New);

void Shadow_MagicObjEnterHandler(Shadow *sh);
void Shadow_Delete(Shadow *sh);

void Shadow_Open(void)
{
    gShadowType = TaskType_New("Shadow", sizeof(Shadow), Task_GetRootType(), 0);
    TaskType_SetHandler(gShadowType, RigidDynCubeObj_GetEvent2(), Shadow_MagicObjEnterHandler);
    TaskType_SetHandler2(gShadowType, 1, sub_4D3710);
    TaskType_SetHandler2(gShadowType, 2, Shadow_Delete);
}
HOOK(0x4E0950, Shadow_Open);

void Shadow_Delete(Shadow *sh)
{
    Task_Delete(sh->polys);
}
HOOK(0x4E09C0, Shadow_Delete);

/* Fades the quad with the owner's height above ground and lays it out
   along the ground direction. Rounding follows the original exactly. */
void Shadow_MagicObjEnterHandler(Shadow *sh)
{
    long double t, n, p0, p1, p2, e2;
    float a0, a1, a2, c0, c1, c2, b0, b1, b2, d0, d1, d2, e0, e1;
    float h;
    Matrix m;
    int i;

    if (!*sh->visible || !*sh->enabled)
        return;
    t = (kOneD - (long double)*sh->height / sh->maxHeight) * kShadowFade;
    if (!(t >= kShadowMinFade))
        t = kShadowMinFade;
    else if (t < kZero)
        t = kZero;
    else if (!(kOne >= t))
        t = kOne;
    for (i = 0; i < 3; i++)
        sh->color[i] = gShadowColor[i];
    for (i = 0; i < 3; i++)
        sh->color[i] = t * sh->color[i];
    Matrix_SetDirection(&m, sh->dir);
    h = sh->size;
    n = -(long double)h;
    c0 = m.m[6] * kShadowDepth;
    a0 = h * m.m[3];
    p0 = (long double)h * m.m[0];
    b0 = p0;
    sh->verts[0][0] = p0 + a0 + c0;
    c1 = m.m[7] * kShadowDepth;
    a1 = h * m.m[4];
    p1 = (long double)h * m.m[1];
    b1 = p1;
    sh->verts[0][1] = p1 + a1 + c1;
    c2 = m.m[8] * kShadowDepth;
    a2 = h * m.m[5];
    p2 = (long double)h * m.m[2];
    b2 = p2;
    sh->verts[0][2] = p2 + a2 + c2;
    p0 = n * m.m[0];
    d0 = p0;
    sh->verts[1][0] = p0 + a0 + c0;
    p1 = n * m.m[1];
    d1 = p1;
    sh->verts[1][1] = p1 + a1 + c1;
    p2 = n * m.m[2];
    d2 = p2;
    sh->verts[1][2] = p2 + a2 + c2;
    e0 = n * m.m[3];
    sh->verts[2][0] = b0 + (long double)e0 + c0;
    e1 = n * m.m[4];
    sh->verts[2][1] = b1 + (long double)e1 + c1;
    e2 = n * m.m[5];
    sh->verts[2][2] = b2 + e2 + c2;
    sh->verts[3][0] = d0 + (long double)e0 + c0;
    sh->verts[3][1] = d1 + (long double)e1 + c1;
    sh->verts[3][2] = d2 + e2 + c2;
    *(Vec3d *)((char *)sh->polys + 0x20) = *sh->pos;
    sh->poly[0].texture = sh->texture;
    sh->poly[1].texture = sh->texture;
    HANDLER(4, sh->polys->type)(sh->polys, 0);
}
HOOK(0x4E09D0, Shadow_MagicObjEnterHandler);

void Shadow_Close(void)
{
    TaskType_Free(gShadowType);
}
HOOK(0x4E0CB0, Shadow_Close);

Shadow *Shadow_New(Task *parent, Vec3d *pos, int *visible, int *enabled, float *height, void *dir,
                   float maxHeight, float size, int texture)
{
    Shadow *sh = (Shadow *)Task_New(parent, gShadowType, NULL);
    ShadowPoly *p;

    sh->world = gCurrentWorld;
    sh->pos = pos;
    sh->visible = visible;
    sh->enabled = enabled;
    sh->height = height;
    sh->dir = dir;
    sh->maxHeight = maxHeight;
    sh->size = size;
    sh->texture = texture;
    sh->polys = PolyList_New(sh->world, NULL, sh->pos, &gIdentity, sh->poly, 2, 0, 0, 0, 0, 0);
    p = &sh->poly[0];
    p->verts[0] = sh->verts[0];
    p->verts[1] = sh->verts[1];
    p->verts[2] = sh->verts[2];
    p->colors[0] = p->colors[1] = p->colors[2] = sh->color;
    p->uv[0] = 0;
    p->uv[1] = 0;
    p->uv[2] = 1.0f;
    p->uv[3] = 0;
    p->uv[4] = 0;
    p->uv[5] = 1.0f;
    p->unk30[0] = p->unk30[1] = p->unk30[2] = 0;
    p->texture = 0;
    p = &sh->poly[1];
    p->verts[0] = sh->verts[1];
    p->verts[1] = sh->verts[3];
    p->verts[2] = sh->verts[2];
    p->colors[0] = p->colors[1] = p->colors[2] = sh->color;
    p->uv[0] = 1.0f;
    p->uv[1] = 0;
    p->uv[2] = 1.0f;
    p->uv[3] = 1.0f;
    p->uv[4] = 0;
    p->uv[5] = 1.0f;
    p->unk30[0] = p->unk30[1] = p->unk30[2] = 0;
    p->texture = 0;
    return sh;
}
HOOK(0x4E0CC0, Shadow_New);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533480) float kShadowDepth INIT(10.0f);
ALIAS(kFlySpeed, kShadowDepth, 0);
ALIAS(kTen, kShadowDepth, 0);
STORE(igd, 548708, 4) float store_548708[4] INIT({0.9f, 0.9f, 0.9f, 0.0f});
ALIAS(gShadowColor, store_548708, 0);
DATA(548708) Matrix gIdentity INIT({.m = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}});
BSS(A5E364) uint16_t gMagicObjContainerType;
PAD(igb, A5E364, A5E366, 0x6);
BSS(A5E364) uint16_t gShadowType;
PAD(igb, A5E364, A5E36E, 0x2);
