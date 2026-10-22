/* Human: the task type shared by the player and the AI soldiers. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "soldier/soldier.h"

typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    void *type;
    int unk14;
} ScriptValue;

typedef struct TargetQuery {
    char result;
    char unk1[3];
    Human *skip[3];
    float chance;
} TargetQuery;

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (int unk));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401B20, void, TaskRef_Remove, (void *ref));
ORIG(0x401F60, void, Task_SetGlobalFlags, (uint32_t flags));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x4168D0, void *, Game_GetTaskList3, (void));
ORIG(0x416930, int, Game_AddTexture, (const char *name));
ORIG(0x4169D0, int, Game_AddSprite, (const char *name));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416FC0, int, Game_CountHuman, (void));
ORIG(0x450C80, uint8_t, sub_450C80, (void));
ORIG(0x450CC0, uint8_t, sub_450CC0, (void));
ORIG(0x463130, int, Human_GetTeam, (Human *h));
ORIG(0x463150, char, sub_463150, (Human *h));
ORIG(0x463580, void, sub_463580, (Human *h));
ORIG(0x464610, void *, HumanView_New, (Human *h));
ORIG(0x466230, uint8_t, sub_466230, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x46AB20, uint8_t, sub_46AB20, (void));
ORIG(0x46C970, uint8_t, sub_46C970, (void));
ORIG(0x46D410, uint8_t, sub_46D410, (void));
ORIG(0x477030, uint8_t, sub_477030, (void));
ORIG(0x477C00, uint8_t, sub_477C00, (void));
ORIG(0x477C10, uint8_t, sub_477C10, (void));
ORIG(0x477C20, uint8_t, sub_477C20, (void));
ORIG(0x477C30, uint8_t, sub_477C30, (void));
ORIG(0x477C40, uint8_t, sub_477C40, (void));
ORIG(0x47CDC0, uint8_t, sub_47CDC0, (void));
ORIG(0x47CDD0, void, AmmoList_Init, (void *list));
ORIG(0x484D30, uint8_t, sub_484D30, (void));
ORIG(0x484DC0, void, sub_484DC0, (Human *h));
ORIG(0x484E00, void, sub_484E00, (Human *h));
ORIG(0x487190, uint8_t, sub_487190, (void));
ORIG(0x489610, uint8_t, sub_489610, (void));
ORIG(0x489620, uint8_t, sub_489620, (void));
ORIG(0x489640, int, AIDetection_AddMaskType, (uint16_t type));
ORIG(0x489680, void, AIDetection_Add, (Human *h));
ORIG(0x4896A0, void, AIDetection_Remove, (Human *h));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B6F30, void, Text_Free, (void *text));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1800, void *, QTaskList_New, (int max));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D14D0, uint8_t, sub_4D14D0, (void));
ORIG(0x4D9610, uint16_t, sub_4D9610, (void));
ORIG(0x4E0CC0, void *, Shadow_New, (Human *h, void *a, int *b, int *c, void *d, void *e, float size, float height,
                                   int tex));
ORIG(0x4E6610, uint8_t, sub_4E6610, (void));
ORIG(0x4F0F20, void *, Param_4F0F20, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1030, Human *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, sub_4F1210, (void));
ORIG(0x4F1220, int, sub_4F1220, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F1FC0, uint8_t, sub_4F1FC0, (void));
ORIG(0x4F2050, void, WeaponSlots_Init, (void **slots));
ORIG(0x4FC840, uint8_t, sub_4FC840, (void));
ORIG(0x4FDEF0, uint8_t, sub_4FDEF0, (void));
ORIG(0x504A30, int, sub_504A30, (const char *name, void *fn));
ORIG(0x5096A0, uint8_t, sub_5096A0, (void));

#define gHumanType VAR(uint16_t, 0x5BDAF8)
#define gHumanEvent0 VAR(uint8_t, 0x53F316)
#define gHumanEvent1 VAR(uint8_t, 0x53F317)
#define gHumanList VAR(void *, 0x5BDC24)
#define gHumanParams VAR(void *, 0x5BDC18)
#define gHumanCriteria VAR(int, 0x5BDB00)
#define gHumanCriteriaN ((int *)0x5BDAD8)
#define gHumanCriteriaNames ((char (*)[0x20])0x5BDB18)
#define gHumanTeamNames ((const char **)0x53F348)
#define gHumanUnk5BDC28 ((int *)0x5BDC28)
#define gShadowTex VAR(int, 0x5BDB0C)
#define gBloodHole1 VAR(int, 0x5BDB08)
#define gBloodHole2 VAR(int, 0x5BDB10)
#define gBloodHole3 VAR(int, 0x5BDB14)
#define gBloodHole4 VAR(int, 0x5BDB04)
#define gBloodSprite1 VAR(int, 0x5BDAD4)
#define gBloodSprite2 VAR(int, 0x5BDAD0)
#define gHumanMaskType VAR(int, 0x5BDC3C)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define gScriptNumberType ((void *)0x567C74)

/* handlers of the Human type that live after this file */
#define Human_Handler45F280 ((void *)0x45F280)
#define Human_Handler45F050 ((void *)0x45F050)
#define Human_Handler45F500 ((void *)0x45F500)
#define Human_Handler45F800 ((void *)0x45F800)
#define Human_GetParams ((void *)0x45F880)
#define Human_ParamsChanged ((void *)0x45F890)
#define Human_Handler45F900 ((void *)0x45F900)
#define Human_Handler45F910 ((void *)0x45F910)
#define Human_Handler45F930 ((void *)0x45F930)
#define Human_Handler45F920 ((void *)0x45F920)
#define Human_Handler45F9D0 ((void *)0x45F9D0)
#define Human_Handler45FA80 ((void *)0x45FA80)
#define Human_Handler416AB0 ((void *)0x416AB0)
#define Human_GetHumanCameraInfoHandler ((void *)0x45FCD0)
#define Human_Handler45FDA0 ((void *)0x45FDA0)
#define Human_Handler45FE30 ((void *)0x45FE30)
#define Human_Handler42FC50 ((void *)0x42FC50)
#define Human_Handler45FE70 ((void *)0x45FE70)
#define WeaponAmmoPickup ((void *)0x45FF80)
#define WeaponGunPickup ((void *)0x45FFC0)
#define Human_Handler460070 ((void *)0x460070)
#define Human_Handler460090 ((void *)0x460090)
#define Human_gun_event_related_3 ((void *)0x4600C0)
#define Human_gun_event_related_4 ((void *)0x4601A0)
#define Human_gun_event_related_5 ((void *)0x460210)
#define Human_Handler4D3710 ((void *)0x4D3710)
#define Human_Handler460280 ((void *)0x460280)
#define Human_Handler460400 ((void *)0x460400)
#define Human_Handler4606D0 ((void *)0x4606D0)
#define Human_Handler4606F0 ((void *)0x4606F0)
#define Human_Handler4F64E0 ((void *)0x4F64E0)
#define Human_Handler460850 ((void *)0x460850)
#define Human_Handler4608D0 ((void *)0x4608D0)
#define Human_Handler460950 ((void *)0x460950)
#define Human_Handler4609A0 ((void *)0x4609A0)
#define Human_Handler4609F0 ((void *)0x4609F0)
#define Human_Handler460AC0 ((void *)0x460AC0)
#define Human_Criteria ((void *)0x460A70)
#define Human_CriteriaN ((void *)0x460A10)

static const float kCutSceneScale = 1.5f;
static const float kShadowSize = 4096000.0f;
static const float kShadowHeight = 0x1.ccccccp+10f;

/* fistp with the current rounding mode */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

void Human_ClearResult(void *task, char *out)
{
    (void)task;
    *out = 0;
}
HOOK(0x45E3D0, Human_ClearResult);

/* Whether h can be picked from the query: not one of the three skipped, then
   by chance for some humans, otherwise when not on the third skipped one's
   team. */
void sub_45E3E0(Human *h, TargetQuery *q)
{
    if (h != q->skip[0] && h != q->skip[1] && h != q->skip[2]) {
        if (sub_463150(h)) {
            if (!(Random_Float(Game_GetRandom()) > q->chance)) {
                q->result = 1;
                return;
            }
        } else {
            if (q->skip[2]) {
                int team = Human_GetTeam(q->skip[2]);

                if (team == Human_GetTeam(h))
                    goto no;
            }
            q->result = 1;
            return;
        }
    }
no:
    q->result = 0;
}
HOOK(0x45E3E0, sub_45E3E0);

void Human_CreateHandler(Human *h);
void Human_RunHandler(Human *h);
void Human_DeleteHandler(Human *h);
void Human_GetCutSceneDeltaTime(ScriptValue *ret);

void Human_Open(void)
{
    Human tmp;
    int i;

    Task_SetGlobalFlags(1);
    gHumanType = TaskType_New("Human", sizeof(Human), sub_4D9610(), 0);
    TaskType_SetFlags(gHumanType, 1);
    gHumanEvent0 = TaskEvent_Alloc(0);
    gHumanEvent1 = TaskEvent_Alloc(0);
    gHumanList = QTaskList_New(0x80);
    TaskType_SetHandler(gHumanType, 0, Human_CreateHandler);
    TaskType_SetHandler2(gHumanType, 1, Human_RunHandler);
    TaskType_SetHandler2(gHumanType, 2, Human_DeleteHandler);
    TaskType_SetHandler(gHumanType, 4, Human_Handler45F280);
    TaskType_SetHandler(gHumanType, sub_4D14D0(), Human_Handler45F050);
    TaskType_SetHandler(gHumanType, 9, Human_Handler45F500);
    TaskType_SetHandler(gHumanType, 8, Human_Handler45F800);
    TaskType_SetHandler(gHumanType, Param_GetTableEvent(), Human_GetParams);
    TaskType_SetHandler(gHumanType, Param_GetChangedEvent(), Human_ParamsChanged);
    TaskType_SetHandler(gHumanType, sub_4F1FC0(), Human_Handler45F900);
    TaskType_SetHandler(gHumanType, sub_47CDC0(), Human_Handler45F910);
    TaskType_SetHandler(gHumanType, Game_GetEvent7(), sub_45E3E0);
    TaskType_SetHandler(gHumanType, Game_GetEvent9(), Human_Handler45F930);
    TaskType_SetHandler(gHumanType, Game_GetEvent8(), Human_Handler45F920);
    TaskType_SetHandler(gHumanType, sub_4FDEF0(), Human_Handler45F920);
    TaskType_SetHandler(gHumanType, Game_GetEvent11(), Human_Handler45F9D0);
    TaskType_SetHandler(gHumanType, sub_466230(), Human_Handler45FA80);
    TaskType_SetHandler(gHumanType, Game_GetEvent13(), Human_Handler416AB0);
    TaskType_SetHandler(gHumanType, sub_484D30(), Human_GetHumanCameraInfoHandler);
    TaskType_SetHandler(gHumanType, sub_450C80(), Human_Handler45FDA0);
    TaskType_SetHandler(gHumanType, sub_489620(), Human_Handler45FE30);
    TaskType_SetHandler(gHumanType, sub_489610(), Human_Handler42FC50);
    TaskType_SetHandler(gHumanType, sub_450CC0(), Human_Handler45FE70);
    TaskType_SetHandler(gHumanType, sub_46D410(), WeaponAmmoPickup);
    TaskType_SetHandler(gHumanType, sub_46C970(), WeaponGunPickup);
    TaskType_SetHandler(gHumanType, sub_477C00(), Human_Handler460070);
    TaskType_SetHandler(gHumanType, sub_477C10(), Human_Handler460090);
    TaskType_SetHandler(gHumanType, sub_477C20(), Human_gun_event_related_3);
    TaskType_SetHandler(gHumanType, sub_477C30(), Human_gun_event_related_4);
    TaskType_SetHandler(gHumanType, sub_477C40(), Human_gun_event_related_5);
    TaskType_SetHandler(gHumanType, sub_4FC840(), Human_Handler4D3710);
    TaskType_SetHandler(gHumanType, ComputerObject_GetEvent0(), Human_Handler460280);
    TaskType_SetHandler(gHumanType, sub_487190(), Human_Handler460400);
    TaskType_SetHandler(gHumanType, sub_4E6610(), Human_Handler4606D0);
    TaskType_SetHandler(gHumanType, gHumanEvent0, Human_Handler4606F0);
    TaskType_SetHandler(gHumanType, sub_477030(), Human_Handler4F64E0);
    TaskType_SetHandler(gHumanType, 0xC, Human_Handler45F920);
    TaskType_SetHandler(gHumanType, sub_5096A0(), Human_ClearResult);
    TaskType_SetHandler2(gHumanType, sub_4F1210(), Human_Handler460850);
    TaskType_SetHandler2(gHumanType, sub_4F1220(), Human_Handler4608D0);
    TaskType_SetHandler(gHumanType, World_GetEvent0(), Human_Handler460950);
    TaskType_SetHandler(gHumanType, World_GetEvent1(), Human_Handler4609A0);
    TaskType_SetHandler(gHumanType, sub_46AB20(), Human_Handler4609F0);
    TaskType_SetHandler2(gHumanType, 3, Human_Handler460AC0);
    gHumanParams = ParamTable_New(9);
    ParamTable_Add(gHumanParams, &tmp, tmp.unkE0, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gHumanParams, &tmp, &tmp.unkF8, Param_4F0F20(), 7, NULL);
    ParamTable_Add(gHumanParams, &tmp, tmp.unk100, Param_String16(), 7, NULL);
    ParamTable_Add(gHumanParams, &tmp, &tmp.unkFC, Param_Int32(), 7, NULL);
    gHumanCriteria = sub_504A30("CRITERIA_HUMAN", Human_Criteria);
    for (i = 0; i < 8; i++) {
        crt_sprintf(gHumanCriteriaNames[i], "CRITERIA_HUMAN%d", i);
        gHumanCriteriaN[i] = sub_504A30(gHumanCriteriaNames[i], Human_CriteriaN);
    }
    for (i = 0; i < 2; i++)
        Script_RegisterConstant(gHumanTeamNames[i], i);
    for (i = 0; i < 5; i++)
        gHumanUnk5BDC28[i] = 0;
    Script_RegisterFunction("Human_GetCutSceneDeltaTime", Human_GetCutSceneDeltaTime, NULL, 0);
    gShadowTex = Game_AddTexture("shadow1.tex");
    gBloodHole1 = Game_AddTexture("hole_blood1.tex");
    gBloodHole2 = Game_AddTexture("hole_blood2.tex");
    gBloodHole3 = Game_AddTexture("hole_blood3.tex");
    gBloodHole4 = Game_AddTexture("hole_blood4.tex");
    gBloodSprite1 = Game_AddSprite("blood1.spr");
    gBloodSprite2 = Game_AddSprite("blood2.spr");
    gHumanMaskType = AIDetection_AddMaskType(gHumanType);
}
HOOK(0x45E450, Human_Open);

/* script function: input 0 of task 0 times 1.5 */
void Human_GetCutSceneDeltaTime(ScriptValue *ret)
{
    long double v = Task_FindById(0)->input[0] * kCutSceneScale;
    ScriptValue r;

    r.i = x87_fistp(v);
    r.d = v;
    r.type = gScriptNumberType;
    memcpy(ret, &r, sizeof(r));
}
HOOK(0x45EAE0, Human_GetCutSceneDeltaTime);

void Human_Reset(Human *h);

void Human_CreateHandler(Human *h)
{
    int i;

    QTaskList_Add(gHumanList, h);
    AIDetection_Add(h);
    QTaskList_Add(Game_GetTaskList3(), h);
    sub_484DC0(h);
    Human_Reset(h);
    memset(h->unkE0, 0, sizeof(h->unkE0));
    h->unkF8 = 0;
    h->unk100[0] = 0;
    h->unkFC = -1;
    WeaponSlots_Init(h->weapons);
    h->fire = 0;
    h->altFire = 0;
    h->fired = 0;
    h->weaponReady = 1;
    h->unk4D4 = 1.0f;
    h->view = HumanView_New(h);
    h->shadow = Shadow_New(h, h->unk7D0, &h->unk7E8, &h->unk7EC, h->unk7F0, h->unk7F0 + 4, kShadowSize,
                           kShadowHeight, gShadowTex + 0x18);
    h->unk7E8 = 1;
    h->unk7EC = 0;
    h->ai = NULL;
    memset(h->unk808, 0, sizeof(h->unk808));
    h->index = Game_CountHuman();
    h->unk7C4 = -1;
    h->deleteMe = 0;
    h->unkCF4 = 0;
    h->textCount = 0;
    for (i = 0; i < 32; i++) {
        h->textBuf[i][0] = 0;
        h->texts[i] = NULL;
    }
}
HOOK(0x45EB30, Human_CreateHandler);

void Human_Reset(Human *h)
{
    memset(h->prevInput, 0, sizeof(h->prevInput));
    memset(h->input, 0, sizeof(h->input));
    h->inputLocked = 0;
    h->dead = 0;
    h->crouch = 0;
    AmmoList_Init(h->ammo);
    h->unk4F0 = 1;
    h->unk4F4 = 0;
    memset(h->unk4F8, 0, sizeof(h->unk4F8));
    h->yaw = 0;
    h->unk510 = 0;
    memset(h->unk514, 0, sizeof(h->unk514));
    memset(h->delta, 0, sizeof(h->delta));
    h->unk52C = 0;
    h->unk530 = 1.0f;
    h->unk538 = -1;
    h->unk64C = 1;
    memset(h->vel, 0, sizeof(h->vel));
    memset(h->unk670, 0, sizeof(h->unk670));
    memset(h->unk67C, 0, sizeof(h->unk67C));
    memset(h->unk688, 0, sizeof(h->unk688));
    h->unk694 = 0;
    h->unk695 = 1;
    h->unk696 = 0;
    h->unk697 = 0;
    h->unk698 = 0;
    memset(h->unk6A0, 0, 0x70);
    sub_463580(h);
    h->groundDistance = 1073741824.0;
    h->groundAngle = 0;
    h->unk720 = 1073741824.0;
    h->ref768 = NULL;
    h->attach = NULL;
    h->ref7B0 = NULL;
    h->ref7B4 = NULL;
    h->unk7B8 = 0;
    h->unk7B9 = 0;
    h->unk7BC = 0;
    h->ref838 = NULL;
    h->ref83C = NULL;
    h->ref840 = NULL;
    h->unk844 = -1;
    h->unk848 = -1;
    h->footstep = 0;
    h->dying = 0;
    h->vulnerable = 1;
}
HOOK(0x45ECD0, Human_Reset);

/* runs the children, or deletes the human when asked to */
void Human_RunHandler(Human *h)
{
    Task *c;
    TaskHandler1 fn;
    int i;

    for (i = 0; i < 32; i++)
        h->textBuf[i][0] = 0;
    h->textCount = 0;
    if (h->deleteMe) {
        Task_Delete(h);
        return;
    }
    c = h->task.head;
    if (!c->next || !c)
        return;
    do {
        gTaskStack[gTaskStackTop++] = c->next && c->next->next ? c->next : NULL;
        fn = (TaskHandler1)TASK_HANDLER(1, c->type);
        if (fn)
            fn(c);
        c = gTaskStack[--gTaskStackTop];
    } while (c);
}
HOOK(0x45EEA0, Human_RunHandler);

void Human_RemoveRefs(Human *h);

void Human_DeleteHandler(Human *h)
{
    TaskHandler1 fn;
    int i;

    QTaskList_Remove(Game_GetTaskList3(), h);
    QTaskList_Remove(gHumanList, h);
    AIDetection_Remove(h);
    sub_484E00(h);
    Human_RemoveRefs(h);
    for (i = 0; i < 32; i++)
        if (h->texts[i])
            Text_Free(h->texts[i]);
    if (Task_GetId(h) >= 0) {
        fn = (TaskHandler1)TASK_HANDLER(sub_4F1220(), h->task.type);
        if (fn)
            fn(h);
    }
}
HOOK(0x45EF40, Human_DeleteHandler);

void Human_RemoveRefs(Human *h)
{
    if (h->ref768)
        TaskRef_Remove(h->ref768);
    if (h->ref7B0)
        TaskRef_Remove(h->ref7B0);
    if (h->ref7B4)
        TaskRef_Remove(h->ref7B4);
    if (h->ref838)
        TaskRef_Remove(h->ref838);
    if (h->ref83C)
        TaskRef_Remove(h->ref83C);
    if (h->ref840)
        TaskRef_Remove(h->ref840);
}
HOOK(0x45EFD0, Human_RemoveRefs);
