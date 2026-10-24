/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "game/game.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A5234, int, crt_fclose, (void *f));
ORIG(0x4A528A, int, crt_fgetc, (void *f));
ORIG(0x4A52A4, int, crt_fseek, (void *f, long offset, int whence));
ORIG(0x4A5350, void *, crt_fopen, (const char *name, const char *mode));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5490, int, crt_atoi, (const char *s));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401780, void, Task_DeleteChildren, (Task *task));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (Task *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401F40, void, Task_ClearPair, (int *a, int *b, ...));
ORIG(0x4027F0, int, Flow_GetType, (int unk));
ORIG(0x402820, void, FramesSet, (int frames));
ORIG(0x402890, Task *, Flow_GetUnk20, (void));
ORIG(0x4028A0, void, Flow_SetRequest, (int request));
ORIG(0x4028D0, void, Flow_PauseBegin, (void));
ORIG(0x4028F0, void, Flow_PauseEnd, (void));
ORIG(0x405BD0, void, Config_Write, (const char *path));
ORIG(0x4061C0, void *, Profile_GetKeys, (int mode, float a0, float a1, float a2, float b0, float b1, float b2,
                                         float c0, float c1, float c2, float d0, float d1, float d2));
ORIG(0x4061E0, int, Profile_GetKeyCount, (void *keys));
ORIG(0x406220, char *, Profile_GetActive, (void));
ORIG(0x4062C0, float, Profile_GetSfxVolume, (void));
ORIG(0x4062D0, float, Profile_GetMusicVolume, (void));
ORIG(0x406340, void, Profile_AdvanceMission, (int mission));
ORIG(0x406390, void, Profile_ApplyPerformance, (void));
ORIG(0x408390, void, GameMaterial_Attach, (void *world));
ORIG(0x413B90, int *, WeaponType_Get, (int i));
ORIG(0x413BB0, int, WeaponType_GetCount, (void));
ORIG(0x437500, void, EnvironmentSound_SetListener, (void *p));
ORIG(0x453270, void, HumanMonitor_Reset, (void));
ORIG(0x4635B0, void, Human_LockInput, (Task *player));
ORIG(0x4635E0, void, Human_UnlockInput, (Task *player));
ORIG(0x468DD0, void, ComputerLog_Clear, (void));
ORIG(0x46B070, void, ComputerRes_Load, (void));
ORIG(0x46B310, void, ComputerRes_Free, (void));
ORIG(0x46C970, uint8_t, GunPickup_GetEvent, (void));
ORIG(0x47CDA0, int, Script_EvalConst, (const char *expr));
ORIG(0x47CE00, void, AmmoList_Add, (void *p, int ammo, int count));
ORIG(0x484D40, void *, HumanCamera_New, (Task *task, void *p));
ORIG(0x484D60, void, HumanCamera_SetHuman, (void *p, Task *task));
ORIG(0x485170, char *, Mission_Find, (int id));
ORIG(0x485300, Task *, GameScreen_New, (Task *parent));
ORIG(0x485320, void *, GameScreen_GetView, (Task *task));
ORIG(0x4868A0, void, StatusMessages_Load, (void));
ORIG(0x4868C0, void, StatusMessages_Free, (void));
ORIG(0x4868D0, void, StatusMessageScreen_Clear, (int unk));
ORIG(0x487180, char *, StatusScreen_Get, (void));
ORIG(0x489450, char, LevelFlow_IsComplete, (void));
ORIG(0x48A440, void *, LoadingScreen_New, (int steps));
ORIG(0x48A5D0, void, LoadingScreen_Delete, (void *ls));
ORIG(0x48A620, void, LoadingScreen_Step, (void *ls, int steps));
ORIG(0x48F210, void, AppContext_SetPlayIntro, (char on));
ORIG(0x48F250, int, AppContext_GetTerrainLightmapsUsed, (void));
ORIG(0x48F360, void, AppMain_ParseCmdLineArgs, (const char *name, void (*fn)(const char *rest)));
ORIG(0x491A90, void, Display_SetMode, (void *mode));
ORIG(0x491E70, void, Display_SetClearColor, (int r, int g, int b));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x495E70, void, Music_SetVolume, (float volume, float volume2));
ORIG(0x495F30, void, Music_SetSfxVolume, (float volume));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497450, int, Sound_HashBucket, (const char *name));
ORIG(0x4974A0, void, SoundParams_Init, (void *p));
ORIG(0x4A28B0, void, BandFilter_SetFont, (const char *font, float a, float b, float c, int d));
ORIG(0x4A29C0, void, HandiCamFilter_SetSprite, (const char *sprite, int a, int b));
ORIG(0x4A29E0, void, HandiCamFilter_SetFont, (const char *font, int a, int b));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B12F0, void, Alias_Set, (const char *name, const char *path));
ORIG(0x4B13A0, void, Alias_Remove, (const char *name));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B24C0, void, Texture_LoadFile, (void *tex, void *info, const char *name));
ORIG(0x4B25B0, void, Texture_BeginList, (float scale, int size));
ORIG(0x4B25F0, void, Texture_EndList, (void));
ORIG(0x4B2610, void, Texture_SetActiveTMU, (int tmu));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B4720, void, Random_Init, (Random *r));
ORIG(0x4B47E0, void, Random_Seed, (Random *r, unsigned int seed));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int *size));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4B66A0, void, Sprite_Free, (void *s));
ORIG(0x4B6720, int, Sprite_Load, (void *s, const char *name));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8870, void, Script_RegisterRealConstant, (const char *name, const double *value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C1780, void, Debug_Log, (void *world));
ORIG(0x4C1800, void *, QTaskList_New, (int max));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C5B20, void *, World_New, (int size));
ORIG(0x4C5CE0, void, World_Free, (void *world));
ORIG(0x4C5F00, void, World_LoadTextures, (void *world, const char *path));
ORIG(0x4C6240, void, Terrain_Load, (void *world, const char *path));
ORIG(0x4CE830, void, DynCube_BuildMasks, (void *world, int i));
ORIG(0x4CEA60, int, Model_GetInitMethod, (void));
ORIG(0x4CEA70, int, Model_GetDeinitMethod, (void));
ORIG(0x4CEEA0, void, VirModel_RestoreCount, (void));
ORIG(0x4CEFA0, void *, ModelFile_Load, (const char *name));
ORIG(0x4CF9D0, void, ModelFile_Free, (void *f));
ORIG(0x4CFBD0, void, Model_SetDir, (const char *path));
ORIG(0x4CFC00, void, Model_SetAnimDir, (const char *path));
ORIG(0x4D2D30, void, BoneObj_SetFrameRate, (float unk));
ORIG(0x4D65D0, void, BoneObj_FreeAll, (void));
ORIG(0x4D9950, void, Camera_Init, (void *p));
ORIG(0x4E6060, void, sub_4E6060, (int unk));
ORIG(0x4E6550, void, SoundSys_LoadDefs, (Game *game, const char *path));
ORIG(0x4E68D0, void, SoundLoad, (const char *path));
ORIG(0x4E69C0, void, SoundFree, (const char *path));
ORIG(0x4E6D60, void, SoundHandle_Play, (int *music));
ORIG(0x4E7130, void, Sound_StopAll, (void));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4E75B0, int, Screen_GetType, (int event));
ORIG(0x4E7BD0, void, Debug_printf, (const char *fmt, ...));
ORIG(0x4E7D40, Task *, CameraControl_New, (Game *game, void *p, void *world));
ORIG(0x4E7D70, void, CameraControl_AddListener, (Game *game, Task *task, void *world));
ORIG(0x4EDCC0, void, Lod_SendChanged, (void));
ORIG(0x4ED5E0, void, InputPort_Setup, (int port, int count, void *bindings, int analog, float a0, float a1, float a2,
                                        float b0, float b1, float b2, float c0, float c1, float c2, float d0,
                                        float d1, float d2));
ORIG(0x4F0CB0, void *, Level_New, (Game *game));
ORIG(0x4F0CD0, void, Level_FreeModels, (void));
ORIG(0x4F0CE0, void, Level_SetModels, (void *models));
ORIG(0x4F0E10, void, LevelLoad, (const char *path, int frames));
ORIG(0x4F0E90, void, Level_Clear, (void));
ORIG(0x4F0ED0, Task *, Level_GetStatic, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F4870, void, Vehicle_SetTaskList, (void *list));
ORIG(0x4F65B0, void, CutScene_SetCamera, (void *p));
ORIG(0x4F65C0, void, CutScene_SetFilterParent, (void *p));
ORIG(0x4F65D0, void, CutScene_SetTicksPerSecond, (int frames));
ORIG(0x4F80E0, void, RainDynCubeObj_SetCallback, (void (*fn)(const char *weather)));
ORIG(0x4F8FD0, uint16_t, TerrainLightMap_GetType, (void));

/* a texture or sprite the mission asked for by name, loaded from common/ */
typedef struct GameTexture {
    char name[0x18];
    char tex[0x24];
} GameTexture;

typedef struct GameSprite {
    char name[0x18];
    char sprite[0x18];
} GameSprite;

typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
    int unkC;
    int depth;
    char unk14;
    char unk15;
    char unk16;
    char unk17;
    int renderMode;
    char device[0x80];
    char desc[0x80];
} DisplayMode;

/* only the fields read here */
typedef struct QTaskList {
    int count;
    int max;
    Task *items[];
} QTaskList;

typedef struct WeaponGive {
    int id;
    int ammo;
} WeaponGive;

/* query for UseQuery_Consider: keeps the item closest to straight ahead */
typedef struct ViewQuery {
    char unk0[8];
    Vec3d origin;
    float m[9];
    char unk44[0x2C];
    float bestPitch;
    float bestYaw;
    double range;
    void *best;
} ViewQuery;

typedef struct Falloff {
    int unk0;
    Vec3d *pos;
    float max;
    float offset;
    float range;
} Falloff;

extern int gMission;
extern const double gMathPi[];
extern const double gWorldMeter[];
extern uint8_t gGameEvents[];
extern signed char gRaining;
extern uint16_t gGameType;
extern GameSprite gGameSprites[];
extern char gMissionPath[];
extern GameTexture gGameTextures[];
extern void *gGameTaskLists[];
extern QTaskList *gCutsceneList;
extern int gGameTextureCount;
extern int gGameSpriteCount;
extern char gMusicEnabled;
extern void *gCurrentWorld;
extern void (*gRenderFuncs[])(void *);
extern int gUnkBC20A0;
extern char gUnkC28C61;
extern char gUnkC28F80;

#define KEY_R 0x13
#define KEY_I 0x17
#define KEY_P 0x19
#define KEY_F12 0x58

void LevelStart(Game *g);
void Game_RunHandler(Game *g);
void LevelQuit(void);
void Game_LoadTextures(void);
void Game_LoadSprites(void);
void Game_FreeTextures(void);
void Game_FreeSprites(void);
void Game_SetDisplayMode(void);
void Game_SetRaining(const char *weather);
void Game_UpdateCheats(void);
void Game_DeleteTasksOfType(Task *parent);
ScriptValue *Game_SetMission(ScriptValue *out, void *data, void *args);
ScriptValue *Game_UpdateVolume(ScriptValue *out);
ScriptValue *Game_DisablePlayerInput(ScriptValue *out);
ScriptValue *Game_EnablePlayerInput(ScriptValue *out);
ScriptValue *Game_SetMusicVolume(ScriptValue *out, void *data, void *args);
ScriptValue *Game_SetSFXVolume(ScriptValue *out, void *data, void *args);
ScriptValue *Game_EnableMusic(ScriptValue *out);
ScriptValue *Game_DisableMusic(ScriptValue *out);
ScriptValue *Game_CutsceneDelete(ScriptValue *out);
void Game_ParseLevelArg(const char *rest);

void Game_Init(void)
{
    int i;

    AppMain_ParseCmdLineArgs("Level", Game_ParseLevelArg);
    gGameType = TaskType_New("Game", sizeof(Game), Task_GetRootType(), 0);
    for (i = 0; i < 15; i++)
        gGameEvents[i] = TaskEvent_Alloc(0);
    TaskType_SetHandler(gGameType, 0, LevelStart);
    TaskType_SetHandler2(gGameType, 1, Game_RunHandler);
    TaskType_SetHandler2(gGameType, 2, LevelQuit);
    gGameTaskLists[0] = QTaskList_New(0x200);
    gGameTaskLists[1] = QTaskList_New(0x40);
    gGameTaskLists[2] = QTaskList_New(0x100);
    gGameTaskLists[3] = QTaskList_New(0x100);
    gGameTaskLists[4] = QTaskList_New(0x10);
    gGameTaskLists[5] = QTaskList_New(0x80);
    Vehicle_SetTaskList(gGameTaskLists[4]);
    Script_RegisterConstant("GAME_FREQUENCY", 30);
    Script_RegisterRealConstant("WORLD_METER", gWorldMeter);
    Script_RegisterRealConstant("MATH_PI", gMathPi);
    Script_RegisterFunction("Game_SetMission", Game_SetMission, NULL, 1);
    Script_RegisterFunction("Game_UpdateVolume", Game_UpdateVolume, NULL, 0);
    Script_RegisterFunction("Game_DisablePlayerInput", Game_DisablePlayerInput, NULL, 0);
    Script_RegisterFunction("Game_EnablePlayerInput", Game_EnablePlayerInput, NULL, 0);
    Script_RegisterFunction("Game_SetMusicVolume", Game_SetMusicVolume, NULL, 1);
    Script_RegisterFunction("Game_SetSFXVolume", Game_SetSFXVolume, NULL, 1);
    Script_RegisterFunction("Game_EnableMusic", Game_EnableMusic, NULL, 0);
    Script_RegisterFunction("Game_DisableMusic", Game_DisableMusic, NULL, 0);
    Script_RegisterFunction("Game_CutsceneDelete", Game_CutsceneDelete, NULL, 1);
    HandiCamFilter_SetFont("LOCAL:computer/font1.fnt", 0x75, 0x14);
    HandiCamFilter_SetSprite("LOCAL:common/sprites/battery.spr", 4, 2);
    BandFilter_SetFont("LOCAL:computer/font1.fnt", 0.9f, 0.2f, -50.0f, 0);
}
HOOK(0x415530, Game_Init);

ScriptValue *Game_DisablePlayerInput(ScriptValue *out)
{
    Task *p;
    int i;

    for (i = 0; i < 4; i++) {
        p = Game_GetPlayer(i);
        if (p)
            Human_LockInput(p);
    }
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x4157D0, Game_DisablePlayerInput);

ScriptValue *Game_EnablePlayerInput(ScriptValue *out)
{
    Task *p;
    int i;

    for (i = 0; i < 4; i++) {
        p = Game_GetPlayer(i);
        if (p)
            Human_UnlockInput(p);
    }
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x415830, Game_EnablePlayerInput);

ScriptValue *Game_SetMission(ScriptValue *out, void *data, void *args)
{
    gMission = Script_GetArgInt(args, 0);
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x415890, Game_SetMission);

ScriptValue *Game_UpdateVolume(ScriptValue *out)
{
    float music = Profile_GetMusicVolume();

    Music_SetVolume(Profile_GetMusicVolume(), music);
    Music_SetSfxVolume(Profile_GetSfxVolume());
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x4158E0, Game_UpdateVolume);

ScriptValue *Game_SetMusicVolume(ScriptValue *out, void *data, void *args)
{
    float v = Script_GetArgReal(args, 0);

    Music_SetVolume(v, v);
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x415950, Game_SetMusicVolume);

ScriptValue *Game_SetSFXVolume(ScriptValue *out, void *data, void *args)
{
    float v = Script_GetArgReal(args, 0);

    Music_SetSfxVolume(v);
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x4159B0, Game_SetSFXVolume);

ScriptValue *Game_EnableMusic(ScriptValue *out)
{
    gMusicEnabled = 1;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x415A10, Game_EnableMusic);

ScriptValue *Game_DisableMusic(ScriptValue *out)
{
    gMusicEnabled = 0;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x415A60, Game_DisableMusic);

ScriptValue *Game_CutsceneDelete(ScriptValue *out)
{
    while (gCutsceneList->count)
        Task_Delete(gCutsceneList->items[0]);
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x415AB0, Game_CutsceneDelete);

/* -Level <id> on the command line starts that mission without the intro */
void Game_ParseLevelArg(const char *rest)
{
    gMission = crt_atoi(rest);
    AppContext_SetPlayIntro(0);
}
HOOK(0x415B10, Game_ParseLevelArg);

/* the Game task's create handler: loads the whole mission */
void LevelStart(Game *g)
{
    char terrain[0x80];
    char path[0x100];
    char path2[0x100];
    char *mission;
    void *loading, *world, *f;
    Task *t;
    int i, ok;

    FramesSet(30);
    Flow_PauseBegin();
    mission = Mission_Find(gMission);
    if (!mission) {
        Error_Show("The mission ID %d does not exist.", gMission);
        for (;;)
            ;
    }
    crt_sprintf(gMissionPath, "LOCAL:%s/", mission + 0x144);
    Alias_Set("MISSION:", gMissionPath);
    gGame = g;
    HumanMonitor_Reset();
    Camera_Init(gGame->unk60);
    Game_SetDisplayMode();
    Profile_ApplyPerformance();
    loading = LoadingScreen_New(9);
    gGame->world = World_New(0xDAC);
    gGame->unkE4 = 0;
    GameMaterial_Attach(gGame->world);
    gCurrentWorld = gGame->world;
    Debug_Log(gGame->world);
    crt_sprintf(terrain, "%sterrain/terrain", gMissionPath);
    World_LoadTextures(gGame->world, terrain);
    Terrain_Load(gGame->world, terrain);
    Texture_SetActiveTMU(0);
    Texture_BeginList(1.0f, 0xDB840);
    VirModel_RestoreCount();
    crt_sprintf(path, "LOCAL:%s/", mission + 0x244);
    Model_SetDir(path);
    Model_SetAnimDir("LOCAL:common/");
    gGame->models = ModelFile_Load(mission + 0x444);
    LoadingScreen_Step(loading, 1);
    crt_sprintf(path2, "LOCAL:%s/", mission + 0x344);
    Model_SetDir(path2);
    Level_SetModels(ModelFile_Load(mission + 0x464));
    LoadingScreen_Step(loading, 1);
    Texture_EndList();
    Texture_SetActiveTMU(-1);
    if (gRenderFuncs[Model_GetInitMethod()])
        gRenderFuncs[Model_GetInitMethod()](NULL);
    SoundLoad("LOCAL:common/sounds");
    SoundLoad("MISSION:sounds");
    LoadingScreen_Step(loading, 1);
    gGame->sprites = Resource_Load("LOCAL:common/sprites/sprites.res", NULL);
    Game_LoadSprites();
    LoadingScreen_Step(loading, 1);
    gGame->textures = Resource_Load("LOCAL:common/textures/textures.res", NULL);
    Game_LoadTextures();
    LoadingScreen_Step(loading, 1);
    ComputerRes_Load();
    LoadingScreen_Step(loading, 1);
    StatusMessages_Load();
    LoadingScreen_Step(loading, 1);
    gGame->unk44 = GameScreen_New(Flow_GetUnk20());
    gGame->unk4C = Level_New(Game_Get());
    LoadingScreen_Step(loading, 1);
    sub_4E6060(0);
    SoundSys_LoadDefs(g, "LOCAL:common/sounds/");
    SoundSys_LoadDefs(g, "MISSION:sounds/");
    world = Game_GetWorld();
    t = CameraControl_New(Game_Get(), gGame->unk60, world);
    gGame->ref54 = TaskRef_Add(t);
    world = Game_GetWorld();
    CameraControl_AddListener(Game_Get(), t, world);
    gGame->unk48 = NULL;
    BoneObj_SetFrameRate(30.0f);
    if (gUnkC28C61) {
        int mode = gUnkC28F80 ? 3 : 0;
        void *keys = Profile_GetKeys(mode, 0.1f, 0x1.2c5f92p-5f, 0x1.2c5f92p-5f, 0.1f, 0x1.2c5f92p-5f, 0x1.2c5f92p-5f,
                                     0.1f, 0x1.2c5f92p-5f, 0x1.2c5f92p-5f, 0.1f, 0x1.2c5f92p-5f, 0x1.2c5f92p-5f);

        /* the original reuses the pushes above as the arguments after keys */
        InputPort_Setup(0, Profile_GetKeyCount(keys), keys, mode, 0.1f, 0x1.2c5f92p-5f, 0x1.2c5f92p-5f, 0.1f,
                        0x1.2c5f92p-5f, 0x1.2c5f92p-5f, 0.1f, 0x1.2c5f92p-5f, 0x1.2c5f92p-5f, 0.1f, 0x1.2c5f92p-5f,
                        0x1.2c5f92p-5f);
        gUnkBC20A0 = 1;
    }
    RainDynCubeObj_SetCallback(Game_SetRaining);
    EnvironmentSound_SetListener(gGame->unk60);
    Lod_SendChanged();
    CutScene_SetCamera(gGame->unk60);
    CutScene_SetTicksPerSecond(30);
    CutScene_SetFilterParent(GameScreen_GetView(Game_Get()->unk44));
    LevelLoad(gMissionPath, 30);
    LoadingScreen_Step(loading, 1);
    for (i = 0; i < 6; i++)
        DynCube_BuildMasks(Game_GetWorld(), i);
    /* disc check: byte 0 of mpzm.afp must be '^' */
    f = crt_fopen(".\\mpzm.afp", "rb");
    ok = 0;
    if (f) {
        crt_fseek(f, 0, 0);
        ok = crt_fgetc(f) == 0x5E;
        crt_fclose(f);
    }
    if (!ok) {
        Error_Show("EXCEPTION FAULT: 0x000001");
        for (;;)
            ;
    }
    gGame->music[2] = 0;
    gGame->music[0] = Sound_HashBucket("game_music");
    gGame->music[1] = Sound_HashName("game_music");
    gGame->music[3] = 0x30;
    SoundParams_Init(gGame->unk100);
    Mem_ClearMode();
    LoadingScreen_Delete(loading);
    Flow_PauseEnd();
}
HOOK(0x415B30, LevelStart);

void Game_SetDisplayMode(void)
{
    char *p = Profile_GetActive();
    DisplayMode m;

    memset(&m, 0, sizeof(m));
    m.renderMode = Direct3D_GetRenderMode();
    m.width = *(int *)(p + 0xC);
    m.height = *(int *)(p + 0x10);
    m.unk14 = 1;
    m.unkC = 1;
    m.depth = *(int *)(p + 0x14);
    m.renderMode = Direct3D_GetRenderMode();
    m.unk16 = 1;
    strcpy(m.desc, p + 0x11F);
    strcpy(m.device, p + 0x1F);
    Display_SetMode(&m);
    Display_SetClearColor(0xAA, 0x84, 0x84);
}
HOOK(0x416010, Game_SetDisplayMode);

void Game_SetRaining(const char *weather)
{
    gRaining = weather[1];
    if (gRaining)
        Debug_printf("Raining:\n");
}
HOOK(0x416110, Game_SetRaining);

void Game_LoadTextures(void)
{
    int i;

    for (i = 0; i < gGameTextureCount; i++) {
        char path[0x100] = "LOCAL:common/textures/";

        strcat(path, gGameTextures[i].name);
        Texture_LoadFile(gGameTextures[i].tex, NULL, path);
    }
}
HOOK(0x416130, Game_LoadTextures);

void Game_LoadSprites(void)
{
    int i;

    for (i = 0; i < gGameSpriteCount; i++) {
        char path[0x100] = "LOCAL:common/sprites/";

        strcat(path, gGameSprites[i].name);
        Sprite_Load(gGameSprites[i].sprite, path);
    }
}
HOOK(0x4161D0, Game_LoadSprites);

/* runs every children's slot 1 handler, the next node is kept on the task
   stack so a handler may delete the following task */
static void RunChildren(Task *task)
{
    GameNode *n = task->head;
    TaskFn1 fn;

    if (!n->next)
        return;
    do {
        gTaskStack[gTaskStackTop++] = n->next && n->next->next ? n->next : NULL;
        fn = gTaskHandlers1[1][((Task *)n)->type];
        if (fn)
            fn(n);
        n = gTaskStack[--gTaskStackTop];
    } while (n);
}

void Game_RunHandler(Game *g)
{
    void *loading, *f;
    int ok;

    Game_UpdateCheats();
    if (LevelFlow_IsComplete() || Game_CheatKeyPressed(KEY_F12)) {
        Mem_PushMode(1);
        loading = LoadingScreen_New(1);
        Mem_PopMode();
        gMission = *(int *)(Mission_Find(gMission) + 0x4A4);
        /* disc check: byte 0 of ymbe.afp must be '@' */
        f = crt_fopen(".\\ymbe.afp", "rb");
        ok = 0;
        if (f) {
            crt_fseek(f, 0, 0);
            ok = crt_fgetc(f) == 0x40;
            crt_fclose(f);
        }
        if (!ok) {
            Error_Show("EXCEPTION FAULT: 0x000000");
            for (;;)
                ;
        }
        if (gMission == -1) {
            Flow_SetRequest(7);
        } else {
            Profile_AdvanceMission(gMission);
            Config_Write("LOCAL:config.qsc");
            Flow_SetRequest(4);
        }
        Task_Delete(g);
        LoadingScreen_Delete(loading);
        return;
    }
    if (!g->paused)
        RunChildren(&g->task);
    if (gMusicEnabled)
        SoundHandle_Play(g->music);
    else
        g->music[2] = 0;
}
HOOK(0x416260, Game_RunHandler);

/* ctrl+shift cheats: P gives all weapons, R restarts, I toggles god mode */
void Game_UpdateCheats(void)
{
    int total, redraw, unused;
    WeaponGive give;
    TaskFn fn;
    uint16_t type;
    int *w;
    int i, ammo;

    Task_ClearPair(&total, &unused, Flow_GetType(1));
    Task_ClearPair(&redraw, &unused, Screen_GetType(Screen_GetDrawEvent()));
    if (total)
        Debug_printf("Time spent redrawing: %.2f%%\n", (double)((long double)redraw / total * 100.0f));
    if (Game_CheatKeyDown(KEY_P) && Game_GetPlayer(0)) {
        for (i = 0; i < WeaponType_GetCount(); i++) {
            w = WeaponType_Get(i);
            give.id = w[0];
            give.ammo = w[0x648 / 4] > 0 ? w[0x648 / 4] : 0;
            type = Game_GetPlayer(0)->type;
            fn = gTaskHandlers[GunPickup_GetEvent()][type];
            if (fn)
                fn(Game_GetPlayer(0), &give);
        }
        ammo = Script_EvalConst("AMMO_ID_M203");
        AmmoList_Add((char *)Game_GetPlayer(0) + 0x340, ammo, 10);
    }
    if (Game_CheatKeyPressed(KEY_R))
        Flow_SetRequest(5);
    if (Game_CheatKeyPressed(KEY_I))
        gGame->godMode ^= 1;
}
HOOK(0x4163E0, Game_UpdateCheats);

/* the Game task's delete handler */
void LevelQuit(void)
{
    if (!Mem_GetMode())
        Mem_SetDefaultMode();
    Sound_StopAll();
    Level_Clear();
    TaskRef_Remove(gGame->ref54);
    if (gGame->unk48)
        Task_Delete(gGame->unk48);
    Task_Delete(gGame->unk44);
    Game_FreeSprites();
    Resource_Flush(gGame->sprites);
    Game_FreeTextures();
    Resource_Flush(gGame->textures);
    ComputerRes_Free();
    StatusMessages_Free();
    if (gRenderFuncs[Model_GetDeinitMethod()])
        gRenderFuncs[Model_GetDeinitMethod()](NULL);
    Level_FreeModels();
    SoundFree("MISSION:sounds");
    SoundFree("LOCAL:common/sounds");
    BoneObj_FreeAll();
    ModelFile_Free(gGame->models);
    World_Free(gGame->world);
    Debug_Log(NULL);
    Alias_Remove("MISSION:");
    gGame = NULL;
}
HOOK(0x416550, LevelQuit);

void Game_FreeTextures(void)
{
    int i;

    for (i = 0; i < gGameTextureCount; i++)
        Texture_Free(gGameTextures[i].tex);
}
HOOK(0x416660, Game_FreeTextures);

void Game_FreeSprites(void)
{
    int i;

    for (i = 0; i < gGameSpriteCount; i++)
        Sprite_Free(gGameSprites[i].sprite);
}
HOOK(0x416690, Game_FreeSprites);

void Game_Close(void)
{
    static const uint8_t order[15] = {13, 7, 8, 9, 10, 11, 12, 6, 5, 4, 3, 2, 1, 0, 14};
    int i;

    for (i = 0; i < 15; i++)
        TaskEvent_Free(gGameEvents[order[i]]);
    TaskType_Free(gGameType);
    for (i = 0; i < 6; i++)
        QTaskList_Free(gGameTaskLists[i]);
}
HOOK(0x4166C0, Game_Close);

uint8_t Game_GetEvent0(void)
{
    return gGameEvents[0];
}
HOOK(0x4167D0, Game_GetEvent0);

uint8_t Game_GetEvent1(void)
{
    return gGameEvents[1];
}
HOOK(0x4167E0, Game_GetEvent1);

uint8_t Game_GetEvent2(void)
{
    return gGameEvents[2];
}
HOOK(0x4167F0, Game_GetEvent2);

uint8_t Game_GetEvent4(void)
{
    return gGameEvents[4];
}
HOOK(0x416800, Game_GetEvent4);

uint8_t Game_GetEvent3(void)
{
    return gGameEvents[3];
}
HOOK(0x416810, Game_GetEvent3);

uint8_t Game_GetEvent6(void)
{
    return gGameEvents[6];
}
HOOK(0x416820, Game_GetEvent6);

uint8_t Game_GetEvent7(void)
{
    return gGameEvents[7];
}
HOOK(0x416830, Game_GetEvent7);

uint8_t Game_GetEvent9(void)
{
    return gGameEvents[9];
}
HOOK(0x416840, Game_GetEvent9);

uint8_t Game_GetEvent8(void)
{
    return gGameEvents[8];
}
HOOK(0x416850, Game_GetEvent8);

uint8_t Game_GetEvent11(void)
{
    return gGameEvents[11];
}
HOOK(0x416860, Game_GetEvent11);

uint8_t Game_GetEvent12(void)
{
    return gGameEvents[12];
}
HOOK(0x416870, Game_GetEvent12);

uint8_t Game_GetEvent13(void)
{
    return gGameEvents[13];
}
HOOK(0x416880, Game_GetEvent13);

uint8_t Game_GetEvent14(void)
{
    return gGameEvents[14];
}
HOOK(0x416890, Game_GetEvent14);

void *Game_GetTaskList0(void)
{
    return gGameTaskLists[0];
}
HOOK(0x4168A0, Game_GetTaskList0);

void *Game_GetTaskList1(void)
{
    return gGameTaskLists[1];
}
HOOK(0x4168B0, Game_GetTaskList1);

void *Game_GetTaskList2(void)
{
    return gGameTaskLists[2];
}
HOOK(0x4168C0, Game_GetTaskList2);

void *Game_GetTaskList3(void)
{
    return gGameTaskLists[3];
}
HOOK(0x4168D0, Game_GetTaskList3);

void *Game_GetTaskList4(void)
{
    return gGameTaskLists[4];
}
HOOK(0x4168E0, Game_GetTaskList4);

void *Game_GetCutsceneList(void)
{
    return gGameTaskLists[5];
}
HOOK(0x4168F0, Game_GetCutsceneList);

Game *Game_New(Task *parent)
{
    Task_New(parent, gGameType, NULL);
    return gGame;
}
HOOK(0x416900, Game_New);

void *Game_GetWorld(void)
{
    return gGame->world;
}
HOOK(0x416920, Game_GetWorld);

GameTexture *Game_AddTexture(const char *name)
{
    int count = gGameTextureCount;
    int i;

    for (i = 0; i < count; i++) {
        if (!strcmp(gGameTextures[i].name, name))
            return &gGameTextures[i];
    }
    strcpy(gGameTextures[i].name, name);
    gGameTextureCount = count + 1;
    return &gGameTextures[i];
}
HOOK(0x416930, Game_AddTexture);

GameSprite *Game_AddSprite(const char *name)
{
    int count = gGameSpriteCount;
    int i;

    for (i = 0; i < count; i++) {
        if (!strcmp(gGameSprites[i].name, name))
            return &gGameSprites[i];
    }
    strcpy(gGameSprites[i].name, name);
    gGameSpriteCount = count + 1;
    return &gGameSprites[i];
}
HOOK(0x4169D0, Game_AddSprite);

/* not inlined, so a NULL player crashes on the read like the original */
__attribute__((noinline)) Task *Game_GetPlayer(int i)
{
    Task **ref = gGame->players[i];

    return ref ? *ref : NULL;
}
HOOK(0x416A70, Game_GetPlayer);

/* search filters: true when item is none of the excluded ones */
void Game_CollFilterSkip3(void *item, char *ctx)
{
    void **ex = (void **)(ctx + 4);

    *ctx = item != ex[0] && item != ex[1] && item != ex[2];
}
HOOK(0x416A90, Game_CollFilterSkip3);

void Game_CollFilterSkip(void *item, char *ctx)
{
    *ctx = item != *(void **)(ctx + 4);
}
HOOK(0x416AB0, Game_CollFilterSkip);

/* fchs on the x87 value, so gcc cannot move the negation after a rounding */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

char UseQuery_Consider(ViewQuery *q, void *item, const Vec3d *pos)
{
    Vec3d d, l;
    long double y, yaw;
    float pitch;

    d.x = pos->x - q->origin.x;
    d.y = pos->y - q->origin.y;
    d.z = pos->z - q->origin.z;
    if (Vec3d_Length(&d) >= q->range)
        return 0;
    l.x = (long double)q->m[6] * d.z + (long double)q->m[3] * d.y + (long double)q->m[0] * d.x;
    y = (long double)q->m[7] * d.z + (long double)q->m[4] * d.y + (long double)q->m[1] * d.x;
    l.y = y;
    l.z = (long double)q->m[8] * d.z + (long double)q->m[5] * d.y + (long double)q->m[2] * d.x;
    if (!(y > 0.0))
        return 0;
    if (!(Vec3d_PitchAngle(l) >= 0.0f))
        pitch = x87_neg(Vec3d_PitchAngle(l));
    else
        pitch = Vec3d_PitchAngle(l);
    if (!(Vec3d_YawAngle(l) >= 0.0f))
        yaw = -Vec3d_YawAngle(l);
    else
        yaw = Vec3d_YawAngle(l);
    if (pitch >= q->bestPitch || yaw >= q->bestYaw)
        return 0;
    q->bestYaw = yaw;
    q->bestPitch = pitch;
    q->best = item;
    return 1;
}
HOOK(0x416AD0, UseQuery_Consider);

/* full value inside the offset, fading to 0 over range */
long double Game_BlastFalloff(Falloff *s, void *unused, const Vec3d *pos, float radius)
{
    Vec3d d;
    long double v;
    float f;

    d.x = s->pos->x - pos->x;
    d.y = s->pos->y - pos->y;
    d.z = s->pos->z - pos->z;
    v = Vec3d_Length(&d) - radius - s->offset;
    f = v;
    if (!(v >= 0.0f))
        return s->max;
    if (!(f >= s->range))
        return ((long double)s->range - f) * s->max / s->range;
    return 0.0f;
}
HOOK(0x416C90, Game_BlastFalloff);

Random *Game_GetRandom(void)
{
    return &gGame->rand1;
}
HOOK(0x416D20, Game_GetRandom);

Random *Game_GetRandom2(void)
{
    return &gGame->rand2;
}
HOOK(0x416D30, Game_GetRandom2);

Game *Game_Get(void)
{
    return gGame;
}
HOOK(0x416D40, Game_Get);

char Game_IsPaused(void)
{
    return gGame->paused;
}
HOOK(0x416D50, Game_IsPaused);

void Game_SetPaused(char on)
{
    gGame->paused = on;
}
HOOK(0x416D60, Game_SetPaused);

/* the god mode cheat */
char PlayerXPHit(void)
{
    return gGame->godMode;
}
HOOK(0x416D80, PlayerXPHit);

void Game_ResetState(void)
{
    Task *t;
    int i;

    Random_Init(&gGame->rand1);
    Random_Init(&gGame->rand2);
    Random_Seed(&gGame->rand1, 0x19111911);
    Random_Seed(&gGame->rand2, 0x19111911);
    gGame->paused = 0;
    gGame->godMode = 0;
    for (i = 0; i < 4; i++)
        gGame->players[i] = NULL;
    for (i = 0; i < 4; i++)
        gGame->unkD0[i] = 0;
    t = Task_FindById(0);
    if (gGame->players[0]) {
        TaskRef_Remove(gGame->players[0]);
        gGame->players[0] = NULL;
    }
    if (t)
        gGame->players[0] = TaskRef_Add(t);
    gGame->unk58 = 0;
    gGame->unk5C = HumanCamera_New(*gGame->ref54, gGame->unk60);
    if (t)
        HumanCamera_SetHuman(gGame->unk5C, t);
    {
        float music = Profile_GetMusicVolume();

        Music_SetVolume(Profile_GetMusicVolume(), music);
    }
    Music_SetSfxVolume(Profile_GetSfxVolume());
    if (!AppContext_GetTerrainLightmapsUsed())
        Game_DeleteTasksOfType(Level_GetStatic());
}
HOOK(0x416D90, Game_ResetState);

void Game_DeleteTasksOfType(Task *parent)
{
    GameNode *n = parent->head, *next;

    if (!n->next)
        return;
    while (n) {
        next = n->next && n->next->next ? n->next : NULL;
        if (((Task *)n)->type == TerrainLightMap_GetType())
            Task_Delete(n);
        else
            Game_DeleteTasksOfType((Task *)n);
        n = next;
    }
}
HOOK(0x416EF0, Game_DeleteTasksOfType);

void Game_ClearPlayers(void)
{
    int i;

    if (StatusScreen_Get())
        StatusMessageScreen_Clear(*(int *)(StatusScreen_Get() + 0x4E4));
    if (*gGame->ref54)
        Task_DeleteChildren(*gGame->ref54);
    for (i = 0; i < 4; i++) {
        if (gGame->players[i]) {
            TaskRef_Remove(gGame->players[i]);
            gGame->players[i] = NULL;
        }
    }
}
HOOK(0x416F40, Game_ClearPlayers);

int Game_IsRaining(void)
{
    return gRaining;
}
HOOK(0x416FB0, Game_IsRaining);

int Game_CountHuman(void)
{
    return gGame->unkE4++;
}
HOOK(0x416FC0, Game_CountHuman);

void LevelRestart(void)
{
    void *loading;
    int i;

    Flow_PauseBegin();
    Mem_SetDefaultMode();
    loading = LoadingScreen_New(5);
    Sound_StopAll();
    HumanMonitor_Reset();
    LoadingScreen_Step(loading, 1);
    StatusMessageScreen_Clear(*(int *)(StatusScreen_Get() + 0x4E4));
    ComputerLog_Clear();
    LoadingScreen_Step(loading, 1);
    Level_Clear();
    LoadingScreen_Step(loading, 1);
    Lod_SendChanged();
    LevelLoad(gMissionPath, 30);
    LoadingScreen_Step(loading, 1);
    for (i = 0; i < 6; i++)
        DynCube_BuildMasks(Game_GetWorld(), i);
    Mem_ClearMode();
    LoadingScreen_Step(loading, 1);
    LoadingScreen_Delete(loading);
    Flow_PauseEnd();
}
HOOK(0x416FE0, LevelRestart);

int Mission_GetFirst(void)
{
    return 1;
}
HOOK(0x417090, Mission_GetFirst);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 539540, 1) __typeof__(gGameEvents[0]) store_539540[16] INIT({255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
ALIAS(gGameEvents, store_539540, 0);
STORE(igd, 539550, 8) __typeof__(gMathPi[0]) store_539550[1] INIT({3.1415926535897944});
ALIAS(gMathPi, store_539550, 0);
STORE(igd, 539550, 8) __typeof__(gWorldMeter[0]) store_539558[1] INIT({4096.0});
ALIAS(gWorldMeter, store_539558, 0);
DATA(539560) int gMission INIT(1);
BSS(57B1A0) signed char gRaining;
PAD(igb, 57B1A0, 57B1A1, 0x1);
BSS(57B1A0) uint16_t gGameType;
PAD(igb, 57B1A0, 57B1A4, 0x4);
PAD(igb, 57B1A0, 57B1A8, 0x3C0);
ALIAS(gGameSprites, pad_57B1A8, 0);
PAD(igb, 57B1A0, 57B568, 0x80);
ALIAS(gMissionPath, pad_57B568, 0);
PAD(igb, 57B1A0, 57B5E8, 0x4B4);
ALIAS(gGameTextures, pad_57B5E8, 0);
PAD(igb, 57B1A0, 57BA9C, 0x14);
ALIAS(gGameTaskLists, pad_57BA9C, 0);
BSS(57B1A0) QTaskList *gCutsceneList;
BSS(57B1A0) int gGameTextureCount;
BSS(57B1A0) int gGameSpriteCount;
BSS(57B1A0) Game *gGame;
BSS(57B1A0) char gMusicEnabled;
PAD(igb, 57B1A0, 57BAC1, 0x3);
