/* The end of the proximity Mine: run, delete, parameters, trigger, map. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "guncasing/guncasing.h"
#include "guncasing/guncasingvars.h"

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4168B0, void *, Game_GetTaskList1, (void));
ORIG(0x4168F0, void *, Game_GetCutsceneList, (void));
ORIG(0x467DF0, void, ComputerObject_DrawSprite, (MapQuery *q));
ORIG(0x46B4D0, char *, ComputerRes_Get, (void));
ORIG(0x481340, uint16_t, ExplodeObject_GetType, (void));
ORIG(0x481360, void, ExplodeObject_Setup, (void *o, float a, float b, float c, float d, int e, int f, int g,
                                          const char *sound));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, Vec3d *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));

extern void *gMineParams;
extern uint16_t gMineType;

void Mine_ParamChangedHandler(Mine *m, void *arg);

void Mine_RunHandler(Mine *m)
{
    TASK_HANDLER(1, ExplodeObject_GetType())(m);
    if (m->armDelay)
        m->armDelay--;
    if (m->triggered) {
        if (m->countdown)
            m->countdown--;
        else
            m->explode = 1;
    }
    if (m->exploded)
        Task_Delete(m);
}
HOOK(0x475CC0, Mine_RunHandler);

void *Mine_GetList(void);

void Mine_DeleteHandler(Mine *m)
{
    QTaskList_Remove(Game_GetTaskList1(), m);
    QTaskList_Remove(Mine_GetList(), m);
    QTaskList_Remove(Game_GetCutsceneList(), m);
    TASK_HANDLER(2, ExplodeObject_GetType())(m);
}
HOOK(0x475D30, Mine_DeleteHandler);

void Mine_GetParamTableHandler(Mine *m, void **out)
{
    *out = gMineParams;
}
HOOK(0x475D80, Mine_GetParamTableHandler);

void Mine_ParamChangedHandler(Mine *m, void *arg)
{
    uint8_t ev = Param_GetChangedEvent();

    TASK_HANDLER(ev, ExplodeObject_GetType())(m, arg);
    m->countdown = round_int(m->delay * kFps);
}
HOOK(0x475D90, Mine_ParamChangedHandler);

/* starts the countdown when something comes within the radius in front */
void Mine_ProximityHandler(Mine *m, MineProbe *p)
{
    Vec3d d;
    const float *r = m->o.matrix.m;
    long double len, y;

    if (m->remote)
        return;
    d.x = p->pos.x - m->o.pos.x;
    d.y = p->pos.y - m->o.pos.y;
    d.z = p->pos.z - m->o.pos.z;
    len = Vec3d_Length(&d);
    if (!(m->radius * kMeter > len - p->radius))
        return;
    y = (r[7] * d.z + r[4] * d.y) + r[1] * d.x;
    if (!(y > kZeroD))
        return;
    if (m->armDelay || m->triggered)
        return;
    {
        void *snd = Sound_Play(Level_GetDynamic(), "proxy_arm");

        if (snd)
            Sound_SetPos(snd, &m->o.pos);
    }
    m->triggered = 1;
}
HOOK(0x475DF0, Mine_ProximityHandler);

void Mine_MapHandler(Mine *m, MapQuery *q)
{
    float s;

    q->pos = &m->o.pos;
    q->matrix = &m->o.matrix;
    s = max_scale(&m->o);
    q->sprite = NULL;
    q->radius = VirModel_GetRadius(m->o.model) * s;
    if (!m->exploded && m->remote) {
        q->sprite = ComputerRes_Get() + 0x360;
        ComputerObject_DrawSprite(q);
        q->sprite = NULL;
    }
}
HOOK(0x475F30, Mine_MapHandler);

void Mine_Close(void)
{
    QTaskList_Free(gMineParams);
    QTaskList_Free(gMineList);
    TaskType_Free(gMineType);
}
HOOK(0x476000, Mine_Close);

void *Mine_GetList(void)
{
    return gMineList;
}
HOOK(0x476030, Mine_GetList);

Mine *Mine_New(void *parent, const Vec3d *pos, const Matrix *matrix, const char *sound, int armDelay, float delay,
               float radius, float power, char remote)
{
    Mine *m;

    Mem_PushMode(1);
    m = (Mine *)Task_New(parent, gMineType, 0);
    m->placePos = *pos;
    m->placeMatrix = *matrix;
    strcpy(m->model, sound);
    m->delay = delay;
    m->armDelay = armDelay;
    m->radius = radius;
    m->remote = remote;
    ExplodeObject_Setup(m, 1.0f, power, 0, 1.0f, 0, 5, 1, "explo_proxy");
    Mine_ParamChangedHandler(m, NULL);
    Mem_PopMode();
    return m;
}
HOOK(0x476040, Mine_New);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BE3A8) void *gMineList;
