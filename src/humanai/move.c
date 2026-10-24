#include <string.h>

#include "hook.h"
#include "humanai/humanai.h"

ORIG(0x4148F0, long double, Vec3d_PitchAngle, (Vec3d v));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x4168A0, TaskArray *, Game_GetTaskList0, (void));
ORIG(0x4B28A0, long double, Math_ACos, (double x));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B3320, char, Vec3d_IsZero, (const Vec3d *v));
ORIG(0x447D70, int, Door_GetState, (Task *door));
ORIG(0x447DA0, void, Door_Toggle, (Task *door));
ORIG(0x452D20, void *, NodeGrid_Place, (int *len, void *a, void *b, void *graph, int node, const Vec3d *pos));
ORIG(0x455980, uint16_t, SoldierFemale_GetType, (void));
ORIG(0x457800, uint16_t, SoldierRPG_GetType, (void));
ORIG(0x45A7E0, uint16_t, SoldierPistol_GetType, (void));
ORIG(0x45D5A0, uint16_t, Soldier_GetType, (void));
ORIG(0x463140, float *, Human_GetMatrix, (Task *human));
ORIG(0x489BE0, void *, AIDetection_FindSeen, (AIDetection *d, Task *t));
ORIG(0x48EF70, void, HumanAI_SetChannel, (HumanAI *ai, int channel, float value));
ORIG(0x5096C0, GraphStep *, AIGraph_GetStep, (void *graph, int from, int to));
ORIG(0x509860, unsigned, AIGraph_GetDoorCriteria, (void));

static const float kInvPi = 0x1.45f306p-2f;
static const float kPi = 0x1.921fb6p+1f;
static const float kHalfPi = 0x1.921fb6p+0f;
static const float kSlowTurn = 0x1.a8c536p-6f;
static const double kReached = 0x1.3333333333333p+10;
static const double kNodeTol = 0x1.3333333333333p-2;

/* what the door search handler fills in */
typedef struct NearQuery {
    Vec3d pos;
    double best;
    Task *found;
} NearQuery;

/* is pos inside the view cone (or the wider secondary one) */
char AIView_CanSee(AIView *v, const Vec3d *pos, char alarm)
{
    long double dx = pos->x - v->eye.x;
    long double dy = pos->y - v->eye.y;
    long double dz = pos->z - v->eye.z;
    long double g, p;
    double len, range;
    float a;
    Vec3d r;

    r.x = v->view[6] * dz + v->view[3] * dy + v->view[0] * dx;
    r.y = v->view[7] * dz + v->view[4] * dy + v->view[1] * dx;
    r.z = v->view[8] * dz + v->view[5] * dy + v->view[2] * dx;
    if (!(r.y >= 0.0))
        return 0;
    len = Vec3d_Length(&r);
    memcpy(&range, alarm ? &v->alarmLength : &v->length, 8);
    p = Vec3d_PitchAngle(r);
    if (!(p >= 0))
        p = -Vec3d_PitchAngle(r);
    else
        p = Vec3d_PitchAngle(r);
    a = p;
    g = Vec3d_YawAngle(r);
    if (!(g >= 0))
        g = -Vec3d_YawAngle(r);
    else
        g = Vec3d_YawAngle(r);
    if (!(len > range) && !(a >= v->alpha) && !(g >= v->gamma))
        return 1;
    if (!(len > (alarm ? v->secAlarmLength : v->secLength)) && !(a >= v->secAlpha) && !(g >= v->secGamma))
        return 1;
    return 0;
}
HOOK(0x4502F0, AIView_CanSee);

void sub_451320(HumanAI *ai)
{
    ai->unk1514--;
}
HOOK(0x451320, sub_451320);

char HumanAI_IsDead(HumanAI *ai)
{
    return Human_IsDead(ai->task.parent);
}
HOOK(0x451330, HumanAI_IsDead);

void *HumanAI_CanSeeTarget(HumanAI *ai)
{
    Task *t = *ai->target->ref;

    if (t)
        return AIDetection_FindSeen(&ai->detection, t);
    return NULL;
}
HOOK(0x451350, HumanAI_CanSeeTarget);

/* where to aim at the target: its feet plus a crouched or standing height */
void HumanAI_GetTargetPos(Vec3d *out, HumanAI *ai)
{
    *out = *DynCubeObj_GetPosition(*ai->target->ref);
    if (Human_IsCrouching(*ai->target->ref))
        out->z = out->z + 1024.0;
    else
        out->z = out->z + 2048.0;
}
HOOK(0x451380, HumanAI_GetTargetPos);

Task *HumanAI_GetTarget(HumanAI *ai)
{
    return *ai->target->ref;
}
HOOK(0x4513E0, HumanAI_GetTarget);

static float clamp_angle(float a, float max)
{
    if (!(a >= -max))
        return -max;
    if (!(max >= a))
        return max;
    return a;
}

/* turns the view pitch back to level */
void HumanAI_LevelAim(HumanAI *ai)
{
    long double a = AIView_GetPitch(&ai->view);
    float c = clamp_angle(a, ai->maxAlpha);

    Debug_Log(ai, -1, "A:  %f, CA: %f", (double)a, (double)c);
    ai->ctrl[CTRL_ALPHA] = c * kInvPi;
}
HOOK(0x4513F0, HumanAI_LevelAim);

/* pitch of the view relative to the body */
long double AIView_GetPitch(AIView *v)
{
    float f1 = v->body[1], f4 = v->body[4], f7 = v->body[7];
    long double a;
    Vec3d r;

    r.y = v->view[7] * f7 + v->view[4] * f4 + v->view[1] * f1;
    r.z = v->view[8] * f7 + v->view[5] * f4 + v->view[2] * f1;
    r.x = 0.0;
    if (!Vec3d_IsZero(&r))
        Vec3d_Normalize(&r);
    a = Math_ACos(r.y);
    if (!(r.z >= 0.0))
        a = -a;
    return a;
}
HOOK(0x451470, AIView_GetPitch);

static void HumanAI_DoneDoor(HumanAI *ai)
{
    if (ai->doorWait) {
        if (ai->door) {
            TaskRef_Remove(ai->door);
            ai->door = NULL;
        }
        ai->doorWait = 0;
    }
}

/* walks towards the first point of the queue: 0 reached it, 1 still going, 2 queue empty */
int HumanAI_MoveStep(HumanAI *ai)
{
    MoveItem *m = (MoveItem *)ai->moves.head;
    long double dx, dy, dz, ax;
    float g, a, cg, ca;
    Vec3d r;

    if (!m->node.next || !m)
        return 2;
    if (m->door == 1) {
        if (!ai->doorWait) {
            NearQuery q;
            int i;

            Graph_GetNodePos(&q.pos, Graph_GetNode(ai->graph, ai->node));
            q.best = -1.0;
            q.found = NULL;
            for (i = 0; i < Game_GetTaskList0()->count; i++) {
                uint16_t type = Game_GetTaskList0()->items[i]->type;
                TaskFn fn = gTaskHandlers[HumanAI_GetDoorSlot()][type];

                if (fn)
                    fn(Game_GetTaskList0()->items[i], &q);
            }
            if (ai->door) {
                TaskRef_Remove(ai->door);
                ai->door = NULL;
            }
            if (q.found)
                ai->door = TaskRef_Add(q.found);
            ai->doorWait = 1;
        }
        if (ai->door && *ai->door) {
            Task *d = *ai->door;

            switch (Door_GetState(d)) {
            case 1:
            case 3:
                Door_Toggle(d);
                HumanAI_FaceTo(ai, &m->pos, 0x1.1df46ap-5f, 1);
                return 1;
            case 2:
                Debug_Log(ai, -1, "Waiting for door to open");
                HumanAI_FaceTo(ai, &m->pos, 0x1.1df46ap-5f, 1);
                return 1;
            }
        }
    }
    dx = m->pos.x - ai->view.pos.x;
    dy = m->pos.y - ai->view.pos.y;
    dz = m->pos.z - ai->view.pos.z;
    r.x = ai->view.body[6] * dz + ai->view.body[3] * dy + ai->view.body[0] * dx;
    r.y = ai->view.body[7] * dz + ai->view.body[4] * dy + ai->view.body[1] * dx;
    r.z = ai->view.body[8] * dz + ai->view.body[5] * dy + ai->view.body[2] * dx;
    g = -Vec3d_YawAngle(r);
    a = AIView_GetPitch(&ai->view);
    Debug_Log(ai, -1, "A:  %f, G:  %f", (double)a, (double)g);
    ca = clamp_angle(a, ai->maxAlpha);
    cg = clamp_angle(g, ai->maxGamma);
    Debug_Log(ai, -1, "CA: %f, CG: %f", (double)ca, (double)cg);
    ax = r.x;
    if (!(ax >= 0))
        ax = -ax;
    if (!(ax > kReached)) {
        ax = r.y;
        if (!(ax >= 0))
            ax = -ax;
        if (!(ax > kReached)) {
            ai->node = m->id;
            List_Remove(m);
            m->node.prev = NULL;
            m->node.next = NULL;
            List_AddTail(&gAIMoveFree, m);
            HumanAI_DoneDoor(ai);
            return 0;
        }
    }
    ai->ctrl[CTRL_FORWARD] = 1.0f;
    ai->ctrl[CTRL_GAMMA] = cg * kInvPi;
    ai->ctrl[CTRL_ALPHA] = ca * kInvPi;
    return 1;
}
HOOK(0x451540, HumanAI_MoveStep);

static char HumanAI_MoveDoor(GraphNode *a, GraphNode *b)
{
    return (a->flags & AIGraph_GetDoorCriteria()) && (b->flags & AIGraph_GetDoorCriteria());
}

/* fills the walk queue with the route to node dest (and on to pos), then walks it */
char HumanAI_MoveTo(HumanAI *ai, int dest, const Vec3d *pos, char replan)
{
    void *graph;
    int from;
    MoveItem *it;

again:
    graph = ai->graph;
    from = -1;
    if (ai->destNode == dest) {
        for (;;) {
            int r = HumanAI_MoveStep(ai);

            if (r == 0)
                continue;
            if (r == 1)
                return 0;
            if (r == 2) {
                HumanAI_ClearMove(ai);
                ai->destNode = -1;
                return 1;
            }
            for (;;)
                ;
        }
    }
    if (replan) {
        if (ai->path) {
            NodeGrid_Release(ai->path, ai->pathLen, ai->pathLen);
            ai->path = NULL;
        }
        if (pos) {
            ai->path = NodeGrid_Place(&ai->pathLen, ai->pathA, ai->pathB, graph, dest, pos);
        } else {
            Vec3d p;

            Graph_GetNodePos(&p, Graph_GetNode(ai->graph, dest));
            ai->path = NodeGrid_Place(&ai->pathLen, ai->pathA, ai->pathB, graph, dest, &p);
        }
    }

    /* not inside the current node: maybe go straight to the next one */
    if (ai->node != -1) {
        GraphNode *cur = Graph_GetNode(ai->graph, ai->node);

        if (cur && !Graph_IsInNode(DynCubeObj_GetPosition(ai->task.parent), cur, kNodeTol, 0)) {
            MoveItem *m;
            Node *next;
            Vec3d np;
            double d;

            Graph_GetNodePos(&np, cur);
            d = Vec3d_Distance(&np, DynCubeObj_GetPosition(ai->task.parent));
            m = (MoveItem *)ai->moves.head;
            from = ai->node;
            for (next = m->node.next; next; next = next->next) {
                int id = m->id;
                GraphNode *n;

                if (id == from) {
                    m = (MoveItem *)next;
                    continue;
                }
                if (id == -1)
                    break;
                n = Graph_GetNode(ai->graph, id);
                if (!n)
                    break;
                if (Graph_IsInNode(DynCubeObj_GetPosition(ai->task.parent), n, kNodeTol, 0)) {
                    from = id;
                } else if (id == dest) {
                    Graph_GetNodePos(&np, n);
                    if (!(Vec3d_Distance(&np, DynCubeObj_GetPosition(ai->task.parent)) >= d))
                        from = id;
                } else {
                    GraphStep *s = AIGraph_GetStep(ai->graph, id, dest);

                    if (!s)
                        Warning_Show("ERROR IN ROUTENET: %d to %d or %d to %d", from, dest, id, dest);
                    else if (!(s->dist >= d))
                        from = id;
                }
                break;
            }
        }
    }

    HumanAI_ClearMove(ai);
    ai->destNode = dest;
    if (from == -1) {
        from = ai->node;
    } else {
        GraphNode *n;
        Vec3d np, hp, dir;

        it = HumanAI_NewMoveItem(ai);
        n = Graph_GetNode(ai->graph, from);
        if (!it)
            goto full;
        Graph_GetNodePos(&np, n);
        hp = *DynCubeObj_GetPosition(ai->task.parent);
        dir.x = np.x - hp.x;
        dir.y = np.y - hp.y;
        dir.z = 0.0;
        Graph_EdgePoint(&it->pos, n, &np, &dir, 0);
        it->door = 0;
        it->id = from;
    }

    if (from == dest) {
        if (pos) {
            it = HumanAI_NewMoveItem(ai);
            if (!it)
                goto full;
            it->pos = *pos;
        } else {
            GraphNode *n;

            it = HumanAI_NewMoveItem(ai);
            n = Graph_GetNode(ai->graph, dest);
            if (!it)
                goto full;
            Graph_GetNodePos(&it->pos, n);
        }
        it->door = 0;
        it->id = dest;
        goto again;
    }

    do {
        GraphStep *s = AIGraph_GetStep(graph, from, dest);
        GraphNode *a, *b;
        Vec3d ap, bp, dir;

        if (!s) {
            Warning_Show("Error in graph %d routenet, Node #%d to #%d", ai->graphId, from, dest);
            return 0;
        }
        a = Graph_GetNode(graph, from);
        b = Graph_GetNode(graph, s->next);
        if (!b) {
            Warning_Show("Error in graph %d nodenet, Node #%d not found", ai->graphId, s->next);
            return 0;
        }
        if (s->next == dest) {
            char inside = 0;

            Graph_GetNodePos(&bp, b);
            if (pos) {
                it = HumanAI_NewMoveItem(ai);
                if (!it)
                    goto full;
                Graph_GetNodePos(&ap, a);
                dir.x = bp.x - ap.x;
                dir.y = bp.y - ap.y;
                dir.z = bp.z - ap.z;
                Graph_EdgePoint(&it->pos, b, &bp, &dir, 0);
                it->door = HumanAI_MoveDoor(a, b);
                it->id = s->next;
                inside = Graph_IsInNode(pos, b, kNodeTol, 0) != 0;
            }
            it = HumanAI_NewMoveItem(ai);
            if (!it)
                goto full;
            it->door = 0;
            if (inside) {
                it->pos = *pos;
                memcpy(&it->pos.z, &bp.z, 8);
            } else {
                it->pos = bp;
                if (HumanAI_MoveDoor(a, b))
                    it->door = 1;
            }
            it->id = s->next;
        } else {
            Graph_GetNodePos(&ap, a);
            Graph_GetNodePos(&bp, b);
            dir.x = bp.x - ap.x;
            dir.y = bp.y - ap.y;
            dir.z = bp.z - ap.z;
            if (from != ai->node || Graph_IsInNode(&ai->view.pos, a, kNodeTol, 0)) {
                it = HumanAI_NewMoveItem(ai);
                if (!it)
                    goto full;
                Graph_EdgePoint(&it->pos, a, &ap, &dir, 1);
                it->door = 0;
                it->id = from;
                if (HumanAI_MoveDoor(a, b))
                    it->door = 1;
            }
            it = HumanAI_NewMoveItem(ai);
            if (!it)
                goto full;
            Graph_EdgePoint(&it->pos, b, &bp, &dir, 0);
            it->door = 0;
            it->id = s->next;
        }
        from = s->next;
    } while (from != dest);
    goto again;

full:
    Warning_Show("Not enough items in movement queue");
    return 0;
}
HOOK(0x4518E0, HumanAI_MoveTo);

MoveItem *HumanAI_NewMoveItem(HumanAI *ai)
{
    Node *n = gAIMoveFree.head;

    if (!n->next)
        return NULL;
    List_Remove(n);
    n->prev = NULL;
    n->next = NULL;
    List_AddTail(&ai->moves, n);
    return (MoveItem *)n;
}
HOOK(0x452080, HumanAI_NewMoveItem);

/* where the line from center along dir leaves the node's square */
void Graph_EdgePoint(Vec3d *out, GraphNode *node, const Vec3d *c, const Vec3d *dir, char add)
{
    double r = node->radius * 4096.0f;
    long double t, s, e;

    if (!Vec3d_IsZero(dir)) {
        if (dir->x > r || -r > dir->x) {
            t = r / dir->x;
            if (!(t >= 0))
                t = -t;
            t = t * dir->y;
            s = t;
            if (!(t >= 0))
                s = -t;
            if (!(s > r)) {
                e = r;
                if (!(dir->x >= 0))
                    e = -e;
                if (add) {
                    out->x = e + c->x;
                    out->y = t + c->y;
                } else {
                    out->x = c->x - e;
                    out->y = c->y - t;
                }
                memcpy(&out->z, &c->z, 8);
                return;
            }
        }
        if (!(r >= dir->y) || -r > dir->y) {
            double td;

            t = r / dir->y;
            if (!(t >= 0))
                t = -t;
            t = t * dir->x;
            td = t;
            s = t;
            if (!(t >= 0))
                s = -t;
            if (!(s > r)) {
                e = r;
                if (!(dir->y >= 0))
                    e = -e;
                if (add) {
                    out->x = td + c->x;
                    out->y = e + c->y;
                } else {
                    out->x = c->x - td;
                    out->y = c->y - e;
                }
                memcpy(&out->z, &c->z, 8);
                return;
            }
        }
    }
    *out = *c;
}
HOOK(0x4520C0, Graph_EdgePoint);

char HumanAI_RunTo(HumanAI *ai, int dest, const Vec3d *pos, char replan)
{
    ai->maxAlpha = kPi;
    ai->maxGamma = kPi;
    return HumanAI_MoveTo(ai, dest, pos, replan);
}
HOOK(0x4522A0, HumanAI_RunTo);

/* turns the view towards pos; 1 when both angles are within tol */
char HumanAI_TurnTo(HumanAI *ai, const Vec3d *pos, float tol, char flat)
{
    const float *m = ai->view.view;
    long double dx, dy, dz, g, x;
    float graw, araw, ga, ca, cg;
    Vec3d p = *pos, r;

    if (flat)
        memcpy(&p.z, &ai->view.eye.z, 8);
    dx = p.x - ai->view.eye.x;
    dy = p.y - ai->view.eye.y;
    dz = p.z - ai->view.eye.z;
    r.x = m[6] * dz + m[0] * dx + m[3] * dy;
    r.y = m[1] * dx + m[4] * dy + m[7] * dz;
    r.z = m[5] * dy + m[8] * dz + m[2] * dx;
    g = -Vec3d_YawAngle(r);
    graw = g;
    x = g;
    if (!(g >= 0))
        x = -g;
    if (!(x > kHalfPi)) {
        ga = Vec3d_PitchAngle(r);
    } else {
        Vec3d f;
        long double q;
        float aq;

        f.x = m[1];
        f.y = m[4];
        f.z = m[7];
        q = -Vec3d_PitchAngle(f);
        ga = q;
        aq = ga;
        if (!(q >= 0))
            aq = -ga;
        if (aq > kHalfPi)
            ga = !(ga >= 0) ? -kPi - ga : kPi - ga;
    }
    araw = ga;
    ca = clamp_angle(ga, ai->maxAlpha);
    cg = clamp_angle(graw, ai->maxGamma);
    Debug_Log(ai, -1, "CA: %f, CG: %f", (double)ca, (double)cg);
    Debug_Log(ai, -1, "A:  %f,  G: %f", (double)araw, (double)graw);
    ai->ctrl[CTRL_GAMMA] = cg * kInvPi;
    ai->ctrl[CTRL_ALPHA] = ca * kInvPi;
    x = araw;
    if (!(araw >= 0))
        x = -x;
    if (x > tol)
        return 0;
    x = graw;
    if (!(graw >= 0))
        x = -x;
    if (x > tol)
        return 0;
    return 1;
}
HOOK(0x4522D0, HumanAI_TurnTo);

char HumanAI_AimAt(HumanAI *ai, const Vec3d *pos, float tol, char flat)
{
    ai->maxAlpha = kSlowTurn;
    ai->maxGamma = kSlowTurn;
    return HumanAI_TurnTo(ai, pos, tol, flat);
}
HOOK(0x452580, HumanAI_AimAt);

char HumanAI_FaceTo(HumanAI *ai, const Vec3d *pos, float tol, char flat)
{
    ai->maxAlpha = kPi;
    ai->maxGamma = kPi;
    return HumanAI_TurnTo(ai, pos, tol, flat);
}
HOOK(0x4525B0, HumanAI_FaceTo);

/* the flag at +0xD00 of four human types; other types read a byte of
   the argument slot, like the original */
static char HumanAI_GetD00(HumanAI *ai)
{
    Human *h = (Human *)ai->task.parent;

    if (h->task.type == Soldier_GetType())
        return h->unkD00;
    if (h->task.type == SoldierPistol_GetType())
        return h->unkD00;
    if (h->task.type == SoldierFemale_GetType())
        return h->unkD00;
    if (h->task.type == SoldierRPG_GetType())
        return h->unkD00;
    return (char)(uintptr_t)ai;
}

void sub_4525E0(HumanAI *ai)
{
    char c = HumanAI_GetD00(ai);

    HumanAI_SetChannel(ai, 0, c == 0);
}
HOOK(0x4525E0, sub_4525E0);

void sub_452660(HumanAI *ai)
{
    char c = HumanAI_GetD00(ai);

    HumanAI_SetChannel(ai, 0, c);
}
HOOK(0x452660, sub_452660);

/* bit 0: a point 12288 to the human's left is in the current node, bit 1: one to the right */
int HumanAI_CheckSides(HumanAI *ai)
{
    Task *h = ai->task.parent;
    double vx = -12288.0, vy = 0.0, vz = 0.0;
    double t1, t2, x, y, z;
    GraphNode *n;
    Vec3d p1, p2;
    int r;

    t2 = Human_GetMatrix(h)[2] * vz;
    t1 = Human_GetMatrix(h)[1] * vy;
    x = Human_GetMatrix(h)[0] * vx + t1 + t2;
    t2 = Human_GetMatrix(h)[5] * vz;
    t1 = Human_GetMatrix(h)[4] * vy;
    y = Human_GetMatrix(h)[3] * vx + t1 + t2;
    t2 = Human_GetMatrix(h)[8] * vz;
    t1 = Human_GetMatrix(h)[7] * vy;
    z = Human_GetMatrix(h)[6] * vx + t1 + t2;
    p1.x = x + DynCubeObj_GetPosition(h)->x;
    p1.y = y + DynCubeObj_GetPosition(h)->y;
    p1.z = z + DynCubeObj_GetPosition(h)->z;
    p2.x = -x + DynCubeObj_GetPosition(h)->x;
    p2.y = -y + DynCubeObj_GetPosition(h)->y;
    p2.z = z + DynCubeObj_GetPosition(h)->z;
    n = Graph_GetNode(ai->graph, ai->node);
    if (!n)
        return 0;
    r = 2;
    if (Graph_IsInNode(&p1, n, 0.0, 0))
        r = 3;
    if (!Graph_IsInNode(&p2, n, 0.0, 0))
        r &= ~2;
    return r;
}
HOOK(0x4526E0, HumanAI_CheckSides);
