/* Bullet flight: each frame a bullet travels its whole speed, hitting,
   piercing and ricocheting off what is in the way. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "ammotype/ammotype.h"
#include "material/materialtypes.h"
#include "coll/colltypes.h"

typedef struct Bullet {
    int used;
    int unk04;
    Vec3d pos;
    Vec3f dir;
    void *type;      /* 0x2C, WeaponType */
    int unk30;
    float speed;     /* 0x34 */
    int unk38;
    float pierce;    /* 0x3C, compared with the material's hardness */
    char ricocheted; /* 0x40 */
    char pierced;    /* 0x41, during the last step */
    char pad42[2];
    int unk44;
} Bullet;

typedef struct GunShot {
    Task task;
    char unk20[0x4C];
    Task **gun;             /* 0x6C */
    Task **ref70;           /* 0x70 */
    char unk74[0x20];
    int active;             /* 0x94 */
    int trailRate;          /* 0x98, steps until the next trail */
    char unk9C[8];
    Bullet *activeList[15]; /* 0xA4 */
} GunShot;

/* the filter Coll_Lines passes to the objects it meets */
typedef struct BulletFilter {
    int result; /* set by the filter handlers, not here */
    int unk4;
    Task *gun;
    Task *owner;
    int unk10;
} BulletFilter;

/* sent to the task that was hit */
typedef struct BulletHitMsg {
    CollHit *hit;
    int unk38;
    float delta[3];
    Task *owner;
    void *type;
    int unk30;
} BulletHitMsg;

/* an impact effect queued until the bullet is done */
typedef struct BulletImpact {
    uint16_t effect;
    char pad2[2];
    void *params;
    CollHit hit;
    Vec3f dir;  /* 0x78 */
    int unk84;
} BulletImpact;

_Static_assert(sizeof(Bullet) == 0x48, "Bullet");
_Static_assert(offsetof(GunShot, activeList) == 0xA4, "GunShot");
_Static_assert(sizeof(BulletImpact) == 0x88, "BulletImpact");

ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x4083C0, void, GameMaterial_Impact, (uint16_t type, void *params, CollHit *hit, int front, Vec3f *dir));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x47C330, void, GunShot_PlayRicochet, (Vec3d *pos));
ORIG(0x47C4C0, void, GunShot_AddTrail, (GunShot *s, Vec3d *a, Vec3d *b, float from, float to, float len, int life));
ORIG(0x4B2B30, long double, Vec3_Angle, (const Vec3f *a, const Vec3f *b));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4B47C0, int, Random_Int, (void *rng, int n));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *results, Vec3d *starts, Vec3d *ends, int count,
                                  uint8_t event, BulletFilter *filter, unsigned int flags));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));

extern int gImpactSoundTime;
extern int gImpactSoundDelay;

extern const float kBZero;
extern const float kBOne;
extern const double kBOneD;
extern const float kDegToRad;
extern const float kHalfPi;
extern const float kPierceChance; /* 0.6 */
extern const float kSurfaceGap;   /* 20.48 */

static inline long double Bullet_Cos(long double x)
{
    __asm__("fcos" : "+t"(x));
    return x;
}

static void Bullet_Normalize(Vec3f *out, const Vec3f *v)
{
    *out = *v;
    if (v->x != kBZero || v->y != kBZero || v->z != kBZero) {
        long double inv = kBOneD / __builtin_sqrtl((long double)v->x * v->x + (long double)v->y * v->y +
                                                   (long double)v->z * v->z);

        out->x = v->x * inv;
        out->y = out->y * inv;
        out->z = out->z * inv;
    }
}

void GunShot_MoveBullets(GunShot *s)
{
    int i;

    for (i = 0; i < s->active; i++) {
        Bullet *b = s->activeList[i];
        BulletImpact impacts[10];
        int skip = 0, count = 0, k;
        float left = b->speed, trailLen = 0;
        Vec3f dir = b->dir;
        Vec3d pos = b->pos;

        while (left > kBZero) {
            BulletFilter filter;
            BulletHitMsg msg;
            CollHit hit;
            Vec3d end, from, to;
            Vec3f step, back;
            GameMaterial *m;
            float dx, dy, dz;
            char pierced = 0;
            uint8_t ev;
            void *world;
            Task *gun;

            step.x = dir.x * left;
            step.y = dir.y * left;
            step.z = dir.z * left;
            filter.unk4 = 0;
            end.x = step.x + pos.x;
            end.y = step.y + pos.y;
            end.z = step.z + pos.z;
            filter.gun = s->gun ? *s->gun : NULL;
            filter.owner = s->ref70 ? *s->ref70 : NULL;
            filter.unk10 = b->unk44;
            ev = Game_GetEvent7();
            world = Game_GetWorld();
            Coll_Lines(world, &hit, &pos, &end, 1, ev, &filter, 0xFFFFFFFF);
            from = pos;
            to = end;
            if (!hit.found) {
                left = 0;
                goto trail;
            }
            msg.type = b->type;
            msg.unk38 = b->unk38;
            msg.hit = &hit;
            msg.unk30 = b->unk30;
            msg.owner = s->ref70 ? *s->ref70 : NULL;
            dx = end.x - pos.x;
            msg.delta[0] = dx;
            dy = end.y - pos.y;
            msg.delta[1] = dy;
            dz = end.z - pos.z;
            msg.delta[2] = dz;
            m = hit.material;
            if (!m) {
                /* leaving a surface from behind */
                if (!hit.back)
                    goto move;
                if (count < 10) {
                    impacts[count].effect = hit.back->backEffect;
                    impacts[count].params = hit.back->backParams;
                    impacts[count].hit = hit;
                    impacts[count].dir.x = dx;
                    impacts[count].dir.y = dy;
                    impacts[count].dir.z = dz;
                    count++;
                }
                skip = 1;
                goto move;
            }
            if (hit.back)
                goto move;
            if (hit.task && HANDLER(Game_GetEvent9(), hit.task->type))
                HANDLER(Game_GetEvent9(), hit.task->type)(hit.task, &msg);
            if (gImpactSoundTime != -1) {
                int d;

                if ((int)((unsigned)Flow_GetFrames() - gImpactSoundTime) < 0)
                    d = (int)((unsigned)gImpactSoundTime - Flow_GetFrames());
                else
                    d = (int)((unsigned)Flow_GetFrames() - gImpactSoundTime);
                if (d <= gImpactSoundDelay)
                    goto sounded;
            }
            {
                const char *name = m->sounds[Random_Int(Game_GetRandom(), 6)];
                Task *snd = Sound_Play(Level_GetDynamic(), name);

                if (snd)
                    Sound_SetPos(snd, &hit.pos);
                GunShot_PlayRicochet(&hit.pos);
                gImpactSoundTime = Flow_GetFrames();
                gImpactSoundDelay = Random_Int(Game_GetRandom(), 6) + 6;
            }
sounded:
            if (skip) {
                left = 0;
                skip = 0;
            } else {
                float angle, chance;

                back.x = -step.x;
                back.y = -step.y;
                back.z = -step.z;
                angle = Vec3_Angle(&back, &hit.normal);
                if (m->ricochet == kBZero) {
                    chance = 0;
                } else {
                    long double max = m->ricochet * kDegToRad;

                    chance = (max - (kHalfPi - angle)) / max;
                }
                if (!b->ricocheted && !(Random_Float(Game_GetRandom()) >= chance)) {
                    /* mirror the path on the surface */
                    Vec3f n = hit.normal;
                    long double nz, c, nzc, tx;
                    float ty, tz;

                    if (hit.normal.x != kBZero || hit.normal.y != kBZero || hit.normal.z != kBZero) {
                        long double inv = kBOneD / __builtin_sqrtl((long double)hit.normal.x * hit.normal.x +
                                                                   (long double)hit.normal.y * hit.normal.y +
                                                                   (long double)hit.normal.z * hit.normal.z);

                        n.x = hit.normal.x * inv;
                        n.y = n.y * inv;
                        nz = inv * n.z;
                    } else {
                        nz = n.z;
                    }
                    c = Bullet_Cos(angle) * left;
                    n.x = c * n.x;
                    n.y = c * n.y;
                    nzc = nz * c;
                    tx = n.x - back.x;
                    ty = n.y - back.y;
                    tz = nzc - back.z;
                    dir.x = tx + n.x;
                    dir.y = ty + n.y;
                    dir.z = tz + nzc;
                    if (dir.x != kBZero || dir.y != kBZero || dir.z != kBZero) {
                        long double inv = kBOneD / __builtin_sqrtl((long double)dir.x * dir.x +
                                                                   (long double)dir.y * dir.y +
                                                                   (long double)dir.z * dir.z);

                        dir.x = dir.x * inv;
                        dir.y = dir.y * inv;
                        dir.z = dir.z * inv;
                    }
                    b->ricocheted = 1;
                    GunShot_PlayRicochet(&hit.pos);
                } else {
                    long double d = b->pierce - m->hardness;
                    int through;

                    if (d == kBZero)
                        through = !(Random_Float(Game_GetRandom()) >= kPierceChance);
                    else
                        through = d >= kBOne;
                    if (through) {
                        pierced = 1;
                        if (!hit.task)
                            left = 0;
                    } else {
                        left = 0;
                    }
                }
            }
            if (count < 10 && *(int *)(m->params + 0x58)) {
                BulletImpact *r = &impacts[count];
                long double ax, az, dot, p;
                Vec3f n;
                float ay, azf;

                r->effect = m->effect;
                r->params = m->params;
                r->hit = hit;
                Bullet_Normalize(&n, &hit.normal);
                count++;
                /* the incoming path reflected on the surface */
                ax = from.x - to.x;
                ay = from.y - to.y;
                az = from.z - to.z;
                azf = az;
                dot = az * n.z + ay * n.y;
                dot = dot + ax * n.x;
                p = dot * n.x;
                n.x = p + p;
                p = dot * n.y;
                n.y = p + p;
                p = dot * n.z;
                n.z = p + p;
                r->dir.x = n.x - ax;
                r->dir.y = n.y - ay;
                r->dir.z = n.z - azf;
            }
move:
            left = left - hit.dist;
            {
                long double gx = dir.x * kSurfaceGap;
                float gy = dir.y * kSurfaceGap;
                float gz = dir.z * kSurfaceGap;

                pos.x = gx + hit.pos.x;
                pos.y = gy + hit.pos.y;
                pos.z = gz + hit.pos.z;
            }
            if (hit.found)
                to = hit.pos;
trail:
            b->pierced = pierced;
            gun = *s->gun;
            if (gun && (*(AmmoType **)((char *)gun + 0x12C))->tracer) {
                if (s->trailRate > 0 && !b->ricocheted && !pierced) {
                    s->trailRate--;
                } else {
                    float tx = from.x - to.x;
                    float ty = from.y - to.y;
                    long double tz = from.z - to.z;
                    long double len = __builtin_sqrtl((long double)tx * tx + (long double)ty * ty + tz * tz);

                    if (len > kBZero) {
                        float next = len + trailLen;

                        s->trailRate = 10;
                        GunShot_AddTrail(s, &from, &to, trailLen, next, b->speed, 30);
                        trailLen = next;
                    }
                }
            }
        }
        for (k = 0; k < count; k++) {
            BulletImpact *r = &impacts[k];
            int front;

            if (b->ricocheted)
                front = 0;
            else if (k < count - 1)
                front = 1;
            else
                front = !((long double)r->dir.x * r->hit.normal.x + (long double)r->dir.y * r->hit.normal.y +
                              (long double)r->dir.z * r->hit.normal.z > kBZero);
            GameMaterial_Impact(r->effect, r->params, &r->hit, front, &r->dir);
        }
        b->used = 0;
        if (i < s->active - 1) {
            s->activeList[i] = s->activeList[s->active - 1];
            i--;
        }
        s->active--;
    }
}
HOOK(0x47B750, GunShot_MoveBullets);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533468) const float kHalfPi INIT(1.5707964f);
DATA(534148) const float kSurfaceGap INIT(20.48f);
ALIAS(kOff0Z, kSurfaceGap, 0);
DATA(540838) int gImpactSoundTime INIT(-1);
BSS(5BE3D8) int gImpactSoundDelay;
