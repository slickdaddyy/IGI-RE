#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "scamera/scamera.h"
#include "qvertex/qvertextypes.h"
#include "coll/colltypes.h"
#include "material/materialtypes.h"

ORIG(0x401AE0, Task **, TaskRef_Add, (Task *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int point));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x4168D0, void *, Game_GetTaskList3, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (void *blast, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x464250, int, Human_GetMaskType, (void));
ORIG(0x465820, void, Human_GetEyePos, (Vec3d *out, Task *human));
ORIG(0x46B4D0, char *, ComputerRes_Get, (void));
ORIG(0x4675B0, void, ComputerObject_Project, (float *out, Vec3f *in, void *view));
ORIG(0x467620, void, ComputerObject_ProjectPos, (float *out, float *depth, Vec3d *pos, void *view));
ORIG(0x480FE0, void, Explosion_Spawn, (Vec3d *pos, int a, int b, float c, float d, float e, float f, int g, int h,
                                       int i, const char *sprite, const char *sound, int j, int k, float l, int m));
ORIG(0x4896C0, void, AIDetector_Init, (void *detect, int a, int b, int mask, int c, int d));
ORIG(0x489750, void, AIDetector_Update, (void *detect, SCameraEye *eye, SCamera *camera, int unk));
ORIG(0x489B90, void, AIDetection_RemoveRefs, (void *detect));
ORIG(0x497F20, void, TransContext_Project, (float *out, float *depth, Vec3d *pos));
ORIG(0x498040, void, TransContext_ScaleSize, (float *out, float *in, float depth));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4B50B0, void, QVertex_DrawFrame, (void *s, float x, float y, float r, float g, float b, float alpha, float z,
                                         int frame, unsigned int flags, int unk20));
ORIG(0x4B53B0, void, QVertex_DrawQuadEx, (void *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                          float x3, float y3, float r, float g, float b, float z, float a, int frame,
                                          unsigned int flags, int unk20));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6D00, int, Sprite_Height, (void *s));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *results, Vec3d *starts, Vec3d *ends, int count, char unk,
                                  void *query, unsigned int flags));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4DCCB0, void, Sector_AddObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));
ORIG(0x4DCF00, void, Sector_MoveObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));

extern char *gAlarmLightSprite;
extern uint16_t gSCameraType;
extern void *gLensList;

extern void (*gRenderFuncs[])(QVertexStrip *);
extern void *gQVertexTexture;
extern int gQVertexStrip;
extern void *gQVertexPalette;

extern float kZero;
extern float kHalf;
extern float kThirty;
extern float kFixed;       /* 4096, one meter */
extern float kDegToRad;
extern float kThirtieth;
extern float kPitchStep;   /* 2.5 degrees */
extern float kPitchRest;   /* -75 degrees */
extern float kTrackMin;    /* -0.75 degrees */
extern float kTrackMax;    /* 0.75 degrees */
extern float kTrackDead;   /* 5 degrees */
extern float kTwelfth;
extern double kGlassStep;  /* 20.48 */

void SCameraLens_UpdateSounds(SCameraLens *l);
void SCameraLens_Track(SCameraLens *l);

void SCameraLens_Create(SCameraLens *l)
{
    Vec3d zero = {0, 0, 0};
    float scale;
    int mask;

    QTaskList_Add(Game_GetTaskList3(), l);
    QTaskList_Add(gLensList, l);
    RigidDynCubeObj_Init(Game_GetWorld(), l, NULL);
    scale = sobj_scale(&l->o);
    DynCube_Add(Game_GetWorld(), l, &zero, VirModel_GetRadius(l->o.model) * scale, 1);
    Sector_AddObject(l->o.world, l, DynCubeObj_GetPosition(l));
    l->camera = (SCamera *)l->o.task.parent;
    l->pitch = 0;
    l->yaw = 0;
    l->yaw_target = 0;
    l->wait = 0;
    l->dir = 0;
    mask = Human_GetMaskType();
    mask |= SCamera_GetDetectMask();
    AIDetector_Init(l->detect, 1, 6, mask, 4, 8);
    l->target = NULL;
    l->lost = 0;
    l->move_sound = NULL;
    l->beep = NULL;
    l->beep_loop = NULL;
    l->damage = 0;
    l->max_damage = 0.5f;
    l->explode = 0;
    l->blink_wait = 0;
    l->blink = 0;
    l->frame_wait = 0;
    l->frame = 0;
}
HOOK(0x4415C0, SCameraLens_Create);

/* the lens model point 1 is the eye */
static void lens_eye(SCameraLens *l)
{
    Vec3d p;

    Model_GetPoint(&p, l, 1);
    p.x = p.x + l->o.pos.x;
    p.y = p.y + l->o.pos.y;
    p.z = p.z + l->o.pos.z;
    l->eye.pos = p;
    l->eye.matrix = l->o.matrix;
}

void SCameraLens_Run(SCameraLens *l)
{
    SCamera *c = l->camera;
    long double t;

    if (l->blink_wait) {
        l->blink_wait--;
    } else {
        l->blink_wait = 7;
        l->blink ^= 1;
    }
    if (l->frame_wait) {
        l->frame_wait--;
    } else {
        char *anim = ComputerRes_Get() + 0x498;

        l->frame_wait = 7;
        l->frame++;
        if (l->frame > *(uint16_t *)(anim + 0x14) - 1)
            l->frame = 0;
    }
    c->last_detect = c->detect;
    if (c->on) {
        if (l->pitch < c->pitch) {
            t = l->pitch + kPitchStep;
            l->pitch = t;
            if (t > c->pitch)
                l->pitch = c->pitch;
        } else if (l->pitch > c->pitch) {
            t = l->pitch - kPitchStep;
            l->pitch = t;
            if (t < c->pitch)
                l->pitch = c->pitch;
        }
        lens_eye(l);
        l->eye.unk180 = 0;
        AIDetector_Update(l->detect, &l->eye, l->camera, 1);
        SCameraLens_Track(l);
        SCameraLens_UpdateSounds(l);
    } else {
        if (l->pitch < kPitchRest) {
            t = l->pitch + kPitchStep;
            l->pitch = t;
            if (t > kPitchRest)
                l->pitch = kPitchRest;
        } else if (l->pitch > kPitchRest) {
            t = l->pitch - kPitchStep;
            l->pitch = t;
            if (t < kPitchRest)
                l->pitch = kPitchRest;
        }
        c->detect = 0;
        if (l->move_sound) {
            Sound_Stop(l->move_sound);
            l->move_sound = NULL;
        }
        if (l->beep) {
            if (*l->beep)
                Sound_Stop(*l->beep);
            TaskRef_Remove(l->beep);
            l->beep = NULL;
        }
        if (l->beep_loop) {
            Sound_Stop(l->beep_loop);
            l->beep_loop = NULL;
        }
    }
    if (l->explode) {
        l->explode = 0;
        c->exploded = 1;
        RigidDynCubeObj_SetModel(l, VirModel_Find(c->broken_model));
        l->o.lightmapRefs = 0;
        Explosion_Spawn(&l->o.pos, 1, 0, 0.25f, 2048.0f, 0, 0, 7, 3, 0, "1009_01_1", "explo_camera", 0x15, 2,
                        102400.0f, 0);
    }
    Matrix_SetEuler(&l->o.matrix, l->pitch, 0, l->yaw + c->yaw + c->yaw_offset);
    EACH_CHILD(&l->o.task, 1);
}
HOOK(0x441770, SCameraLens_Run);

/* the short beep counts down to a detection, the long one runs while detecting */
void SCameraLens_UpdateSounds(SCameraLens *l)
{
    SCamera *c = l->camera;
    Task *beep = NULL;

    if (l->beep) {
        beep = *l->beep;
        if (!beep) {
            TaskRef_Remove(l->beep);
            l->beep = NULL;
        }
    }
    if (c->detect_time) {
        if (!beep) {
            beep = Sound_Play(Level_GetDynamic(), "camera_beep_s");
            if (beep) {
                Sound_SetPos(beep, &l->o.pos);
                l->beep = TaskRef_Add(beep);
            }
        }
    } else if (beep) {
        Sound_Stop(beep);
        TaskRef_Remove(l->beep);
        l->beep = NULL;
    }
    if (c->detect) {
        if (!l->beep_loop) {
            l->beep_loop = Sound_Play(Level_GetDynamic(), c->alarm ? "camera_beep_al" : "camera_beep_l");
            if (l->beep_loop)
                Sound_SetPos(l->beep_loop, &l->o.pos);
        }
    } else if (l->beep_loop) {
        Sound_Stop(l->beep_loop);
        l->beep_loop = NULL;
    }
}
HOOK(0x441AE0, SCameraLens_UpdateSounds);

void SCameraLens_GetTargetPos(Vec3d *out, Task *t);

static Task *first_visible(DetectItem *it)
{
    for (; it->next; it = it->next) {
        if (it->visible)
            return *it->target;
    }
    return NULL;
}

/* turns towards a seen target, or sweeps between the yaw limits */
void SCameraLens_Track(SCameraLens *l)
{
    SCamera *c = l->camera;
    float old_yaw = l->yaw;
    Task *target = NULL, *t;

    c->detect = 0;
    c->alarm = 0;
    if ((t = first_visible(l->seen_alarm)) != NULL) {
        target = t;
        c->alarm = 1;
    }
    if ((t = first_visible(l->seen)) != NULL) {
        target = t;
        c->alarm = 0;
    }
    if (target) {
        Vec3d p, v;
        long double dx, dy, dz, a, abs_a, lo, hi;
        float *m = l->o.matrix.m;
        float yaw;

        if (l->target != target) {
            l->target = target;
            c->detect_time = c->alarm ? 0 : 90;
        }
        if (c->detect_time)
            c->detect_time--;
        if (!c->detect_time)
            c->detect = 1;
        SCameraLens_GetTargetPos(&p, target);
        dx = p.x - l->o.pos.x;
        dy = p.y - l->o.pos.y;
        dz = p.z - l->o.pos.z;
        v.x = m[6] * dz + m[3] * dy + m[0] * dx;
        v.y = m[7] * dz + m[4] * dy + m[1] * dx;
        v.z = m[8] * dz + m[5] * dy + m[2] * dx;
        a = Vec3d_YawAngle(v);
        abs_a = a < kZero ? -a : a;
        if (abs_a > kTrackDead) {
            if (a < kTrackMin)
                a = kTrackMin;
            else if (a > kTrackMax)
                a = kTrackMax;
            yaw = a + l->yaw;
            l->yaw = yaw;
            lo = -c->yaw_min * kDegToRad;
            if (yaw < lo) {
                l->yaw = lo;
            } else {
                hi = c->yaw_max * kDegToRad;
                if (yaw > hi)
                    l->yaw = hi;
            }
        }
    } else {
        c->detect_time = 0;
        if (l->target) {
            l->target = NULL;
            l->lost = 60;
        }
        if (l->lost)
            l->lost--;
        if (!l->lost) {
            if (!l->dir) {
                l->dir = 1;
                l->yaw_target = c->yaw_max * kDegToRad;
            }
            if (l->wait) {
                l->wait--;
            } else {
                long double y;

                if (l->yaw < l->yaw_target) {
                    y = c->turn_speed * kDegToRad * kThirtieth + l->yaw;
                    l->yaw = y;
                    if (y > l->yaw_target)
                        l->yaw = l->yaw_target;
                } else if (l->yaw > l->yaw_target) {
                    y = l->yaw - c->turn_speed * kDegToRad * kThirtieth;
                    l->yaw = y;
                    if (y < l->yaw_target)
                        l->yaw = l->yaw_target;
                }
                if (l->yaw == l->yaw_target) {
                    l->wait = round_int(c->wait * kThirty);
                    if (l->dir == -1) {
                        l->dir = 1;
                        l->yaw_target = c->yaw_max * kDegToRad;
                    } else {
                        l->dir = -1;
                        l->yaw_target = -c->yaw_min * kDegToRad;
                    }
                }
            }
        }
    }
    if (old_yaw == l->yaw) {
        if (l->move_sound) {
            Sound_Stop(l->move_sound);
            l->move_sound = NULL;
        }
    } else if (!l->move_sound) {
        l->move_sound = Sound_Play(l, "camera_move");
        if (l->move_sound)
            Sound_SetPos(l->move_sound, &l->o.pos);
    }
}
HOOK(0x441BF0, SCameraLens_Track);

void SCameraLens_GetTargetPos(Vec3d *out, Task *t)
{
    if (TaskType_IsA(t->type, Human_GetType()))
        Human_GetEyePos(out, t);
    else if (t->type == gSCameraType)
        *out = ((SCamera *)t)->place;
}
HOOK(0x442020, SCameraLens_GetTargetPos);

void SCameraLens_Delete(SCameraLens *l)
{
    if (l->beep) {
        TaskRef_Remove(l->beep);
        l->beep = NULL;
    }
    QTaskList_Remove(Game_GetTaskList3(), l);
    QTaskList_Remove(gLensList, l);
    Sector_RemoveObject(l);
    DynCube_Remove(l->o.world, l);
    AIDetection_RemoveRefs(l->detect);
}
HOOK(0x442070, SCameraLens_Delete);

void SCameraLens_ParamsChanged(SCameraLens *l)
{
    SCamera *c = l->camera;
    void **model = VirModel_Find(c->lens_model);
    float scale, pitch;
    Vec3d p;

    if (model)
        RigidDynCubeObj_SetModel(l, model);
    Model_GetPoint(&p, l->camera, 0);
    p.x = p.x + c->o.pos.x;
    p.y = p.y + c->o.pos.y;
    p.z = p.z + c->o.pos.z;
    scale = sobj_scale(&l->o);
    DynCube_Move(l->o.world, l, &p, VirModel_GetRadius(l->o.model) * scale);
    Sector_MoveObject(l->o.world, l, DynCubeObj_GetPosition(l));
    pitch = c->pitch;
    l->pitch = pitch;
    Matrix_SetEuler(&l->o.matrix, pitch, 0, c->yaw + c->yaw_offset);
    lens_eye(l);
    l->eye.half_x = (c->fov_x / 2) * kDegToRad;
    l->eye.half_y = (c->fov_y / 2) * kDegToRad;
    l->eye.range = c->range * kFixed;
}
HOOK(0x4420D0, SCameraLens_ParamsChanged);

/* arg[1] is the damage */
void SCameraLens_Hit(SCameraLens *l, float *arg)
{
    long double t;

    if (l->camera->exploded)
        return;
    t = arg[1] + l->damage;
    l->damage = t;
    if (!(t < l->max_damage))
        l->explode = 1;
}
HOOK(0x4422B0, SCameraLens_Hit);

void SCameraLens_Blast(SCameraLens *l, void *blast)
{
    float scale;
    long double t;

    if (l->camera->exploded)
        return;
    scale = sobj_scale(&l->o);
    t = Game_BlastFalloff(blast, l, &l->o.pos, VirModel_GetRadius(l->o.model) * scale, 1);
    t = t + l->damage;
    l->damage = t;
    if (!(t < l->max_damage))
        l->explode = 1;
}
HOOK(0x4422F0, SCameraLens_Blast);

/* arg: {result, ..., task at +8, task at +0xC}; false when either is our camera */
void SCameraLens_IsOther(SCameraLens *l, char *arg)
{
    if (*(SCamera **)(arg + 8) != l->camera && *(SCamera **)(arg + 0xC) != l->camera) {
        arg[0] = 1;
        return;
    }
    arg[0] = 0;
}
HOOK(0x4423C0, SCameraLens_IsOther);

/* the blinking alarm light: red while detecting, green otherwise */
void SCameraLens_Draw(SCameraLens *l)
{
    SCamera *c = l->camera;

    if (c->on) {
        float scr[8], size[2], depth, r, g, x0, y0, x1, y1;
        Vec3d p;

        Model_GetPoint(&p, l, 0);
        p.x = p.x + l->o.pos.x;
        p.y = p.y + l->o.pos.y;
        p.z = p.z + l->o.pos.z;
        TransContext_Project(scr, &depth, &p);
        if (depth > kZero) {
            if (!c->detect && !c->detect_time) {
                r = 0;
                g = 128.0f;
                if (l->blink) {
                    size[0] = size[1] = 0x1.99999ap+9f; /* 819.2 */
                } else {
                    size[0] = size[1] = 0x1.333334p+8f; /* 307.2 */
                    r = 0;
                    g = 64.0f;
                }
            } else {
                r = 128.0f;
                g = 0;
                if (l->blink)
                    size[0] = size[1] = 0x1.666666p+10f; /* 1433.6 */
                else
                    size[0] = size[1] = 512.0f;
            }
            TransContext_ScaleSize(size, size, depth);
            x0 = scr[0] - size[0];
            y0 = scr[1] - size[1];
            x1 = scr[0] + size[0];
            y1 = scr[1] + size[1];
            QVertex_DrawQuadEx(gAlarmLightSprite + 0x18, x0, y0, x1, y0, x0, y1, x1, y1, r, g, 0, depth, 0.5f, 0,
                               0x1040008, 0);
        }
    }
    HANDLER(4, RigidDynCubeObj_GetType())(l, 0);
}
HOOK(0x442A70, SCameraLens_Draw);

/* the camera icon on the map computer; arg: {center, matrix, ...} */
void SCameraLens_DrawOnMap(SCameraLens *l, void **arg)
{
    SCamera *c = l->camera;
    Vec3d *center;
    float *m, dx, dy, dz, pos[2];
    Vec3f v;
    char *spr;
    int frame;

    if (c->exploded)
        return;
    center = arg[0];
    dx = l->o.pos.x - center->x;
    dy = l->o.pos.y - center->y;
    dz = l->o.pos.z - center->z;
    m = arg[1];
    v.x = dz * m[6] + dy * m[3] + dx * m[0];
    v.y = dz * m[7] + dy * m[4] + dx * m[1];
    v.z = dz * m[8] + dy * m[5] + dx * m[2];
    pos[0] = v.x;
    pos[1] = v.y;
    if (c->on) {
        spr = ComputerRes_Get() + 0x498;
        frame = l->frame;
    } else {
        spr = ComputerRes_Get() + 0x480;
        frame = 0;
    }
    ComputerObject_Project(pos, &v, arg);
    pos[0] = pos[0] - Sprite_Width(spr) * kHalf;
    pos[1] = pos[1] - Sprite_Height(spr) * kHalf;
    QVertex_DrawFrame(spr, pos[0], pos[1], 128.0f, 128.0f, 128.0f, 1.0f, 0x1.0a3d7p+9f, frame, 0x400C0000, -1);
}
HOOK(0x442C60, SCameraLens_DrawOnMap);

static void cone_vertex(QVertex *v, const float *p)
{
    v->x = p[0];
    v->y = p[1];
    v->z = 0x1.0a3d7p+9f; /* 533.28 */
    v->u = 0;
    v->v = 0;
    v->a = 0x1.666666p-2f; /* 0.35 */
    v->r = 1.0f;
    v->g = 0;
    v->b = 0;
}

/* the view cone on the map computer: 12 rays across the field of view, each
   stopped at the first wall it hits but carried on through glass */
void SCameraLens_DrawViewCone(SCameraLens *l, void *arg)
{
    SCamera *c = l->camera;
    struct {
        int unk0;
        SCameraLens *l;
    } ignore;
    Vec3d p, start, end, ends[12];
    QVertex v[3];
    QVertexStrip strip;
    CollHit hit;
    Matrix m;
    float ax, ay, az, step, scr[2], depth = 0;
    long double fov;
    int i;

    if (c->exploded)
        return;
    Model_GetPoint(&p, l, 1);
    m = l->o.matrix;
    p.x = p.x + l->o.pos.x;
    p.y = p.y + l->o.pos.y;
    p.z = p.z + l->o.pos.z;
    Matrix_GetEuler(m.m, &ax, &ay, &az);
    ignore.l = l;
    ax = 0;
    ay = 0;
    fov = c->fov_y * kDegToRad;
    az = fov * kHalf + az;
    step = fov * kTwelfth;
    for (i = 0; i < 12; i++) {
        Vec3d d;
        long double rx;

        start = p;
        d.x = 0;
        d.y = c->range * kFixed;
        d.z = 0;
        Matrix_SetEuler(&m, ax, ay, az);
        rx = m.m[2] * d.z + m.m[1] * d.y + m.m[0] * d.x;
        end.y = m.m[5] * d.z + m.m[4] * d.y + m.m[3] * d.x;
        end.z = m.m[8] * d.z + m.m[7] * d.y + m.m[6] * d.x;
        end.x = start.x + rx;
        end.y = start.y + end.y;
        end.z = start.z + end.z;
        for (;;) {
            uint8_t ev = Game_GetEvent8();
            Vec3d dir;
            long double sx;

            Coll_Lines(Game_GetWorld(), &hit, &start, &end, 1, ev, &ignore, 0xFFFFFFFF);
            if (!hit.found)
                break;
            if (!hit.unk54 || !hit.material || !(hit.material->unk00 > kZero)) {
                end = hit.pos;
                break;
            }
            dir.x = hit.pos.x - start.x;
            dir.y = hit.pos.y - start.y;
            dir.z = hit.pos.z - start.z;
            Vec3d_Normalize(&dir);
            sx = dir.x * kGlassStep;
            dir.y = dir.y * kGlassStep;
            dir.z = dir.z * kGlassStep;
            start.x = sx + hit.pos.x;
            start.y = dir.y + hit.pos.y;
            start.z = dir.z + hit.pos.z;
        }
        az = az - step;
        ends[i] = end;
    }
    gQVertexTexture = 0;
    gQVertexPalette = 0;
    strip.lines = 0;
    strip.verts = v;
    strip.flags = 0x400C0000;
    strip.queue = -1;
    strip.count = 3;
    ComputerObject_ProjectPos(scr, &depth, &p, arg);
    cone_vertex(&v[0], scr);
    for (i = 1; i < 12; i++) {
        ComputerObject_ProjectPos(scr, &depth, &ends[i - 1], arg);
        cone_vertex(&v[1], scr);
        ComputerObject_ProjectPos(scr, &depth, &ends[i], arg);
        cone_vertex(&v[2], scr);
        gRenderFuncs[gQVertexStrip](&strip);
    }
}
HOOK(0x442DD0, SCameraLens_DrawViewCone);

/* arg: {unused, world, ...} */
void SCameraLens_WorldAdd(SCameraLens *l, void **arg)
{
    SCamera *c = l->camera;
    float scale;
    Vec3d p;

    Model_GetPoint(&p, c, 0);
    p.x = p.x + c->o.pos.x;
    p.y = p.y + c->o.pos.y;
    p.z = p.z + c->o.pos.z;
    scale = sobj_scale(&l->o);
    DynCube_Add(arg[1], l, &p, VirModel_GetRadius(l->o.model) * scale, 1);
    Sector_AddObject(l->o.world, l, DynCubeObj_GetPosition(l));
    EACH_CHILD(&l->o.task, World_GetEvent0(), arg);
}
HOOK(0x4432F0, SCameraLens_WorldAdd);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533514) float kTrackMax INIT(0.01308997f);
DATA(533678) float kThirtieth INIT(0.033333335f);
DATA(533678) float kDegToRad INIT(0.017453292f);
ALIAS(kBurnScale, kThirtieth, 0);
ALIAS(kFrame, kThirtieth, 0);
DATA(533874) float kPitchRest INIT(-1.3089969f);
DATA(533874) float kTrackMin INIT(-0.01308997f);
DATA(533880) double kGlassStep INIT(20.48);
DATA(533880) float kTwelfth INIT(0.083333336f);
PAD(igd, 533880, 53388C, 0x4);
BSS(57C17C) char *gAlarmLightSprite;
