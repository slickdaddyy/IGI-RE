#include <string.h>

#include "hook.h"
#include "mission/mission.h"
#include "sysmisc/sysmisctypes.h"

/* a mission defined by DefineMission in LOCAL:missions/<folder>/mission.qsc */
typedef struct Mission {
    int id;
    char name[0x20];
    char text[0x100];
    char unk124[0x20];
    char path[0x100];
    char unk244[0x100];
    char unk344[0x100];
    char models[0x20];
    char unk464[0x20];
    char picture[0x20];
    int next;
} Mission;

_Static_assert(offsetof(Mission, path) == 0x144, "Mission.path");
_Static_assert(offsetof(Mission, picture) == 0x484, "Mission.picture");
_Static_assert(sizeof(Mission) == 0x4A8, "Mission");

ORIG(0x48F340, int, AppContext_GetQvmOnly, (void));
ORIG(0x4B09C0, int, Wildcard_Find, (int *start, int *end, int *groups, const char *wildcard, const char *str,
                                    int nocase));
ORIG(0x4B1420, void, File_ScanDir, (const char *path, void *fn, void *user));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x516770, char *, crt_strlwr, (char *s));

extern Mission gMissions[];
extern int gMissionCount;

ScriptValue *DefineMission(ScriptValue *out, void *data, void *args);
void Mission_LoadAll(void);
Mission *Mission_Find(int id);

/* File_ScanDir callback: runs the mission script of every folder found */
void MissionOpen(DirEntry *e)
{
    char name[260];
    const char *file;
    void *script;

    if (e->isDir) {
        File_ScanDir(e->name, MissionOpen, NULL);
        return;
    }
    strcpy(name, e->name);
    crt_strlwr(name);
    file = AppContext_GetQvmOnly() ? "mission.qvm" : "mission.qsc";
    if (!Wildcard_Find(NULL, NULL, NULL, file, name, 1))
        return;
    script = Script_Load(e->name);
    if (!script) {
        Error_Show("Mission_Open(): Couldn't load script: %s", file);
        for (;;)
            ;
    }
    Script_Run(script);
    Script_Free(script);
}
HOOK(0x484E60, MissionOpen);

void Mission_Init(void)
{
    Script_RegisterFunction("DefineMission", DefineMission, NULL, -1);
    Script_RegisterConstant("MISSION_NEXT_MISSION_UNDEFINED", -1);
    Mission_LoadAll();
}
HOOK(0x484F30, Mission_Init);

ScriptValue *DefineMission(ScriptValue *out, void *data, void *args)
{
    ScriptValue v;

    gMissions[gMissionCount].id = Script_GetArgInt(args, 0);
    if (Mission_Find(gMissions[gMissionCount].id)) {
        Error_Show("Mission ID %d not unique", gMissions[gMissionCount].id);
        for (;;)
            ;
    }
    Script_GetArgString(args, 1, gMissions[gMissionCount].name, 0x20);
    Script_GetArgString(args, 2, gMissions[gMissionCount].text, 0x100);
    Script_GetArgString(args, 3, gMissions[gMissionCount].unk124, 0x20);
    gMissions[gMissionCount].next = Script_GetArgInt(args, 4);
    Script_GetArgString(args, 5, gMissions[gMissionCount].path, 0x100);
    Script_GetArgString(args, 6, gMissions[gMissionCount].unk244, 0x100);
    Script_GetArgString(args, 7, gMissions[gMissionCount].unk344, 0x100);
    Script_GetArgString(args, 8, gMissions[gMissionCount].models, 0x20);
    Script_GetArgString(args, 9, gMissions[gMissionCount].unk464, 0x20);
    Script_GetArgString(args, 10, gMissions[gMissionCount].picture, 0x20);
    gMissionCount++;
    v.i = 1;
    v.d = 1.0;
    v.s = gEmptyString;
    *out = v;
    return out;
}
HOOK(0x484F60, DefineMission);

void Mission_Close(void)
{
    gMissionCount = 0;
}
HOOK(0x485140, Mission_Close);

void Mission_LoadAll(void)
{
    gMissionCount = 0;
    File_ScanDir("LOCAL:missions", MissionOpen, NULL);
}
HOOK(0x485150, Mission_LoadAll);

Mission *Mission_Find(int id)
{
    int i;

    for (i = 0; i < gMissionCount; i++) {
        if (gMissions[i].id == id)
            return &gMissions[i];
    }
    return NULL;
}
HOOK(0x485170, Mission_Find);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, 5C1590, 5C1590, 0x746C);
ALIAS(gMissions, pad_5C1590, 0);
BSS(5C1590) int gMissionCount;
