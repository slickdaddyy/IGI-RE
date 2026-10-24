/* Human: the task type shared by the player and the AI soldiers. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "soldier/soldier.h"

FN_EXTERN(Car_CollFilter);
FN_EXTERN(Human_ActivateHandler);
FN_EXTERN(Human_BlastHandler);
FN_EXTERN(Human_CollLineHandler);
FN_EXTERN(Human_CollLinesHandler);
FN_EXTERN(Human_CountCriteria);
FN_EXTERN(Human_CountTeamCriteria);
FN_EXTERN(Human_CrushHandler);
FN_EXTERN(Human_DeactivateHandler);
FN_EXTERN(Human_DrawHandler);
FN_EXTERN(Human_GetAmmoHandler);
FN_EXTERN(Human_GetHumanCameraInfoHandler);
FN_EXTERN(Human_GetParamTableHandler);
FN_EXTERN(Human_GetSoundInfoHandler);
FN_EXTERN(Human_GetWeaponBoneHandler);
FN_EXTERN(Human_GetWeaponHandler);
FN_EXTERN(Human_GetWeaponsHandler);
FN_EXTERN(Human_GunshotHandler);
FN_EXTERN(Human_HitHandler);
FN_EXTERN(Human_ParamChangedHandler);
FN_EXTERN(Human_PreDeleteHandler);
FN_EXTERN(Human_RegisterVarsHandler);
FN_EXTERN(Human_RenderHandler);
FN_EXTERN(Human_UnregisterVarsHandler);
FN_EXTERN(Task_FalseHandler);
FN_EXTERN(Task_Nothing);
FN_EXTERN(Task_ReturnZero);
FN_EXTERN(WeaponAmmoPickup);
FN_EXTERN(WeaponGunPickup);
FN_EXTERN(Game_CollFilterSkip);
FN_EXTERN(Human_NearClipHandler);
FN_EXTERN(Human_AIEventHandler);
FN_EXTERN(Human_DetectInfoHandler);
FN_EXTERN(Human_NearestAllyHandler);
FN_EXTERN(Human_FirstPersonHandler);
FN_EXTERN(Human_GetViewBarrelHandler);
FN_EXTERN(Human_GetViewPartHandler);
FN_EXTERN(Human_MapMarkerHandler);
FN_EXTERN(Human_MapSelectHandler);

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
ORIG(0x450C80, uint8_t, HumanAI_GetEventSlot, (void));
ORIG(0x450CC0, uint8_t, HumanAI_GetEvent3Slot, (void));
ORIG(0x463130, int, Human_GetTeam, (Human *h));
ORIG(0x463150, char, Human_IsPlayer, (Human *h));
ORIG(0x463580, void, Human_EnableCollision, (Human *h));
ORIG(0x464610, void *, HumanView_New, (Human *h));
ORIG(0x466230, uint8_t, DeathZone_GetEvent, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x46AB20, uint8_t, ComputerMap_GetSelectEvent, (void));
ORIG(0x46C970, uint8_t, GunPickup_GetEvent, (void));
ORIG(0x46D410, uint8_t, AmmoPickup_GetEvent, (void));
ORIG(0x477030, uint8_t, Grenade_GetEvent, (void));
ORIG(0x477C00, uint8_t, Gun_GetEvent0, (void));
ORIG(0x477C10, uint8_t, Gun_GetEvent1, (void));
ORIG(0x477C20, uint8_t, Gun_GetEvent2, (void));
ORIG(0x477C30, uint8_t, Gun_GetEvent3, (void));
ORIG(0x477C40, uint8_t, Gun_GetEvent4, (void));
ORIG(0x47CDC0, uint8_t, AmmoList_GetEvent, (void));
ORIG(0x47CDD0, void, AmmoList_Init, (void *list));
ORIG(0x484D30, uint8_t, HumanCamera_GetEvent, (void));
ORIG(0x484DC0, void, HumanCamera_AddHuman, (Human *h));
ORIG(0x484E00, void, HumanCamera_RemoveHuman, (Human *h));
ORIG(0x487190, uint8_t, StatusScreen_GetEvent, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x489620, uint8_t, AIDetection_GetEvent1, (void));
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
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4E0CC0, void *, Shadow_New, (Human *h, void *a, int *b, int *c, void *d, void *e, float size, float height,
                                   int tex));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1030, Human *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F1FC0, uint8_t, Weapon_GetSlotsEvent, (void));
ORIG(0x4F2050, void, WeaponSlots_Init, (void **slots));
ORIG(0x4FC840, uint8_t, PhysicsObj_GetEvent0, (void));
ORIG(0x4FDEF0, uint8_t, Particle2D_GetCollideEvent, (void));
ORIG(0x504A30, int, AreaActivate_AddCriteria, (const char *name, void *fn));
ORIG(0x5096A0, uint8_t, AIGraph_GetEvent, (void));

extern uint16_t gHumanType;
extern uint8_t gHumanGunshotEvent;
extern uint8_t gHumanPunchEvent;
extern void *gHumanList;
extern void *gHumanParams;
extern int gHumanCriteria;
extern int gHumanTeamCriteria[];
extern char gHumanCriteriaNames[][0x20];
extern const char *gHumanTeamNames[];
extern int gHumanUnk5BDC28[];
extern int gShadowTex;
extern int gBloodHole1;
extern int gBloodHole2;
extern int gBloodHole3;
extern int gBloodHole4;
extern int gBloodSprite1;
extern int gBloodSprite2;
extern int gHumanMaskType;
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern char gScriptNumberType[];

/* handlers of the Human type that live after this file */
#define Human_Handler45F280 (FN(0x45F280, Human_DrawHandler))
#define Human_Handler45F050 (FN(0x45F050, Human_NearClipHandler))
#define Human_Handler45F500 (FN(0x45F500, Human_CollLinesHandler))
#define Human_Handler45F800 (FN(0x45F800, Human_CollLineHandler))
#define Human_GetParams (FN(0x45F880, Human_GetParamTableHandler))
#define Human_ParamsChanged (FN(0x45F890, Human_ParamChangedHandler))
#define Human_Handler45F900 (FN(0x45F900, Human_GetWeaponsHandler))
#define Human_Handler45F910 (FN(0x45F910, Human_GetAmmoHandler))
#define Human_Handler45F930 (FN(0x45F930, Human_HitHandler))
#define Human_Handler45F920 (FN(0x45F920, Task_FalseHandler))
#define Human_Handler45F9D0 (FN(0x45F9D0, Human_BlastHandler))
#define Human_Handler45FA80 (FN(0x45FA80, Human_CrushHandler))
#define Human_Handler416AB0 (FN(0x416AB0, Game_CollFilterSkip))
#define Human_GetHumanCameraInfoHandler (FN(0x45FCD0, Human_GetHumanCameraInfoHandler))
#define Human_Handler45FDA0 (FN(0x45FDA0, Human_AIEventHandler))
#define Human_Handler45FE30 (FN(0x45FE30, Human_DetectInfoHandler))
#define Human_Handler42FC50 (FN(0x42FC50, Car_CollFilter))
#define Human_Handler45FE70 (FN(0x45FE70, Human_NearestAllyHandler))
#define WeaponAmmoPickup (FN(0x45FF80, WeaponAmmoPickup))
#define WeaponGunPickup (FN(0x45FFC0, WeaponGunPickup))
#define Human_Handler460070 (FN(0x460070, Human_GetWeaponHandler))
#define Human_Handler460090 (FN(0x460090, Human_FirstPersonHandler))
#define Human_gun_event_related_3 (FN(0x4600C0, Human_GetWeaponBoneHandler))
#define Human_gun_event_related_4 (FN(0x4601A0, Human_GetViewBarrelHandler))
#define Human_gun_event_related_5 (FN(0x460210, Human_GetViewPartHandler))
#define Human_Handler4D3710 (FN(0x4D3710, Task_Nothing))
#define Human_Handler460280 (FN(0x460280, Human_MapMarkerHandler))
#define Human_Handler460400 (FN(0x460400, Human_RenderHandler))
#define Human_Handler4606D0 (FN(0x4606D0, Human_GetSoundInfoHandler))
#define Human_Handler4606F0 (FN(0x4606F0, Human_GunshotHandler))
#define Human_Handler4F64E0 (FN(0x4F64E0, Task_ReturnZero))
#define Human_Handler460850 (FN(0x460850, Human_RegisterVarsHandler))
#define Human_Handler4608D0 (FN(0x4608D0, Human_UnregisterVarsHandler))
#define Human_Handler460950 (FN(0x460950, Human_ActivateHandler))
#define Human_Handler4609A0 (FN(0x4609A0, Human_DeactivateHandler))
#define Human_Handler4609F0 (FN(0x4609F0, Human_MapSelectHandler))
#define Human_Handler460AC0 (FN(0x460AC0, Human_PreDeleteHandler))
#define Human_Criteria (FN(0x460A70, Human_CountCriteria))
#define Human_CriteriaN (FN(0x460A10, Human_CountTeamCriteria))

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
void Human_CollFilterHandler(Human *h, TargetQuery *q)
{
    if (h != q->skip[0] && h != q->skip[1] && h != q->skip[2]) {
        if (Human_IsPlayer(h)) {
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
HOOK(0x45E3E0, Human_CollFilterHandler);

void Human_CreateHandler(Human *h);
void Human_RunHandler(Human *h);
void Human_DeleteHandler(Human *h);
ScriptValue *Human_GetCutSceneDeltaTime(ScriptValue *ret);

void Human_Open(void)
{
    Human tmp;
    int i;

    Task_SetGlobalFlags(1);
    gHumanType = TaskType_New("Human", sizeof(Human), BoneDynCubeObj_GetType(), 0);
    TaskType_SetFlags(gHumanType, 1);
    gHumanGunshotEvent = TaskEvent_Alloc(0);
    gHumanPunchEvent = TaskEvent_Alloc(0);
    gHumanList = QTaskList_New(0x80);
    TaskType_SetHandler(gHumanType, 0, Human_CreateHandler);
    TaskType_SetHandler2(gHumanType, 1, Human_RunHandler);
    TaskType_SetHandler2(gHumanType, 2, Human_DeleteHandler);
    TaskType_SetHandler(gHumanType, 4, Human_Handler45F280);
    TaskType_SetHandler(gHumanType, QCube_GetNearClipEvent(), Human_Handler45F050);
    TaskType_SetHandler(gHumanType, 9, Human_Handler45F500);
    TaskType_SetHandler(gHumanType, 8, Human_Handler45F800);
    TaskType_SetHandler(gHumanType, Param_GetTableEvent(), Human_GetParams);
    TaskType_SetHandler(gHumanType, Param_GetChangedEvent(), Human_ParamsChanged);
    TaskType_SetHandler(gHumanType, Weapon_GetSlotsEvent(), Human_Handler45F900);
    TaskType_SetHandler(gHumanType, AmmoList_GetEvent(), Human_Handler45F910);
    TaskType_SetHandler(gHumanType, Game_GetEvent7(), Human_CollFilterHandler);
    TaskType_SetHandler(gHumanType, Game_GetEvent9(), Human_Handler45F930);
    TaskType_SetHandler(gHumanType, Game_GetEvent8(), Human_Handler45F920);
    TaskType_SetHandler(gHumanType, Particle2D_GetCollideEvent(), Human_Handler45F920);
    TaskType_SetHandler(gHumanType, Game_GetEvent11(), Human_Handler45F9D0);
    TaskType_SetHandler(gHumanType, DeathZone_GetEvent(), Human_Handler45FA80);
    TaskType_SetHandler(gHumanType, Game_GetEvent13(), Human_Handler416AB0);
    TaskType_SetHandler(gHumanType, HumanCamera_GetEvent(), Human_GetHumanCameraInfoHandler);
    TaskType_SetHandler(gHumanType, HumanAI_GetEventSlot(), Human_Handler45FDA0);
    TaskType_SetHandler(gHumanType, AIDetection_GetEvent1(), Human_Handler45FE30);
    TaskType_SetHandler(gHumanType, AIDetection_GetEvent0(), Human_Handler42FC50);
    TaskType_SetHandler(gHumanType, HumanAI_GetEvent3Slot(), Human_Handler45FE70);
    TaskType_SetHandler(gHumanType, AmmoPickup_GetEvent(), WeaponAmmoPickup);
    TaskType_SetHandler(gHumanType, GunPickup_GetEvent(), WeaponGunPickup);
    TaskType_SetHandler(gHumanType, Gun_GetEvent0(), Human_Handler460070);
    TaskType_SetHandler(gHumanType, Gun_GetEvent1(), Human_Handler460090);
    TaskType_SetHandler(gHumanType, Gun_GetEvent2(), Human_gun_event_related_3);
    TaskType_SetHandler(gHumanType, Gun_GetEvent3(), Human_gun_event_related_4);
    TaskType_SetHandler(gHumanType, Gun_GetEvent4(), Human_gun_event_related_5);
    TaskType_SetHandler(gHumanType, PhysicsObj_GetEvent0(), Human_Handler4D3710);
    TaskType_SetHandler(gHumanType, ComputerObject_GetEvent0(), Human_Handler460280);
    TaskType_SetHandler(gHumanType, StatusScreen_GetEvent(), Human_Handler460400);
    TaskType_SetHandler(gHumanType, Sound_GetInfoEvent(), Human_Handler4606D0);
    TaskType_SetHandler(gHumanType, gHumanGunshotEvent, Human_Handler4606F0);
    TaskType_SetHandler(gHumanType, Grenade_GetEvent(), Human_Handler4F64E0);
    TaskType_SetHandler(gHumanType, 0xC, Human_Handler45F920);
    TaskType_SetHandler(gHumanType, AIGraph_GetEvent(), Human_ClearResult);
    TaskType_SetHandler2(gHumanType, Level_GetEventDA8(), Human_Handler460850);
    TaskType_SetHandler2(gHumanType, Level_GetEventDAC(), Human_Handler4608D0);
    TaskType_SetHandler(gHumanType, World_GetEvent0(), Human_Handler460950);
    TaskType_SetHandler(gHumanType, World_GetEvent1(), Human_Handler4609A0);
    TaskType_SetHandler(gHumanType, ComputerMap_GetSelectEvent(), Human_Handler4609F0);
    TaskType_SetHandler2(gHumanType, 3, Human_Handler460AC0);
    gHumanParams = ParamTable_New(9);
    ParamTable_Add(gHumanParams, &tmp, tmp.unkE0, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gHumanParams, &tmp, &tmp.unkF8, Param_Angle(), 7, NULL);
    ParamTable_Add(gHumanParams, &tmp, tmp.unk100, Param_String16(), 7, NULL);
    ParamTable_Add(gHumanParams, &tmp, &tmp.unkFC, Param_Int32(), 7, NULL);
    gHumanCriteria = AreaActivate_AddCriteria("CRITERIA_HUMAN", Human_Criteria);
    for (i = 0; i < 8; i++) {
        crt_sprintf(gHumanCriteriaNames[i], "CRITERIA_HUMAN%d", i);
        gHumanTeamCriteria[i] = AreaActivate_AddCriteria(gHumanCriteriaNames[i], Human_CriteriaN);
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
ScriptValue *Human_GetCutSceneDeltaTime(ScriptValue *ret)
{
    long double v = Task_FindById(0)->input[0] * kCutSceneScale;
    ScriptValue r;

    r.i = x87_fistp(v);
    r.d = v;
    r.type = gScriptNumberType;
    memcpy(ret, &r, sizeof(r));
    return ret;
}
HOOK(0x45EAE0, Human_GetCutSceneDeltaTime);

void Human_Reset(Human *h);

void Human_CreateHandler(Human *h)
{
    int i;

    QTaskList_Add(gHumanList, h);
    AIDetection_Add(h);
    QTaskList_Add(Game_GetTaskList3(), h);
    HumanCamera_AddHuman(h);
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
    Human_EnableCollision(h);
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
    HumanCamera_RemoveHuman(h);
    Human_RemoveRefs(h);
    for (i = 0; i < 32; i++)
        if (h->texts[i])
            Text_Free(h->texts[i]);
    if (Task_GetId(h) >= 0) {
        fn = (TaskHandler1)TASK_HANDLER(Level_GetEventDAC(), h->task.type);
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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(str_53F350);
REF_EXTERN(str_53F360);
STORE(igd, 53F348, 4) __typeof__(gHumanTeamNames[0]) store_53F348[2] INIT({(__typeof__(store_53F348[0]))ref_str_53F360, (__typeof__(store_53F348[1]))ref_str_53F350});
ALIAS(gHumanTeamNames, store_53F348, 0);
DATA(53F348) char str_53F350[15] INIT("HUMAN_TEAM_BAD");
PAD(igd, 53F348, 53F35F, 0x1);
DATA(53F348) char str_53F360[16] INIT("HUMAN_TEAM_GOOD");
BSS(5BDAF8) uint16_t gHumanType;
PAD(igb, 5BDAF8, 5BDAFA, 0x2);
BSS(5BDB00) int gHumanCriteria;
BSS(5BDB0C) int gShadowTex;
PAD(igb, 5BDB18, 5BDB18, 0x100);
ALIAS(gHumanCriteriaNames, pad_5BDB18, 0);
BSS(5BDB18) void *gHumanParams;
BSS(5BDC24) void *gHumanList;
PAD(igb, 5BDC24, 5BDC28, 0x14);
ALIAS(gHumanUnk5BDC28, pad_5BDC28, 0);
