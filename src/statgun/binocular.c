#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"
#include "binocular/binoculartypes.h"

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
    o->dir = gViewUnkBCAAF8;
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

    o->matrix = v->cam;
    Matrix_SetEuler(&r, kTurn, 0.0f, 0.0f);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            m.m[i * 3 + j] = a[i * 3] * r.m[j] + a[i * 3 + 2] * r.m[6 + j] + a[i * 3 + 1] * r.m[3 + j];
    m.count = o->matrix.count + r.count + 1;
    o->matrix = m;
    o->pos = *(Vec3d *)v->eye;
    MOV32(o->fov, v->fovX);
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
    o->dir = *v;
}
HOOK(0x472650, BinocularOverlay_SetUnk7C);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BE348) void *gBinocularHolderParams;
