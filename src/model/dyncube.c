/* Sending events to the tasks in the dyncube trees, and the per node masks. */
#include "hook.h"
#include "igi.h"
#include "terrain/terrain.h"
#include "sound/soundtypes.h"

ORIG(0x4C58B0, int, DynCubeTree_GetChildCount, (int tree));
ORIG(0x4C7750, DynCubeNode *, DynCube_GetRoot_, (Terrain *t, int tree));
ORIG(0x4C8130, void, DynCube_SendEventLines, (Terrain *t, int slot, void *query, const Vec3d *starts, const Vec3d *ends,
                                  int count, unsigned int flags, int tree));
ORIG(0x4CC2A0, void, Coll_Lines, (Terrain *t, void *results, const Vec3d *starts, const Vec3d *ends, int count,
                                  char unk, int unk2, unsigned int flags));
ORIG(0x4E53C0, void *, CellMask_Create, (int size, Terrain *t, int unk, const Vec3d *pos, float scale));
ORIG(0x4E54E0, void, CellMask_Free, (void *mask));
ORIG(0x4E5580, int, CellMask_Test, (void *mask, int x, int y));
ORIG(0x4E5C90, void, CellMask_Enable, (void));
ORIG(0x4E5CA0, void, CellMask_Disable, (void));
ORIG(0x50FAE0, void, TerrainMesh_SetupLighting, (Terrain *t, int unk));

extern TaskType gTaskTypes[];
extern TaskHandler gTaskHandlers[][TASK_MAX_TYPES];
extern int gTaskHandlerChanges;
extern const float gPixelCenter;
extern const float gZero;
extern int gCollSightCalls;
extern int gCellMaskEnabled;
extern uint8_t gWorldEvents[];

void DynCube_UpdateSlotCounts(Terrain *t, int tree);
void DynCube_UpdateStart(Terrain *t, int tree);
void DynCubeNode_BuildMasks(Terrain *t, DynCubeNode *node, int unk, const Vec3d *pos);

/* Argument of event 8, sent along a line of sight. A handler can lower
   visibility or set blocked. */
typedef struct SightQuery {
    const Vec3d *from;
    const Vec3d *to;
    char unk8;
    int unkC;
    Terrain *terrain;
    char blocked;
    float visibility;
} SightQuery;

static inline int is_pow2(int m)
{
    return (~(m - 1) & m) == m;
}

/* Offset of child i from its parent's center, for a child of the given size */
static inline int child_dx(int i, int size)
{
    return ((i & 1) * 2 - 1) * size;
}

static inline int child_dy(int i, int size)
{
    return ((i & 2) - 1) * size;
}

static inline int child_dz(int i, int size)
{
    return (((i >> 1) & 2) - 1) * size;
}

/* Calls the slot handler of every task in the cells along the path from
   the start node down to p whose box contains p. */
void Coll_SendEvent(Terrain *t, uint8_t slot, void *arg, const Vec3d *p, unsigned int mask, int tree)
{
    DynCubeNode *node;
    Vec3d pt, base;
    int depth;
    unsigned int x, y, z;

    DynCube_UpdateSlotCounts(t, tree);
    if (!t->slot_count[tree][slot])
        return;
    DynCube_UpdateStart(t, tree);
    pt = *p;
    x = Terrain_Round(p->x) + t->offset[0];
    y = Terrain_Round(p->y) + t->offset[1];
    z = Terrain_Round(p->z) + t->offset[2];
    node = t->start[tree];
    depth = t->start_depth[tree];
    base = t->start_pos[tree];
    for (;;) {
        CellMask *cm = node->unk3C;
        DynCubeObj *o;
        unsigned int i;

        if (cm) {
            long double n = node->size;
            long double f = (long double)cm->size * gPixelCenter / n;
            int ix = Terrain_Round(f * ((p->x - (long double)base.x) + n));
            int iy = Terrain_Round(f * ((p->y - (long double)base.y) + n));

            if (!CellMask_Test(cm, ix, iy))
                return;
        }
        for (o = node->objs; o; o = o->next) {
            TaskHandler h = gTaskHandlers[slot][o->task.type];
            long double r;

            if (!h)
                continue;
            r = o->radius;
            if (o->pos.x - r > pt.x || !(o->pos.x + r >= pt.x))
                continue;
            if (o->pos.y - r > pt.y || !(o->pos.y + r >= pt.y))
                continue;
            if (o->pos.z - r > pt.z || !(o->pos.z + r >= pt.z))
                continue;
            if (gTaskTypes[o->task.type].flags & mask)
                h(&o->task, arg);
        }
        i = ((z >> (depth & 31)) & 1) << 2 | ((y >> (depth & 31)) & 1) << 1 | ((x >> (depth & 31)) & 1);
        node = node->children[i];
        if (!node)
            return;
        depth--;
        if (depth == 29) {
            /* the root's children are stored mirrored */
            unsigned int ni = ~i;

            base.x += (int)(((ni & 1) << 30) - 0x20000000);
            base.y += (int)(((ni & 2) << 29) - 0x20000000);
            base.z += (int)(((ni & 4) - 2) << 28);
        } else {
            base.x += (int)((unsigned int)((i & 1) * 2 - 1) << (depth & 31));
            base.y += (int)((unsigned int)((i & 2) - 1) << (depth & 31));
            base.z += (int)((unsigned int)((((int)i >> 1) & 2) - 1) << (depth & 31));
        }
    }
}
HOOK(0x4CE090, Coll_SendEvent);

void Coll_SendEventAll(Terrain *t, uint8_t slot, void *arg, const Vec3d *p, unsigned int mask)
{
    int i;

    for (i = 0; i < DYNCUBE_TREES; i++)
        Coll_SendEvent(t, slot, arg, p, mask, i);
}
HOOK(0x4CE390, Coll_SendEventAll);

void Terrain_InvalidateLighting(Terrain *t)
{
    t->ready = 0;
}
HOOK(0x4CE3D0, Terrain_InvalidateLighting);

void Terrain_SetMaterial(Terrain *t, int i, void *m)
{
    ((void **)t->materials)[i] = m;
}
HOOK(0x4CE3E0, Terrain_SetMaterial);

void *Terrain_GetMaterial(Terrain *t, int i)
{
    return ((void **)t->materials)[i];
}
HOOK(0x4CE400, Terrain_GetMaterial);

/* Sends event 8 along the segment to the tasks of trees 0, 1, 4 and 5 picked
   by the category bits in trees, then checks the terrain if flags has bit
   31. Returns 1 when nothing blocks; *visibility gets 0..1. */
int Coll_LineOfSight(Terrain *t, const Vec3d *from, const Vec3d *to, char unk, int unk2, unsigned int flags,
                     unsigned int trees, float *visibility)
{
    char hit[0x70];
    SightQuery q;
    int i;

    gCollSightCalls++;
    q.from = from;
    q.to = to;
    q.unk8 = unk;
    q.unkC = unk2;
    q.terrain = t;
    q.blocked = 0;
    q.visibility = 1.0f;
    for (i = 0; i < DYNCUBE_TREES; i++) {
        if (i != 1 && i != 0 && i != 4 && i != 5)
            continue;
        if (trees & DynCubeTree_GetChildCount(i))
            DynCube_SendEventLines(t, 8, &q, from, to, 1, flags, i);
    }
    if ((q.visibility < gZero || q.visibility > gZero) && (flags & 0x80000000)) {
        Coll_Lines(t, hit, from, to, 1, (char)0xFF, 0, 0x80000000);
        if (*(int *)(hit + 0x58)) {
            q.visibility = 0.0f;
            q.blocked = 1;
        }
    }
    if (visibility)
        *visibility = q.visibility;
    return q.blocked == 0;
}
HOOK(0x4CE410, Coll_LineOfSight);

/* Adds the handler counts of every task below node to the tree's slot counts. */
void DynCube_CountSlots(Terrain *t, DynCubeNode *node)
{
    DynCubeObj *o;
    int i;

    for (i = 0; i < 8; i++)
        if (node->children[i])
            DynCube_CountSlots(t, node->children[i]);
    for (o = node->objs; o; o = o->next)
        for (i = 0; i < TASK_MAX_SLOTS; i++)
            t->slot_count[node->tree][i] += gTaskHandlers[i][o->task.type] != 0;
}
HOOK(0x4CE530, DynCube_CountSlots);

/* Recounts when any task type handler changed since the last count. */
void DynCube_UpdateSlotCounts(Terrain *t, int tree)
{
    int i;

    if (t->handler_changes[tree] == gTaskHandlerChanges)
        return;
    for (i = 0; i < TASK_MAX_SLOTS; i++)
        t->slot_count[tree][i] = 0;
    DynCube_CountSlots(t, DynCube_GetRoot_(t, tree));
    t->handler_changes[tree] = gTaskHandlerChanges;
}
HOOK(0x4CE5C0, DynCube_UpdateSlotCounts);

/* Finds where queries start: walks down from the root while a node has a
   single child and no tasks. The root's children are seen mirrored, and
   only its children 0-3 count as present. */
void DynCube_UpdateStart(Terrain *t, int tree)
{
    DynCubeNode copy, *root, *node;
    double x, y, z;
    int depth = 30, i;

    if (t->start_valid[tree])
        return;
    root = t->roots[tree];
    x = (int)(0u - t->offset[0]);
    y = (int)(0u - t->offset[1]);
    z = (int)(0u - t->offset[2]);
    copy = *root;
    copy.child_mask = 0;
    for (i = 0; i < 4; i++) {
        copy.children[i] = root->children[7 - i];
        copy.children[7 - i] = root->children[i];
        copy.child_mask |= ((root->child_mask >> i) & 1) << (7 - i);
    }
    node = &copy;
    if (!node->child_mask || !is_pow2(node->child_mask) || node->objs) {
        t->start[tree] = t->roots[tree];
    } else {
        do {
            int c = __builtin_ctz(node->child_mask);
            int s;

            node = node->children[c];
            s = node->size;
            x += child_dx(c, s);
            y += child_dy(c, s);
            z += child_dz(c, s);
            depth--;
        } while (node->child_mask && is_pow2(node->child_mask) && !node->objs);
        t->start[tree] = node;
    }
    t->start_pos[tree].x = x;
    t->start_pos[tree].y = y;
    t->start_pos[tree].z = z;
    t->start_depth[tree] = depth;
    t->start_valid[tree] = 1;
}
HOOK(0x4CE620, DynCube_UpdateStart);

/* Rebuilds the cell masks of tree 0. The walk starts at a mirrored copy of
   the root, so the root itself never gets one. */
void DynCube_BuildMasks(Terrain *t, int unk)
{
    DynCubeNode copy, *root;
    Vec3d pos;
    int i;

    pos.x = (int)(0u - t->offset[0]);
    pos.y = (int)(0u - t->offset[1]);
    pos.z = (int)(0u - t->offset[2]);
    if (unk)
        return;
    t->unkDF0 = 1;
    if (!gCellMaskEnabled)
        return;
    root = t->roots[0];
    copy = *root;
    copy.child_mask = 0;
    for (i = 0; i < 8; i++) {
        copy.children[i] = root->children[7 - i];
        copy.child_mask |= ((root->child_mask >> i) & 1) << (7 - i);
    }
    CellMask_Disable();
    DynCubeNode_BuildMasks(t, &copy, 0, &pos);
    CellMask_Enable();
}
HOOK(0x4CE830, DynCube_BuildMasks);

/* pos is the node's center. Nodes of size 2^20 get a 256x256 mask. */
void DynCubeNode_BuildMasks(Terrain *t, DynCubeNode *node, int unk, const Vec3d *pos)
{
    int i;

    for (i = 0; i < 8; i++) {
        DynCubeNode *c = node->children[i];
        Vec3d p;

        if (!c)
            continue;
        p = *pos;
        p.x += child_dx(i, c->size);
        p.y += child_dy(i, c->size);
        p.z += child_dz(i, c->size);
        DynCubeNode_BuildMasks(t, c, unk, &p);
    }
    if (node->unk3C)
        CellMask_Free(node->unk3C);
    if (node->size == 0x100000)
        node->unk3C = CellMask_Create(0x100, t, unk, pos, 1048576.0f);
}
HOOK(0x4CE910, DynCubeNode_BuildMasks);

uint8_t World_GetEvent0(void)
{
    return gWorldEvents[0];
}
HOOK(0x4CEA10, World_GetEvent0);

uint8_t World_GetEvent1(void)
{
    return gWorldEvents[1];
}
HOOK(0x4CEA20, World_GetEvent1);

void Terrain_UpdateLighting(Terrain *t)
{
    if (!t->ready) {
        TerrainMesh_SetupLighting(t, 0);
        t->ready = 1;
    }
}
HOOK(0x4CEA30, Terrain_UpdateLighting);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A44394) int gCollSightCalls;
