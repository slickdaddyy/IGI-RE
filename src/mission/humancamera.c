/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "mission/mission.h"
#include "coll/colltypes.h"
#include "camera/cameratypes.h"
#include "camera/cameravars.h"

ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x483100, void, HumanCamera_LookAt, (Matrix *out, const Vec3d *from, const Vec3d *to));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *hits, Vec3d *starts, Vec3d *ends, int count, uint8_t event,
                                  CollIgnore *filter, int flags));

extern HumanCameraEntry gHumanCameraList[];

#define HUMAN_LIST_SIZE 128

static const double kHeadUp = 4096.0;
static const double kZero = 0.0;
static const double kBackOff = 10240.0;
static const float kRotX = -0x1.921fb6p+0f;

/* looks from behind the human's head at the one who killed it */
void HumanCamera_WatchKiller(HumanCamera *c)
{
    Task *h = *c->ref;
    Task *killer = **(Task ***)((char *)h + 0xDE8);
    Vec3d from, to, head, d;
    CollHit hit;
    CollIgnore filter;
    Matrix m, r, out;
    long double len, k, kz;
    uint8_t event;

    from = *(Vec3d *)((char *)h + 0x20);
    to = *(Vec3d *)((char *)killer + 0x20);
    to.z += kHeadUp;
    head = from;
    head.z = from.z + kHeadUp;
    d.x = to.x - from.x;
    d.y = to.y - from.y;
    d.z = to.z - from.z;
    len = Vec3d_Length(&d);
    if (len > kZero) {
        k = kBackOff / len;
        filter.task = h;
        d.x = k * d.x;
        d.y = k * d.y;
        kz = k * d.z;
        d.z = kz;
        from.x = head.x - d.x;
        from.y = head.y - d.y;
        from.z = head.z - kz;
        event = Game_GetEvent8();
        Coll_Lines(Game_GetWorld(), &hit, &head, &from, 1, event, &filter, -1);
        head = hit.found ? hit.pos : from;
    }
    HumanCamera_LookAt(&m, &head, &to);
    c->pos = head;
    Matrix_SetRotX(&r, kRotX);
    out.m[0] = m.m[2] * r.m[6] + m.m[1] * r.m[3] + m.m[0] * r.m[0];
    out.m[1] = m.m[2] * r.m[7] + m.m[1] * r.m[4] + m.m[0] * r.m[1];
    out.m[2] = m.m[2] * r.m[8] + m.m[1] * r.m[5] + m.m[0] * r.m[2];
    out.m[3] = m.m[5] * r.m[6] + m.m[4] * r.m[3] + m.m[3] * r.m[0];
    out.m[4] = m.m[5] * r.m[7] + m.m[4] * r.m[4] + m.m[3] * r.m[1];
    out.m[5] = m.m[5] * r.m[8] + m.m[4] * r.m[5] + m.m[3] * r.m[2];
    out.m[6] = m.m[8] * r.m[6] + m.m[7] * r.m[3] + m.m[6] * r.m[0];
    out.count = r.count + m.count + 1;
    out.m[7] = m.m[8] * r.m[7] + m.m[7] * r.m[4] + m.m[6] * r.m[1];
    out.m[8] = m.m[8] * r.m[8] + m.m[7] * r.m[5] + m.m[6] * r.m[2];
    c->matrix = out;
}
HOOK(0x4849B0, HumanCamera_WatchKiller);

void HumanCamera_DeleteHandler(HumanCamera *c)
{
    c->ref = NULL;
}
HOOK(0x484CE0, HumanCamera_DeleteHandler);

void HumanCamera_Close(void)
{
    int i;

    TaskEvent_Free(gHumanCameraEvent);
    TaskType_Free(gHumanCameraType);
    for (i = 0; i < HUMAN_LIST_SIZE; i++) {
        if (gHumanCameraList[i].used) {
            Error_Show("HumanCamera humanlist not empty");
            for (;;)
                ;
        }
    }
}
HOOK(0x484CF0, HumanCamera_Close);

uint8_t HumanCamera_GetEvent(void)
{
    return gHumanCameraEvent;
}
HOOK(0x484D30, HumanCamera_GetEvent);

HumanCamera *HumanCamera_New(void *parent, void *camera)
{
    HumanCamera *c = (HumanCamera *)Task_New(parent, gHumanCameraType, NULL);

    c->camera = camera;
    return c;
}
HOOK(0x484D40, HumanCamera_New);

static void list_error(void)
{
    Error_Show("ERROR IN HUMANCAMERA HUMANLIST");
    for (;;)
        ;
}

/* follows human h, which must be in the list */
void HumanCamera_SetHuman(HumanCamera *c, Task *h)
{
    int i;

    for (i = 0; !gHumanCameraList[i].used || *gHumanCameraList[i].ref != h; i++) {
        if (i + 1 >= HUMAN_LIST_SIZE)
            list_error();
    }
    c->ref = gHumanCameraList[i].ref;
    c->savedRef = gHumanCameraList[i].ref;
    c->index = i;
    c->savedIndex = i;
}
HOOK(0x484D60, HumanCamera_SetHuman);

void HumanCamera_AddHuman(Task *h)
{
    int i;

    for (i = 0; i < HUMAN_LIST_SIZE; i++) {
        if (!gHumanCameraList[i].used) {
            gHumanCameraList[i].ref = TaskRef_Add(h);
            gHumanCameraList[i].used = 1;
            return;
        }
    }
}
HOOK(0x484DC0, HumanCamera_AddHuman);

void HumanCamera_RemoveHuman(Task *h)
{
    int i;

    for (i = 0; i < HUMAN_LIST_SIZE; i++) {
        Task *t;

        if (!gHumanCameraList[i].used)
            continue;
        t = *gHumanCameraList[i].ref;
        if (!t)
            list_error();
        if (t == h) {
            TaskRef_Remove(gHumanCameraList[i].ref);
            gHumanCameraList[i].ref = NULL;
            gHumanCameraList[i].used = 0;
            return;
        }
    }
}
HOOK(0x484E00, HumanCamera_RemoveHuman);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, 5C1188, 4) __typeof__(gHumanCameraList[0]) store_5C1188[129];
ALIAS(gHumanCameraList, store_5C1188, 0);
