#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "levelflow/levelflow.h"

ORIG(0x4017C0, int, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x4148F0, long double, Vec3d_PitchAngle, (Vec3d v));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x4AF8F0, void, List_AddTail, (void *list, void *node));
ORIG(0x4AF960, void, List_Remove, (void *node));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4C1790, void, QTaskList_Add, (QTaskList *list, void *task));
ORIG(0x4C17C0, void, QTaskList_Remove, (QTaskList *list, void *task));
ORIG(0x4C1800, QTaskList *, QTaskList_New, (int max));
/* line of sight from the viewer to the target */
ORIG(0x489B20, char, AIDetection_IsVisible, (Task *self, void *view, Task *target, Vec3d *pos));

#define AIDETECT_MAX 0xC0
#define AIDETECT_NO_TYPE 0x181

/* what a detectable task answers for AIDetection event 1 */
typedef struct AIDetectInfo {
    uint32_t mask;
    int unk4;
    Vec3d pos;
    int team;
    char alive;
    char valid;
    char unk26[2];
} AIDetectInfo;

/* the eye of a detector: two view cones, the second optional */
typedef struct AIView {
    Vec3d pos;
    float m[9];
    int unk3C;
    double maxPitch;
    double maxYaw;
    double range;
    double maxPitch2;
    double maxYaw2;
    double range2;
    char hasSecond;
} AIView;

/* one per task on the detection list, on the friends or enemies list
   while active */
typedef struct AIDetectSlot {
    struct AIDetectSlot *next;
    struct AIDetectSlot *prev;
    Task **ref;
    int unkC;
    int viewTimer;
    int sightTimer;
    int since;
    int unk1C;
    double dist;
    char inView;
    char seen;
    char active;
    char unk2B[5];
} AIDetectSlot;

typedef struct AIDetector {
    void *unk0;
    int viewInterval;
    int sightInterval;
    int phase;
    uint32_t flags;
    uint32_t mask;
    List enemies;
    List friends;
    AIDetectSlot slots[AIDETECT_MAX];
} AIDetector;

_Static_assert(sizeof(AIDetectInfo) == 0x28, "AIDetectInfo");
_Static_assert(offsetof(AIView, hasSecond) == 0x70, "AIView");
_Static_assert(sizeof(AIDetectSlot) == 0x30, "AIDetectSlot");
_Static_assert(offsetof(AIDetector, slots) == 0x30, "AIDetector");

extern QTaskList *gAIDetectList;
extern uint16_t gAIDetectMaskTypes[];
extern int gAIDetectPhase;
extern int gAIDetectUnk5C8B0C;
extern int gAIDetectUnk5C8B10;
extern int gAIDetectUnk5C8B14;
extern uint8_t gAIDetectEvent0;
extern uint8_t gAIDetectEvent1;
extern uint8_t gAIDetectEvent2;
extern float kZero;

void AIDetector_RemoveSlot(AIDetectSlot *s)
{
    TaskRef_Remove(s->ref);
    s->active = 0;
    List_Remove(s);
}
HOOK(0x4894E0, AIDetector_RemoveSlot);

AIDetectSlot *AIDetector_AddSlot(AIDetector *d, List *list, Task *task, int i)
{
    AIDetectSlot *s = &d->slots[i];

    s->ref = TaskRef_Add(task);
    s->viewTimer = 0;
    s->sightTimer = 0;
    s->since = Flow_GetFrames();
    s->dist = 0.0;
    s->inView = 0;
    s->seen = 0;
    s->active = 1;
    s->prev = NULL;
    s->next = NULL;
    List_AddTail(list, s);
    return s;
}
HOOK(0x489500, AIDetector_AddSlot);

void AIDetection_Init(void)
{
    int i;

    gAIDetectUnk5C8B0C = 0;
    gAIDetectUnk5C8B10 = 0;
    gAIDetectUnk5C8B14 = 0;
    gAIDetectList = QTaskList_New(AIDETECT_MAX);
    for (i = 0; i < 32; i++)
        gAIDetectMaskTypes[i] = AIDETECT_NO_TYPE;
    gAIDetectEvent0 = TaskEvent_Alloc(0);
    gAIDetectEvent1 = TaskEvent_Alloc(0);
    gAIDetectEvent2 = TaskEvent_Alloc(0);
}
HOOK(0x489560, AIDetection_Init);

void AIDetection_Shutdown(void)
{
    TaskEvent_Free(gAIDetectEvent0);
    TaskEvent_Free(gAIDetectEvent1);
    TaskEvent_Free(gAIDetectEvent2);
    QTaskList_Free(gAIDetectList);
}
HOOK(0x4895D0, AIDetection_Shutdown);

uint8_t AIDetection_GetEvent0(void)
{
    return gAIDetectEvent0;
}
HOOK(0x489610, AIDetection_GetEvent0);

uint8_t AIDetection_GetEvent1(void)
{
    return gAIDetectEvent1;
}
HOOK(0x489620, AIDetection_GetEvent1);

uint8_t AIDetection_GetEvent2(void)
{
    return gAIDetectEvent2;
}
HOOK(0x489630, AIDetection_GetEvent2);

/* gives a task type a bit of the detection mask */
int AIDetection_AddMaskType(uint16_t type)
{
    int i;

    for (i = 0; i < 32; i++) {
        if (gAIDetectMaskTypes[i] == AIDETECT_NO_TYPE) {
            gAIDetectMaskTypes[i] = type;
            return 1 << i;
        }
    }
    Warning_Show("AIDetection - QTask mask is full");
    return 0;
}
HOOK(0x489640, AIDetection_AddMaskType);

void AIDetection_Add(void *task)
{
    QTaskList_Add(gAIDetectList, task);
}
HOOK(0x489680, AIDetection_Add);

void AIDetection_Remove(void *task)
{
    QTaskList_Remove(gAIDetectList, task);
}
HOOK(0x4896A0, AIDetection_Remove);

/* the two intervals are in frames; each detector gets its own frame of
   every four */
void AIDetector_Init(AIDetector *d, void *unk0, uint32_t flags, uint32_t mask, int viewInterval,
                     int sightInterval)
{
    int i;

    d->unk0 = unk0;
    d->flags = flags;
    d->mask = mask;
    d->phase = gAIDetectPhase;
    gAIDetectPhase = (gAIDetectPhase + 1) % 4;
    d->viewInterval = viewInterval / 4;
    d->sightInterval = sightInterval / 4;
    d->enemies.head = &d->enemies.tail;
    d->enemies.tail = NULL;
    d->enemies.tailpred = &d->enemies.head;
    d->friends.head = &d->friends.tail;
    d->friends.tail = NULL;
    d->friends.tailpred = &d->friends.head;
    for (i = 0; i < AIDETECT_MAX; i++)
        d->slots[i].active = 0;
}
HOOK(0x4896C0, AIDetector_Init);

void AIDetector_UpdateSlot(AIDetector *d, AIDetectSlot *s, Vec3d *pos, AIView *view, Task *self);

/* flags: bits 0/1 see alive/dead team mates, bits 2/3 alive/dead enemies */
void AIDetector_Update(AIDetector *d, AIView *view, Task *self, int team)
{
    int n = gAIDetectList->count;
    int i;

    if (d->phase != Flow_GetFrames() % 4)
        return;
    for (i = 0; i < n; i++) {
        AIDetectSlot *s = &d->slots[i];
        Task *t = gAIDetectList->items[i];
        AIDetectInfo info;
        List *list;

        if (t == self)
            goto drop;
        HANDLER(AIDetection_GetEvent1(), t->type)(t, &info);
        if (!info.valid || !(info.mask & d->mask))
            goto drop;
        if (info.team == team) {
            if (!(d->flags & 3))
                goto drop;
            if (info.alive ? !(d->flags & 1) : !(d->flags & 2))
                goto drop;
            list = &d->friends;
        } else {
            if (!(d->flags & 0xC))
                goto drop;
            if (info.alive ? !(d->flags & 4) : !(d->flags & 8))
                goto drop;
            list = &d->enemies;
        }
        if (!s->active) {
            AIDetector_AddSlot(d, list, t, i);
        } else if (*s->ref != t) {
            AIDetector_RemoveSlot(s);
            AIDetector_AddSlot(d, list, t, i);
        }
        AIDetector_UpdateSlot(d, s, &info.pos, view, self);
        continue;
    drop:
        if (s->active)
            AIDetector_RemoveSlot(s);
    }
}
HOOK(0x489750, AIDetector_Update);

char AIView_Contains(AIView *v, Vec3d *p);

void AIDetector_UpdateSlot(AIDetector *d, AIDetectSlot *s, Vec3d *pos, AIView *view, Task *self)
{
    if (!s->viewTimer) {
        if (AIView_Contains(view, pos)) {
            s->inView = 1;
        } else {
            s->inView = 0;
            s->seen = 0;
        }
        s->viewTimer = d->viewInterval + 1;
    }
    if (s->inView) {
        if (!s->sightTimer) {
            s->seen = AIDetection_IsVisible(self, view, *s->ref, pos);
            s->sightTimer = d->sightInterval + 1;
        }
        s->sightTimer--;
    }
    s->viewTimer--;
    s->dist = Vec3d_Distance(&view->pos, pos);
}
HOOK(0x4898D0, AIDetector_UpdateSlot);

char AIView_Contains(AIView *v, Vec3d *p)
{
    long double dx = p->x - v->pos.x;
    long double dy = p->y - v->pos.y;
    long double dz = p->z - v->pos.z;
    Vec3d l;
    double len;
    float pitch;
    long double yaw;

    l.x = v->m[6] * dz + v->m[3] * dy + v->m[0] * dx;
    l.y = v->m[7] * dz + v->m[4] * dy + v->m[1] * dx;
    l.z = v->m[8] * dz + v->m[5] * dy + v->m[2] * dx;
    len = Vec3d_Length(&l);
    if (!(Vec3d_PitchAngle(l) >= kZero))
        pitch = -Vec3d_PitchAngle(l);
    else
        pitch = Vec3d_PitchAngle(l);
    if (!(Vec3d_YawAngle(l) >= kZero))
        yaw = -Vec3d_YawAngle(l);
    else
        yaw = Vec3d_YawAngle(l);
    if (!(len > v->range) && !(pitch >= v->maxPitch) && !(yaw >= v->maxYaw))
        return 1;
    if (v->hasSecond && !(len > v->range2) && !(pitch >= v->maxPitch2) && !(yaw >= v->maxYaw2))
        return 1;
    return 0;
}
HOOK(0x489960, AIView_Contains);
