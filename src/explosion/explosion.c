#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "explosion/explosion.h"

FN_EXTERN(Explosion_CreateHandler);
FN_EXTERN(Explosion_RunHandler);
FN_EXTERN(FireBall_DeleteHandler);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x4168D0, TaskArray *, Game_GetTaskList3, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416930, char *, Game_AddTexture, (const char *name));
ORIG(0x4169D0, char *, Game_AddSprite, (const char *name));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B3790, void, Matrix_RotateZ, (Matrix *out, const Matrix *m, float a));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B4010, void, Matrix_SetDirection, (Matrix *m, const Vec3f *dir));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4C1780, void, Debug_Log, (void *a, void *b));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4CFC30, int, Mesh3D_GetDrawRigidMethod, (void));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D0D90, int, ModelLod_GetTexAnimCount, (void *model));
ORIG(0x4D0DA0, int, ModelLod_GetTexAnimFrames, (void *model, int a));
ORIG(0x4D0DC0, void, ModelLod_SetTexAnimFrame, (void *model, int a, int frame));
ORIG(0x4DA180, void, Haze_GetVisible, (int *a, int *b));
ORIG(0x4DAA20, int, Light_GetActive, (int *a));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4FDF00, void *, Particle2D_New, (void *root, void *world, void *sprite, Vec3f *dir, Vec3f *color, float *size,
                                    float life, int blend, int d, float e, int f, int count, void *h));
ORIG(0x4FE120, void, Particle2D_Add, (void *ps, Vec3d *pos, Vec3f *dir, float size, float a, float b, float c, int d,
                                  int e));
ORIG(0x4FE5D0, void, Particle2D_SetAutoDelete, (void *ps, int on));
ORIG(0x4FE5E0, void, Particle2D_SetLevel, (void *ps, float a));
ORIG(0x503BF0, uint16_t, SimpointObj_GetType, (void));
ORIG(0x503CC0, Fragment *, SimpointObjContainer_Add, (void *group, uint16_t type, Vec3d *pos, Matrix *m, Vec3f *vel));
ORIG(0x503D50, void, SimpointObjContainer_SetAutoDelete, (void *group, int a));

extern uint16_t gExplosionType;
extern uint16_t gFragmentType;
extern char *gFireballSprite;
extern uint16_t gExplosionSmokeType;
extern char *gSmoke2Sprite;
extern char *gBurnTexture;
extern uint16_t gFireBallType;
extern Vec3d gViewPos;
extern void (*gRenderFuncs[])(RenderArg *);

#define Explosion_Create (FN(0x480F30, Explosion_CreateHandler))
#define Explosion_Run (FN(0x480F40, Explosion_RunHandler))
#define FireBall_Delete (FN(0x480F00, FireBall_DeleteHandler))

static const double kOne = 1.0;
static const double kZeroD = 0.0;
static const float kZero = 0.0f;
static const float kOneF = 1.0f;
static const float kLife = 819200.0f;
static const float kFragmentSize = 0x1.99999ap+7f; /* 204.8 */
static const float kThousandth = 0x1.0624dep-10f;
static const float kSmokeColor = 0x1.966666p+3f; /* 12.7 */
static const float kRiseFast = 0x1.1d7218p+1f;   /* 2.23 */
static const float kRiseSlow = 0x1.1d7218p+0f;   /* 1.115 */
extern const float kTenth;
extern const float kFifth;
extern const float kHalf;
extern const float kQuarter;
extern const float kThreeQuarters;
extern const float kFourFifths;
extern const float kNineTenths;
extern const float kThreeTenths;
extern const float kHundredth;
extern const float kThreeHalves;
extern const float k4096;
extern const float kPi;
extern const float kTwoPi;
extern const float kBlastSmokeSpin;
extern const float kFragmentSpin;
extern const float kSmokeRate;
extern const float kTrailRate;
extern const float kTumble;
extern const float kFragmentScale;
extern const double kFireballScale;
extern const double kSmokeScale;
extern const double kHalfD;
extern const float kSmokeFrames;
extern const float kDelaySpread;

FireBall *FireBall_New(Vec3d *pos, const char *model, float scale, int life, int a5, int a6, int delay,
                       int animFrames, float angle, float spin);
void Fragment_Run(Fragment *f);
void FireBall_Create(FireBall *fb);
void FireBall_Run(FireBall *fb);
void FireBall_Draw(FireBall *fb);
void ExplosionSmoke_Run(ExplosionSmoke *s);

/* starts a smoke emitter at s and sends the first particle */
void *ExplosionSmoke_Emit(ExplosionSmoke *s, int blend, float *size, float life, int a5, int a6, float psize)
{
    Vec3f dir = {0.0f, 0.0f, kRiseFast};
    Vec3f color = {kSmokeColor, kSmokeColor, kSmokeColor};
    Vec3f up = {0.0f, 0.0f, 1.0f};
    void *world = Game_GetWorld();
    void *root = Level_GetDynamic();
    void *ps = Particle2D_New(root, world, gSmoke2Sprite + 0x18, &dir, &color, size, kLife, blend, 0, life, 0, 1, NULL);
    float a, b;

    Particle2D_SetAutoDelete(ps, 1);
    a = (float)(Random_Float(Game_GetRandom()) * kTwoPi);
    b = (float)((kHalf - Random_Float(Game_GetRandom())) * kBlastSmokeSpin);
    Particle2D_Add(ps, &s->pos, &up, psize, b, a, 1.0f, a5, a6);
    return ps;
}
HOOK(0x47EAD0, ExplosionSmoke_Emit);

/* the emitters of a burning (2) or smoking (1) fragment */
void Fragment_InitSmoke(Fragment *f, float size)
{
    Vec3f dir = {0.0f, 0.0f, kRiseSlow};
    Vec3f color = {kSmokeColor, kSmokeColor, kSmokeColor};
    float sz[2];
    long double t;
    float life;
    void *world, *root;

    sz[0] = sz[1] = size * kHalf;
    t = f->size * kSmokeRate;
    if (f->kind == 2) {
        life = (float)(kOneF / t);
        world = Game_GetWorld();
        f->ps = Particle2D_New(f, world, gFireballSprite + 0x18, &dir, &color, sz, kLife, 8, 0, life, 0, 200, NULL);
        Particle2D_SetAutoDelete(f->ps, 0);
        world = Game_GetWorld();
        root = Level_GetDynamic();
        f->ps2 = Particle2D_New(root, world, gSmoke2Sprite + 0x18, &dir, &color, sz, kLife, 0x10, 0, life, 0, 200, NULL);
        Particle2D_SetAutoDelete(f->ps2, 0);
    } else if (f->kind == 1) {
        life = (float)(kOneF / t);
        world = Game_GetWorld();
        root = Level_GetDynamic();
        f->ps2 = Particle2D_New(root, world, gSmoke2Sprite + 0x18, &dir, &color, sz, kLife, 8, 0, life, 0, 200, NULL);
        Particle2D_SetAutoDelete(f->ps2, 0);
    }
}
HOOK(0x47EBD0, Fragment_InitSmoke);

static void puff(void *ps, Vec3d *pos, Vec3f *zero, float size, int n1, int n2)
{
    float a = (float)(Random_Float(Game_GetRandom()) * kTwoPi);
    float b = (float)((kHalf - Random_Float(Game_GetRandom())) * kFragmentSpin);

    Particle2D_Add(ps, pos, zero, size, b, a, 1.0f, n1, n2);
}

/* fire while young, then smoke; or a trail of smoke along the last move */
void Fragment_Smoke(Fragment *f)
{
    Vec3f zero = {0.0f, 0.0f, 0.0f};
    float r27, half, a, b;
    int n1, n2;

    r27 = f->size * kTrailRate;
    half = f->start * kHalf;
    n1 = round_int(r27 * kTenth);
    n2 = round_int((Random_Float(Game_GetRandom()) * kFifth + kFourFifths) * r27);
    if (f->kind == 2) {
        long double h;

        if ((long double)f->life > half) {
            a = (float)((((long double)f->life - half) * kFifth) / half);
            b = (float)(Random_Float(Game_GetRandom()) * a);
            puff(f->ps, &f->pos, &zero, b, n1, n2);
        }
        h = half * kThreeHalves;
        if ((long double)f->life < h) {
            a = (float)(((long double)f->life / h) * kFifth);
            b = (float)(Random_Float(Game_GetRandom()) * a);
            puff(f->ps2, &f->pos, &zero, b, n1, n2);
        }
    } else if (f->kind == 1 && (long double)f->life > half) {
        long double lim = f->size * k4096, step, s;
        double len, stepD, dist = 0.0, d[3];
        Vec3d p;

        len = x87_sqrt((f->vel.x * f->vel.x + f->vel.y * f->vel.y) + f->vel.z * f->vel.z);
        step = len > lim ? len / lim : len;
        stepD = step;
        p = f->pos;
        d[0] = f->vel.x * step;
        d[1] = f->vel.y * step;
        d[2] = f->vel.z * step;
        if (!(kZeroD < len))
            return;
        do {
            a = (float)((((long double)f->life - half) * kFifth) / half);
            b = (float)(Random_Float(Game_GetRandom()) * a);
            puff(f->ps2, &p, &zero, b, n1, n2);
            p.x = p.x + d[0];
            p.y = p.y + d[1];
            p.z = p.z + d[2];
            s = (long double)stepD + dist;
            dist = s;
        } while (s < len);
    }
}
HOOK(0x47ED50, Fragment_Smoke);

Fragment *Fragment_New(Explosion *e, Vec3d *pos, Vec3f *vel, int kind, float size)
{
    Matrix m;
    Fragment *f;
    float ax, ay, az, s;

    az = (float)(Random_Float(Game_GetRandom()) * kTumble);
    ay = (float)(Random_Float(Game_GetRandom()) * kTumble);
    ax = (float)(Random_Float(Game_GetRandom()) * kTumble);
    Matrix_SetEuler(&m, ax, ay, az);
    f = SimpointObjContainer_Add(e->group, gFragmentType, pos, &m, vel);
    s = size * kFragmentScale;
    f->size = s;
    f->scale[0] = s;
    f->scale[1] = s;
    f->scale[2] = s;
    f->start = *(int *)((char *)e->group + 0x20);
    f->ps = NULL;
    f->ps2 = NULL;
    f->kind = kind;
    if (kind)
        Fragment_InitSmoke(f, size + size);
    return f;
}
HOOK(0x47F0D0, Fragment_New);

ExplosionSmoke *ExplosionSmoke_New(Vec3d *pos, float size, int frames)
{
    ExplosionSmoke *s = (ExplosionSmoke *)Task_New(Level_GetDynamic(), gExplosionSmokeType, NULL);

    s->pos = *pos;
    s->timer = frames;
    s->frames = frames;
    s->ps2 = NULL;
    s->size = size;
    return s;
}
HOOK(0x47F1B0, ExplosionSmoke_New);

/* out = a * b, each sum added in the order the original's inlined code uses */
static void mat_mul(Matrix *out, const Matrix *a, const Matrix *b)
{
    int r, c;

    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            out->m[r * 3 + c] = (a->m[r * 3 + 2] * b->m[6 + c] + a->m[r * 3 + 1] * b->m[3 + c])
                                + a->m[r * 3] * b->m[c];
}

/* one bang: fireballs, smoke, fragments, sound and the blast */
void Explosion_Burst(Explosion *e)
{
    double size = e->size;
    FireBall *fb = NULL; /* the original sends stack garbage with no fireballs */
    Vec3d pos;
    int i, frames;

    if (!(e->size < kZeroD || e->size > kZeroD))
        return;
    pos = e->pos;
    if (e->fireballs)
        fb = FireBall_New(&pos, "1006_01_1", e->fireballs > 1 ? (float)(size * kThreeQuarters) : (float)size, 75, 0,
                          60, 0, 75, 0.0f, 0.0f);
    if (e->fireballs - 1 > 0) {
        float sizeF = size;

        for (i = 0; i < e->fireballs - 1;) {
            float sc;
            double dx, dy, dz;
            int life, k, delay;
            Vec3d p;

            sc = (Random_Float(Game_GetRandom()) * kQuarter + kHalf) * sizeF;
            Random_Float(Game_GetRandom());
            dx = (Random_Float(Game_GetRandom()) - kHalfD) * size;
            dy = (Random_Float(Game_GetRandom()) - kHalfD) * size;
            dz = (Random_Float(Game_GetRandom()) - kHalfD) * size;
            life = round_int((Random_Float(Game_GetRandom()) + kOneF) * fb->frames * kHalf);
            i++;
            k = round_int(Random_Float(Game_GetRandom()) * kDelaySpread);
            delay = 75 / e->fireballs * i - k;
            p = fb->obj.pos;
            p.x = p.x + dx;
            p.y = p.y + dy;
            p.z = p.z + dz;
            FireBall_New(&p, "1006_01_1", sc, life, 0, round_int((long double)life * kNineTenths), delay, life, 0.0f,
                         0.0f);
        }
    }
    i = e->fireballs;
    if (i > 1)
        frames = round_int(((kSmokeFrames - kSmokeFrames / (long double)i) - kOneF) + kSmokeFrames);
    else
        frames = 150;
    if (i)
        ExplosionSmoke_New(&pos, (float)(size * kSmokeScale), frames);
    if (e->fragments) {
        for (i = 0; i < e->fragments; i++) {
            Vec3f dir = e->dir, vel;
            Matrix m, r, t;
            float speed;
            int k;

            speed = Random_Float(Game_GetRandom()) * (e->maxSpeed - e->minSpeed) + e->minSpeed;
            Matrix_SetDirection(&m, &dir);
            for (k = 0; k < 9; k++)
                t.m[k] = m.m[(k % 3) * 3 + k / 3];
            Matrix_SetRotZ(&r, Random_Float(Game_GetRandom()) * kPi);
            mat_mul(&m, &t, &r);
            /* the original re-orthonormalizes both products after 1000
               changes, but only into copies it never reads */
            Matrix_SetRotX(&r, Random_Float(Game_GetRandom()) * e->spread);
            mat_mul(&t, &m, &r);
            vel.x = t.m[2] * speed;
            vel.y = t.m[5] * speed;
            vel.z = t.m[8] * speed;
            Random_Float(Game_GetRandom());
            Fragment_New(e, &pos, &vel, 0, kFragmentSize);
        }
        SimpointObjContainer_SetAutoDelete(e->group, 1);
    }
    {
        Task *sound = Sound_Play(Level_GetDynamic(), e->sound);

        if (sound)
            Sound_SetPos(sound, &pos);
    }
    if (e->blast < kZero || e->blast > kZero) {
        BlastMsg msg;

        msg.fireball = &fb->obj.task;
        msg.pos = &pos;
        msg.blast = e->blast;
        msg.size = e->size;
        msg.unk58 = e->unk58;
        msg.unk14 = 90;
        msg.unk84 = e->unk84;
        for (i = 0; i < Game_GetTaskList3()->count; i++) {
            uint16_t type = Game_GetTaskList3()->tasks[i]->type;
            TaskFn fn = HANDLER(Game_GetEvent11(), type);

            fn(Game_GetTaskList3()->tasks[i], &msg);
        }
    }
}
HOOK(0x47F200, Explosion_Burst);

static long double max_scale(const float *s)
{
    float m = s[2] < s[1] ? s[1] : s[2];

    return s[0] > m ? s[0] : m;
}

FireBall *FireBall_New(Vec3d *pos, const char *model, float scale, int life, int a5, int a6, int delay,
                       int animFrames, float angle, float spin)
{
    FireBall *fb = (FireBall *)Task_New(Level_GetDynamic(), gFireBallType, NULL);
    void **vm = VirModel_Find(model);
    float r;

    if (!vm) {
        Error_Show("VirModel is null");
        for (;;)
            ;
    }
    RigidDynCubeObj_SetModel(fb, vm);
    r = max_scale(fb->obj.scale);
    r = VirModel_GetRadius(fb->obj.model) * r;
    DynCube_Move(Game_GetWorld(), fb, pos, r);
    fb->animFrames = animFrames;
    fb->obj.scale[0] = kThousandth;
    fb->obj.scale[1] = kThousandth;
    fb->obj.scale[2] = kThousandth;
    fb->life = life;
    fb->frames = life;
    fb->delay = delay;
    fb->unk100 = a5;
    fb->unk104 = a5;
    fb->size = scale * kFireballScale;
    fb->angle = angle;
    fb->spin = spin;
    Debug_Log(fb->obj.unkA4 + 0x1C, vm);
    return fb;
}
HOOK(0x480060, FireBall_New);

void Explosion_Init(void)
{
    gExplosionType = TaskType_New("Explosion0", 0x88, Task_GetRootType(), 0x20);
    TaskType_SetHandler(gExplosionType, 0, Explosion_Create);
    TaskType_SetHandler2(gExplosionType, 1, Explosion_Run);
    TaskType_SetHandler2(gExplosionType, 2, TASK_NOTHING);

    gFireBallType = TaskType_New("FireBall", 0x118, RigidDynCubeObj_GetType(), 0x60);
    TaskType_SetHandler(gFireBallType, 0, FireBall_Create);
    TaskType_SetHandler(gFireBallType, 4, FireBall_Draw);
    TaskType_SetHandler(gFireBallType, Game_GetEvent7(), TASK_FALSE);
    TaskType_SetHandler(gFireBallType, Game_GetEvent8(), TASK_FALSE);
    TaskType_SetHandler(gFireBallType, Game_GetEvent13(), TASK_FALSE);
    TaskType_SetHandler(gFireBallType, 0xC, TASK_FALSE);
    TaskType_SetHandler2(gFireBallType, 1, FireBall_Run);
    TaskType_SetHandler2(gFireBallType, 2, FireBall_Delete);
    TaskType_SetHandler(gFireBallType, AIDetection_GetEvent0(), TASK_FALSE);

    gFragmentType = TaskType_New("Fragment", 0x118, SimpointObj_GetType(), 0xA0);
    TaskType_SetHandler2(gFragmentType, 1, Fragment_Run);
    TaskType_SetHandler(gFragmentType, 0xC, TASK_FALSE);
    TaskType_SetHandler(gFragmentType, Game_GetEvent7(), NULL);
    TaskType_SetHandler(gFragmentType, Game_GetEvent8(), NULL);
    TaskType_SetHandler(gFragmentType, Game_GetEvent13(), NULL);
    TaskType_SetHandler(gFragmentType, AIDetection_GetEvent0(), NULL);

    gExplosionSmokeType = TaskType_New("ExplosionSmoke", 0x48, Task_GetRootType(), 0x40);
    TaskType_SetHandler(gExplosionSmokeType, 0, TASK_NOTHING);
    TaskType_SetHandler2(gExplosionSmokeType, 1, ExplosionSmoke_Run);
    TaskType_SetHandler2(gExplosionSmokeType, 2, TASK_NOTHING);
    gBurnTexture = Game_AddTexture("burn1.tex");
    gSmoke2Sprite = Game_AddSprite("smoke2.spr");
    gFireballSprite = Game_AddSprite("fireball.spr");
}
HOOK(0x4801B0, Explosion_Init);

/* a fast puff at the start and a slow one at 90%, which fades in and out */
void ExplosionSmoke_Run(ExplosionSmoke *s)
{
    float sz[2];
    int total;

    if (s->timer <= 0) {
        Task_Delete(s);
        return;
    }
    total = s->frames;
    if (s->timer == total) {
        int n1 = round_int((long double)total * kTenth), n2;
        float tf = total, life, psize;
        long double r = Random_Float(Game_GetRandom());

        sz[0] = s->size;
        sz[1] = s->size;
        n2 = round_int((r * kFifth + kFourFifths) * tf);
        life = kOneF / tf;
        psize = (Random_Float(Game_GetRandom()) + kOneF) * kTenth;
        ExplosionSmoke_Emit(s, 0x1000008, sz, life, n1, n2, psize);
    }
    total = s->frames;
    if (s->timer == round_int((long double)total * kNineTenths)) {
        sz[0] = s->size;
        sz[1] = s->size;
        s->ps2 = ExplosionSmoke_Emit(s, 0x1000010, sz, (float)(kOneF / (long double)total), total, 0, kThousandth);
    }
    if (s->ps2) {
        long double t = s->timer, T = s->frames, h, v;
        float tf = T;

        h = T * kHalf;
        if (t > h)
            v = kOneF - (t - h) / (tf * kNineTenths - h);
        else
            v = t / h;
        Particle2D_SetLevel(s->ps2, (float)(v * kThreeTenths));
    }
    s->timer--;
}
HOOK(0x480420, ExplosionSmoke_Run);

void Fragment_Run(Fragment *f)
{
    if (f->kind)
        Fragment_Smoke(f);
    ((void (*)(void *))HANDLER(1, SimpointObj_GetType()))(f);
}
HOOK(0x4805B0, Fragment_Run);

void FireBall_Create(FireBall *fb)
{
    Vec3d zero = {0.0, 0.0, 0.0};
    float r;

    RigidDynCubeObj_Init(Game_GetWorld(), fb, NULL);
    r = max_scale(fb->obj.scale);
    r = VirModel_GetRadius(fb->obj.model) * r;
    DynCube_Add(Game_GetWorld(), fb, &zero, r, 2);
    fb->angle = 0.0f;
    fb->frame = -1;
}
HOOK(0x4805F0, FireBall_Create);

/* waits out the delay, then steps the animation over its life */
void FireBall_Run(FireBall *fb)
{
    if (fb->delay > 0) {
        fb->delay--;
        return;
    }
    if (ModelLod_GetTexAnimCount(*fb->obj.model)) {
        int n = ModelLod_GetTexAnimFrames(*fb->obj.model, 0);

        fb->frame = round_int((long double)(fb->frames - fb->life) / fb->animFrames * n) % n;
    } else {
        fb->frame = -1;
    }
    if (fb->life)
        fb->life--;
    else
        Task_Delete(fb);
}
HOOK(0x4806E0, FireBall_Run);

static void scale_vec(float *v, long double len2)
{
    long double s = kOne / x87_sqrt(len2);

    v[0] = (float)(v[0] * s);
    v[1] = (float)(v[1] * s);
    v[2] = (float)(v[2] * s);
}

static int nonzero(const float *v)
{
    return v[0] != kZero || v[1] != kZero || v[2] != kZero;
}

/* turns to face the camera, spins and draws the current frame */
void FireBall_Draw(FireBall *fb)
{
    float *m = fb->obj.matrix.m;
    Matrix n, t;
    float a[3], b[3], z;
    int junk[3];
    DrawMsg msg;
    Task *c;
    int i;

    if (fb->delay > 0)
        return;
    z = (kOneF - (long double)fb->life / fb->frames) * fb->size + kHundredth;
    fb->obj.scale[0] = fb->size;
    fb->obj.scale[1] = fb->size;
    fb->obj.scale[2] = z;
    m[6] = gViewPos.x - fb->obj.pos.x;
    m[7] = gViewPos.y - fb->obj.pos.y;
    z = gViewPos.z - fb->obj.pos.z;
    m[8] = z;
    if (m[6] == kZero && m[7] == kZero && z == kZero)
        m[6] = 1.0f;
    else
        scale_vec(m + 6, (z * z + m[6] * m[6]) + m[7] * m[7]);
    if (kZero == m[6] && kZero == m[7] && *(uint32_t *)&m[8] == 0x3F800000) {
        m[0] = 1.0f;
        m[1] = 0.0f;
        m[2] = 0.0f;
        m[3] = 0.0f;
        m[4] = 1.0f;
        m[5] = 0.0f;
    } else {
        m[0] = kZero - m[7];
        m[1] = -(kZero - m[6]);
        m[2] = 0.0f;
        m[3] = m[7] * m[2] - m[8] * m[1];
        m[4] = -(m[6] * m[2] - m[8] * m[0]);
        m[5] = m[6] * m[1] - m[7] * m[0];
    }

    /* orthonormal rows: x, z cross x, x cross that */
    a[0] = m[0];
    a[1] = m[1];
    a[2] = m[2];
    if (nonzero(a))
        scale_vec(a, (a[0] * a[0] + a[1] * a[1]) + a[2] * a[2]);
    b[0] = m[7] * m[2] - m[8] * m[1];
    b[1] = -(m[6] * m[2] - m[8] * m[0]);
    b[2] = m[6] * m[1] - m[7] * m[0];
    n.m[0] = a[0];
    n.m[1] = a[1];
    n.m[2] = a[2];
    n.m[3] = b[0];
    n.m[4] = b[1];
    n.m[5] = b[2];
    n.m[6] = b[2] * a[1] - b[1] * a[2];
    n.m[7] = -(b[2] * a[0] - b[0] * a[2]);
    n.m[8] = b[1] * a[0] - b[0] * a[1];
    if (nonzero(b))
        scale_vec(n.m + 3, (b[2] * b[2] + b[1] * b[1]) + b[0] * b[0]);
    if (nonzero(n.m + 6))
        scale_vec(n.m + 6, (n.m[6] * n.m[6] + n.m[7] * n.m[7]) + n.m[8] * n.m[8]);
    for (i = 0; i < 9; i++)
        m[i] = n.m[i];
    fb->obj.matrix.stamp = 0;

    for (i = 0; i < 9; i++)
        n.m[i] = i % 4 == 0 ? 1.0f : 0.0f;
    n.stamp = 0;
    Matrix_RotateZ(&n, &n, fb->angle);
    fb->angle = fb->spin + fb->angle;

    /* matrix = transpose(matrix) * n */
    for (i = 0; i < 9; i++)
        t.m[i] = m[(i % 3) * 3 + i / 3];
    t.stamp = fb->obj.matrix.stamp;
    for (i = 0; i < 3; i++) {
        m[i] = (n.m[i] * t.m[0] + n.m[3 + i] * t.m[1]) + n.m[6 + i] * t.m[2];
        m[3 + i] = (n.m[6 + i] * t.m[5] + n.m[i] * t.m[3]) + n.m[3 + i] * t.m[4];
        m[6 + i] = (n.m[6 + i] * t.m[8] + n.m[3 + i] * t.m[7]) + n.m[i] * t.m[6];
    }
    fb->obj.matrix.stamp = t.stamp + n.stamp + 1;
    Light_GetActive(&junk[0]);
    Haze_GetVisible(&junk[1], &junk[2]);
    if (fb->frame >= 0) {
        RenderArg ra;

        ModelLod_SetTexAnimFrame(*fb->obj.model, 0, fb->frame);
        ra.obj = fb;
        ra.unk4 = Light_GetActive(&ra.unk8);
        Haze_GetVisible(&ra.unkC, &ra.unk10);
        gRenderFuncs[Mesh3D_GetDrawRigidMethod()](&ra);
    }
    msg.pos = fb->obj.pos;
    msg.matrix = fb->obj.matrix;
    c = fb->obj.task.head;
    if (!c->next || !c)
        return;
    do {
        TaskFn fn;

        gTaskStack[gTaskStackTop++] = c->next && c->next->next ? c->next : NULL;
        fn = HANDLER(RigidDynCubeObj_GetEvent2(), c->type);
        if (fn)
            fn(c, &msg);
        c = gTaskStack[--gTaskStackTop];
    } while (c);
}
HOOK(0x480790, FireBall_Draw);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5333C8) const float kTenth INIT(0.1f);
ALIAS(kDepthFade2, kTenth, 0);
DATA(533518) const float kQuarter INIT(0.25f);
DATA(53353C) const float kThreeHalves INIT(1.5f);
ALIAS(kOneHalf, kThreeHalves, 0);
ALIAS(kSplitDist, kThreeHalves, 0);
DATA(5336A4) const float kThreeTenths INIT(0.3f);
ALIAS(kShadowMinFade, kThreeTenths, 0);
ALIAS(kSmokeSize, kThreeTenths, 0);
PAD_CONST(igd, 534180, 534180, 0x4) INIT({0x94, 0xCE, 0xA6, 0x3C});
DATA(534180) const float kBlastSmokeSpin INIT(0.018849557f);
DATA(534180) const float kSmokeRate INIT(15.0f);
DATA(534180) const float kFragmentSpin INIT(0.006283186f);
DATA(534180) const float kTrailRate INIT(27.0f);
DATA(534180) const float kFragmentScale INIT(0.00048828125f);
DATA(534180) const float kTumble INIT(0.03141593f);
PAD_CONST(igd, 534180, 53419C, 0x4);
DATA(534180) const double kSmokeScale INIT(0.8999999761581421);
DATA(534180) const float kSmokeFrames INIT(150.0f);
DATA(534180) const float kDelaySpread INIT(-15.0f);
DATA(534180) const double kFireballScale INIT(0.00048828125);
BSS(5C1154) uint16_t gExplosionType;
BSS(5C1154) uint16_t gFragmentType;
BSS(5C1154) char *gFireballSprite;
BSS(5C1154) uint16_t gExplosionSmokeType;
PAD(igb, 5C1154, 5C115E, 0x2);
BSS(5C1154) char *gSmoke2Sprite;
BSS(5C1154) char *gBurnTexture;
BSS(5C1154) uint16_t gFireBallType;
PAD(igb, 5C1154, 5C116A, 0x2);
