/* keep each module step a real call, as in the original */
#pragma GCC optimize("no-inline")

#include "freecam/freecam.h"

/* Start up and shut down of the engine modules, in the order the game
   uses. Debug_Log is an empty function here. */

ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4B0B90, void, sub_4B0B90, (void));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x4010D0, void, Task_InitSystem, (void));
ORIG(0x4011F0, void, Task_ShutdownSystem, (void));
ORIG(0x4B8B20, void, Script_Init, (void));
ORIG(0x4B8CC0, void, Script_Shutdown, (void));

ORIG(0x4EC160, void, AnimTriggerEvent_Init, (void));
ORIG(0x4EBE70, void, AnimTrigger_Init, (void));
ORIG(0x4ED000, void, InputPort_ResetUnused, (void));
ORIG(0x4EBFC0, void, AnimTrigger_Shutdown, (void));
ORIG(0x4EC180, void, AnimTriggerEvent_Shutdown, (void));

ORIG(0x4E6070, void, SoundSys_Init, (void));
ORIG(0x4EBC60, void, SoundSysMicrophone_Init, (void));
ORIG(0x4EB1D0, void, SoundDefGroup_Init, (void));
ORIG(0x4E8390, void, SoundDefSound_Init, (void));
ORIG(0x4EA3D0, void, SoundDefGraph_Init, (void));
ORIG(0x4E9520, void, SoundDefTriggerOnce_Init, (void));
ORIG(0x4E7370, void, Screen_Init, (void));
ORIG(0x4E7D90, void, Viewport_Init, (void));
ORIG(0x4E82C0, void, TerraDraw_Init, (void));
ORIG(0x4E7C50, void, CameraControl_Init, (void));
ORIG(0x4E7840, void, DebugText_Init, (void));
ORIG(0x4E92C0, void, FadeOut_Init, (void));
ORIG(0x4E9480, void, GlowTask_Init, (void));
ORIG(0x4A2A00, void, CameraFilter_Init, (void));
ORIG(0x4A4F60, void, CameraFilter_Close, (void));
ORIG(0x4E94E0, void, GlowTask_Shutdown, (void));
ORIG(0x4E9410, void, FadeOut_Shutdown, (void));
ORIG(0x4E7B40, void, DebugText_Shutdown, (void));
ORIG(0x4E7D30, void, CameraControl_Shutdown, (void));
ORIG(0x4E8350, void, TerraDraw_Shutdown, (void));
ORIG(0x4E80D0, void, Viewport_Shutdown, (void));
ORIG(0x4E7500, void, Screen_Shutdown, (void));
ORIG(0x4E9E80, void, SoundDefTriggerOnce_Shutdown, (void));
ORIG(0x4EACE0, void, SoundDefGraph_Shutdown, (void));
ORIG(0x4E8C20, void, SoundDefSound_Shutdown, (void));
ORIG(0x4EBA50, void, SoundDefGroup_Shutdown, (void));
ORIG(0x4EBDB0, void, SoundSysMicrophone_Shutdown, (void));
ORIG(0x4E63E0, void, SoundSys_Shutdown, (void));

ORIG(0x4CEEB0, void, Model_Init, (void));
ORIG(0x52D420, void, Sky_Init, (void));
ORIG(0x518260, void, LensFlare_Init, (void));
ORIG(0x4A1470, void, RigidDynCubeObj_OpenD3D, (void));
ORIG(0x4D9A00, void, MagicObj_Open, (void));
ORIG(0x5125A0, void, BoneDynCubeObj_OpenD3D, (void));
ORIG(0x49A520, void, PolyList_Init, (void));
ORIG(0x4E41B0, void, SplineObjSegment_Init, (void));
ORIG(0x4E0950, void, Shadow_Open, (void));
ORIG(0x4D0B30, void, Mesh3D_Init, (void));
ORIG(0x4DCF20, void, Sector_Open, (void));
ORIG(0x4E0840, void, MagicObjContainer_Open, (void));
ORIG(0x498AB0, void, Rain_Init, (void));
ORIG(0x52D360, void, SkyDome_Init, (void));
ORIG(0x49BB40, void, Direct3DRender_Init, (void));
ORIG(0x4A0110, void, Direct3DRender_Shutdown, (void));
ORIG(0x499CE0, void, Rain_Close, (void));
ORIG(0x4E0920, void, MagicObjContainer_Close, (void));
ORIG(0x4DEE40, void, Sector_Close, (void));
ORIG(0x4D0C50, void, Mesh3D_Shutdown, (void));
ORIG(0x4E0CB0, void, Shadow_Close, (void));
ORIG(0x4E43B0, void, SplineObjSegment_Shutdown, (void));
ORIG(0x49B260, void, PolyList_Close, (void));
ORIG(0x5127D0, void, BoneDynCubeObj_CloseD3D, (void));
ORIG(0x4D9F60, void, MagicObj_Close, (void));
ORIG(0x4A23D0, void, RigidDynCubeObj_CloseD3D, (void));
ORIG(0x518290, void, LensFlare_Shutdown, (void));
ORIG(0x4CEF40, void, Model_Shutdown, (void));

ORIG(0x4E5D90, void, QCube_InitTables, (void));
ORIG(0x4D0EF0, void, QCube_Init, (void));
ORIG(0x50FAC0, void, TerrainMesh_Init, (void));
ORIG(0x52DD70, void, WaterMesh_Open, (void));
ORIG(0x4DA1D0, void, HazeLayer_Open, (void));
ORIG(0x4DA2E0, void, HazeLayer_Close, (void));
ORIG(0x4D1260, void, QCube_Shutdown, (void));
ORIG(0x4E5EA0, void, QCube_FreeTables, (void));
ORIG(0x4C5900, void, World_Init, (void));
ORIG(0x4E55C0, void, CellMask_Init, (void));
ORIG(0x4E55F0, void, CellMask_Shutdown, (void));
ORIG(0x4C5AA0, void, World_Shutdown, (void));
ORIG(0x4D9730, void, QObj_Open, (void));
ORIG(0x4D9810, void, Camera_Open, (void));
ORIG(0x4E5370, void, DynCubeObj_Init, (void));
ORIG(0x4D2C30, void, BoneObj_Init, (void));
ORIG(0x4DA880, void, Light_Open, (void));
ORIG(0x4DA8A0, void, Light_Close, (void));
ORIG(0x4D2CF0, void, BoneObj_Shutdown, (void));
ORIG(0x4E53A0, void, DynCubeObj_Shutdown, (void));
ORIG(0x4D9830, void, Camera_Close, (void));
ORIG(0x4D9790, void, QObj_Close, (void));

ORIG(0x498950, void, App_ResetErrorHandlers, (void));
ORIG(0x4AF8B0, void, Error_SetUnusedFlag, (int v));
ORIG(0x48F020, void, AppContext_Open, (void));
ORIG(0x4B46A0, void, Math_InitTables, (void));
ORIG(0x4B0BA0, void, Mem_Init, (void));
ORIG(0x4B1200, void, File_Init, (void));
ORIG(0x4B0DC0, void, RenderMode_Init, (void));
ORIG(0x491700, void, Display_Open, (void));
ORIG(0x4B5E70, void, Resource_Init, (void));
ORIG(0x4B7C80, void, Palette_Register, (void));
ORIG(0x4B2180, void, Texture_Init, (void));
ORIG(0x495280, void, Sound_Init, (void));
ORIG(0x4B59A0, void, QVertex_Init, (void));
ORIG(0x48FFF0, void, Keyboard_Open, (void));
ORIG(0x48FDE0, void, Mouse_Open, (void));
ORIG(0x509980, void, Joystick_Init, (void));
ORIG(0x497550, void, Scene_InitList, (void));
ORIG(0x497980, void, TransContext_Init, (void));
ORIG(0x490360, void, Timer_Open, (void));
ORIG(0x4B73C0, void, SortRender_Init, (void));
ORIG(0x490380, void, Movie_Init, (void));
ORIG(0x4903A0, void, Movie_Shutdown, (void));
ORIG(0x4B73E0, void, SortRender_Shutdown, (void));
ORIG(0x497D40, void, TransContext_Shutdown, (void));
ORIG(0x509E90, void, Joystick_Shutdown, (void));
ORIG(0x48FFA0, void, Mouse_Close, (void));
ORIG(0x4902F0, void, Keyboard_Close, (void));
ORIG(0x4B59D0, void, QVertex_Shutdown, (void));
ORIG(0x495A90, void, Sound_Shutdown, (void));
ORIG(0x4B2250, void, Texture_Shutdown, (void));
ORIG(0x4B7CC0, void, Palette_Unregister, (void));
ORIG(0x4B5EC0, void, Resource_Shutdown, (void));
ORIG(0x491930, void, Display_Close, (void));
ORIG(0x4B1260, void, File_Shutdown, (void));
ORIG(0x4B0BE0, void, Mem_Shutdown, (void));
ORIG(0x48F070, void, AppContext_Close, (void));

/* rewritten in this folder */
void Lzss_Register(void);
void FreeCamera_Init(void);
void FreeCamera_Shutdown(void);
void ParticleSystem_Init(void);
void ParticleSystem_Shutdown(void);
void SpriteParticleSystem_Init(void);
void SpriteParticleSystem_Shutdown(void);
void FlashTask_Init(void);
void FlashTask_Shutdown(void);
void LightObj_Init(void);
void LightObj_Shutdown(void);
void Sprite3D_Init(void);
void Sprite3D_Shutdown(void);
void BoneMagicObj_Init(void);
void BoneMagicObj_Shutdown(void);
void TransparencyList_Init(void);
void TransparencyList_Shutdown(void);
void WaterMesh_Close(void);

extern uint8_t gEngineUnusedEvent0;
extern uint8_t gEngineUnusedEvent1;

void ViewModules_Init(void);
void ViewModules_Shutdown(void);
void EngineEvents_Init(void);
void EngineEvents_Shutdown(void);
void EngineModules_Init(void);
void EngineModules_Shutdown(void);
void RenderModules_Init(void);
void RenderModules_Shutdown(void);
void TerrainModules_Init(void);
void TerrainModules_Shutdown(void);
void WorldModules_Init(void);
void WorldModules_Shutdown(void);
void ObjModules_Init(void);
void ObjModules_Shutdown(void);
void CoreTask_Init(void);
void CoreTask_Shutdown(void);
void CoreScript_Init(void);
void CoreScript_Shutdown(void);
void Core_Init(void);
void Core_Shutdown(void);

void Engine_Init(void)
{
    ViewModules_Init();
    EngineEvents_Init();
    AnimTriggerEvent_Init();
    AnimTrigger_Init();
    Debug_Log();
    Lzss_Register();
    InputPort_ResetUnused();
}
HOOK(0x530270, Engine_Init);

void Engine_Shutdown(void)
{
    Debug_Log();
    Debug_Log();
    Debug_Log();
    AnimTrigger_Shutdown();
    AnimTriggerEvent_Shutdown();
    EngineEvents_Shutdown();
    ViewModules_Shutdown();
}
HOOK(0x5302A0, Engine_Shutdown);

void EngineEvents_Init(void)
{
    gEngineUnusedEvent1 = TaskEvent_Alloc(0);
    gEngineUnusedEvent0 = TaskEvent_Alloc(0);
}
HOOK(0x530960, EngineEvents_Init);

void EngineEvents_Shutdown(void)
{
    TaskEvent_Free(gEngineUnusedEvent0);
    TaskEvent_Free(gEngineUnusedEvent1);
}
HOOK(0x530980, EngineEvents_Shutdown);

void ViewModules_Init(void)
{
    EngineModules_Init();
    SoundSys_Init();
    SoundSysMicrophone_Init();
    SoundDefGroup_Init();
    SoundDefSound_Init();
    SoundDefGraph_Init();
    SoundDefTriggerOnce_Init();
    Screen_Init();
    Viewport_Init();
    TerraDraw_Init();
    CameraControl_Init();
    FreeCamera_Init();
    DebugText_Init();
    FadeOut_Init();
    GlowTask_Init();
    ParticleSystem_Init();
    SpriteParticleSystem_Init();
    FlashTask_Init();
    CameraFilter_Init();
}
HOOK(0x5309C0, ViewModules_Init);

void ViewModules_Shutdown(void)
{
    CameraFilter_Close();
    FlashTask_Shutdown();
    SpriteParticleSystem_Shutdown();
    ParticleSystem_Shutdown();
    GlowTask_Shutdown();
    FadeOut_Shutdown();
    DebugText_Shutdown();
    FreeCamera_Shutdown();
    CameraControl_Shutdown();
    TerraDraw_Shutdown();
    Viewport_Shutdown();
    Screen_Shutdown();
    SoundDefTriggerOnce_Shutdown();
    SoundDefGraph_Shutdown();
    SoundDefSound_Shutdown();
    SoundDefGroup_Shutdown();
    SoundSysMicrophone_Shutdown();
    SoundSys_Shutdown();
    EngineModules_Shutdown();
}
HOOK(0x530A20, ViewModules_Shutdown);

void EngineModules_Init(void)
{
    CoreTask_Init();
    RenderModules_Init();
}
HOOK(0x5311E0, EngineModules_Init);

void EngineModules_Shutdown(void)
{
    RenderModules_Shutdown();
    CoreTask_Shutdown();
}
HOOK(0x5311F0, EngineModules_Shutdown);

void RenderModules_Init(void)
{
    TerrainModules_Init();
    Model_Init();
    LightObj_Init();
    Sky_Init();
    LensFlare_Init();
    Sprite3D_Init();
    RigidDynCubeObj_OpenD3D();
    MagicObj_Open();
    BoneDynCubeObj_OpenD3D();
    PolyList_Init();
    Debug_Log();
    SplineObjSegment_Init();
    Shadow_Open();
    Mesh3D_Init();
    BoneMagicObj_Init();
    Sector_Open();
    MagicObjContainer_Open();
    Rain_Init();
    SkyDome_Init();
    Direct3DRender_Init();
}
HOOK(0x531540, RenderModules_Init);

void RenderModules_Shutdown(void)
{
    Direct3DRender_Shutdown();
    Debug_Log();
    Debug_Log();
    Rain_Close();
    MagicObjContainer_Close();
    Sector_Close();
    BoneMagicObj_Shutdown();
    Mesh3D_Shutdown();
    Shadow_Close();
    SplineObjSegment_Shutdown();
    Debug_Log();
    PolyList_Close();
    BoneDynCubeObj_CloseD3D();
    MagicObj_Close();
    RigidDynCubeObj_CloseD3D();
    Sprite3D_Shutdown();
    LensFlare_Shutdown();
    Debug_Log();
    LightObj_Shutdown();
    Model_Shutdown();
    TerrainModules_Shutdown();
}
HOOK(0x5315B0, RenderModules_Shutdown);

void TerrainModules_Init(void)
{
    WorldModules_Init();
    QCube_InitTables();
    Debug_Log();
    QCube_Init();
    Debug_Log();
    Debug_Log();
    TerrainMesh_Init();
    WaterMesh_Open();
    HazeLayer_Open();
    TransparencyList_Init();
}
HOOK(0x531D30, TerrainModules_Init);

void TerrainModules_Shutdown(void)
{
    TransparencyList_Shutdown();
    HazeLayer_Close();
    WaterMesh_Close();
    sub_4B0B90();
    Debug_Log();
    Debug_Log();
    QCube_Shutdown();
    Debug_Log();
    QCube_FreeTables();
    WorldModules_Shutdown();
}
HOOK(0x531D70, TerrainModules_Shutdown);

void WorldModules_Init(void)
{
    ObjModules_Init();
    World_Init();
    CellMask_Init();
}
HOOK(0x531DB0, WorldModules_Init);

void WorldModules_Shutdown(void)
{
    CellMask_Shutdown();
    World_Shutdown();
    ObjModules_Shutdown();
}
HOOK(0x531DC0, WorldModules_Shutdown);

void ObjModules_Init(void)
{
    QObj_Open();
    Camera_Open();
    DynCubeObj_Init();
    BoneObj_Init();
    Light_Open();
}
HOOK(0x531DD0, ObjModules_Init);

void ObjModules_Shutdown(void)
{
    Light_Close();
    BoneObj_Shutdown();
    DynCubeObj_Shutdown();
    Camera_Close();
    QObj_Close();
}
HOOK(0x531DF0, ObjModules_Shutdown);

void CoreTask_Init(void)
{
    CoreScript_Init();
    Task_InitSystem();
    Debug_Log();
}
HOOK(0x5323C0, CoreTask_Init);

void CoreTask_Shutdown(void)
{
    Debug_Log();
    Task_ShutdownSystem();
    CoreScript_Shutdown();
}
HOOK(0x5323D0, CoreTask_Shutdown);

void CoreScript_Init(void)
{
    Core_Init();
    Script_Init();
}
HOOK(0x5323E0, CoreScript_Init);

void CoreScript_Shutdown(void)
{
    Script_Shutdown();
    Core_Shutdown();
}
HOOK(0x5323F0, CoreScript_Shutdown);

void Core_Init(void)
{
    Debug_Log();
    App_ResetErrorHandlers();
    Error_SetUnusedFlag(1);
    AppContext_Open();
    Debug_Log();
    Math_InitTables();
    Mem_Init();
    Debug_Log();
    File_Init();
    RenderMode_Init();
    Display_Open();
    Resource_Init();
    Palette_Register();
    Texture_Init();
    Sound_Init();
    QVertex_Init();
    Keyboard_Open();
    Mouse_Open();
    Joystick_Init();
    Scene_InitList();
    TransContext_Init();
    Timer_Open();
    Debug_Log();
    SortRender_Init();
    Movie_Init();
}
HOOK(0x532400, Core_Init);

void Core_Shutdown(void)
{
    Movie_Shutdown();
    SortRender_Shutdown();
    Debug_Log();
    Timer_Open();
    TransContext_Shutdown();
    Debug_Log();
    Joystick_Shutdown();
    Mouse_Close();
    Keyboard_Close();
    QVertex_Shutdown();
    Sound_Shutdown();
    Texture_Shutdown();
    Palette_Unregister();
    Resource_Shutdown();
    Display_Close();
    Debug_Log();
    File_Shutdown();
    sub_4B0B90();
    Mem_Shutdown();
    Debug_Log();
    Debug_Log();
    AppContext_Close();
    sub_4B0B90();
    Debug_Log();
}
HOOK(0x532490, Core_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A845E0) uint8_t gEngineUnusedEvent0;
BSS(A845E0) uint8_t gEngineUnusedEvent1;
PAD(igb, A845E0, A845E2, 0x2);
