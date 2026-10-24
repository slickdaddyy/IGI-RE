#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "binocular/binocular.h"

FN_EXTERN(EditorMagicObj_GetArgSize);
FN_EXTERN(ProxMine_Move);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4071D0, uint8_t, WeaponConfig_GetEvent, (void));
ORIG(0x413B30, uint8_t, WeaponType_GetSizeEvent2, (void));
ORIG(0x413B40, uint8_t, WeaponType_GetInitEvent2, (void));
ORIG(0x460BF0, uint8_t, Human_GetEvent0, (void));
ORIG(0x460C10, uint8_t, Human_GetEvent1, (void));
ORIG(0x4659D0, void, HumanView_SetZoomed, (void *view));
ORIG(0x472600, Task *, BinocularOverlay_New, (void *parent));
ORIG(0x472630, void, BinocularOverlay_SetPos, (Task *o, const Vec3d *pos));
ORIG(0x472650, void, BinocularOverlay_SetUnk7C, (Task *o, const float *v));
ORIG(0x477BF0, uint16_t, Gun_GetType, (void));
ORIG(0x487190, uint8_t, StatusScreen_GetEvent, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1FE0, uint8_t, Weapon_GetMoveEvent, (void));
ORIG(0x4F2030, int, Weapon_GetDeselectEvent, (void));
ORIG(0x4FE650, char *, TagItem_AddReal32, (void *config, const char *name, float value));
ORIG(0x4FE730, char *, TagItem_Find, (void *config, const char *name));

#define WEAPON_HANDLER_4754C0 (FN(0x4754C0, ProxMine_Move))
#define WEAPON_GETARGSIZE_51BAA0 (FN(0x51BAA0, EditorMagicObj_GetArgSize))

extern uint16_t gBinocularType;

/* "Binocular" (0x1C8 bytes), a weapon */
typedef struct Binocular {
    RigidDynCubeObj obj;
    char unkF0[0x14];
    char id[0x24];          /* 0x104 */
    char *weaponType;       /* 0x128, its +0x6E4 holds the zoom limits */
    char unk12C[0x24];
    Vec3d pos;              /* 0x150 */
    float dir[3];           /* 0x168 */
    char unk174[0x3C];
    int zoomed;             /* 0x1B0 */
    int unk1B4;
    int busy;               /* 0x1B8 */
    int toggled;            /* 0x1BC */
    Task **overlay;         /* 0x1C0 */
    Task **sound;           /* 0x1C4 */
} Binocular;

_Static_assert(offsetof(Binocular, weaponType) == 0x128, "Binocular");
_Static_assert(offsetof(Binocular, zoomed) == 0x1B0, "Binocular");
_Static_assert(sizeof(Binocular) == 0x1C8, "Binocular");

#define HUMAN_F(h, off) (*(float *)((h) + (off)))
#define VIEW_ZOOM(v) (*(float *)((char *)(v) + 0x1E4))

F32(kZero, 0x00000000);
F32(kZoomOut, 0x3F851EB8);  /* 1.04 */
F32(kZoomIn, 0x3F75C28F);   /* 0.96 */
F32(kDegToRad, 0x3C8EFA35);
F32(kHalf, 0x3F000000);
static const double kOne = 1.0;

void Binocular_Create(Binocular *b, void *arg);
void Binocular_Delete(Binocular *b);
void Binocular_Use(Binocular *b, WeaponUseArg *a);
void Binocular_StatusDisplay(Binocular *b, void *arg);
void Binocular_ParamsChanged(Binocular *b, void *arg);
void Binocular_ParseArg(void *unused, ScriptArg *a);
void Binocular_Unzoom(Binocular *b);
void Binocular_Config(void *unused, ConfigArg *a);

void Binocular_Open(void)
{
    gBinocularType = TaskType_New("Binocular", 0x1C8, Gun_GetType(), 0);
    TaskType_SetHandler(gBinocularType, 0, Binocular_Create);
    TaskType_SetHandler2(gBinocularType, 2, Binocular_Delete);
    TaskType_SetHandler(gBinocularType, Weapon_GetMoveEvent(), WEAPON_HANDLER_4754C0);
    TaskType_SetHandler(gBinocularType, Human_GetEvent0(), Binocular_Use);
    TaskType_SetHandler(gBinocularType, Human_GetEvent1(), TASK_NOTHING);
    TaskType_SetHandler(gBinocularType, StatusScreen_GetEvent(), Binocular_StatusDisplay);
    TaskType_SetHandler(gBinocularType, Param_GetChangedEvent(), Binocular_ParamsChanged);
    TaskType_SetHandler(gBinocularType, WeaponType_GetInitEvent2(), Binocular_ParseArg);
    TaskType_SetHandler(gBinocularType, WeaponType_GetSizeEvent2(), WEAPON_GETARGSIZE_51BAA0);
    TaskType_SetHandler2(gBinocularType, Weapon_GetDeselectEvent(), Binocular_Unzoom);
    TaskType_SetHandler(gBinocularType, WeaponConfig_GetEvent(), Binocular_Config);
    Script_RegisterConstant("TASKTYPE_BINOCULAR", gBinocularType);
}
HOOK(0x470610, Binocular_Open);

void Binocular_Create(Binocular *b, void *arg)
{
    HANDLER(0, Gun_GetType())(b, arg);
    crt_sprintf(b->id, "WEAPON_ID_BINOCULARS");
    b->zoomed = 0;
    b->unk1B4 = 0;
    b->busy = 0;
    b->toggled = 0;
    b->overlay = TaskRef_Add(BinocularOverlay_New(b));
    b->sound = NULL;
}
HOOK(0x470760, Binocular_Create);

void Binocular_StopSound(Binocular *b);

void Binocular_Delete(Binocular *b)
{
    Binocular_StopSound(b);
    TaskRef_Remove(b->overlay);
    ((void (*)(void *))HANDLER(2, Gun_GetType()))(b);
}
HOOK(0x4707D0, Binocular_Delete);

void Binocular_StopSound(Binocular *b)
{
    if (b->sound) {
        if (*b->sound)
            Sound_Stop(*b->sound);
        TaskRef_Remove(b->sound);
        b->sound = NULL;
    }
}
HOOK(0x470800, Binocular_StopSound);

void Binocular_Unzoom(Binocular *b)
{
    uint16_t type;
    int slot;

    Binocular_StopSound(b);
    b->zoomed = 0;
    b->busy = 1;
    type = Gun_GetType();
    slot = Weapon_GetDeselectEvent();
    ((void (*)(void *))gTaskHandlers[slot][type])(b);
}
HOOK(0x470840, Binocular_Unzoom);

/* zooms with the human's inputs while the fire input is held */
void Binocular_Use(Binocular *b, WeaponUseArg *a)
{
    char *h = a->human;
    void *view = *(void **)(h + 0x4EC);
    float *fov = *(float **)(b->weaponType + 0x6E4);

    EACH_CHILD_ANY(&b->obj.task, Human_GetEvent0(), a);
    if (h[0xCF5]) {
        b->zoomed = 0;
        b->busy = 0;
        return;
    }
    if (!a->trigger || b->busy) {
        b->busy = 0;
        return;
    }
    if (a->pos)
        b->pos = *a->pos;
    if (a->matrix) {
        b->dir[0] = a->matrix->m[1];
        b->dir[1] = a->matrix->m[4];
        b->dir[2] = a->matrix->m[7];
    } else {
        b->dir[0] = b->obj.matrix.m[1];
        b->dir[1] = b->obj.matrix.m[4];
        b->dir[2] = b->obj.matrix.m[7];
    }
    if (!b->zoomed) {
        HumanView_SetZoomed(*(void **)(h + 0x4EC));
        VIEW_ZOOM(view) = fov[0];
        b->unk1B4 = 0;
        b->zoomed = 1;
        b->toggled = 0;
        b->busy = 0;
        return;
    }
    if (HUMAN_F(h, 0x1CC) == kZero.f && HUMAN_F(h, 0x12C) != kZero.f)
        b->toggled = b->toggled == 0;
    if (HUMAN_F(h, 0x1CC) != kZero.f && HUMAN_F(h, 0x12C) == kZero.f && b->toggled) {
        VIEW_ZOOM(view) = fov[0];
        Binocular_StopSound(b);
        goto stop;
    }
    if (HUMAN_F(h, 0x1E8) != kZero.f || (HUMAN_F(h, 0x1CC) != kZero.f && !b->toggled)) {
        if (!(VIEW_ZOOM(view) > fov[1]))
            goto stop;
        VIEW_ZOOM(view) = VIEW_ZOOM(view) * kZoomIn.f;
    } else {
        if (HUMAN_F(h, 0x1EC) == kZero.f || !(VIEW_ZOOM(view) < fov[2]))
            goto stop;
        VIEW_ZOOM(view) = VIEW_ZOOM(view) * kZoomOut.f;
    }
    if (!b->sound || !*b->sound) {
        Task *s;

        Binocular_StopSound(b);
        s = Sound_Play(h, "bin_zoom_1");
        if (!s) {
            b->busy = 0;
            return;
        }
        b->busy = 0;
        b->sound = TaskRef_Add(s);
        return;
    }
    b->busy = 0;
    return;
stop:
    Binocular_StopSound(b);
    b->busy = 0;
}
HOOK(0x470890, Binocular_Use);

void Binocular_StatusDisplay(Binocular *b, void *arg)
{
    if (!b->zoomed)
        return;
    BinocularOverlay_SetPos(*b->overlay, &b->pos);
    BinocularOverlay_SetUnk7C(*b->overlay, b->dir);
    EACH_CHILD(&b->obj.task, StatusScreen_GetEvent(), arg);
}
HOOK(0x470BC0, Binocular_StatusDisplay);

void Binocular_ParamsChanged(Binocular *b, void *arg)
{
    uint8_t slot = Param_GetChangedEvent();

    gTaskHandlers[slot][Gun_GetType()](b, arg);
    if (!*b->overlay) {
        TaskRef_Remove(b->overlay);
        b->overlay = TaskRef_Add(BinocularOverlay_New(b));
    }
}
HOOK(0x470C80, Binocular_ParamsChanged);

/* three view angles in degrees, kept as tangents of the half angle */
void Binocular_ParseArg(void *unused, ScriptArg *a)
{
    int i = a->index;
    float *out = a->out;

    out[1] = x87_tan((long double)(Script_GetArgInt(a->args, i) / 2) * kDegToRad.f);
    out[2] = x87_tan((long double)(Script_GetArgInt(a->args, i + 1) / 2) * kDegToRad.f);
    out[0] = x87_tan((long double)(Script_GetArgInt(a->args, i + 2) / 2) * kDegToRad.f);
    a->index = i + 3;
}
HOOK(0x470CE0, Binocular_ParseArg);

void Binocular_Config(void *unused, ConfigArg *a)
{
    static const char kDefault[] = "Binocular - Default FOV (degrees)";
    static const char kMax[] = "Binocular - Max FOV (degrees)";
    static const char kMin[] = "Binocular - Min FOV (degrees)";
    F32(k70, 0x428C0000);
    F32(k90, 0x42B40000);
    F32(k5, 0x40A00000);
    char *def, *max, *min;
    float tdef, tmax, tmin, *p;

    if (*(void **)(a->weaponType + 0x6E4)) {
        Mem_Free(*(void **)(a->weaponType + 0x6E4));
        *(void **)(a->weaponType + 0x6E4) = NULL;
    }
    def = TagItem_Find(a->config, kDefault);
    if (!def)
        def = TagItem_AddReal32(a->config, kDefault, k70.f);
    max = TagItem_Find(a->config, kMax);
    if (!max)
        max = TagItem_AddReal32(a->config, kMax, k90.f);
    min = TagItem_Find(a->config, kMin);
    if (!min)
        min = TagItem_AddReal32(a->config, kMin, k5.f);
    tdef = x87_tan(*(float *)(def + 0x124) * kHalf.f * kDegToRad.f);
    tmax = x87_tan(*(float *)(max + 0x124) * kHalf.f * kDegToRad.f);
    tmin = x87_tan(*(float *)(min + 0x124) * kHalf.f * kDegToRad.f);
    *(void **)(a->weaponType + 0x6E4) = Mem_Alloc(0xC, 4);
    p = *(float **)(a->weaponType + 0x6E4);
    p[0] = tdef;
    p[1] = tmin;
    p[2] = tmax;
}
HOOK(0x470D70, Binocular_Config);

void Binocular_Close(void)
{
    TaskType_Free(gBinocularType);
}
HOOK(0x470EB0, Binocular_Close);

/* screen position and depth of a world point */
void View_Project(TransContext *v, float *out, float *depth, const Vec3d *p)
{
    long double dx = p->x - v->pos.x;
    long double dy = p->y - v->pos.y;
    long double dz = p->z - v->pos.z;
    float a = dz * v->matrix.m[2] + dy * v->matrix.m[1] + dx * v->matrix.m[0];
    float b = dz * v->matrix.m[5] + dy * v->matrix.m[4] + dx * v->matrix.m[3];
    long double c = dz * v->matrix.m[8] + dy * v->matrix.m[7] + dx * v->matrix.m[6];
    long double k;

    *depth = c;
    if (!(c > kZero.f))
        return;
    k = kOne / c;
    out[0] = v->scaleX * a * k + v->centerX;
    out[1] = v->scaleY * b * k + v->centerY;
}
HOOK(0x470EC0, View_Project);

/* screen size of something of the given size at a depth */
void View_ScaleSize(TransContext *v, float *out, const float *size, float depth)
{
    long double k = kOne / depth;

    out[0] = v->scaleX * size[0] * k;
    out[1] = v->scaleY * size[1] * k;
}
HOOK(0x470F70, View_ScaleSize);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BE33C) uint16_t gBinocularType;
PAD(igb, 5BE33C, 5BE33E, 0x2);
