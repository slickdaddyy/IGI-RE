#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"

/* the binocular view drawn over the screen ("BinocularOverlay", 0x2E00 bytes) */
typedef struct BinocularOverlay {
    Task task;
    char unk20[0x18];
    Matrix matrix;      /* 0x38 */
    Vec3d pos;          /* 0x60 */
    float fov;          /* 0x78 */
    Vec3f unk7C;
} BinocularOverlay;

/* "BinocularOverlayHolder" (0x78 bytes): shows an overlay while "active" is true */
typedef struct BinocularHolder {
    Task task;
    Task **overlay;     /* 0x20 */
    VarString active;   /* 0x24 */
    char isActive;      /* 0x74 */
    char pad75[3];
} BinocularHolder;

_Static_assert(offsetof(BinocularOverlay, unk7C) == 0x7C, "BinocularOverlay");
_Static_assert(sizeof(BinocularHolder) == 0x78, "BinocularHolder");

extern uint16_t gBinocularOverlayType;
extern uint16_t gBinocularHolderType;
extern void *gBinocularHolderParams;
extern Vec3d gViewPos;
extern Vec3f gViewUnkBCAAF8;

ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x471480, void, BinocularOverlay_Draw, (BinocularOverlay *o));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));

void BinocularHolder_GetParams(void *task, void **out)
{
    *out = gBinocularHolderParams;
}
HOOK(0x4723D0, BinocularHolder_GetParams);

void BinocularHolder_Changed(BinocularHolder *h)
{
    VarString_Bind(&h->active, h);
}
HOOK(0x4723E0, BinocularHolder_Changed);

void BinocularOverlay_DrawHandler(BinocularOverlay *o)
{
    o->pos = gViewPos;
    o->unk7C = gViewUnkBCAAF8;
    BinocularOverlay_Draw(o);
}
HOOK(0x472400, BinocularOverlay_DrawHandler);

/* follows the view of the human looking through the binoculars */
void BinocularOverlay_UseHandler(BinocularOverlay *o, WeaponUseMsg *msg)
{
    static const float kTurn = 0x1.2d97c8p+2f; /* 3 pi / 2 */
    HumanView *v = msg->h->view;
    float *a = o->matrix.m;
    Matrix r, m;
    int i, j;

    o->matrix = v->unkF8;
    Matrix_SetEuler(&r, kTurn, 0.0f, 0.0f);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            m.m[i * 3 + j] = a[i * 3] * r.m[j] + a[i * 3 + 2] * r.m[6 + j] + a[i * 3 + 1] * r.m[3 + j];
    m.unk24 = o->matrix.unk24 + r.unk24 + 1;
    o->matrix = m;
    o->pos = v->unkE0;
    MOV32(o->fov, v->fov);
}
HOOK(0x472440, BinocularOverlay_UseHandler);

void BinocularOverlay_Close(void)
{
    QTaskList_Free(gBinocularHolderParams);
    TaskType_Free(gBinocularOverlayType);
    TaskType_Free(gBinocularHolderType);
}
HOOK(0x4725D0, BinocularOverlay_Close);

Task *BinocularOverlay_New(void *parent)
{
    Task *t;

    Mem_PushMode(1);
    t = Task_New(parent, gBinocularOverlayType, NULL);
    Mem_PopMode();
    return t;
}
HOOK(0x472600, BinocularOverlay_New);

void BinocularOverlay_SetPos(BinocularOverlay *o, const Vec3d *pos)
{
    o->pos = *pos;
}
HOOK(0x472630, BinocularOverlay_SetPos);

void BinocularOverlay_SetUnk7C(BinocularOverlay *o, const Vec3f *v)
{
    o->unk7C = *v;
}
HOOK(0x472650, BinocularOverlay_SetUnk7C);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BE348) void *gBinocularHolderParams;
