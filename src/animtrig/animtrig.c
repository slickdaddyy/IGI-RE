#include <string.h>

#include "hook.h"
#include "igi.h"
#include "animtrig/animtrig.h"

ORIG(0x4A5980, int, crt_stricmp, (const char *a, const char *b));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x48F340, int, AppContext_GetQvmOnly, (void));
ORIG(0x4B1420, void, File_ScanDir, (const char *dir, void *fn, void *user));

/* "DefineAnimTrigger(id, type, ...)": a numbered trigger whose type parses
   and keeps its own data */
typedef struct AnimTrigger {
    int id;
    uint16_t type;
    void *data;
} AnimTrigger;

typedef struct AnimTriggerParseArg {
    void *data;
    ScriptArgs *args;
    int first;
} AnimTriggerParseArg;

typedef struct AnimTriggerFireArg {
    void *obj;
    void *data;
    void *pos;
} AnimTriggerFireArg;

typedef struct FileInfo {
    const char *path;
    void *data;
    char isDir;
} FileInfo;

extern AnimTrigger gAnimTriggers[];
extern int gAnimTriggerCount;
extern uint8_t gAnimTriggerFireEvent;
extern uint8_t gAnimTriggerSizeEvent;
extern uint8_t gAnimTriggerParseEvent;
extern uint8_t gUnkA5EF88;
extern int gUnkA5EF90;

void AnimTrigger_ParseConfigFile(const char *path);

/* runs every .qsc (.qvm when only compiled scripts are used) of a directory */
void AnimTrigger_LoadFile(FileInfo *f)
{
    const char *ext;
    size_t len, extLen;

    if (f->isDir)
        return;
    ext = AppContext_GetQvmOnly() ? "qvm" : "qsc";
    len = strlen(f->path);
    extLen = strlen(ext);
    if (len > extLen && crt_stricmp(f->path + strlen(f->path) - strlen(ext), ext) == 0)
        AnimTrigger_ParseConfigFile(f->path);
}
HOOK(0x4EBDF0, AnimTrigger_LoadFile);

ScriptValue *DefineAnimTrigger(ScriptValue *out, void *data, ScriptArgs *args);

void AnimTrigger_Init(void)
{
    gAnimTriggerFireEvent = TaskEvent_Alloc(0);
    gAnimTriggerSizeEvent = TaskEvent_Alloc(0);
    gAnimTriggerParseEvent = TaskEvent_Alloc(0);
    Script_RegisterFunction("DefineAnimTrigger", DefineAnimTrigger, NULL, -1);
}
HOOK(0x4EBE70, AnimTrigger_Init);

/* The type's size handler gives the size of its data, the parse handler
   reads the script arguments from the third on into it. */
ScriptValue *DefineAnimTrigger(ScriptValue *out, void *data, ScriptArgs *args)
{
    AnimTrigger *t = &gAnimTriggers[gAnimTriggerCount];
    TaskFn fn;
    int size;

    t->id = Script_GetArgInt(args, 0);
    t->type = Script_GetArgInt(args, 1);
    t->data = NULL;
    fn = gTaskHandlers[gAnimTriggerSizeEvent][t->type];
    if (fn) {
        AnimTriggerParseArg arg;

        fn(NULL, &size);
        arg.data = Mem_Alloc(size, 4);
        arg.args = args;
        arg.first = 2;
        t = &gAnimTriggers[gAnimTriggerCount];
        t->data = arg.data;
        gTaskHandlers[gAnimTriggerParseEvent][t->type](NULL, &arg);
    }
    gAnimTriggerCount++;
    out->i = 1;
    out->d = 1.0;
    out->s = gScriptEmptyString;
    return out;
}
HOOK(0x4EBEB0, DefineAnimTrigger);

void AnimTrigger_Shutdown(void)
{
    int i;

    for (i = 0; i < gAnimTriggerCount; i++) {
        if (gAnimTriggers[i].data) {
            Mem_Free(gAnimTriggers[i].data);
            gAnimTriggers[i].data = NULL;
        }
    }
    TaskEvent_Free(gAnimTriggerFireEvent);
    gAnimTriggerFireEvent = 0xFF;
    TaskEvent_Free(gAnimTriggerSizeEvent);
    gAnimTriggerSizeEvent = 0xFF;
    TaskEvent_Free(gAnimTriggerParseEvent);
    gAnimTriggerParseEvent = 0xFF;
}
HOOK(0x4EBFC0, AnimTrigger_Shutdown);

uint8_t AnimTrigger_GetFireEvent(void)
{
    return gAnimTriggerFireEvent;
}
HOOK(0x4EC040, AnimTrigger_GetFireEvent);

uint8_t AnimTrigger_GetSizeEvent(void)
{
    return gAnimTriggerSizeEvent;
}
HOOK(0x4EC050, AnimTrigger_GetSizeEvent);

uint8_t AnimTrigger_GetParseEvent(void)
{
    return gAnimTriggerParseEvent;
}
HOOK(0x4EC060, AnimTrigger_GetParseEvent);

void AnimTrigger_ParseConfigFile(const char *path)
{
    void *script;

    if (crt_strstr(path, "vssver"))
        return;
    script = Script_Load(path);
    if (!script) {
        Error_Show("AnimTrigger_ParseConfigFile(): Couldn't load script: %s", path);
        for (;;)
            ;
    }
    Script_Run(script);
    Script_Free(script);
}
HOOK(0x4EC070, AnimTrigger_ParseConfigFile);

void AnimTriggerLoad(const char *dir)
{
    File_ScanDir(dir, AnimTrigger_LoadFile, NULL);
}
HOOK(0x4EC0C0, AnimTriggerLoad);

/* sends the fire event of trigger id, if there is one */
void AnimTrigger_Fire(int id, void *obj, void *pos)
{
    AnimTriggerFireArg arg;
    int i;

    for (i = 0; i < gAnimTriggerCount; i++) {
        if (gAnimTriggers[i].id == id) {
            arg.obj = obj;
            arg.data = gAnimTriggers[i].data;
            arg.pos = pos;
            gTaskHandlers[gAnimTriggerFireEvent][gAnimTriggers[i].type](NULL, &arg);
            return;
        }
    }
}
HOOK(0x4EC0E0, AnimTrigger_Fire);

void sub_4EC160(void)
{
    gUnkA5EF88 = TaskEvent_Alloc(0);
    gUnkA5EF90 = 0;
}
HOOK(0x4EC160, sub_4EC160);

void sub_4EC180(void)
{
    TaskEvent_Free(gUnkA5EF88);
}
HOOK(0x4EC180, sub_4EC180);
