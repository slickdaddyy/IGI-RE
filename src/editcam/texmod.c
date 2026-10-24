#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "editcam/editcam.h"

FN_EXTERN(Task_ReturnZero);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4E5360, uint8_t, DynCubeObj_GetMaskVisibleEvent, (void));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B47E0, void, Random_Seed, (void *r, unsigned int seed));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4C7350, void, DynCube_AddBox, (void *world, void *obj, Vec3d *center, Vec3d *min, Vec3d *max, int tree));
ORIG(0x4C6E70, void, DynCube_MoveBox, (void *world, void *obj, Vec3d *center, Vec3d *min, Vec3d *max));
ORIG(0x4CABF0, void, Coll_DropPoints, (void *world, CollHit *hit, Vec3d *points, int count, const double *dz,
                                       int unk, int unk2, unsigned int flags));

#define DynCubeObj_MaskVisible (FN(0x4F64E0, Task_ReturnZero))

/* a box of terrain whose .bit map entry marks material holes */
typedef struct TextureModifier {
    DynCubeObj obj;
    Terrain *world;
    int unk6C;
    Vec3d place;
    Vec3d min;
    Vec3d max;
    int shift;          /* cell size is 2^31 >> shift */
    int bitIndex;       /* .bit table entry, -1 none */
    int lastBit;
    int material;
    int texShift;
    int sizeShift;
    char exclusive;     /* the only entry with its flag set */
    char unkD1;
    char shared;
    char moved;
    char isChild;
    char dropToGround;
    char unkD6[2];
    int size;           /* map width in cells */
    int shown;
    int unkE0;
    int slot;           /* in gTexMods and gCollHoles */
    Task *children;
    int unkEC;
} TextureModifier;

_Static_assert(sizeof(TextureModifier) == 0xF0, "TextureModifier");
_Static_assert(offsetof(TextureModifier, slot) == 0xE4, "TextureModifier.slot");

extern void *gTexModParams;
extern uint16_t gTexModType;
extern TextureModifier *gTexMods[];
extern char gTexModRandom[];
extern int gTexModCount;
extern char gBitResident[];
extern int gCollHoleList[];
extern int gCollHoleCount;
extern unsigned int gCollHoleBits;
extern CollHole gCollHoles[];

static const double kLiftZ = 268435456.0;
static const double kHalf = 0.5;
static const double kDropZ = -536870912.0;
static const double kLayerGap = 4096.0;
static const float kOne = 1.0f;

void TextureModifier_Create(TextureModifier *tm)
{
    tm->world = gCurrentWorld;
    tm->bitIndex = -1;
    tm->lastBit = -1;
    tm->slot = -1;
    tm->place.x = tm->place.y = tm->place.z = 0;
    tm->min.x = tm->min.y = tm->min.z = 0;
    tm->max.x = tm->max.y = tm->max.z = 0;
    tm->shift = 10;
    tm->material = 0;
    tm->exclusive = 0;
    tm->size = 0x20;
    tm->unkD1 = 0;
    tm->isChild = 0;
    tm->unkE0 = 0;
    tm->shared = 0;
    tm->moved = 1;
    tm->children = NULL;
    tm->dropToGround = 1;
    Random_Seed(gTexModRandom, 0x19111911);
}
HOOK(0x505D50, TextureModifier_Create);

/* pushes the hole while the walk is inside the box */
void TextureModifier_Enter(TextureModifier *tm)
{
    if (tm->bitIndex >= 0 && tm->world->bit[tm->bitIndex].data) {
        if (gCollHoleCount >= 20) {
            Error_Show("Too many items in texture modifier stack!");
            for (;;)
                ;
        }
        tm->shown = 1;
        gCollHoleList[gCollHoleCount] = tm->slot;
        gCollHoleBits = gCollHoleBits << 8 | (tm->slot + 1);
        gCollHoleCount++;
        return;
    }
    tm->shown = 0;
}
HOOK(0x505E40, TextureModifier_Enter);

void TextureModifier_Leave(TextureModifier *tm)
{
    int n;

    if (!tm->shown)
        return;
    n = --gCollHoleCount;
    gCollHoleBits >>= 8;
    if (n - 4 >= 0)
        gCollHoleBits |= (gCollHoleList[n - 4] << 24) + 1;
}
HOOK(0x505ED0, TextureModifier_Leave);

void TextureModifier_Collect(TextureModifier *tm)
{
    if (tm->bitIndex >= 0 && gCollHoles[tm->slot].bits)
        gCollHoleList[gCollHoleCount++] = tm->slot;
}
HOOK(0x505F20, TextureModifier_Collect);

void TextureModifier_Delete(TextureModifier *tm);
void TextureModifier_GetParams(TextureModifier *tm, void **out);
void TextureModifier_Changed(TextureModifier *tm);

void TextureModifier_Init(void)
{
    TextureModifier t;
    uint16_t type;

    Random_Init(gTexModRandom);
    Random_Seed(gTexModRandom, 0x19111911);
    type = TaskType_New("TextureModifier", sizeof(TextureModifier), gDynCubeObjType, 0, 0);
    gTexModType = type;
    TaskType_SetHandler(type, 13, TextureModifier_Collect);
    TaskType_SetHandler(gTexModType, 0, TextureModifier_Create);
    TaskType_SetHandler(gTexModType, 16, TextureModifier_Leave);
    TaskType_SetHandler2(gTexModType, 2, TextureModifier_Delete);
    TaskType_SetHandler(gTexModType, Param_GetTableEvent(), TextureModifier_GetParams);
    TaskType_SetHandler(gTexModType, Param_GetChangedEvent(), TextureModifier_Changed);
    TaskType_SetHandler(gTexModType, 10, NULL);
    TaskType_SetHandler(gTexModType, 11, NULL);
    TaskType_SetHandler(gTexModType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gTexModType, 12, NULL);
    TaskType_SetHandler(gTexModType, 15, TextureModifier_Enter);
    TaskType_SetHandler(gTexModType, DynCubeObj_GetMaskVisibleEvent(), DynCubeObj_MaskVisible);
    TaskType_SetHandler(gTexModType, 8, NULL);
    TaskType_SetHandler(gTexModType, 4, NULL);
    TaskType_SetHandler(gTexModType, 5, NULL);
    gTexModParams = ParamTable_New(9);
    ParamTable_Add(gTexModParams, &t, &t.place, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gTexModParams, &t, &t.shift, Param_Int32(), 7, NULL);
    ParamTable_Add(gTexModParams, &t, &t.material, Param_Int32(), 7, NULL);
    ParamTable_Add(gTexModParams, &t, &t.bitIndex, Param_Int32(), 7, NULL);
    ParamTable_Add(gTexModParams, &t, &t.size, Param_Int32(), 7, NULL);
    ParamTable_Add(gTexModParams, &t, &t.exclusive, Param_Bool8(), 7, NULL);
    ParamTable_Add(gTexModParams, &t, &t.unkD1, Param_PushButton(), 6, NULL);
    ParamTable_Add(gTexModParams, &t, &t.unkE0, Param_PushButton(), 6, NULL);
    ParamTable_Add(gTexModParams, &t, &t.dropToGround, Param_Bool8(), 6, NULL);
    gCollHoleCount = 0;
}
HOOK(0x505F60, TextureModifier_Init);

void TextureModifier_Delete(TextureModifier *tm)
{
    if (!tm->isChild && tm->slot != -1) {
        int last = gTexModCount - 1;

        if (tm->slot != last) {
            TextureModifier *m = gTexMods[last];

            gTexMods[tm->slot] = m;
            m->slot = tm->slot;
        }
        gTexModCount--;
    }
    DynCube_Remove(tm->world, tm);
}
HOOK(0x506240, TextureModifier_Delete);

void TextureModifier_GetParams(TextureModifier *tm, void **out)
{
    *out = gTexModParams;
}
HOOK(0x506290, TextureModifier_GetParams);

int TextureModifier_RoundSize(int n);
TextureModifier *TextureModifier_NewChild(Task *parent, TextureModifier *tm, Vec3d *place);

/* a cleared map of size x size bits */
static void *new_bits(int size)
{
    int n = size * size / 8;
    void *p = Mem_Alloc(n, 4);

    memset(p, 0, n);
    return p;
}

/* Snaps the box to the cell grid, takes a .bit entry and adds a child box
   for every cell height of ground found under it. */
void TextureModifier_Changed(TextureModifier *tm)
{
    double dz = kDropZ;
    int cell2 = (0x40000000 >> tm->shift) * 2;
    int mode = 0;
    TerrainMap *bits = NULL;
    Terrain *w;
    Vec3d p;
    CollHit hit;
    int ix, iy, iz;
    double min_y, min_z;
    long double h;
    CollHole *hole;
    int n;

    if (!tm->isChild) {
        mode = Mem_GetMode();
        if (!mode)
            Mem_SetDefaultMode();
        if (tm->children) {
            Task_Delete(tm->children);
            tm->children = NULL;
        }
        bits = tm->world->bit;
        if (tm->slot == -1) {
            gTexMods[gTexModCount] = tm;
            tm->slot = gTexModCount;
            if (++gTexModCount >= 0x100) {
                Error_Show("Too many texturemodfiers");
                for (;;)
                    ;
            }
        }
    }
    gCollHoles[tm->slot].bits = NULL;
    p = tm->place;
    if (!tm->isChild && tm->dropToGround) {
        Vec3d q = tm->place;
        int i;

        q.z = q.z + kLiftZ;
        hit.found = 0;
        for (i = 0; i < 5; i++) {
            Coll_DropPoints(tm->world, &hit, &q, 1, &dz, 0xFF, 0, 0x80000000);
            if (hit.found)
                break;
        }
        if (hit.found)
            p.z = hit.pos.z;
    }
    w = tm->world;
    p.x = w->offset[0] + p.x;
    p.y = w->offset[1] + p.y;
    p.z = w->offset[2] + p.z;
    ix = x87_fistp(p.x) & -cell2;
    tm->min.x = ix;
    iy = x87_fistp(p.y) & -cell2;
    min_y = iy;
    tm->min.y = min_y;
    iz = x87_fistp(p.z) & -cell2;
    min_z = iz;
    tm->min.z = min_z;
    p = tm->min;
    tm->max.x = (long double)cell2 + ix;
    tm->max.y = (long double)cell2 + min_y;
    tm->max.z = (long double)cell2 + min_z;
    h = cell2 * kHalf;
    p.x = h + p.x;
    p.y = h + p.y;
    p.z = h + p.z;
    p.x = p.x - w->offset[0];
    p.y = p.y - w->offset[1];
    p.z = p.z - w->offset[2];
    tm->min.x = tm->min.x - w->offset[0];
    tm->min.y = tm->min.y - w->offset[1];
    tm->min.z = tm->min.z - w->offset[2];
    tm->max.x = tm->max.x - w->offset[0];
    tm->max.y = tm->max.y - w->offset[1];
    tm->max.z = tm->max.z - w->offset[2];
    if (tm->moved)
        DynCube_AddBox(w, tm, &p, &tm->min, &tm->max, 4);
    DynCube_MoveBox(tm->world, tm, &p, &tm->min, &tm->max);
    if (!tm->isChild) {
        if (tm->bitIndex >= 0x100)
            tm->bitIndex = 0xFF;
        else if (tm->bitIndex < -1)
            tm->bitIndex = -1;
        if (tm->bitIndex != -1) {
            int size = TextureModifier_RoundSize(tm->size);
            int idx = tm->bitIndex;
            int s;

            tm->size = size;
            if (!bits[idx].data) {
                for (idx = 0; idx < 0x100 && bits[idx].data; idx++)
                    ;
                if (idx == 0x100) {
                    Warning_Show("limit reached");
                    idx = 0xFF;
                }
                bits[idx].size = size;
                bits[idx].data = new_bits(size);
                gBitResident[idx] = 0;
                tm->bitIndex = idx;
            } else {
                if (tm->moved)
                    tm->shared = 1;
                if (idx == tm->lastBit) {
                    TerrainMap *e = &bits[idx];

                    if (e->size != size) {
                        if (!gBitResident[idx]) {
                            Mem_Free(e->data);
                            e->data = NULL;
                        } else {
                            tm->shared = 0;
                        }
                        e->size = size;
                        e->data = new_bits(size);
                        gBitResident[idx] = 0;
                    }
                } else {
                    tm->size = bits[idx].size;
                    tm->exclusive = bits[idx].flag;
                }
            }
            switch (size) {
            case 0x100: s = 7; break;
            case 0x80: s = 6; break;
            case 0x40: s = 5; break;
            case 0x20: s = 4; break;
            case 0x10: s = 3; break;
            case 8: s = 2; break;
            default:
                Error_Show("Illegal shift size");
                for (;;)
                    ;
            }
            tm->texShift = 30 - tm->shift - s;
            tm->sizeShift = s - 2;
        }
        if (tm->bitIndex == -1 && tm->lastBit != -1) {
            if (!gBitResident[tm->lastBit]) {
                Mem_Free(bits[tm->lastBit].data);
                bits[tm->lastBit].data = NULL;
            }
            bits[tm->lastBit].data = NULL;
        }
        tm->lastBit = tm->bitIndex;
        if (tm->exclusive) {
            int i;

            for (i = 0; i < 0x100; i++)
                bits[i].flag = 0;
            if (tm->bitIndex >= 0)
                bits[tm->bitIndex].flag = 1;
        } else if (tm->bitIndex >= 0) {
            bits[tm->bitIndex].flag = 0;
        }
        tm->moved = 0;
        if (tm->dropToGround) {
            Vec3d c = tm->obj.pos, q;
            double lo, hi, cell;
            int spawned = 0;

            lo = hi = c.z;
            cell = 0x40000000 >> tm->shift;
            for (n = 50; n; n--) {
                long double r;

                q = c;
                r = Random_Float(gTexModRandom);
                q.x = (r + r - kOne) * cell + q.x;
                r = Random_Float(gTexModRandom);
                q.y = (r + r - kOne) * cell + q.y;
                q.z = q.z + kLiftZ;
                Coll_DropPoints(tm->world, &hit, &q, 1, &dz, 0xFF, 0, 0x80000000);
                while (hit.found) {
                    long double v = hit.pos.z;

                    if (v < lo)
                        lo = v;
                    if (v > hi)
                        hi = v;
                    q = hit.pos;
                    q.z = v - kLayerGap;
                    Coll_DropPoints(tm->world, &hit, &q, 1, &dz, 0xFF, 0, 0x80000000);
                }
            }
            q = c;
            q.z = c.z - cell;
            while (!(lo >= q.z)) {
                if (!spawned) {
                    tm->children = Task_New(tm, Task_GetRootType(), NULL);
                    spawned = 1;
                }
                q.z = q.z - cell;
                TextureModifier_NewChild(tm->children, tm, &q);
                q.z = q.z - cell;
            }
            q = c;
            q.z = cell + c.z;
            while (hi > q.z) {
                if (!spawned) {
                    tm->children = Task_New(tm, Task_GetRootType(), NULL);
                    spawned = 1;
                }
                q.z = q.z + cell;
                TextureModifier_NewChild(tm->children, tm, &q);
                q.z = q.z + cell;
            }
        }
        if (!mode)
            Mem_ClearMode();
    }
    hole = &gCollHoles[tm->slot];
    bits = tm->world->bit;
    hole->sizeShift = tm->sizeShift;
    n = bits[tm->bitIndex].size;
    hole->scale = (long double)(n - 1) / n;
    hole->material = tm->material;
    hole->texShift = tm->texShift;
    hole->bits = bits[tm->bitIndex].data;
    hole->scale = kOne / (1 << hole->texShift) * hole->scale;
    hole->x = tm->obj.pos.x - (0x40000000 >> tm->shift);
    hole->y = tm->obj.pos.y - (0x40000000 >> tm->shift);
    tm->moved = 0;
    tm->exclusive = bits[tm->bitIndex].flag;
}
HOOK(0x5062A0, TextureModifier_Changed);

int TextureModifier_RoundSize(int n)
{
    if (n >= 0x100)
        return 0x100;
    if (n >= 0x80)
        return 0x80;
    if (n >= 0x40)
        return 0x40;
    if (n >= 0x20)
        return 0x20;
    return n < 0x10 ? 8 : 0x10;
}
HOOK(0x506C60, TextureModifier_RoundSize);

void TextureModifier_Shutdown(void)
{
    QTaskList_Free(gTexModParams);
    TaskType_Free(gTexModType);
}
HOOK(0x506CB0, TextureModifier_Shutdown);

/* a box one cell above or below, sharing the parent's map */
TextureModifier *TextureModifier_NewChild(Task *parent, TextureModifier *tm, Vec3d *place)
{
    TextureModifier *t;
    TaskFn fn;

    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        t = (TextureModifier *)Task_New(parent, gTexModType, NULL);
        Mem_ClearMode();
    } else {
        t = (TextureModifier *)Task_New(parent, gTexModType, NULL);
    }
    t->isChild = 1;
    t->shift = tm->shift;
    t->size = tm->size;
    t->texShift = tm->texShift;
    t->sizeShift = tm->sizeShift;
    t->material = tm->material;
    t->slot = tm->slot;
    t->bitIndex = tm->bitIndex;
    t->max = tm->max;
    t->min = tm->min;
    t->place = *place;
    fn = HANDLER(Param_GetChangedEvent(), t->obj.task.type);
    if (fn)
        fn(t, NULL);
    return t;
}
HOOK(0x506CD0, TextureModifier_NewChild);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A7A8EC) void *gTexModParams;
BSS(A7A8EC) uint16_t gTexModType;
PAD(igb, A7A8EC, A7A8F2, 0x2);
PAD(igb, A7A8EC, A7A8F4, 0x404);
ALIAS(gTexMods, pad_A7A8F4, 0);
PAD(igb, A7A8EC, A7ACF8, 0x10);
ALIAS(gTexModRandom, pad_A7ACF8, 0);
BSS(A7A8EC) int gTexModCount;
PAD(igb, B81A20, B81A20, 0x100);
ALIAS(gBitResident, pad_B81A20, 0);
ALIAS(gUnkB81A20, pad_B81A20, 0);
PAD(igb, B97DA0, B97DA0, 0x50);
ALIAS(gCollHoleList, pad_B97DA0, 0);
PAD(igb, BA5080, BA5080, 0x3000);
ALIAS(gCollHoles, pad_BA5080, 0);
