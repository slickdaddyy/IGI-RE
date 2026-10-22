#include "hook.h"
#include "igi.h"
#include "math/x87.h"
#include "terrain/terrain.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B3360, char, Vec3d_Equal, (const Vec3d *a, const Vec3d *b));
ORIG(0x4C5C90, void, DynCubeNode_Init, (Terrain *t, DynCubeNode *node, int child, unsigned int size, int tree));
ORIG(0x4D97F0, void, sub_4D97F0, (DynCubeObj *obj, int v));
ORIG(0x4E54E0, void, sub_4E54E0, (void *p));

#define gTaskTypes ((TaskType *)0xAF5EE0)
#define gTaskHandlers ((TaskHandler (*)[TASK_MAX_TYPES])0xA96AE0)

_Static_assert(sizeof(DynCubeNode) == 0x40, "DynCubeNode");
_Static_assert(offsetof(DynCubeObj, node) == 0x50, "DynCubeObj");
_Static_assert(offsetof(DynCubeObj, unk60) == 0x60, "DynCubeObj");

static DynCubeNode *node_new(Pool *p)
{
    DynCubeNode *n = (DynCubeNode *)(p->data + p->slots[p->used] * p->stride);

    *(int *)((char *)n + p->index_offset) = p->used;
    p->used++;
    return n;
}

static void node_delete(Pool *p, DynCubeNode *n)
{
    int index = *(int *)((char *)n + p->index_offset);
    char *last;
    int last_index, tmp;

    p->used--;
    if (index == p->used)
        return;
    last = p->data + p->slots[p->used] * p->stride;
    tmp = p->slots[index];
    last_index = *(int *)(last + p->index_offset);
    p->slots[index] = p->slots[last_index];
    p->slots[last_index] = tmp;
    *(int *)(last + p->index_offset) = index;
}

static void free_child(Terrain *t, DynCubeNode *parent, int i)
{
    if (parent->children[i]->unk3C)
        sub_4E54E0(parent->children[i]->unk3C);
    node_delete(&t->nodes, parent->children[i]);
    parent->children[i] = NULL;
    parent->child_mask &= ~(1 << i);
}

static uint16_t objs_mask(const DynCubeNode *node)
{
    uint16_t m = 0x8000;
    const DynCubeObj *o;

    for (o = node->objs; o; o = o->next)
        m |= o->mask;
    return m;
}

static uint16_t children_mask(const DynCubeNode *node)
{
    uint16_t m = 0x8000;
    int i;

    for (i = 0; i < 8; i++)
        if (node->children[i])
            m |= node->children[i]->sub_mask | node->children[i]->mask;
    return m;
}

/* Size of the smallest cell that can hold both corners. */
unsigned int DynCube_CellSize(int x0, int y0, int z0, int x1, int y1, int z1)
{
    unsigned int d = (x0 ^ x1) | (y0 ^ y1) | (z0 ^ z1), s;

    if (!d)
        return DYNCUBE_MIN_SIZE;
    s = 1u << (31 - __builtin_clz(d));
    if (s < DYNCUBE_MIN_SIZE)
        return DYNCUBE_MIN_SIZE;
    if (s > DYNCUBE_MAX_SIZE)
        s = DYNCUBE_MAX_SIZE;
    return s;
}
HOOK(0x4C67E0, DynCube_CellSize);

void DynCube_ListRemove(Terrain *t, DynCubeObj *obj, int list)
{
    if (obj->prev)
        obj->prev->next = obj->next;
    else
        t->lists[list + 1] = obj->next;
    if (obj->next)
        obj->next->prev = obj->prev;
}
HOOK(0x4C6840, DynCube_ListRemove);

void DynCube_ListAdd(Terrain *t, DynCubeObj *obj, int list)
{
    DynCubeObj **head = &t->lists[list + 1];

    if (*head)
        (*head)->prev = obj;
    obj->next = *head;
    obj->prev = NULL;
    *head = obj;
}
HOOK(0x4C6890, DynCube_ListAdd);

void DynCube_Move(Terrain *t, DynCubeObj *obj, const Vec3d *pos, float radius)
{
    DynCubeNode *node = obj->node;
    unsigned int size, span, cell;
    int r, x, y, z, ox, oy, oz;

    /* a NaN radius counts as unchanged, like the original's fcomp */
    if (Vec3d_Equal(pos, &obj->pos) && !(radius < obj->radius || radius > obj->radius))
        return;
    r = Terrain_Round(radius);
    x = Terrain_Round(pos->x) + t->offset[0];
    y = Terrain_Round(pos->y) + t->offset[1];
    z = Terrain_Round(pos->z) + t->offset[2];
    ox = Terrain_Round(obj->pos.x) + t->offset[0];
    oy = Terrain_Round(obj->pos.y) + t->offset[1];
    oz = Terrain_Round(obj->pos.z) + t->offset[2];
    obj->pos = *pos;
    obj->radius = radius;
    size = node->size;
    span = DynCube_CellSize(ox, oy, oz, x, y, z);
    cell = DynCube_CellSize(x - r, y - r, z - r, x + r, y + r, z + r);
    if (cell > span)
        span = cell;
    else if (size == span && span == cell)
        return;
    DynCubeNode_Unlink(node, obj);
    if (size < span)
        DynCube_Ascend(t, span, size, &node);
    else
        node->mask = objs_mask(node);
    DynCube_Insert(t, node, obj, cell, x, y, z);
}
HOOK(0x4C6940, DynCube_Move);

/* Walks up from *node until the node size reaches size, freeing nodes left empty. */
void DynCube_Ascend(Terrain *t, unsigned int size, unsigned int cur, DynCubeNode **node)
{
    DynCubeNode *n = *node, *parent;

    if (size <= cur) {
        *node = n;
        return;
    }
    do {
        int i = n->child;
        DynCubeObj *objs = n->objs;

        parent = n->parent;
        if (!objs)
            parent->children[i]->mask = 0x8000;
        else
            parent->children[i]->mask = objs_mask(parent->children[i]);
        parent->sub_mask = children_mask(parent);
        if (!n->child_mask && !objs)
            free_child(t, parent, i);
        cur *= 2;
        n = parent;
    } while (cur < size);
    *node = parent;
}
HOOK(0x4C6B20, DynCube_Ascend);

void DynCubeNode_Unlink(DynCubeNode *node, DynCubeObj *obj)
{
    DynCubeObj *next = obj->next, *prev;

    if (obj == node->objs) {
        node->objs = next;
        if (next)
            next->prev = NULL;
    } else {
        prev = obj->prev;
        if (next)
            next->prev = prev;
        if (prev)
            prev->next = next;
    }
}
HOOK(0x4C6CE0, DynCubeNode_Unlink);

/* Descends from node to the cell of the given size holding (x, y, z),
   creating nodes on the way, and links obj there. */
void DynCube_Insert(Terrain *t, DynCubeNode *node, DynCubeObj *obj, unsigned int size,
                    unsigned int x, unsigned int y, unsigned int z)
{
    unsigned int s = node->size;

    node->mask = objs_mask(node);
    t->start_valid[node->tree] = 0;
    while (s != size) {
        int i = 0;

        node->sub_mask |= obj->mask;
        if (x & s)
            i = 1;
        if (y & s)
            i |= 2;
        if (z & s)
            i |= 4;
        s >>= 1;
        node->child_mask |= 1 << i;
        if (!node->children[i]) {
            DynCubeNode *c = node_new(&t->nodes);

            node->children[i] = c;
            DynCubeNode_Init(t, c, i, s, node->tree);
            node->children[i]->parent = node;
        }
        node = node->children[i];
    }
    node->mask |= obj->mask;
    DynCubeNode_Link(node, obj);
}
HOOK(0x4C6D20, DynCube_Insert);

void DynCubeNode_Link(DynCubeNode *node, DynCubeObj *obj)
{
    DynCubeObj *head = node->objs;

    if (head)
        head->prev = obj;
    obj->next = head;
    obj->prev = NULL;
    obj->node = node;
    node->objs = obj;
}
HOOK(0x4C6E30, DynCubeNode_Link);

void DynCube_MoveBox(Terrain *t, DynCubeObj *obj, const Vec3d *pos, const Vec3d *min, const Vec3d *max)
{
    DynCubeNode *node = obj->node;
    unsigned int size, span, cell;
    int x, y, z, x0, y0, z0, x1, y1, z1, ox, oy, oz;
    long double dx, dy, dz;

    if (Vec3d_Equal(pos, &obj->pos))
        return;
    x = Terrain_Round(pos->x) + t->offset[0];
    y = Terrain_Round(pos->y) + t->offset[1];
    z = Terrain_Round(pos->z) + t->offset[2];
    x0 = Terrain_Round(min->x) + t->offset[0];
    y0 = Terrain_Round(min->y) + t->offset[1];
    z0 = Terrain_Round(min->z) + t->offset[2];
    x1 = Terrain_Round(max->x) + t->offset[0];
    y1 = Terrain_Round(max->y) + t->offset[1];
    z1 = Terrain_Round(max->z) + t->offset[2];
    ox = Terrain_Round(obj->pos.x) + t->offset[0];
    oy = Terrain_Round(obj->pos.y) + t->offset[1];
    oz = Terrain_Round(obj->pos.z) + t->offset[2];
    obj->pos = *pos;
    dx = (long double)max->x - min->x;
    dy = (long double)max->y - min->y;
    dz = (long double)max->z - min->z;
    obj->radius = x87_sqrt(dz * dz + dy * dy + dx * dx) * 0.5f;
    size = node->size;
    span = DynCube_CellSize(ox, oy, oz, x, y, z);
    cell = DynCube_CellSize(x0, y0, z0, x1 - 1, y1 - 1, z1 - 1);
    if (cell > span)
        span = cell;
    else if (size == span && span == cell)
        return;
    DynCubeNode_Unlink(node, obj);
    if (size < span)
        DynCube_Ascend(t, span, size, &node);
    DynCube_Insert(t, node, obj, cell, x, y, z);
}
HOOK(0x4C6E70, DynCube_MoveBox);

static void count_slots(Terrain *t, int tree, const DynCubeObj *obj, int add)
{
    int i;

    for (i = 0; i < TASK_MAX_SLOTS; i++)
        t->slot_count[tree][i] += add * (gTaskHandlers[i][obj->task.type] != 0);
}

void DynCube_Add(Terrain *t, DynCubeObj *obj, const Vec3d *pos, float radius, int category)
{
    int tree = DynCube_GetTree(category, obj->task.type);
    DynCubeNode *root = DynCube_GetRoot(t, tree);
    int x, y, z, r;
    unsigned int cell;

    x = Terrain_Round(pos->x) + t->offset[0];
    y = Terrain_Round(pos->y) + t->offset[1];
    z = Terrain_Round(pos->z) + t->offset[2];
    r = Terrain_Round(radius);
    cell = DynCube_CellSize(x - r, y - r, z - r, x + r, y + r, z + r);
    obj->pos = *pos;
    obj->radius = radius;
    obj->mask = gTaskTypes[obj->task.type].mask;
    obj->unk60 = 0;
    DynCube_Insert(t, root, obj, cell, x, y, z);
    count_slots(t, tree, obj, 1);
}
HOOK(0x4C7140, DynCube_Add);

/* Category bits 1, 2, 4, 8 to tree 0-5. Category 2 is split by which
   event handlers the task type has. */
int DynCube_GetTree(int category, uint16_t type)
{
    int a = gTaskHandlers[4][type] || gTaskHandlers[5][type] || gTaskHandlers[15][type]
            || gTaskHandlers[16][type];
    int b = gTaskHandlers[11][type] || gTaskHandlers[9][type] || gTaskHandlers[10][type]
            || gTaskHandlers[8][type];

    if (category == 4)
        return 4;
    if (category == 8)
        return 5;
    if (category == 2) {
        if (a)
            return b ? 1 : 2;
        return b ? 1 : 3;
    }
    if (category != 1) {
        Error_Show("Invalid category");
        for (;;)
            ;
    }
    return 0;
}
HOOK(0x4C7290, DynCube_GetTree);

void DynCube_AddBox(Terrain *t, DynCubeObj *obj, const Vec3d *pos, const Vec3d *min, const Vec3d *max,
                    int category)
{
    int tree = DynCube_GetTree(category, obj->task.type);
    DynCubeNode *root = DynCube_GetRoot(t, tree);
    int x, y, z, x0, y0, z0, x1, y1, z1;
    unsigned int cell;

    x = Terrain_Round(pos->x) + t->offset[0];
    y = Terrain_Round(pos->y) + t->offset[1];
    z = Terrain_Round(pos->z) + t->offset[2];
    x0 = Terrain_Round(min->x) + t->offset[0];
    y0 = Terrain_Round(min->y) + t->offset[1];
    z0 = Terrain_Round(min->z) + t->offset[2];
    x1 = Terrain_Round(max->x) + t->offset[0];
    y1 = Terrain_Round(max->y) + t->offset[1];
    z1 = Terrain_Round(max->z) + t->offset[2];
    obj->pos = *pos;
    /* the original stores sqrt(0) * 0.5 */
    obj->radius = 0.0f;
    obj->mask = gTaskTypes[obj->task.type].mask;
    obj->unk60 = 0;
    cell = DynCube_CellSize(x0, y0, z0, x1 - 1, y1 - 1, z1 - 1);
    DynCube_Insert(t, root, obj, cell, x, y, z);
    count_slots(t, tree, obj, 1);
}
HOOK(0x4C7350, DynCube_AddBox);

void DynCube_Remove(Terrain *t, DynCubeObj *obj)
{
    DynCubeNode *node = obj->node;

    t->start_valid[node->tree] = 0;
    DynCubeNode_Unlink(node, obj);
    DynCube_Prune(t, node);
    count_slots(t, node->tree, obj, -1);
    obj->node = NULL;
}
HOOK(0x4C7560, DynCube_Remove);

/* Recomputes masks up the tree after an object left node, freeing empty nodes. */
void DynCube_Prune(Terrain *t, DynCubeNode *node)
{
    uint16_t m = objs_mask(node);
    int changed = m != node->mask;

    node->mask = m;
    for (;;) {
        int i = node->child;
        DynCubeNode *parent = node->parent;

        if (!parent)
            return;
        if (!node->child_mask && !node->objs)
            free_child(t, parent, i);
        else if (!changed)
            return;
        if (changed) {
            m = children_mask(parent);
            if (parent->sub_mask == m)
                changed = 0;
            parent->sub_mask = m;
        }
        node = parent;
    }
}
HOOK(0x4C75F0, DynCube_Prune);

DynCubeNode *DynCube_GetRoot(Terrain *t, int tree)
{
    return t->roots[tree];
}
HOOK(0x4C7750, DynCube_GetRoot);

void sub_4C7760(DynCubeObj *obj)
{
    sub_4D97F0(obj, 0);
}
HOOK(0x4C7760, sub_4C7760);
