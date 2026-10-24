#include <string.h>

#include "hook.h"
#include "humanai/humanai.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (int unk));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F2070, void, WeaponSlots_Set, (Task **slots, int i, Task *weapon));
ORIG(0x4F9290, void *, Graph_Find, (int id));
ORIG(0x5096F0, void, AIGraph_FindNearest, (int **out, void *graph, const Vec3d *pos));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x413C80, WeaponType *, Weapon_GetType, (Task *weapon));
ORIG(0x415020, int, Game_CheatKeyPressed, (int key));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4028C0, int, Flow_GetRenders, (void));
ORIG(0x406F70, void, HumanAIConfig_Apply, (HumanAI *ai));
ORIG(0x440C00, char, AlarmControl_IsAlarm, (Task *t));
ORIG(0x440C10, char, AlarmControl_IsLastAlarm, (Task *t));
ORIG(0x452D20, void *, NodeGrid_Place, (int *len, void *a, void *b, void *graph, int node, Vec3d *pos));
ORIG(0x453390, void, AIClaim_Release, (void *p));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460C60, QTaskList *, Human_GetList, (void));
ORIG(0x460C70, void, Human_SetAI, (Task *human, HumanAI *ai));
ORIG(0x464250, int, Human_GetMaskType, (void));
ORIG(0x465800, void, HumanView_GetEye, (Vec3d *eye, void *view));
ORIG(0x465880, void, HumanView_GetMatrix, (float *m, void *view));
ORIG(0x465920, void, HumanView_SetTarget, (void *view, int a, int b));
ORIG(0x477BF0, uint16_t, Gun_GetType, (void));
ORIG(0x488610, uint16_t, GuardGenerator_GetType, (void));
ORIG(0x4896C0, void, AIDetector_Init, (AIDetection *d, int team, int a, int b, int c, int e));
ORIG(0x489750, void, AIDetector_Update, (AIDetection *d, AIDetectView *v, Human *h, int team));
ORIG(0x489B90, void, AIDetection_RemoveRefs, (AIDetection *d));
ORIG(0x489BE0, void *, AIDetection_FindSeen, (AIDetection *d, Task *t));
ORIG(0x48A340, float *, Human_GetBonePos, (float *out, Human *h, int unk));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));

extern uint16_t gHumanAIType;
extern void *gHumanAIParams;
extern const char *gAITypeNames[];
extern const char *gAIEventNames[];
extern void *gAIScripts[];

/* what the AI hands its human each frame */
typedef struct AIControlMsg {
    int unk0;
    float *ctrl;
} AIControlMsg;

void HumanAI_Create(HumanAI *ai, void *arg)
{
    int i;
    AIParam *p;

    Human_SetAI(ai->task.parent, ai);
    ai->graphId = -1;
    ai->scriptPath[0] = 0;
    ai->graph = NULL;
    ai->script = NULL;
    ai->detectionActive = 0;
    ai->moves.head = (Node *)&ai->moves.tail;
    ai->moves.tailpred = (Node *)&ai->moves;
    ai->moves.tail = NULL;
    HumanAI_InitEvents(ai);
    ai->actions.head = (Node *)&ai->actions.tail;
    ai->actions.tailpred = (Node *)&ai->actions;
    ai->actions.tail = NULL;
    for (i = 0; i < AI_POOL; i++)
        ai->actionPool[i].used = 0;
    ai->freeParams.head = (Node *)&ai->freeParams.tail;
    ai->freeParams.tailpred = (Node *)&ai->freeParams;
    ai->freeParams.tail = NULL;
    for (i = 0, p = ai->paramPool; i < AI_POOL; i++, p++) {
        p->node.prev = NULL;
        p->node.next = NULL;
        List_AddTail(&ai->freeParams, p);
    }
    ai->unk1514 = 0;
    ai->target = NULL;
    ai->targetSeen = NULL;
    ai->deadDone = 0;
    ai->isDetection = 0;
    ai->alarmed = 0;
    ai->unk3ABC = 0;
    ai->unk3ABD = 0;
    ai->unk3AC0 = -1;
    ai->alarmControlId = -1;
    ai->unk3AC8 = -1;
    ai->unk3ACC = 0;
    ai->unk3AD0 = -1;
    ai->unk3AB0 = NULL;
    ai->unk3AB4 = NULL;
    ai->animTimer = -1;
    ai->animEnabled = 0;
    ai->maxAlpha = 0x1.a8c536p-6f;
    ai->maxGamma = 0x1.a8c536p-6f;
    ai->view.alpha = 0x1.921fb6p-1;
    ai->view.gamma = 0x1.becde6p-1;
    ai->view.length = 122880.0;
    ai->view.alarmLength = 122880.0;
    ai->view.secAlpha = 0x1.921fb6p-1;
    ai->view.secGamma = 0x1.657186p+0;
    ai->view.secLength = 40960.0;
    ai->view.secAlarmLength = 40960.0;
    ai->aiType = 8;
    strcpy(ai->aiTypeExpr, gAITypeNames[8]);
    ai->unk3D40 = 300;
    ai->unk3DB0 = 6144.0;
    ai->unk3DD0 = -1;
    ai->unk3DD4 = 0;
    ai->unk3DF4 = 0;
    ai->unk3DD8 = 0;
    ai->unk3DF8 = -1;
    ai->unk3DDC = 0;
    ai->unk3DE0 = 0;
    ai->hasGun = 0;
    ai->hasGrenade = 0;
    ai->gunId = -1;
    ai->grenadeId = -1;
    ai->unk3DE8 = -1;
    ai->unk3DEC = -1;
    ai->unk3DF0 = 0;
    ai->unk3DF1 = 0;
    ai->path = NULL;
    ai->unk3E10 = ai->unk3D40;
    ai->symbolsAdded = 0;
    ai->started = 0;
    ai->unk3E70 = 0;
    ai->unk3E78 = 0;
    ai->unk3E80 = 0;
    ai->doorWait = 0;
    ai->door = NULL;
    ai->unk3E28 = 0;
    ai->node = -1;
    ai->destNode = -1;
    ai->unk3E48 = 0;
    ai->unk3E3C = -1;
    ai->unk3E44 = -1;
    ai->unk3E50 = 0;
    ai->unk3E54 = 0;
    ai->unk3E58 = 0;
    ai->unk3E5C = -1;
    ai->unk3E60 = 0;
    ai->unk3E64 = 0;
    ai->unk3E69 = 0;
    ai->unk3E6A = 0;
    ai->unk3E6B = 0;
    ai->unk3E74 = 0;
    ai->unk3E7C = 0;
    ai->unk3E84 = 0;
}
HOOK(0x44F060, HumanAI_Create);

void HumanAI_InitEvents(HumanAI *ai)
{
    int i;
    AIEvent *e;

    ai->freeEvents.tailpred = (Node *)&ai->freeEvents;
    ai->freeEvents.head = (Node *)&ai->freeEvents.tail;
    ai->freeEvents.tail = NULL;
    for (i = 0; i < AI_EVENT_TYPES; i++) {
        ai->events[i].head = (Node *)&ai->events[i].tail;
        ai->events[i].tailpred = (Node *)&ai->events[i];
        ai->events[i].tail = NULL;
        ai->eventEnabled[i] = 0;
        ai->eventCounts[i] = 0;
    }
    for (i = 0, e = ai->eventPool; i < AI_POOL; i++, e++) {
        e->node.prev = NULL;
        e->node.next = NULL;
        List_AddTail(&ai->freeEvents, e);
    }
    ai->eventCount = 0;
}
HOOK(0x44F380, HumanAI_InitEvents);

/* runs the script once for an event that is not in the queue */
static char HumanAI_RunFakeEvent(HumanAI *ai, int type, int index)
{
    AIEvent e;
    char r;

    e.node.next = NULL;
    e.node.prev = NULL;
    e.type = type;
    List_AddTail(&ai->events[type], &e);
    r = HumanAI_RunScript(ai, type, index);
    List_Remove(&e);
    return r;
}

static void HumanAI_SendControl(HumanAI *ai)
{
    Task *h = ai->task.parent;
    AIControlMsg m;
    TaskFn fn;

    m.unk0 = -1;
    m.ctrl = ai->ctrl;
    fn = HANDLER(Vehicle_GetControlEvent(), h->type);
    if (fn)
        fn(h, &m);
}

static void HumanAI_SendSeen(HumanAI *ai, int type, Task *t)
{
    AIMessage m;

    m.type = type;
    m.flags = 0;
    m.team = Human_GetTeam(t);
    m.about = t;
    m.from = t;
    m.pos = DynCubeObj_GetPosition(t);
    m.radius = 0;
    HumanAI_Send(&ai->task, &m);
}

void HumanAI_Update(HumanAI *ai, void *arg)
{
    Human *h;
    HumanModel *mdl;
    float tmp[3];
    float *off;
    char alarmed;
    int cheat;
    Node *n, *next;
    AITarget *tg;

    memset(ai->ctrl, 0, sizeof(ai->ctrl));
    alarmed = 0;
    cheat = Game_CheatKeyPressed(0x37);
    ai->ctrl[CTRL_CHEAT] = cheat;
    h = (Human *)ai->task.parent;
    off = Human_GetBonePos(tmp, h, 0);
    ai->view.pos.x = h->pos.x;
    ai->view.pos.y = h->pos.y;
    ai->view.pos.z = h->pos.z - off[2];
    memcpy(ai->view.body, h->matrix, sizeof(ai->view.body));
    HumanView_GetEye(&ai->view.eye, h->view);
    HumanView_GetMatrix(ai->view.view, h->view);
    HumanView_SetTarget(h->view, 0, 0);
    if (!ai->script) {
        Warning_Show("No AI script in HumanAI #%d", Task_GetId(ai));
        return;
    }
    if (!ai->graph) {
        Warning_Show("No AI graph in HumanAI #%d", Task_GetId(ai));
        return;
    }
    if (!ai->started) {
        HumanAI_SendControl(ai);
        HumanAI_FindWeapons(ai);
        ai->started = 1;
        HumanAI_RunFakeEvent(ai, EVENT_CREATE, -1);
        goto run;
    }
    if (HumanAI_IsDead(ai) && !ai->deadDone) {
        int i, index = -1;

        n = ai->actions.head;
        for (next = n->next; next; next = next->next) {
            HumanAI_EndAction(ai, (AIAction *)n);
            n = next;
        }
        for (i = 0; i < ai->eventOrderCount; i++)
            if (ai->eventOrder[i] == EVENT_DEAD)
                index = i;
        HumanAI_RunFakeEvent(ai, EVENT_DEAD, index);
        ai->deadDone = HumanAI_IsDead(ai);
    }
    if (!HumanAI_IsDead(ai)) {
        mdl = h->model;
        ai->dview.eye.x = mdl->eye[0] + h->pos.x;
        ai->dview.eye.y = mdl->eye[1] + h->pos.y;
        ai->dview.eye.z = mdl->eye[2] + h->pos.z;
        memcpy(ai->dview.body, h->model->matrix, sizeof(ai->dview.body));
        memcpy(&ai->dview.alpha, &ai->view.alpha, 8);
        memcpy(&ai->dview.gamma, &ai->view.gamma, 8);
        memcpy(&ai->dview.secAlpha, &ai->view.secAlpha, 8);
        memcpy(&ai->dview.secGamma, &ai->view.secGamma, 8);
        if (ai->alarmed) {
            memcpy(&ai->dview.length, &ai->view.alarmLength, 8);
            memcpy(&ai->dview.secLength, &ai->view.secAlarmLength, 8);
        } else {
            memcpy(&ai->dview.length, &ai->view.length, 8);
            memcpy(&ai->dview.secLength, &ai->view.secLength, 8);
        }
        ai->dview.active = 1;
        AIDetector_Update(&ai->detection, &ai->dview, h, Human_GetTeam(&h->task));

        n = ai->detection.enemies.head;
        for (next = n->next; next; next = next->next) {
            AISeen *s = (AISeen *)n;

            if (s->isNew && *s->ref && !Human_IsDead(*s->ref))
                HumanAI_SendSeen(ai, EVENT_ENEMYDETECTION, *s->ref);
            n = next;
        }
        n = ai->detection.friends.head;
        for (next = n->next; next; next = next->next) {
            AISeen *s = (AISeen *)n;

            if (s->isNew && *s->ref && Human_IsDead(*s->ref))
                HumanAI_SendSeen(ai, EVENT_FRIENDLYDETECTION, *s->ref);
            n = next;
        }

        if (ai->alarmControlId != -1) {
            Task *ac = Task_FindById(ai->alarmControlId);

            if (!ac) {
                Warning_Show("AlarmControl ID #%d not defined", ai->alarmControlId);
                ai->alarmControlId = -1;
            } else if (AlarmControl_IsAlarm(ac)) {
                HumanAI_SendSimple(&ai->task, EVENT_ALARMON, 0);
                alarmed = 1;
            } else if (AlarmControl_IsLastAlarm(ac)) {
                HumanAI_SendSimple(&ai->task, EVENT_ALARMOFF, 0);
            }
        }

        if (ai->animTimer >= 0 && ai->animEnabled) {
            int t = ai->animTimer;

            if (t == 0) {
                HumanAI_SendSimple(&ai->task, EVENT_ANIMATION, 0);
                t = Random_Int(Game_GetRandom(), ai->animRandom) + ai->animBase;
            } else {
                t--;
            }
            ai->animTimer = t;
        }
    }

    tg = ai->target;
    if (tg) {
        Task *t = *tg->ref;

        if (t) {
            ai->targetSeen = AIDetection_FindSeen(&ai->detection, t);
            if (ai->targetSeen) {
                alarmed = 1;
                goto seen;
            }
            tg = ai->target;
        }
        AITarget_Remove(tg, ai, ai->deadDone);
        ai->target = NULL;
    }
seen:
    ai->alarmed = alarmed;
    if (!HumanAI_GetTopAction(ai)) {
        AIMessage m;

        m.type = EVENT_IDLE;
        m.flags = 0;
        m.team = -1;
        m.about = NULL;
        m.from = NULL;
        m.pos = NULL;
        m.radius = 0;
        HumanAI_Send(&ai->task, &m);
    }
    Debug_Log(ai, -1, "Events: %d", ai->eventCount);

run:
    HumanAI_ProcessEvents(ai);
    HumanAI_RunAction(ai);
    if (Human_IsActive(&h->task))
        HumanAI_SendControl(ai);
    if (HumanAI_IsDead(ai) && Human_IsActive(&h->task) && h->unk33C) {
        HumanAI_ClearMove(ai);
        if (ai->unk3AB0) {
            AIClaim_Release(ai->unk3AB0);
            ai->unk3AB0 = NULL;
        }
        if (ai->unk3AB4) {
            AIClaim_Release(ai->unk3AB4);
            ai->unk3AB4 = NULL;
        }
        if (ai->path) {
            NodeGrid_Release(ai->path, ai->pathLen, ai->pathLen);
            ai->path = NULL;
        }
        if (ai->unk3E10 < 0) {
            if (h->task.parent->type == GuardGenerator_GetType() || h->unk7C4 == -1 || Flow_GetRenders() - h->unk7C4 > 1) {
                void *world = Game_GetWorld();
                TaskFn fn = HANDLER(World_GetEvent1(), h->task.type);

                if (fn)
                    fn(h, &world);
            }
        } else {
            ai->unk3E10--;
        }
    }
}
HOOK(0x44F410, HumanAI_Update);

/* finds the gun and the grenades among the human's children */
void HumanAI_FindWeapons(HumanAI *ai)
{
    Task *h = ai->task.parent;
    Task *w = h->head;

    if (!w->next || !w)
        return;
    do {
        if (TaskType_IsA(w->type, Gun_GetType())) {
            WeaponType *wt = Weapon_GetType(w);

            if (!wt) {
                Warning_Show("Weapon not initialized, human #%d", Task_GetId(h));
            } else if (wt->kind >= 0) {
                if (wt->kind <= 2) {
                    ai->hasGun = 1;
                    ai->gunId = wt->id;
                    WeaponSlots_Set(((Human *)h)->weapons, 0, w);
                } else if (wt->kind == 4) {
                    PhysType *pt = PhysicsObjType_Find(wt->name);

                    ai->hasGrenade = 1;
                    ai->grenadeTime = round_int(*pt->time * 30.0f);
                    ai->grenadeId = wt->id;
                }
            }
        }
        w = w->next;
    } while (w && w->next && w);
}
HOOK(0x44FBA0, HumanAI_FindWeapons);

/* runs the AI script for one event type; returns 1 when it pushed actions */
char HumanAI_RunScript(HumanAI *ai, int type, int index)
{
    List actions;
    Node *n, *next;
    int count;
    char r;

    actions.head = (Node *)&actions.tail;
    actions.tailpred = (Node *)&actions;
    gAIActionList = &actions;
    gAIEventList = &ai->events[type];
    gAIEventIndex = index;
    gAICurrent = ai;
    r = 0;
    actions.tail = NULL;
    Script_Run(ai->script);
    count = List_Count(&actions);
    if (count > 1) {
        HumanAI_StartActions(gAICurrent, &actions);
        return 1;
    }
    if (count == 1) {
        n = actions.head;
        next = n->next;
        if (next) {
            r = 1;
            do {
                List_Remove(n);
                HumanAI_StartAction(gAICurrent, (AIAction *)n);
                n = next;
                next = next->next;
            } while (next);
        }
    }
    return r;
}
HOOK(0x44FC90, HumanAI_RunScript);

void HumanAI_ProcessEvents(HumanAI *ai)
{
    int limit = AI_EVENT_TYPES;
    char done = 0;
    int i;

    if (!ai->script)
        return;
    if (HumanAI_GetTopAction(ai))
        limit = HumanAI_GetTopAction(ai)->event;
    for (i = 0; i < ai->eventOrderCount; i++) {
        int t = ai->eventOrder[i];
        Node *e, *next;

        if (i < limit && List_Count(&ai->events[t]) && !done)
            done = HumanAI_RunScript(ai, t, i);
        ai->eventCounts[t] = 0;
        e = ai->events[t].head;
        for (next = e->next; next; next = next->next) {
            HumanAI_FreeEvent(ai, (AIEvent *)e);
            ai->eventCounts[t]++;
            e = next;
        }
    }
}
HOOK(0x44FD50, HumanAI_ProcessEvents);

void HumanAI_FreeEvent(HumanAI *ai, AIEvent *e)
{
    List_Remove(e);
    if (e->about) {
        TaskRef_Remove(e->about);
        e->about = NULL;
    }
    if (e->from) {
        TaskRef_Remove(e->from);
        e->from = NULL;
    }
    e->node.prev = NULL;
    e->node.next = NULL;
    List_AddTail(&ai->freeEvents, e);
    ai->eventCount--;
}
HOOK(0x44FE50, HumanAI_FreeEvent);

/* runs the first action that has an update; counts down the delay of one waiting action */
void HumanAI_RunAction(HumanAI *ai)
{
    char counted = 0;
    AIAction *a = (AIAction *)ai->actions.head;
    Node *next;

    for (next = a->node.next; next; next = next->next) {
        if (a->update) {
            Debug_Log(ai, -1, "Action pushed by: %s", gAIEventNames[ai->eventOrder[a->event]]);
            a->update(ai, a);
            return;
        }
        if (a->delay && !counted) {
            counted = 1;
            a->delay--;
        }
        a = (AIAction *)next;
    }
    Debug_Log(ai, -1, "No items on actionstack was run");
}
HOOK(0x44FEC0, HumanAI_RunAction);

void HumanAI_ClearMove(HumanAI *ai)
{
    HumanAI_ClearMoveQueue(ai);
    if (ai->doorWait) {
        if (ai->door) {
            TaskRef_Remove(ai->door);
            ai->door = NULL;
        }
        ai->doorWait = 0;
    }
}
HOOK(0x44FF40, HumanAI_ClearMove);

void HumanAI_ClearMoveQueue(HumanAI *ai)
{
    Node *n = ai->moves.head, *next;

    for (next = n->next; next; next = next->next) {
        List_Remove(n);
        n->prev = NULL;
        n->next = NULL;
        List_AddTail(&gAIMoveFree, n);
        n = next;
    }
}
HOOK(0x44FF80, HumanAI_ClearMoveQueue);

void HumanAI_Destroy(HumanAI *ai, void *arg)
{
    Node *n, *next;

    if (ai->door) {
        TaskRef_Remove(ai->door);
        ai->door = NULL;
    }
    if (ai->script)
        Script_Free(ai->script);
    if (ai->path)
        NodeGrid_Release(ai->path, ai->pathLen, ai->pathLen);
    HumanAI_ClearMove(ai);
    HumanAI_FreeEvents(ai);
    if (ai->unk3AB0)
        AIClaim_Release(ai->unk3AB0);
    if (ai->unk3AB4)
        AIClaim_Release(ai->unk3AB4);
    if (Task_GetId(ai) >= 0) {
        TaskFn fn = gTaskHandlers[Level_GetEventDAC()][ai->task.type];

        if (fn)
            ((void (*)(void *))fn)(ai);
    }
    n = ai->actions.head;
    for (next = n->next; next; next = next->next) {
        HumanAI_EndAction(ai, (AIAction *)n);
        n = next;
    }
    if (ai->detectionActive)
        AIDetection_RemoveRefs(&ai->detection);
}
HOOK(0x44FFC0, HumanAI_Destroy);

void HumanAI_FreeEvents(HumanAI *ai)
{
    int i;

    for (i = 0; i < AI_EVENT_TYPES; i++) {
        Node *e = ai->events[i].head, *next;

        for (next = e->next; next; next = next->next) {
            HumanAI_FreeEvent(ai, (AIEvent *)e);
            e = next;
        }
    }
    ai->eventCount = 0;
}
HOOK(0x4500C0, HumanAI_FreeEvents);

/* the AI event handler: filters the message and queues it for the script */
void HumanAI_OnEvent(HumanAI *ai, AIMessage *m)
{
    unsigned f = m->flags;
    char ok = 1;
    Task *h = ai->task.parent;
    List *list;
    AIEvent *e;

    if (ai->deadDone || !Human_IsActive(h) || !ai->script)
        return;
    if (h == m->about || h == m->from)
        return;
    if (!ai->started) {
        if (m->type != EVENT_CREATE)
            return;
    } else if (!ai->eventEnabled[m->type]) {
        return;
    }
    list = &ai->events[m->type];
    if (f) {
        if (f & AIMSG_IN_RADIUS) {
            f &= ~AIMSG_IN_RADIUS;
            if (!(m->radius >= Vec3d_Distance(m->pos, &ai->view.pos)))
                ok = 0;
        }
        if ((f & AIMSG_IN_VIEW) && ok) {
            f &= ~AIMSG_IN_VIEW;
            if (!AIView_CanSee(&ai->view, m->pos, ai->alarmed))
                ok = 0;
        }
        if ((f & AIMSG_HUMAN) && ok) {
            f &= ~AIMSG_HUMAN;
            if (!m->about || !TaskType_IsA(m->about->type, Human_GetType()))
                ok = 0;
        }
        if ((f & (AIMSG_HUMAN | AIMSG_ALIVE)) && ok) {
            f &= ~(AIMSG_HUMAN | AIMSG_ALIVE);
            if (Human_IsDead(m->about))
                ok = 0;
        }
        if ((f & (AIMSG_HUMAN | AIMSG_DEAD)) && ok) {
            f &= ~(AIMSG_HUMAN | AIMSG_DEAD);
            if (!Human_IsDead(m->about))
                ok = 0;
        }
        if ((f & (AIMSG_HUMAN | AIMSG_SAME_TEAM)) && ok) {
            f &= ~(AIMSG_HUMAN | AIMSG_SAME_TEAM);
            if (m->team != Human_GetTeam(h))
                ok = 0;
        }
        if (f & (AIMSG_HUMAN | AIMSG_OTHER_TEAM)) {
            if (!ok || m->team == Human_GetTeam(h))
                return;
        }
        if (!ok)
            return;
    }
    e = HumanAI_NewEvent(ai, m);
    if (e)
        List_AddTail(list, e);
}
HOOK(0x450110, HumanAI_OnEvent);

AIEvent *HumanAI_NewEvent(HumanAI *ai, AIMessage *m)
{
    AIEvent *e = (AIEvent *)ai->freeEvents.head;

    if (!e->node.next || !e)
        return NULL;
    List_Remove(e);
    e->node.prev = NULL;
    e->node.next = NULL;
    e->type = m->type;
    e->team = m->team;
    if (m->pos)
        e->pos = *m->pos;
    e->about = m->about ? TaskRef_Add(m->about) : NULL;
    e->from = m->from ? TaskRef_Add(m->from) : NULL;
    ai->eventCount++;
    return e;
}
HOOK(0x450520, HumanAI_NewEvent);

void HumanAI_GetParams(HumanAI *ai, void **out)
{
    *out = gHumanAIParams;
}
HOOK(0x4505C0, HumanAI_GetParams);

void HumanAI_ParamsChanged(HumanAI *ai)
{
    int mask;

    gAICurrent = ai;
    ai->aiType = Script_EvalExpr(ai->aiTypeExpr);
    if (ai->script) {
        Script_Free(ai->script);
        ai->script = NULL;
    }
    if (Task_GetId(ai) != -1) {
        crt_sprintf(ai->scriptPath, "MISSION:AI/%d.qsc", Task_GetId(ai));
        ai->script = Script_Load(ai->scriptPath);
        ai->eventOrderCount = 0;
    }
    if (ai->graphId != -1) {
        ai->graph = Graph_Find(ai->graphId);
        if (!ai->graph) {
            Warning_Show("HumanAI #%d has AIGraph ID (#%d) but no matching AIGraph was found", Task_GetId(ai),
                         ai->graphId);
        } else {
            int *near;
            Vec3d *pos = DynCubeObj_GetPosition(ai->task.parent);

            AIGraph_FindNearest(&near, ai->graph, pos);
            if (!near) {
                Warning_Show("HumanAI #%d couldn't find closest node in graph #%d", Task_GetId(ai), ai->graphId);
            } else {
                if (ai->node != *near) {
                    pos = DynCubeObj_GetPosition(ai->task.parent);
                    ai->path = NodeGrid_Place(&ai->pathLen, ai->pathA, ai->pathB, ai->graph, *near, pos);
                }
                ai->node = *near;
            }
        }
    }
    mask = Human_GetMaskType();
    AIDetector_Init(&ai->detection, Human_GetTeam(ai->task.parent), 6, mask, 4, 8);
    ai->detectionActive = 1;
    HumanAIConfig_Apply(ai);
}
HOOK(0x4505D0, HumanAI_ParamsChanged);

void HumanAI_AddSymbols(HumanAI *ai)
{
    char name[64];
    char buf[128];

    if (!ai->symbolsAdded) {
        ai->symbolsAdded = 1;
        Task_GetName(name, ai);
        crt_sprintf(buf, "%s.isDetection", name);
        Script_RegisterBool8(buf, &ai->isDetection);
    }
}
HOOK(0x450770, HumanAI_AddSymbols);

void HumanAI_RemoveSymbols(HumanAI *ai)
{
    char name[64];
    char buf[128];

    if (ai->symbolsAdded) {
        Task_GetName(name, ai);
        ai->symbolsAdded = 0;
        crt_sprintf(buf, "%s.isDetection", name);
        Script_RemoveSymbol(buf);
    }
}
HOOK(0x4507D0, HumanAI_RemoveSymbols);

void HumanAI_Shutdown(void)
{
    int i;

    QTaskList_Free(gHumanAIParams);
    TaskType_Free(gHumanAIType);
    if (gAIEventSlot != 0xFF)
        TaskEvent_Free(gAIEventSlot);
    if (gAIDoorSlot != 0xFF)
        TaskEvent_Free(gAIDoorSlot);
    if (gAIEvent3Slot != 0xFF)
        TaskEvent_Free(gAIEvent3Slot);
    for (i = 0; i < AI_TARGETS; i++)
        if (gAITargetPool[i].used)
            TaskRef_Remove(gAITargetPool[i].ref);
    HumanAI_UnregisterConstants();
    HumanAI_UnregisterFunctions();
    for (i = 0; i < 9; i++)
        if (gAIScripts[i])
            Script_Free(gAIScripts[i]);
}
HOOK(0x450830, HumanAI_Shutdown);

static const char *const kAIFunctions[] = {
    "AIAction_Patrol",
    "AIAction_Combat",
    "AIAction_Dead",
    "AIAction_FallFlat",
    "AIAction_Activate",
    "AIAction_WalkToNode",
    "AIAction_RunToNode",
    "AIAction_FireAtNode",
    "AIAction_FireAtTask",
    "AIAction_PlayAnimation",
    "AIAction_MoveToEvent",
    "AIAction_LookAtEvent",
    "AIAction_Stunned",
    "AIAction_KickGrenade",
    "AIAction_RunPanicking",
    "AIAction_Idle",
    "AIAction_SetCombat",
    "AIFunction_RemoveAlarmActions",
    "AIFunction_SetViewLength",
    "AIFunction_SetAlarmViewLength",
    "AIFunction_SetViewAlpha",
    "AIFunction_SetViewGamma",
    "AIFunction_SetSecondaryViewLength",
    "AIFunction_SetSecondaryAlarmViewLength",
    "AIFunction_SetSecondaryViewAlpha",
    "AIFunction_SetSecondaryViewGamma",
    "AIFunction_SetEventPriority",
    "AIFunction_SetInvulnerability",
    "AIFunction_SetInstantDeath",
    "AIFunction_SetDeathAnimation",
    "AIFunction_SetAlarmTriggerID",
    "AIFunction_SetAlarmControlID",
    "AIFunction_SetAlarmAccess",
    "AIFunction_SetGunnerID",
    "AIFunction_GetAlarmTriggerID",
    "AIFunction_GetAlarmControlID",
    "AIFunction_GetAlarmAccess",
    "AIFunction_GetGunnerID",
    "AIFunction_GetAlarmControlStatus",
    "AIFunction_GetGunnerStatus",
    "AIFunction_SetScriptIntegerValue",
    "AIFunction_SetScriptRealValue",
    "AIFunction_GetScriptIntegerValue",
    "AIFunction_GetScriptRealValue",
    "AIFunction_GetCurrentEventType",
    "AIFunction_GetRandomValue",
    "AIFunction_DefaultHandler",
    "AIAction_PlaySound",
    "AIFunction_IsEventBehind",
    "AIFunction_GetEventDistance",
    "AIFunction_GetAlarmTriggerDistance",
    "AIFunction_SetAnimationInterval",
    "AIFunction_AddAnimationEntry",
    "AIFunction_GetAnimationToPlay",
    "AIFunction_SendResponse",
};

void HumanAI_UnregisterFunctions(void)
{
    size_t i;

    for (i = 0; i < sizeof(kAIFunctions) / sizeof(kAIFunctions[0]); i++)
        Script_UnregisterSymbol(kAIFunctions[i]);
}
HOOK(0x4508E0, HumanAI_UnregisterFunctions);

static const char *const kAIConstants[] = {
    "AIEVENT_CREATE",
    "AIEVENT_DELETE",
    "AIEVENT_DEAD",
    "AIEVENT_ANIMATION",
    "AIEVENT_IDLE",
    "AIEVENT_ALERT",
    "AIEVENT_ALERT_RESPONSE",
    "AIEVENT_COMBAT",
    "AIEVENT_ALARMON",
    "AIEVENT_ALARMOFF",
    "AIEVENT_WALK",
    "AIEVENT_GROUNDIMPACT",
    "AIEVENT_DOOR",
    "AIEVENT_FENCE",
    "AIEVENT_LADDER",
    "AIEVENT_TAKINGDAMAGE",
    "AIEVENT_GUNSHOT",
    "AIEVENT_GRENADETHROWN",
    "AIEVENT_GRENADELAND",
    "AIEVENT_FLASHBANG",
    "AIEVENT_GUNSHOTMISS",
    "AIEVENT_EXPLOSION",
    "AIEVENT_ENEMYDETECTION",
    "AIEVENT_FRIENDLYDETECTION",
    "AIACTIONFLAG_NONE",
    "AIACTIONFLAG_PUSHABLE",
    "HUMANAI_DETECTIONEVENT_GUNSHOT_RANGE",
    "HUMANAI_DETECTIONEVENT_GUNSHOT_SILENCED_RANGE",
};

void HumanAI_UnregisterConstants(void)
{
    size_t i;

    for (i = 0; i < sizeof(kAIConstants) / sizeof(kAIConstants[0]); i++)
        Script_UnregisterSymbol(kAIConstants[i]);
    for (i = 0; i < 29; i++)
        Script_UnregisterSymbol(gAITypeNames[i]);
    Script_UnregisterSymbol("AIALARMACCESS_BEFORECOMBAT");
    Script_UnregisterSymbol("AIALARMACCESS_AFTERCOMBAT");
}
HOOK(0x450B20, HumanAI_UnregisterConstants);

uint8_t HumanAI_GetEventSlot(void)
{
    if (gAIEventSlot == 0xFF)
        gAIEventSlot = TaskEvent_Alloc(0);
    return gAIEventSlot;
}
HOOK(0x450C80, HumanAI_GetEventSlot);

uint8_t HumanAI_GetDoorSlot(void)
{
    if (gAIDoorSlot == 0xFF)
        gAIDoorSlot = TaskEvent_Alloc(0);
    return gAIDoorSlot;
}
HOOK(0x450CA0, HumanAI_GetDoorSlot);

uint8_t HumanAI_GetEvent3Slot(void)
{
    if (gAIEvent3Slot == 0xFF)
        gAIEvent3Slot = TaskEvent_Alloc(0);
    return gAIEvent3Slot;
}
HOOK(0x450CC0, HumanAI_GetEvent3Slot);

/* sends a message to every HumanAI */
void HumanAI_Broadcast(AIMessage *m)
{
    int i;

    for (i = 0; i < Human_GetList()->count; i++) {
        uint16_t type = Human_GetList()->items[i]->type;
        TaskFn fn = gTaskHandlers[HumanAI_GetEventSlot()][type];

        fn(Human_GetList()->items[i], m);
    }
}
HOOK(0x450CE0, HumanAI_Broadcast);

void HumanAI_Send(Task *t, AIMessage *m)
{
    gTaskHandlers[HumanAI_GetEventSlot()][t->type](t, m);
}
HOOK(0x450D50, HumanAI_Send);

/* team and radius are left uninitialised, like the original */
void HumanAI_SendSimple(Task *t, int type, unsigned flags)
{
    AIMessage m;

    m.type = type;
    m.about = NULL;
    m.from = NULL;
    m.pos = NULL;
    m.flags = flags;
    HumanAI_Send(t, &m);
}
HOOK(0x450D80, HumanAI_SendSimple);

AIAction *HumanAI_GetTopAction(HumanAI *ai)
{
    return ai->actions.head->next ? (AIAction *)ai->actions.head : NULL;
}
HOOK(0x450DC0, HumanAI_GetTopAction);

AIAction *HumanAI_FindFreeAction(HumanAI *ai)
{
    int i;

    for (i = 0; i < AI_POOL; i++)
        if (!ai->actionPool[i].used)
            return &ai->actionPool[i];
    Error_Show("ActionStack full");
    for (;;)
        ;
}
HOOK(0x450DE0, HumanAI_FindFreeAction);

/* called by the AIAction_ script functions */
void HumanAI_PushAction(AIActionFn start, Node *param, unsigned flags)
{
    AIAction *a = HumanAI_FindFreeAction(gAICurrent);

    a->node.prev = NULL;
    a->node.next = NULL;
    a->update = NULL;
    a->event = gAIEventIndex;
    a->flags = flags;
    a->unk1C = 0;
    a->end = NULL;
    a->unk24 = 0;
    a->start = start;
    a->param = param;
    a->used = 1;
    List_AddTail(gAIActionList, a);
}
HOOK(0x450E10, HumanAI_PushAction);

static int HumanAI_RandomDelay(void)
{
    return round_int((Random_Float(Game_GetRandom()) + 1.0f) * 0x1.99999ap-3f * 30.0f);
}

void HumanAI_StartAction(HumanAI *ai, AIAction *a)
{
    HumanAI_PopActions(ai);
    a->node.prev = NULL;
    a->node.next = NULL;
    List_AddHead(&ai->actions, a);
    a->delay = HumanAI_RandomDelay();
    a->start(ai, a);
}
HOOK(0x450E70, HumanAI_StartAction);

/* ends the actions on top of the stack that cannot be pushed down */
void HumanAI_PopActions(HumanAI *ai)
{
    AIAction *a = (AIAction *)ai->actions.head;
    Node *next;

    for (next = a->node.next; next && !(a->flags & AIACTION_PUSHABLE); next = next->next) {
        HumanAI_EndAction(ai, a);
        a = (AIAction *)next;
    }
}
HOOK(0x450EE0, HumanAI_PopActions);

/* starts the actions of a script run, last first; all get the flags of the last one */
void HumanAI_StartActions(HumanAI *ai, List *list)
{
    char first = 0;
    unsigned flags = 0;
    AIAction *a;
    Node *prev;

    HumanAI_PopActions(ai);
    a = (AIAction *)list->tailpred;
    for (prev = a->node.prev; prev; prev = prev->prev) {
        if (!first) {
            flags = a->flags;
            first = 1;
        }
        a->flags = flags;
        a->node.prev = NULL;
        a->node.next = NULL;
        List_AddHead(&ai->actions, a);
        a->delay = HumanAI_RandomDelay();
        a->start(ai, a);
        a = (AIAction *)prev;
    }
}
HOOK(0x450F10, HumanAI_StartActions);

void HumanAI_EndAction(HumanAI *ai, AIAction *a)
{
    if (a->end)
        a->end(ai, a);
    List_Remove(a);
    a->used = 0;
    if (a->param)
        HumanAI_FreeParam(ai, a->param);
}
HOOK(0x450FF0, HumanAI_EndAction);

Node *HumanAI_AllocParam(HumanAI *ai)
{
    Node *p = ai->freeParams.head;

    if (!p->next)
        return NULL;
    List_Remove(p);
    return p;
}
HOOK(0x451030, HumanAI_AllocParam);

void HumanAI_FreeParam(HumanAI *ai, Node *p)
{
    p->prev = NULL;
    p->next = NULL;
    List_AddTail(&ai->freeParams, p);
}
HOOK(0x451050, HumanAI_FreeParam);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 53C920, 53C920, 0x2) INIT({0x81, 0x01});
DATA(53C920) uint8_t gAIEventSlot INIT(255);
DATA(53C920) uint8_t gAIDoorSlot INIT(255);
DATA(53C920) uint8_t gAIEvent3Slot INIT(255);
PAD(igd, 53C920, 53C925, 0x3) INIT({0xFF});
PAD(igd, 53C9C0, 53C9C0, 0x78) INIT({0x00, 0xD3, 0x53, 0x00, 0xF0, 0xD2, 0x53, 0x00, 0xE0, 0xD2, 0x53, 0x00, 0xD4, 0xD2, 0x53, 0x00, 0xC8, 0xD2, 0x53, 0x00, 0xB8, 0xD2, 0x53, 0x00, 0xA8, 0xD2, 0x53, 0x00, 0x94, 0xD2, 0x53, 0x00, 0x80, 0xD2, 0x53, 0x00, 0x6C, 0xD2, 0x53, 0x00, 0x54, 0xD2, 0x53, 0x00, 0x40, 0xD2, 0x53, 0x00, 0x30, 0xD2, 0x53, 0x00, 0x1C, 0xD2, 0x53, 0x00, 0x08, 0xD2, 0x53, 0x00, 0xEC, 0xD1, 0x53, 0x00, 0xD0, 0xD1, 0x53, 0x00, 0xB8, 0xD1, 0x53, 0x00, 0xA0, 0xD1, 0x53, 0x00, 0x84, 0xD1, 0x53, 0x00, 0x6C, 0xD1, 0x53, 0x00, 0x54, 0xD1, 0x53, 0x00, 0x3C, 0xD1, 0x53, 0x00, 0x20, 0xD1, 0x53, 0x00, 0x04, 0xD1, 0x53, 0x00, 0xE8, 0xD0, 0x53, 0x00, 0xCC, 0xD0, 0x53, 0x00, 0xB4, 0xD0, 0x53, 0x00, 0x98, 0xD0, 0x53, 0x00, 0x58, 0xCD, 0x53});
ALIAS(gAITypeNames, pad_53C9C0, 0);
PAD(igd, 53C9C0, 53CA38, 0x60) INIT({0x48, 0xCD, 0x53, 0x00, 0x38, 0xCD, 0x53, 0x00, 0x2C, 0xCD, 0x53, 0x00, 0x1C, 0xCD, 0x53, 0x00, 0x10, 0xCD, 0x53, 0x00, 0x04, 0xCD, 0x53, 0x00, 0xEC, 0xCC, 0x53, 0x00, 0xDC, 0xCC, 0x53, 0x00, 0xCC, 0xCC, 0x53, 0x00, 0xBC, 0xCC, 0x53, 0x00, 0xB0, 0xCC, 0x53, 0x00, 0x9C, 0xCC, 0x53, 0x00, 0x90, 0xCC, 0x53, 0x00, 0x84, 0xCC, 0x53, 0x00, 0x74, 0xCC, 0x53, 0x00, 0x60, 0xCC, 0x53, 0x00, 0x50, 0xCC, 0x53, 0x00, 0x3C, 0xCC, 0x53, 0x00, 0x28, 0xCC, 0x53, 0x00, 0x18, 0xCC, 0x53, 0x00, 0x04, 0xCC, 0x53, 0x00, 0xF4, 0xCB, 0x53, 0x00, 0xDC, 0xCB, 0x53, 0x00, 0xC4, 0xCB, 0x53});
ALIAS(gAIEventNames, pad_53CA38, 0);
PAD(igb, 57D838, 57D838, 0x980);
ALIAS(gAITargetPool, pad_57D838, 0);
PAD(igb, 58A9F4, 58A9F4, 0x24);
ALIAS(gAIScripts, pad_58A9F4, 0);
BSS(5BDA5C) List *gAIActionList;
BSS(5BDA64) int gAIEventIndex;
