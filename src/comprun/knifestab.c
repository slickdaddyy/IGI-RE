#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "comprun/comprun.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x413B20, uint8_t, Weapon_GetFireEvent, (void));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4528F0, int, AI_HasTarget, (void *ai));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x4B3790, void, Matrix_RotateZ, (Matrix *out, const Matrix *m, float a));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *results, Vec3d *starts, Vec3d *ends, int count, char unk,
                                  void *query, unsigned int flags));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));

/* what Weapon_Fire hands to the weapon's task type */
typedef struct FireArgs {
    void *unk0;
    void *unk4;
    Vec3d *pos;
    int unkC;
    int unk10;
    Matrix *matrix;
    int unk18;
    int unk1C;
    char *weapon;
    int unk24;
} FireArgs;

/* the hit message a stab sends to the task it hits */
typedef struct StabHit {
    void *hit;
    float damage;
    float dir[3];
    int unk14;
    char *weapon;
    int unk1C;
} StabHit;

extern uint16_t gKnifeStabType;

static const double kReach = 8192.0;
static const float kStep = 0x1.657186p-3f;

int KnifeStab_Try(FireArgs *f, float angle);

void KnifeStab_Fire(void *t, FireArgs *f)
{
    float a = -0x1.0c1524p-2f;
    int i;

    (void)t;
    for (i = 0; i < 3; i++) {
        if (KnifeStab_Try(f, a))
            return;
        a += kStep;
    }
}
HOOK(0x46DF70, KnifeStab_Fire);

void KnifeStab_Init(void)
{
    gKnifeStabType = TaskType_New("KnifeStab", 0x20, Vehicle_GetType(), 0);
    TaskType_SetHandler(gKnifeStabType, Weapon_GetFireEvent(), KnifeStab_Fire);
    Script_RegisterConstant("TASKTYPE_KNIFESTAB", gKnifeStabType);
}
HOOK(0x46DF10, KnifeStab_Init);

/* stabs along the weapon's direction turned by angle, returns 1 on a hit */
int KnifeStab_Try(FireArgs *f, float angle)
{
    struct {
        int unk0;
        int unk4;
        void *unk8;
        void *unkC;
        float unk10;
    } query = {0, 0, 0, 0, 0.0f};
    char hit[0x70];
    Vec3d start, end;
    Matrix m;
    StabHit msg;
    double dy, dz;
    Task *victim;

    Matrix_RotateZ(&m, f->matrix, angle);
    dy = m.m[4];
    dz = m.m[7];
    start = *f->pos;
    dy *= kReach;
    dz *= kReach;
    end.x = start.x + m.m[1] * kReach;
    end.y = start.y + dy;
    end.z = start.z + dz;
    query.unk4 = 0;
    query.unk8 = f->unk4;
    query.unkC = f->unk0;
    query.unk10 = 1.0f;
    {
        uint8_t ev = Game_GetEvent7();

        Coll_Lines(Game_GetWorld(), hit, &start, &end, 1, ev, &query, 0xFFFFFFFF);
    }
    if (!*(int *)(hit + 0x58))
        return 0;
    msg.hit = hit;
    msg.dir[0] = m.m[1];
    msg.dir[1] = m.m[4];
    msg.dir[2] = m.m[7];
    msg.unk14 = 0;
    msg.weapon = f->weapon;
    msg.unk1C = f->unk24;
    msg.damage = *(float *)(f->weapon + 0x654);
    victim = *(Task **)hit;
    if (!victim)
        return 0;
    if (!gTaskHandlers[Game_GetEvent9()][victim->type])
        return 0;
    if (TaskType_IsA(victim->type, Human_GetType())) {
        void *ai = *(void **)((char *)victim + 0x800);

        if (ai && !AI_HasTarget(ai))
            msg.damage = 1000.0f;
    }
    gTaskHandlers[Game_GetEvent9()][victim->type](victim, &msg);
    return 1;
}
HOOK(0x46DFB0, KnifeStab_Try);

void KnifeStab_Close(void)
{
    TaskType_Free(gKnifeStabType);
}
HOOK(0x46E200, KnifeStab_Close);
