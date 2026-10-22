#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "computer/computer.h"

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4168B0, TaskArray *, Game_GetTaskList1, (void));
ORIG(0x4168E0, TaskArray *, Game_GetTaskList4, (void));
ORIG(0x418E40, void *, MenuManager_FindFont, (void *mm, const char *name));
ORIG(0x443660, TaskArray *, SCameraLens_GetList, (void));
ORIG(0x447D20, uint16_t, Door_GetType, (void));
ORIG(0x460C60, TaskArray *, sub_460C60, (void));
ORIG(0x469450, int, Computer_DrawWrapped, (int x, int y, float z, int width, int unused, const char *text, void *font,
                                           int color));
ORIG(0x46AB10, char *, sub_46AB10, (void));
ORIG(0x46AB30, int16_t, sub_46AB30, (void));
ORIG(0x46B4D0, char *, sub_46B4D0, (void));
ORIG(0x46C070, void *, Computer_GetView, (void));
ORIG(0x46EAD0, TaskArray *, sub_46EAD0, (void));
ORIG(0x491CF0, int *, Display_GetMode, (void));
ORIG(0x491D40, long double, Display_GetAspect, (void));
ORIG(0x498040, void, sub_498040, (float *out, float *in, float depth));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const Matrix *m, float *ax, float *ay, float *az));
ORIG(0x4B50B0, void, QVertex_DrawFrame, (void *s, float x, float y, float r, float g, float b, float a, float z,
                                         int frame, unsigned int flags, int unk20));
ORIG(0x4B5860, void, QVertex_DrawPolyline, (const float *p, int n, float ox, float oy, float z, float r, float g,
                                            float b, float a, float angle, float sx, float sy, unsigned int flags,
                                            int unk20));
ORIG(0x4B5F00, char *, Resource_Load, (const char *name, int *size));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6D00, int, Sprite_Height, (void *s));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D0A50, void, sub_4D0A50, (Vec3d *out, void *task, int i));
ORIG(0x4D0D20, int, sub_4D0D20, (void *model));
ORIG(0x4D2B80, void, sub_4D2B80, (void *fn));
ORIG(0x4D2BD0, void, sub_4D2BD0, (void *fn));
ORIG(0x4E1130, void, sub_4E1130, (Vec3d *a, Vec3d *b, float *colorA, float alphaA, float *colorB, float alphaB));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4E8210, char *, sub_4E8210, (void *view));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));

/* draws the map computer: which object lists to ask */
typedef struct ComputerObject {
    Task task;
    unsigned int flags;
} ComputerObject;

enum {
    MAP_HUMANS = 1,
    MAP_TASKLIST4 = 2,
    MAP_HILIGHTS = 4,
    MAP_CAMERAS = 8,
    MAP_OBJECTS = 0x10,
};

/* the list passed to 0x4D2B80 callbacks */
typedef struct ObjEntry {
    char kind;
    char pad[3];
    Task *task;
} ObjEntry;

typedef struct ObjList {
    ObjEntry *items;
    int count;
} ObjList;

/* marker shapes: points and point count */
typedef struct MarkerShape {
    const float *points;
    int count;
} MarkerShape;

#define gObjectType VAR(uint16_t, 0x53F580)
#define gObjectEvent0 VAR(uint8_t, 0x53F582)
#define gObjectEvent1 VAR(uint8_t, 0x53F583)
#define gObjectEvent2 VAR(uint8_t, 0x53F584)
#define gMarkerShapes ((const MarkerShape *)0x53F5F0)
#define gMarkerColors ((const float (*)[3])0x53F608)
#define gGame VAR(char *, 0xBC2360)

static const float kHalf = 0.5f;
static const float kTen = 10.0f;
static const float kMinMarker = 3072.0f;

void ComputerObject_Create(ComputerObject *o);
void ComputerObject_Delete(ComputerObject *o);
void ComputerObject_Draw(ComputerObject *o);
void ComputerObject_DrawVehicle(Obj *v, MapQuery *q);
void ComputerObject_DrawLine(Obj *o);

void ComputerObject_Project(float *out, float *in, MapQuery *q)
{
    float s, t;

    s = Display_GetMode()[1] * kHalf / (in[2] * q->zoom);
    out[0] = Display_GetMode()[1] * kHalf + s * in[0];
    t = Display_GetAspect() * in[1] * s;
    out[1] = Display_GetMode()[2] * kHalf + t;
}
HOOK(0x4675B0, ComputerObject_Project);

void ComputerObject_ProjectPos(float *out, float *depth, Vec3d *pos, MapQuery *q)
{
    long double dx, dy, dz;
    float *m = q->view;
    float v0, v1, v2, s, h;

    dx = pos->x - q->center->x;
    dy = pos->y - q->center->y;
    dz = pos->z - q->center->z;
    v0 = dz * m[2] + dy * m[1] + dx * m[0];
    v1 = dz * m[5] + dy * m[4] + dx * m[3];
    v2 = dz * m[8] + dy * m[7] + dx * m[6];
    s = Display_GetMode()[1] * kHalf / (v2 * q->zoom);
    out[0] = Display_GetMode()[1] * kHalf + s * v0;
    h = Display_GetMode()[2] * kHalf;
    out[1] = Display_GetAspect() * s * v1 + h;
    if (depth)
        *depth = v2;
}
HOOK(0x467620, ComputerObject_ProjectPos);

void ComputerObject_Open(void)
{
    uint8_t ev;

    gObjectType = TaskType_New("ComputerObject", sizeof(ComputerObject), Task_GetRootType(), 0);
    TaskType_SetHandler(gObjectType, 0, ComputerObject_Create);
    TaskType_SetHandler2(gObjectType, 2, ComputerObject_Delete);
    TaskType_SetHandler2(gObjectType, Screen_GetDrawEvent(), ComputerObject_Draw);
    gObjectEvent0 = ComputerObject_GetEvent0();
    gObjectEvent1 = ComputerObject_GetEvent1();
    gObjectEvent2 = ComputerObject_GetEvent2();
    ev = ComputerObject_GetEvent0();
    TaskType_SetHandler(Vehicle_GetType(), ev, ComputerObject_DrawVehicle);
    ev = ComputerObject_GetEvent2();
    TaskType_SetHandler(RigidDynCubeObj_GetType(), ev, ComputerObject_DrawLine);
    Script_RegisterConstant("MARKER_NONE", -1);
    Script_RegisterConstant("MARKER_BOX", 0);
    Script_RegisterConstant("MARKER_COLOR_NONE", -1);
    Script_RegisterConstant("MARKER_COLOR_RED", 0);
    Script_RegisterConstant("MARKER_COLOR_YELLOW", 1);
    Script_RegisterConstant("MARKER_COLOR_GREEN", 2);
    Script_RegisterConstant("MARKER_COLOR_BLUE", 4);
}
HOOK(0x467700, ComputerObject_Open);

/* the line between model points 10 and 11 (doors bright, others dim) */
void ComputerObject_DrawLine(Obj *o)
{
    float color[3];
    Vec3d a, b;
    int i;

    if (!o->model || sub_4D0D20(*o->model) <= 10)
        return;
    if (o->task.type == Door_GetType()) {
        for (i = 0; i < 3; i++)
            color[i] = 1.0f;
    } else {
        for (i = 0; i < 3; i++)
            color[i] = 0.5f;
    }
    sub_4D0A50(&a, o, 10);
    sub_4D0A50(&b, o, 11);
    a.x = a.x + o->pos.x;
    a.y = a.y + o->pos.y;
    a.z = a.z + o->pos.z;
    b.x = b.x + o->pos.x;
    b.y = b.y + o->pos.y;
    b.z = b.z + o->pos.z;
    sub_4E1130(&a, &b, color, 1.0f, color, 1.0f);
}
HOOK(0x467800, ComputerObject_DrawLine);

void ComputerObject_Notify(ObjList *l);

void ComputerObject_Create(ComputerObject *o)
{
    sub_4D2B80(ComputerObject_Notify);
}
HOOK(0x4678F0, ComputerObject_Create);

void ComputerObject_Notify(ObjList *l)
{
    TaskFn fn;
    Task *t;
    int i;

    for (i = 0; i < l->count; i++) {
        if (l->items[i].kind != 4)
            continue;
        t = l->items[i].task;
        fn = HANDLER(ComputerObject_GetEvent2(), t->type);
        if (fn)
            fn(t, NULL);
    }
}
HOOK(0x467900, ComputerObject_Notify);

void ComputerObject_Delete(ComputerObject *o)
{
    sub_4D2BD0(ComputerObject_Notify);
}
HOOK(0x467960, ComputerObject_Delete);

/* the list function is called again for every access, like the original */
#define SEND_LIST(list, q)                                              \
    do {                                                                \
        int i_;                                                         \
        for (i_ = 0; i_ < list()->count; i_++) {                        \
            uint16_t type_ = list()->items[i_]->type;                   \
            TaskFn fn_ = HANDLER(ComputerObject_GetEvent0(), type_);    \
            fn_(list()->items[i_], q);                                  \
        }                                                               \
    } while (0)

void ComputerObject_Draw(ComputerObject *o)
{
    char *view = sub_4E8210(Computer_GetView());
    MapQuery q;
    Task *t;
    TaskFn fn;
    int i;

    q.center = (Vec3d *)(view + 0x28);
    q.view = (float *)view;
    memcpy(&q.zoom, view + 0x40, sizeof(float));
    q.marker = -1;
    q.color = -1;
    q.pos = NULL;
    q.sprite = NULL;
    q.radius = 0;
    q.text = NULL;
    q.text2 = NULL;
    if (o->flags & MAP_HUMANS)
        SEND_LIST(sub_460C60, &q);
    if (o->flags & MAP_TASKLIST4)
        SEND_LIST(Game_GetTaskList4, &q);
    if (o->flags & MAP_HILIGHTS)
        SEND_LIST(ComputerHilight_GetList, &q);
    if ((o->flags & MAP_CAMERAS) && sub_46AB30() >= 1) {
        SEND_LIST(SCameraLens_GetList, &q);
        t = *(Task **)(sub_46AB10() + 0xA4);
        if (t)
            HANDLER(ComputerObject_GetEvent1(), t->type)(t, &q);
    }
    if (o->flags & MAP_OBJECTS) {
        for (i = 0; i < Game_GetTaskList1()->count; i++) {
            uint16_t type = Game_GetTaskList1()->items[i]->type;

            fn = HANDLER(ComputerObject_GetEvent0(), type);
            if (fn)
                fn(Game_GetTaskList1()->items[i], &q);
        }
        SEND_LIST(sub_46EAD0, &q);
    }
}
HOOK(0x467970, ComputerObject_Draw);

/* vehicles shown with a yellow box */
void ComputerObject_DrawVehicle(Obj *v, MapQuery *q)
{
    float scale;

    if (!sub_46AB30() || sub_46AB30() == 3 || !((char *)v)[0x745])
        return;
    q->pos = &v->pos;
    q->matrix = &v->matrix;
    scale = max_scale(v);
    q->radius = VirModel_GetRadius(v->model) * scale;
    q->marker = 0;
    q->color = 1;
    ComputerObject_DrawMarker(q);
}
HOOK(0x467C60, ComputerObject_DrawVehicle);

void ComputerObject_Close(void)
{
    TaskEvent_Free(gObjectEvent0);
    TaskEvent_Free(gObjectEvent1);
    TaskEvent_Free(gObjectEvent2);
    TaskType_Free(gObjectType);
}
HOOK(0x467D30, ComputerObject_Close);

ComputerObject *ComputerObject_New(void *parent)
{
    ComputerObject *o = (ComputerObject *)Task_New(parent, gObjectType, NULL);

    o->flags = 0;
    return o;
}
HOOK(0x467D70, ComputerObject_New);

uint8_t ComputerObject_GetEvent0(void)
{
    if (gObjectEvent0 == 0xFF)
        gObjectEvent0 = TaskEvent_Alloc(0x181);
    return gObjectEvent0;
}
HOOK(0x467D90, ComputerObject_GetEvent0);

uint8_t ComputerObject_GetEvent1(void)
{
    if (gObjectEvent1 == 0xFF)
        gObjectEvent1 = TaskEvent_Alloc(0x181);
    return gObjectEvent1;
}
HOOK(0x467DB0, ComputerObject_GetEvent1);

uint8_t ComputerObject_GetEvent2(void)
{
    if (gObjectEvent2 == 0xFF)
        gObjectEvent2 = TaskEvent_Alloc(0x181);
    return gObjectEvent2;
}
HOOK(0x467DD0, ComputerObject_GetEvent2);

/* object position in map space: the view matrix applied transposed */
static void to_view(float *v, MapQuery *q)
{
    float d[3];
    float *m;

    d[0] = q->pos->x - q->center->x;
    d[1] = q->pos->y - q->center->y;
    d[2] = q->pos->z - q->center->z;
    m = q->view;
    v[0] = d[2] * m[6] + d[1] * m[3] + d[0] * m[0];
    v[1] = d[2] * m[7] + d[1] * m[4] + d[0] * m[1];
    v[2] = d[2] * m[8] + d[1] * m[5] + d[0] * m[2];
}

void ComputerObject_DrawSprite(MapQuery *q)
{
    void *spr = q->sprite;
    float in[3], out[3];

    if (!spr)
        return;
    to_view(in, q);
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    ComputerObject_Project(out, in, q);
    out[0] = out[0] - Sprite_Width(spr) * kHalf;
    out[1] = out[1] - Sprite_Height(spr) * kHalf;
    out[0] = out[0] - kTen;
    QVertex_DrawFrame(spr, out[0], out[1], 128.0f, 128.0f, 128.0f, 1.0f, 0x1.0a3d7p+9f, 0, 0x400C0000, -1);
}
HOOK(0x467DF0, ComputerObject_DrawSprite);

void ComputerObject_DrawMarker(MapQuery *q)
{
    int marker = q->marker;
    float size = q->radius;
    float in[3], out[3], sz[2], scr[2], ax, ay, az;
    const float *c;

    if (marker == -1)
        return;
    if (!(size >= kMinMarker))
        size = kMinMarker;
    to_view(in, q);
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    c = gMarkerColors[q->color];
    ComputerObject_Project(out, in, q);
    sz[0] = size;
    sz[1] = size;
    sub_498040(scr, sz, in[2]);
    Matrix_GetEuler(q->matrix, &ax, &ay, &az);
    QVertex_DrawPolyline(gMarkerShapes[marker].points, gMarkerShapes[marker].count, out[0], out[1], 0x1.0a3d7p+9f,
                         c[0], c[1], c[2], 0.75f, -az, scr[0], scr[1], 0x400C0000, -1);
}
HOOK(0x467F30, ComputerObject_DrawMarker);

static void draw_label(float *pos, const char *name)
{
    char buf[256];
    const char *s;
    void *font;
    int x, y;

    if (!strlen(name))
        return;
    if (Resource_Find(name)) {
        s = Resource_Load(name, NULL);
    } else {
        crt_sprintf(buf, "RM: %s", name);
        s = buf;
    }
    font = MenuManager_FindFont(*(void **)(gGame + 0x120), "font4.fnt");
    y = (int)round_ll(pos[1]);
    x = (int)round_ll(pos[0]);
    Computer_DrawWrapped(x, y, 0x1.0a3d7p+9f, 0x80, 0x80, s, font, 0);
}

void ComputerObject_DrawText(MapQuery *q)
{
    float in[3], out[2];

    to_view(in, q);
    sub_46B4D0();
    ComputerObject_Project(out, in, q);
    if (sub_46AB30() == 1)
        draw_label(out, q->text);
    else if (sub_46AB30() > 1)
        draw_label(out, q->text2);
}
HOOK(0x4680B0, ComputerObject_DrawText);

void ComputerObject_SetFlags(ComputerObject *o, unsigned int flags)
{
    o->flags = flags;
}
HOOK(0x4682E0, ComputerObject_SetFlags);
