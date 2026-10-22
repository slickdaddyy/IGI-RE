#pragma GCC optimize("excess-precision=standard")

#include <float.h>

#include "hook.h"
#include "igi.h"
#include "math/x87.h"
#include "terrain/terrain.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B3360, char, Vec3d_Equal, (const Vec3d *a, const Vec3d *b));
ORIG(0x4C5C90, void, DynCubeNode_Init, (Terrain *t, DynCubeNode *node, int child, unsigned int size, int tree));
ORIG(0x4CE5C0, void, DynCube_UpdateSlotCounts, (Terrain *t, int tree));
ORIG(0x4CE620, void, DynCube_UpdateStart, (Terrain *t, int tree));
ORIG(0x4D97F0, void, sub_4D97F0, (DynCubeObj *obj, int v));
ORIG(0x4E54E0, void, sub_4E54E0, (void *p));
ORIG(0x4E5580, int, CellMask_Test, (const void *mask, int x, int y));
ORIG(0x4E5BA0, int, CellMask_TestRect, (const void *mask, float x, float y, float w, float h));

#define gTaskTypes ((TaskType *)0xAF5EE0)
#define gTaskHandlers ((TaskHandler (*)[TASK_MAX_TYPES])0xA96AE0)
#define gTaskSlotEvent ((const signed char *)0xA969E0)
#define gDynCubeChildDir ((const int (*)[3])0x548000)
#define gPixelCenter VAR(const float, 0x533504)
#define gSqrt3 VAR(const float, 0x534AC8)

/* Visibility bitmap a node may carry in unk3C. */
typedef struct CellMask {
    void *unk0;
    int shift;
    int size;
} CellMask;

/* One level of the explicit stack used by the event queries. */
typedef struct QueryFrame {
    DynCubeNode *node;
    uint8_t next;   /* child to visit next */
    uint8_t todo;   /* children still to visit */
    uint8_t inside; /* the whole node lies inside the query box */
    int x, y, z;    /* node center */
} QueryFrame;

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

/* Lowest set bit, 7 for none. */
static inline int first_child(unsigned int m)
{
    return m ? __builtin_ctz(m) : 7;
}

/* Copy of the node where queries start. The root is seen mirrored, and
   only its children 0-3 count as present. */
static void start_node(Terrain *t, int tree, int depth, DynCubeNode *copy)
{
    uint8_t m;
    int i;

    *copy = *t->start[tree];
    if (depth != 30)
        return;
    m = copy->child_mask;
    copy->child_mask = 0;
    for (i = 0; i < 4; i++) {
        DynCubeNode *c = copy->children[i];

        copy->children[i] = copy->children[7 - i];
        copy->children[7 - i] = c;
        copy->child_mask |= ((m >> i) & 1) << (7 - i);
    }
}

static unsigned int event_mask(uint8_t slot)
{
    if (gTaskSlotEvent[slot] != -1)
        return 1u << (gTaskSlotEvent[slot] & 31);
    return 0xFFFF;
}

/* Calls the slot handler of the tasks in node whose sphere touches the box lo-hi. */
static void send_box(DynCubeNode *node, uint8_t slot, void *arg, unsigned int flags, const Vec3d *lo, const Vec3d *hi)
{
    DynCubeObj *o;

    for (o = node->objs; o; o = o->next) {
        long double r = o->radius;
        TaskHandler h;

        if (o->pos.x - r > hi->x || !(o->pos.x + r >= lo->x))
            continue;
        if (o->pos.y - r > hi->y || !(o->pos.y + r >= lo->y))
            continue;
        if (o->pos.z - r > hi->z || !(o->pos.z + r >= lo->z))
            continue;
        if (!(gTaskTypes[o->task.type].flags & flags))
            continue;
        h = gTaskHandlers[slot][o->task.type];
        if (h)
            h(&o->task, arg);
    }
}

static void send_all(DynCubeNode *node, uint8_t slot, void *arg, unsigned int flags)
{
    DynCubeObj *o;

    for (o = node->objs; o; o = o->next) {
        TaskHandler h;

        if (!(gTaskTypes[o->task.type].flags & flags))
            continue;
        h = gTaskHandlers[slot][o->task.type];
        if (h)
            h(&o->task, arg);
    }
}

/* Whether the cell mask of a node of the given size, whose corner is at
   (x, y) in world space, has anything set under the box lo-hi. Boxes up to
   4 units wide test a single cell. */
static int cellmask_pass(const CellMask *cm, unsigned int size, int x, int y, int y_point, const Vec3d *lo,
                         const Vec3d *hi)
{
    long double dx = (long double)hi->x - lo->x;
    float fsize, scale;
    int ix, iy;

    if (dx > 4.0 || (long double)hi->y - lo->y > 4.0) {
        double fy = y;

        fsize = size;
        scale = (long double)cm->size * gPixelCenter / fsize;
        return CellMask_TestRect(cm, ((lo->x - (long double)x) + fsize) * scale,
                                 ((lo->y - (long double)fy) + fsize) * scale, scale * dx,
                                 ((long double)hi->y - lo->y) * scale) != 0;
    }
    fsize = size;
    scale = (long double)cm->size * gPixelCenter / fsize;
    ix = Terrain_Round(((lo->x - (long double)x) + fsize) * scale);
    iy = Terrain_Round(((lo->y - (long double)y_point) + fsize) * scale);
    if ((unsigned int)ix >= (unsigned int)cm->size || (unsigned int)iy >= (unsigned int)cm->size)
        return 1;
    return CellMask_Test(cm, ix, iy) != 0;
}

static inline unsigned int child_x(int i, int depth)
{
    return (unsigned int)gDynCubeChildDir[i][0] << (depth & 31);
}

static inline unsigned int child_y(int i, int depth)
{
    return (unsigned int)gDynCubeChildDir[i][1] << (depth & 31);
}

static inline unsigned int child_z(int i, int depth)
{
    return (unsigned int)gDynCubeChildDir[i][2] << (depth & 31);
}

/* Children of a node centered at (x, y, z) that the box lo-hi can touch. */
static uint8_t clip_box(uint8_t m, int x, int y, int z, const int *lo, const int *hi)
{
    if (hi[0] <= x)
        m &= 0x55;
    else if (lo[0] >= x)
        m &= 0xAA;
    if (hi[1] <= y)
        m &= 0x33;
    else if (lo[1] >= y)
        m &= 0xCC;
    if (hi[2] <= z)
        m &= 0x0F;
    else if (lo[2] >= z)
        m &= 0xF0;
    return m;
}

/* Calls the slot handler of every task of the tree whose sphere touches
   the box min-max and whose type has one of the flags. */
void DynCube_SendEventBox(Terrain *t, uint8_t slot, void *arg, const Vec3d *min, const Vec3d *max,
                          unsigned int flags, int tree)
{
    QueryFrame stack[50], *f = stack;
    DynCubeNode start;
    Vec3d lo_d, hi_d;
    int lo[3], hi[3], depth, n = 1;
    unsigned int evmask, size;

    DynCube_UpdateSlotCounts(t, tree);
    if (!t->slot_count[tree][slot])
        return;
    lo_d = *min;
    hi_d = *max;
    lo[0] = Terrain_Round(lo_d.x) + t->offset[0];
    lo[1] = Terrain_Round(lo_d.y) + t->offset[1];
    lo[2] = Terrain_Round(lo_d.z) + t->offset[2];
    hi[0] = Terrain_Round(hi_d.x) + t->offset[0];
    hi[1] = Terrain_Round(hi_d.y) + t->offset[1];
    hi[2] = Terrain_Round(hi_d.z) + t->offset[2];
    evmask = event_mask(slot);
    DynCube_UpdateStart(t, tree);
    depth = t->start_depth[tree];
    start_node(t, tree, depth, &start);
    size = start.size;
    f->node = &start;
    f->inside = 0;
    f->x = Terrain_Round(t->offset[0] + (long double)t->start_pos[tree].x);
    f->y = Terrain_Round(t->offset[1] + (long double)t->start_pos[tree].y);
    f->z = Terrain_Round(t->offset[2] + (long double)t->start_pos[tree].z);
    f->todo = clip_box(start.child_mask, f->x, f->y, f->z, lo, hi);
    f->next = first_child(f->todo);
    do {
        QueryFrame *p;
        DynCubeNode *c;
        uint8_t m;
        int i, k;

        if (!f->todo) {
            if ((uint16_t)(f->node->mask & evmask)) {
                if (f->inside)
                    send_all(f->node, slot, arg, flags);
                else
                    send_box(f->node, slot, arg, flags, &lo_d, &hi_d);
            }
            f--;
            depth++;
            size <<= 1;
            n--;
            continue;
        }
        i = f->next;
        c = f->node->children[i];
        f->todo ^= 1 << i;
        if (f->todo)
            f->next = first_child(f->todo);
        size >>= 1;
        depth--;
        n++;
        p = f++;
        f->node = c;
        f->todo = m = c->child_mask;
        if (p->inside) {
            f->inside = 1;
            if (m)
                f->next = first_child(m);
            continue;
        }
        f->inside = 0;
        if (!m)
            continue;
        f->x = (int)(child_x(i, depth) + p->x);
        f->y = (int)(child_y(i, depth) + p->y);
        f->z = (int)(child_z(i, depth) + p->z);
        m = clip_box(m, f->x, f->y, f->z, lo, hi);
        for (k = 0; k < 8; k++) {
            DynCubeNode *cc = c->children[k];
            int wx, wy;

            if (!(m & (1 << k)))
                continue;
            if (!(uint16_t)((cc->sub_mask | cc->mask) & evmask)) {
                m &= ~(1 << k);
                continue;
            }
            if (!cc->unk3C)
                continue;
            wx = (int)(child_x(k, depth - 1) - t->offset[0] + f->x);
            wy = (int)(child_y(k, depth - 1) - t->offset[1] + f->y);
            if (!cellmask_pass(cc->unk3C, cc->size, wx, wy, wy, min, max))
                m &= ~(1 << k);
        }
        f->todo = m;
        if (m)
            f->next = first_child(m);
        if (hi[0] >= (int)(f->x + size) && lo[0] <= (int)(f->x - size) && hi[1] >= (int)(f->y + size)
            && lo[1] <= (int)(f->y - size) && hi[2] >= (int)(f->z + size) && lo[2] <= (int)(f->z - size))
            f->inside = 1;
    } while (n);
}
HOOK(0x4C7770, DynCube_SendEventBox);

/* Like clip_box, but a child is only dropped when the box is strictly on the other side. */
static uint8_t clip_lines(uint8_t m, int x, int y, int z, const int *lo, const int *hi)
{
    if (hi[0] < x)
        m &= 0x55;
    if (lo[0] > x)
        m &= 0xAA;
    if (hi[1] < y)
        m &= 0x33;
    if (lo[1] > y)
        m &= 0xCC;
    if (hi[2] < z)
        m &= 0x0F;
    if (lo[2] > z)
        m &= 0xF0;
    return m;
}

#define LINES_MAX 128

/* Calls the slot handler of every task of the tree whose sphere touches
   the bounding box of the segments starts[i]-ends[i]. Cells are skipped
   when their bounding sphere misses every segment's line and box. */
void DynCube_SendEventLines(Terrain *t, uint8_t slot, void *arg, const Vec3d *starts, const Vec3d *ends, int count,
                            unsigned int flags, int tree)
{
    QueryFrame stack[50], *f = stack;
    DynCubeNode start;
    float dir[LINES_MAX][3], inv_len2[LINES_MAX];
    Vec3d seg_lo[LINES_MAX], seg_hi[LINES_MAX], lo_d, hi_d;
    int lo[3], hi[3], depth, n = 1, i;
    unsigned int evmask, size;

    DynCube_UpdateSlotCounts(t, tree);
    if (!t->slot_count[tree][slot])
        return;
    for (i = 0; i < count; i++) {
        long double dz;

        dir[i][0] = (long double)ends[i].x - starts[i].x;
        dir[i][1] = (long double)ends[i].y - starts[i].y;
        dz = (long double)ends[i].z - starts[i].z;
        dir[i][2] = dz;
        inv_len2[i] = 1.0f / ((long double)dir[i][0] * dir[i][0] + dz * dz + (long double)dir[i][1] * dir[i][1]);
    }
    lo_d.x = lo_d.y = lo_d.z = DBL_MAX;
    hi_d.x = hi_d.y = hi_d.z = -DBL_MAX;
    for (i = 0; i < count; i++) {
        const Vec3d *s = &starts[i], *e = &ends[i];

        seg_lo[i].x = !(s->x >= e->x) ? s->x : e->x;
        seg_lo[i].y = e->y > s->y ? s->y : e->y;
        seg_lo[i].z = e->z > s->z ? s->z : e->z;
        seg_hi[i].x = s->x > e->x ? s->x : e->x;
        seg_hi[i].y = !(e->y >= s->y) ? s->y : e->y;
        seg_hi[i].z = !(e->z >= s->z) ? s->z : e->z;
        if (!(seg_lo[i].x >= lo_d.x))
            lo_d.x = seg_lo[i].x;
        if (!(seg_lo[i].y >= lo_d.y))
            lo_d.y = seg_lo[i].y;
        if (!(seg_lo[i].z >= lo_d.z))
            lo_d.z = seg_lo[i].z;
        if (seg_hi[i].x > hi_d.x)
            hi_d.x = seg_hi[i].x;
        if (seg_hi[i].y > hi_d.y)
            hi_d.y = seg_hi[i].y;
        if (seg_hi[i].z > hi_d.z)
            hi_d.z = seg_hi[i].z;
    }
    lo[0] = Terrain_Round(lo_d.x) + t->offset[0];
    lo[1] = Terrain_Round(lo_d.y) + t->offset[1];
    lo[2] = Terrain_Round(lo_d.z) + t->offset[2];
    hi[0] = Terrain_Round(hi_d.x) + t->offset[0];
    hi[1] = Terrain_Round(hi_d.y) + t->offset[1];
    hi[2] = Terrain_Round(hi_d.z) + t->offset[2];
    evmask = event_mask(slot);
    DynCube_UpdateStart(t, tree);
    depth = t->start_depth[tree];
    start_node(t, tree, depth, &start);
    size = start.size;
    f->node = &start;
    f->x = Terrain_Round(t->offset[0] + (long double)t->start_pos[tree].x);
    f->y = Terrain_Round(t->offset[1] + (long double)t->start_pos[tree].y);
    f->z = Terrain_Round(t->offset[2] + (long double)t->start_pos[tree].z);
    f->todo = clip_lines(start.child_mask, f->x, f->y, f->z, lo, hi);
    f->next = first_child(f->todo);
    do {
        QueryFrame *p;
        DynCubeNode *c;
        double cx, cy, cz;
        long double h;
        float half, r;
        uint8_t m;
        int k;

        if (!f->todo) {
            if ((uint16_t)(f->node->mask & evmask))
                send_box(f->node, slot, arg, flags, &lo_d, &hi_d);
            f--;
            depth++;
            size <<= 1;
            n--;
            continue;
        }
        i = f->next;
        c = f->node->children[i];
        f->todo ^= 1 << i;
        if (f->todo)
            f->next = first_child(f->todo);
        size >>= 1;
        depth--;
        n++;
        p = f++;
        f->node = c;
        f->todo = m = c->child_mask;
        if (!m)
            continue;
        f->x = (int)(child_x(i, depth) + p->x);
        f->y = (int)(child_y(i, depth) + p->y);
        f->z = (int)(child_z(i, depth) + p->z);
        cx = (int)((unsigned int)f->x - t->offset[0]);
        cy = (int)((unsigned int)f->y - t->offset[1]);
        cz = (int)((unsigned int)f->z - t->offset[2]);
        h = size >> 1;
        half = h;
        r = h * gSqrt3;
        for (k = 0; k < 8; k++) {
            DynCubeNode *cc = c->children[k];
            double px, py, pz;
            int hit = 0, j;

            if (!(m & (1 << k)))
                continue;
            if (!(uint16_t)((cc->sub_mask | cc->mask) & evmask)) {
                m &= ~(1 << k);
                continue;
            }
            px = (long double)((k & 1) * 2 - 1) * half + cx;
            py = (long double)((k & 2) - 1) * half + cy;
            pz = (long double)(((k >> 1) & 2) - 1) * half + cz;
            for (j = 0; j < count && !hit; j++) {
                long double dx = px - starts[j].x, dy = py - starts[j].y, dz = pz - starts[j].z;
                double ddx = dx, ddy = dy, ddz = dz;
                long double dot = dz * dir[j][2] + dx * dir[j][0] + dy * dir[j][1];
                long double len2 = (long double)ddx * ddx + (long double)ddy * ddy + (long double)ddz * ddz;

                if (!((long double)r * r >= len2 - dot * dot * inv_len2[j]))
                    continue;
                if (!(r + px >= seg_lo[j].x) || !(r + py >= seg_lo[j].y) || !(r + pz >= seg_lo[j].z))
                    continue;
                if (px - r > seg_hi[j].x || py - r > seg_hi[j].y || pz - r > seg_hi[j].z)
                    continue;
                hit = 1;
            }
            if (!hit)
                m &= ~(1 << k);
            if (cc->unk3C) {
                int wx = (int)(child_x(k, depth - 1) - t->offset[0] + f->x);
                int wy = (int)(child_y(k, depth - 1) - t->offset[1] + f->y);
                /* the single cell test uses the x offset for y */
                int wy_point = (int)(child_y(k, depth - 1) - t->offset[0] + f->y);

                if (!cellmask_pass(cc->unk3C, cc->size, wx, wy, wy_point, &lo_d, &hi_d))
                    m &= ~(1 << k);
            }
        }
        f->todo = m;
        if (m)
            f->next = first_child(m);
    } while (n);
}
HOOK(0x4C8130, DynCube_SendEventLines);
