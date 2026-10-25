#pragma GCC optimize("excess-precision=standard")
#include <math.h>

#include "hook.h"
#include "compmap/compmap.h"
#include "config/configvars.h"
#include "win/winvars.h"

FN_EXTERN(crt_CIpow);

/* the area the map camera may move in */
typedef struct MapLimit {
    Vec3d pos;
    float size;
} MapLimit;

ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x401AE0, Task **, TaskRef_Add, (Task *task));
ORIG(0x4B7F40, void, Script_RegisterInt16, (const char *name, void *data));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B34F0, void, Matrix_RotateX, (Matrix *out, const Matrix *m, float a));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x489470, long double, LevelFlow_GetHour, (void));
ORIG(0x4E75A0, int, Screen_GetFrame, (void));
ORIG(0x4D9870, void, Camera_Set, (void *cam, const Vec3d *pos, const Matrix *axes, float fov, float aspect,
                                  float range, int frame, void *owner, float time, int unk10));
ORIG(0x4E8200, const float *, Viewport_GetRect, (void *view));
ORIG(0x4E8210, MapCamera *, Viewport_GetCamera, (void *view));
ORIG(0x4E8220, void, Viewport_SetList, (void *view, int i, char on));
ORIG(0x4E8250, void, Viewport_SetDrawWorld, (void *view, char on));
ORIG(0x491D40, long double, Display_GetAspect, (void));
ORIG(0x460C60, QTaskList *, Human_GetList, (void));
ORIG(0x476030, QTaskList *, Mine_GetList, (void));
ORIG(0x443660, QTaskList *, SCameraLens_GetList, (void));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (Task *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *results, Vec3d *starts, Vec3d *ends, int count, int unk,
                                  void *query, unsigned int flags));
ORIG(0x4CB9F0, void, Coll_DropPointsMat, (void *world, CollHit *results, Vec3d *points, int count, const double *dz,
                                          int unk, int unk2, unsigned int flags));
ORIG(0x466F60, MapLimit *, ComputerLimit_Get, (void));
ORIG(0x467D70, char *, ComputerObject_New, (void *parent));
ORIG(0x4A23E0, void *, LineFilter_New, (void *parent, int a, int b, int c, float z, int d));
ORIG(0x4A4FF0, void, LineFilter_SetLevel, (void *noise, float level));
ORIG(0x466240, char *, ComputerStartup_New, (void *parent, int count, const float *sizes, const char **names,
                                             const float *colors, const float *colors2, float z));
ORIG(0x466390, void, ComputerStartup_SetCallback, (char *status, int i, void *fn));
ORIG(0x505A30, int, MenuItem_FindSelected, (Task *panel));
ORIG(0x488700, void, SubLevel_Run, (void *task));
ORIG(0x424280, void, MenuWindow_Draw, (void *task));

static const float kHalfPi = 0x1.921fb6p+0f;
static const float kButtonX = 0x1.ab3334p-1f;
static const float kButtonY = 0x1.6cccccp-1f;
static const float kTabX = 0x1.99999ap-4f;
static const float kMapTabY = 0x1.9ddddep-1f;
static const float kTabY = 0x1.8cccccp-1f;
static const float kViewZ = 0x1.143d70p+9f;
static const float kStatusZ = 0x1.0a3d70p+9f;
static const float kBack = -921600.0f;
static const float kPan = 0x1.aaaaaap+14f;
static const float kDrag = 409600.0f;
static const float kPanPower = 0x1.666666p-1f;
static const float kNoiseBase = 0x1.e66666p-1f;
static const float kNoiseVar = 0x1.99999ap-5f;
static const float kNoiseMin = 0x1.99999ap-4f;
static const float kNoiseRand = 0x1.99999ap-2f;
static const float kNoiseFade = 0x1.111112p-5f;
static const double kDrop = -1843200.0;
static const double kFar = 4096000.0;
static const double kNear = 167772160.0;

void ComputerMap_Create(ComputerMap *map);
void ComputerMap_Run(ComputerMap *map);
void ComputerMap_Delete(ComputerMap *map);
void ComputerMap_OnTab(ComputerItem *item, int *id);
void ComputerMap_OnButton(ComputerButtonItem *b, int *id);
void ComputerMap_OnStatus(ComputerMap *map, int state);
void ComputerMap_LookAtTask(ComputerMap *map, Task *t);
void ComputerMap_SetFov(ComputerMap *map, float fov);
void ComputerMap_ClearTarget(ComputerMap *map);
void ComputerMap_Trace(ComputerMap *map);

/* the C runtime's pow on the x87 stack (_CIpow) */
static inline long double ci_pow(long double x, long double y)
{
    long double r;

    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "r"(FN(0x4A5760, crt_CIpow)) : "st(1)", "eax", "ecx", "edx", "memory");
    return r;
}

void ComputerMap_OnStatus(ComputerMap *map, int state)
{
    switch (state) {
    case 0:
        ComputerObject_SetFlags(map->object, 4);
        /* fall through */
    case 1:
        ComputerObject_SetFlags(map->object, 0x15);
        break;
    case 2:
        ComputerObject_SetFlags(map->object, 0x1F);
        break;
    }
}
HOOK(0x469660, ComputerMap_OnStatus);

void ComputerMap_Init(void)
{
    uint16_t type = TaskType_New("ComputerMap", sizeof(ComputerMap), Task_GetRootType(), 0);

    gMapType = type;
    TaskType_SetHandler(type, 0, ComputerMap_Create);
    TaskType_SetHandler2(gMapType, 1, ComputerMap_Run);
    TaskType_SetHandler2(gMapType, 2, ComputerMap_Delete);
    TaskType_SetHandler2(gMapType, Screen_GetDrawEvent(), MenuWindow_Draw);
    gMapSelectEvent = TaskEvent_Alloc(0);
    Script_RegisterInt16("Computer_nZoomLevel", &gMapZoom);
}
HOOK(0x4696B0, ComputerMap_Init);

void ComputerMap_Create(ComputerMap *map)
{
    static const float sizes[3] = {9.0f, 4.5f, 9.0f};
    static const float on[9] = {0.1f, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f};
    static const float off[9] = {0, 0.5f, 0.1f, 0, 0.5f, 0.1f, 0, 0.5f, 0.1f};
    const char *names[3] = {"INITIALIZING", "CONNECTING", "DOWNLOADING"};
    ComputerRes *r = ComputerRes_Get();
    void *vp = Computer_GetCamera();
    float fw = Display_GetMode()->width;
    float fh = Display_GetMode()->height;
    float t, x, y, tx;
    int frame;
    Task *panel;
    const char *s;
    Sprite *font;

    gMap = map;
    Matrix_SetRotX(&map->tilt, -kHalfPi);
    Matrix_RotateX(&map->view, &map->tilt, -kHalfPi);
    if (gMapStartTask) {
        ComputerMap_LookAtTask(map, gMapStartTask);
        ComputerMap_SetStartTask(NULL);
    }
    map->fov = ftan(gMapZoomAngle[gMapZoom]);
    t = LevelFlow_GetHour();
    frame = Screen_GetFrame();
    Camera_Set(vp, &gMapPos, &map->view, map->fov, 1.0f, 300.0f, frame, map, t, 0);
    Viewport_SetDrawWorld(Computer_GetView(), 1);
    Viewport_SetList(Computer_GetView(), 0, 1);
    map->clickTime = 0;
    map->rclickTime = 0;
    map->object = ComputerObject_New(map);
    map->noise = LineFilter_New(map, 0, 0, 0, kViewZ, 0);
    map->noiseLevel = 1.0f;
    map->noiseTime = 60;
    panel = MenuItemTask_New(map);
    ComputerMenu_SetPanel(panel);
    x = fw * kButtonX;
    y = fh * kButtonY;
    map->target = NULL;
    map->camera = NULL;
    ComputerButtonItem_New(panel, 3, 0, 0, 0, 0, 0, x - 23.0f, y - 47.0f, 0, NULL, gEmptyString,
                           (Sprite *)&r->arrow[0][0], (Sprite *)&r->arrow[0][1], (Sprite *)&r->arrow[0][2],
                           ComputerMap_OnButton);
    ComputerButtonItem_New(panel, 4, 0, 0, 0, 0, 0, x - 23.0f, y + 18.0f, 0, NULL, gEmptyString,
                           (Sprite *)&r->arrow[2][0], (Sprite *)&r->arrow[2][1], (Sprite *)&r->arrow[2][2],
                           ComputerMap_OnButton);
    ComputerButtonItem_New(panel, 5, 0, 0, 0, 0, 0, x - 47.0f, y - 23.0f, 0, NULL, gEmptyString,
                           (Sprite *)&r->arrow[3][0], (Sprite *)&r->arrow[3][1], (Sprite *)&r->arrow[3][2],
                           ComputerMap_OnButton);
    ComputerButtonItem_New(panel, 6, 0, 0, 0, 0, 0, x + 18.0f, y - 23.0f, 0, NULL, gEmptyString,
                           (Sprite *)&r->arrow[1][0], (Sprite *)&r->arrow[1][1], (Sprite *)&r->arrow[1][2],
                           ComputerMap_OnButton);
    ComputerButtonItem_New(panel, 7, 0, 0, 0, 0, 0, x - 48.0f, y + 48.0f, 0, NULL, gEmptyString,
                           (Sprite *)&r->plus[0], (Sprite *)&r->plus[1], (Sprite *)&r->plus[2], ComputerMap_OnButton);
    ComputerButtonItem_New(panel, 8, 0, 0, 0, 0, 0, x + 24.0f, y + 48.0f, 0, NULL, gEmptyString,
                           (Sprite *)&r->minus[0], (Sprite *)&r->minus[1], (Sprite *)&r->minus[2],
                           ComputerMap_OnButton);
    ComputerButtonItem_New(panel, 9, 0, 0, 0, 0, 0, x - 17.0f, y - 17.0f, 0, NULL, gEmptyString,
                           (Sprite *)&r->sphere[0], (Sprite *)&r->sphere[1], (Sprite *)&r->sphere[2],
                           ComputerMap_OnButton);
    tx = fw * kTabX;
    s = Resource_Load("OBJECTIVES", 0);
    font = MenuManager_FindFont(MENU_MANAGER(), "font3.fnt");
    ComputerTextItem_New(panel, 1, 0, 0, 0, 0, 0, tx, fh * kMapTabY, font, s, 1, ComputerMap_OnTab);
    s = Resource_Load("LOG", 0);
    font = MenuManager_FindFont(MENU_MANAGER(), "font3.fnt");
    ComputerTextItem_New(panel, 2, 0, 0, 0, 0, 0, tx, fh * kTabY, font, s, 1, ComputerMap_OnTab);
    map->startup = ComputerStartup_New(map, 3, sizes, names, on, off, kStatusZ);
    ComputerStartup_SetCallback(map->startup, 0, ComputerMap_OnStatus);
    ComputerStartup_SetCallback(map->startup, 1, ComputerMap_OnStatus);
    ComputerStartup_SetCallback(map->startup, 2, ComputerMap_OnStatus);
    ComputerStartup_Start(map->startup);
    map->lastButtons = 0;
}
HOOK(0x469740, ComputerMap_Create);

/* moves the camera over the position the task reports (handler slot 7) */
void ComputerMap_LookAtTask(ComputerMap *map, Task *t)
{
    struct {
        Vec3d pos;
        char unk18[0x38];
        int unk50;
    } info;

    info.unk50 = 0;
    ((void (*)(Task *, void *))HANDLER(7, t->type))(t, &info);
    ComputerMap_LookAt(map, &info.pos);
}
HOOK(0x469CC0, ComputerMap_LookAtTask);

void ComputerMap_LookAt(ComputerMap *map, const Vec3d *p)
{
    long double x = map->tilt.m[1] * kBack;
    float y = map->tilt.m[4] * kBack;
    float z = map->tilt.m[7] * kBack;

    gMapPos.x = x + p->x;
    gMapPos.y = y + p->y;
    gMapPos.z = z + p->z;
}
HOOK(0x469D00, ComputerMap_LookAt);

void ComputerMap_OnTab(ComputerItem *item, int *id)
{
    if (!(ComputerMenu_GetPressed() & 0x10))
        return;
    if (*id == 1)
        ComputerMenu_SetScreen(2);
    else if (*id == 2)
        ComputerMenu_SetScreen(3);
}
HOOK(0x469D70, ComputerMap_OnTab);

/* arrows pan, plus and minus zoom, the sphere drags the map with the mouse */
void ComputerMap_OnButton(ComputerButtonItem *b, int *id)
{
    ComputerMap *map = gMap;
    char *in = (char *)(Computer_Get()->obj.task.parent);
    float sound[3] = {1.0f, 1.0f, 0};
    float dx, dy, sdx;
    long double f;
    char down;

    if (b->base.id != *id)
        return;
    Computer_GetCamera();
    down = ComputerMenu_GetButtons() & 0x10;
    b->wasPressed = b->pressed;
    b->pressed = down;
    dx = 0;
    dy = 0;
    if (down) {
        switch (b->base.id) {
        case 3:
            ComputerMap_ClearTarget(map);
            dy = kPan;
            break;
        case 4:
            ComputerMap_ClearTarget(map);
            dy = -kPan;
            break;
        case 5:
            ComputerMap_ClearTarget(map);
            dx = -kPan;
            break;
        case 6:
            ComputerMap_ClearTarget(map);
            dx = kPan;
            break;
        case 7:
            if (b->wasPressed)
                break;
            if ((uint16_t)gMapZoom < 3) {
                gMapZoom++;
                goto zoomed;
            }
            goto sound;
        case 8:
            if (b->wasPressed)
                break;
            if (gMapZoom != 0) {
                gMapZoom--;
            zoomed:
                ComputerObject_SetFlags(map->object, 0);
                ComputerStartup_Start(map->startup);
            }
        sound:
            Sound_PlayDefined("mapcomp_01", 1, sound);
            break;
        }
    }
    ComputerMap_SetFov(map, ftan(gMapZoomAngle[gMapZoom]));
    if (b->base.id == 9) {
        ComputerMenu *m = ComputerMenu_Get2();

        if (down) {
            char *p = Profile_GetActive();
            float mx = *(float *)(in + 0x1B0);
            float my = *(float *)(in + 0x1B4);

            if (!*(int *)(p + 0x538))
                my = -my;
            m->dragging = 1;
            dx = mx * kDrag;
            dy = my * kDrag;
            ComputerMap_ClearTarget(map);
        } else {
            m->dragging = 0;
        }
    }
    f = ci_pow(map->fov, kPanPower);
    sdx = f * dx;
    gMapPos.x = sdx + gMapPos.x;
    gMapPos.y = f * dy + gMapPos.y;
    gMapPos.z = 0.0f + gMapPos.z;
}
HOOK(0x469DA0, ComputerMap_OnButton);

void ComputerMap_SetFov(ComputerMap *map, float fov)
{
    float sound[3] = {1.0f, 1.0f, 0};
    float level;
    int n;

    if (!islessgreater(fov, map->fov))
        return;
    map->fov = fov;
    level = Random_Float(Game_GetRandom()) * kNoiseVar + kNoiseBase;
    n = Random_Int(Game_GetRandom(), 75);
    ComputerMap_SetNoise(map, level, n + 15);
    Sound_PlayDefined("mapcomp_05", 1, sound);
    Sound_PlayDefined("mapcomp_03", 1, sound);
}
HOOK(0x469FE0, ComputerMap_SetFov);

void ComputerMap_SetNoise(ComputerMap *map, float level, int time)
{
    map->noiseTime = time;
    map->noiseLevel = level;
}
HOOK(0x46A080, ComputerMap_SetNoise);

void ComputerMap_ClearTarget(ComputerMap *map)
{
    if (map->target) {
        TaskRef_Remove(map->target);
        map->target = NULL;
    }
}
HOOK(0x46A0A0, ComputerMap_ClearTarget);

void ComputerMap_Run(ComputerMap *map)
{
    void *vp = Computer_GetCamera();
    float t, level;
    int frame;

    CALL_CHILDREN(&map->task, RUN_SLOT);
    ComputerMap_Trace(map);
    ComputerMap_DropCamera(map);
    ComputerMap_ClampCamera(map);
    t = LevelFlow_GetHour();
    frame = Screen_GetFrame();
    Camera_Set(vp, &gMapPos, &map->view, map->fov, 1.0f, 300.0f, frame, map, t, 0);
    if (map->noiseTime) {
        map->noiseTime--;
    } else {
        long double v;

        map->noiseLevel = kNoiseMin;
        v = Random_Float(Game_GetRandom()) * kNoiseRand;
        map->noiseTime = 15;
        map->noiseLevel = v + map->noiseLevel;
        map->noiseTime += Random_Int(Game_GetRandom(), 75);
    }
    level = map->noiseLevel - kNoiseFade;
    map->noiseLevel = level;
    LineFilter_SetLevel(map->noise, level);
}
HOOK(0x46A0D0, ComputerMap_Run);

void ComputerMap_ClampCamera(ComputerMap *map)
{
    MapLimit *l = ComputerLimit_Get();
    double half, neg, dx, dz;
    long double dy;

    if (!l)
        return;
    half = l->size * 0.5f * 4096.0f;
    dx = gMapPos.x - l->pos.x;
    dy = gMapPos.y - l->pos.y;
    dz = gMapPos.z - l->pos.z;
    neg = -half;
    if (!(dx >= neg))
        dx = neg;
    if (dx > half)
        dx = half;
    if (!(dy >= neg))
        dy = neg;
    if (dy > half)
        dy = half;
    gMapPos.x = dx + l->pos.x;
    gMapPos.y = dy + l->pos.y;
    gMapPos.z = dz + l->pos.z;
}
HOOK(0x46A200, ComputerMap_ClampCamera);

/* keeps the camera a fixed height above the ground below it */
void ComputerMap_DropCamera(ComputerMap *map)
{
    double dz = kDrop;
    CollHit hit;

    Coll_DropPointsMat(Game_GetWorld(), &hit, &gMapPos, 1, &dz, 0xFF, 0, 0x80000000);
    if (hit.found)
        ComputerMap_LookAt(map, &hit.pos);
}
HOOK(0x46A2D0, ComputerMap_DropCamera);

/* mouse clicks on the map: zoom on double clicks and pick what was clicked */
void ComputerMap_Trace(ComputerMap *map)
{
    char clicked = 0;

    Computer_Get();
    if (!MenuItem_FindSelected(ComputerMenu_GetPanel())) {
        int dbl = 0, rdbl = 0;

        if (ComputerMenu_GetPressed() & 0x10) {
            if (map->clickTime == 0) {
                ComputerMenu *m = ComputerMenu_Get2();

                clicked = 1;
                map->clickX = m->x;
                map->clickY = m->y;
                map->clickTime = 13;
            } else {
                dbl = 1;
                map->clickTime = 0;
            }
        }
        if (ComputerMenu_GetPressed() & 0x20) {
            if (map->rclickTime == 0) {
                map->rclickTime = 13;
            } else {
                rdbl = 1;
                map->rclickTime = 0;
            }
        }
        if (((gMouse.buttons & 0x100) && !(map->lastButtons & 0x100)) || dbl) {
            if ((uint16_t)gMapZoom < 3) {
                gMapZoom++;
                ComputerMap_SetFov(map, ftan(gMapZoomAngle[gMapZoom]));
                ComputerStartup_Start(map->startup);
            }
        }
        if (((gMouse.buttons & 0x200) && !(map->lastButtons & 0x200)) || rdbl) {
            if (gMapZoom != 0) {
                gMapZoom--;
                ComputerMap_SetFov(map, ftan(gMapZoomAngle[gMapZoom]));
                ComputerStartup_Start(map->startup);
            }
        }
    }
    if (map->clickTime > 0)
        map->clickTime--;
    if (map->rclickTime > 0)
        map->rclickTime--;
    if (clicked) {
        float mx = map->clickX, my = map->clickY;
        const float *rect = Viewport_GetRect(Computer_GetView());
        MapCamera *cam;
        QTaskList *humans, *objs, *cams;
        float aspect;
        double sx, sy, t;
        long double fx;
        Vec3d start, dir, n, end, tmp;
        CollHit hit;
        int i, count, found1 = 0, found2 = 0, found3 = 0;
        Task *h = NULL;

        if (!(mx >= rect[0]) || !(my >= rect[1]) || !(rect[2] + rect[0] > mx) || !(rect[3] + rect[1] > my))
            goto done;
        cam = Viewport_GetCamera(Computer_GetView());
        aspect = Display_GetAspect();
        t = ((double)(mx + mx) - Display_GetMode()->width) * cam->fov;
        sx = t / Display_GetMode()->width;
        t = ((double)(my + my) - Display_GetMode()->height) * (aspect * cam->fov);
        sy = t / Display_GetMode()->width;
        humans = Human_GetList();
        objs = Mine_GetList();
        cams = SCameraLens_GetList();
        start = cam->pos;
        dir.x = cam->m[1] * sy + cam->m[0] * sx + cam->m[2];
        dir.y = cam->m[4] * sy + cam->m[3] * sx + cam->m[5];
        dir.z = cam->m[7] * sy + cam->m[6] * sx + cam->m[8];
        n = dir;
        Vec3d_Normalize(&n);
        fx = n.x * kFar;
        n.y = n.y * kFar;
        n.z = n.z * kFar;
        end.x = start.x + fx;
        end.y = start.y + n.y;
        end.z = start.z + n.z;
        Coll_Lines(Game_GetWorld(), &hit, &start, &end, 1, 0xFF, 0, 0xFFFFFFFF);

        count = humans->count;
        for (i = 0; i < count; i++) {
            tmp = hit.pos;
            h = humans->items[i];
            tmp.z = DynCubeObj_GetPosition(h)->z;
            if (Vec3d_DistanceSq(DynCubeObj_GetPosition(h), &tmp) < map->fov * kNear) {
                found1 = 1;
                break;
            }
        }
        count = objs->count;
        for (i = 0; i < count; i++) {
            char *o = (char *)objs->items[i];

            tmp = hit.pos;
            tmp.z = *(double *)(o + 0x30);
            if (Vec3d_DistanceSq((Vec3d *)(o + 0x20), &tmp) < map->fov * kNear) {
                o[0x248] = 1;
                *(int *)(o + 0x23C) = 9;
                found2 = 1;
                break;
            }
        }
        count = cams->count;
        for (i = 0; i < count; i++) {
            Task *c = cams->items[i];

            tmp = hit.pos;
            tmp.z = DynCubeObj_GetPosition(c)->z;
            if (Vec3d_DistanceSq(DynCubeObj_GetPosition(c), &tmp) < map->fov * kNear) {
                map->camera = c;
                found3 = 1;
                break;
            }
        }
        if (hit.found) {
            if (!found2 && !found3 && ComputerMap_InBounds(map, &hit.pos))
                ComputerMap_LookAt(map, &hit.pos);
            if (found1) {
                ComputerMap_SetTarget(map, h);
                ComputerMap_LookAtTask(map, h);
                goto done;
            }
            if (hit.task) {
                ComputerMap_SetTarget(map, hit.task);
                goto done;
            }
        }
        ComputerMap_ClearTarget(map);
    } else if (map->target) {
        Task *t = *map->target;

        if (t) {
            ComputerMap_LookAtTask(map, t);
            goto done;
        }
        ComputerMap_ClearTarget(map);
    }
done:
    map->lastButtons = gMouse.buttons;
}
HOOK(0x46A330, ComputerMap_Trace);

char ComputerMap_InBounds(ComputerMap *map, const Vec3d *p)
{
    MapLimit *l = ComputerLimit_Get();
    long double half, dx;
    double dy, neg;

    if (!l)
        return 1;
    half = l->size * 0.5f * 4096.0f;
    dx = p->x - l->pos.x;
    dy = p->y - l->pos.y;
    neg = -half;
    if (!(dx >= neg) || dx > half || !(dy >= neg))
        return 0;
    return !(dy > half);
}
HOOK(0x46A970, ComputerMap_InBounds);

/* asks the task whether it can be followed (select event) and keeps a reference */
void ComputerMap_SetTarget(ComputerMap *map, Task *t)
{
    ComputerMap *self = map;

    ComputerMap_ClearTarget(map);
    if (!HANDLER(7, t->type) || !HANDLER(ComputerMap_GetSelectEvent(), t->type))
        return;
    /* the answer is a byte written over the low byte of the map argument */
    ((void (*)(Task *, void *))HANDLER(ComputerMap_GetSelectEvent(), t->type))(t, &map);
    if (*(char *)&map)
        self->target = TaskRef_Add(t);
}
HOOK(0x46A9F0, ComputerMap_SetTarget);

void ComputerMap_Delete(ComputerMap *map)
{
    void *view = Computer_GetView();

    gMap = NULL;
    if (map->target)
        TaskRef_Remove(map->target);
    if (view) {
        Viewport_SetDrawWorld(view, 0);
        Viewport_SetList(view, 0, 0);
    }
}
HOOK(0x46AA80, ComputerMap_Delete);

void ComputerMap_Shutdown(void)
{
    TaskType_Free(gMapType);
    TaskEvent_Free(gMapSelectEvent);
}
HOOK(0x46AAD0, ComputerMap_Shutdown);

Task *ComputerMap_New(void *parent)
{
    return Task_New(parent, gMapType, 0);
}
HOOK(0x46AAF0, ComputerMap_New);

ComputerMap *ComputerMap_Get(void)
{
    return gMap;
}
HOOK(0x46AB10, ComputerMap_Get);

uint8_t ComputerMap_GetSelectEvent(void)
{
    return gMapSelectEvent;
}
HOOK(0x46AB20, ComputerMap_GetSelectEvent);

int16_t ComputerMap_GetZoom(void)
{
    return gMapZoom;
}
HOOK(0x46AB30, ComputerMap_GetZoom);

void ComputerMap_ResetZoom(void)
{
    gMapZoom = 0;
}
HOOK(0x46AB40, ComputerMap_ResetZoom);

void ComputerMap_SetStartTask(Task *t)
{
    gMapStartTask = t;
}
HOOK(0x46AB50, ComputerMap_SetStartTask);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 53F750, 4) const float store_53F750[4] INIT({0.7853982f, 0.32724923f, 0.13962634f, 0.009599311f});
ALIAS(gMapZoomAngle, store_53F750, 0);
DATA(53F760) uint8_t gMapSelectEvent INIT(255);
PAD(igd, 53F760, 53F761, 0x3);
BSS(5BDD78) Vec3d gMapPos;
BSS(5BDD78) uint16_t gMapType;
PAD(igb, 5BDD78, 5BDD92, 0x6);
BSS(5BDD78) ComputerMap *gMap;
BSS(5BDD78) Task *gMapStartTask;
BSS(5BDD78) int16_t gMapZoom;
PAD(igb, 5BDD78, 5BDDA2, 0x6);
