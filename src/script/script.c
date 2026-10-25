#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "core/mem.h"
#include "script/script.h"
#include "script/symbol.h"

FN_EXTERN(BreakScript);
FN_EXTERN(Script_SetWarningLevelCmd);

ORIG(0x48F1D0, int, AppContext_GetCompileScripts, (void));
ORIG(0x48F340, int, AppContext_GetQvmOnly, (void));
ORIG(0x4B1E90, char *, Path_GetExtension, (char *buf, int size, char *path));
ORIG(0x4B5B60, int, Resource_Exists, (const char *path));
ORIG(0x4B6440, int, Resource_GetTime, (void *time, const char *path));
ORIG(0x4B6490, int, Resource_CompareTime, (void *a, void *b));
ORIG(0x4B1AC0, void, File_Delete, (const char *path));
ORIG(0x4B5F00, void *, Resource_Load, (const char *path, int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *path));
ORIG(0x4B7E10, void, Script_Error, (const char *fmt, ...));
ORIG(0x4BBCB0, int, QscGen_Compile, (const char *qasPath, Script *s));
ORIG(0x4BB270, int, QvmAsm_Assemble, (const char *qvmPath, const char *qasPath));
ORIG(0x4BD220, void *, Qvm_Load, (const char *path, int unk1, int unk2));
ORIG(0x4BD380, void, Qvm_Free, (void *qvm));
ORIG(0x4BD080, int, Qvm_Run, (void *qvm));
ORIG(0x4BCAB0, int, Qvm_GetIntArg, (ScriptArgs *args, int i));
ORIG(0x4BD010, long double, Qvm_GetRealArg, (ScriptArgs *args, int i));
ORIG(0x4BD040, char *, Qvm_GetStringArg, (ScriptArgs *args, int i, char *buf, int size));
ORIG(0x4BD070, int, Qvm_GetArgLine, (ScriptArgs *args, int i));
ORIG(0x4C0200, void, Lexer_Init, (ScriptLexer *lex, ScriptReader *reader, char *buf, int size));
ORIG(0x4BF1C0, void, Reader_InitMem, (ScriptReader *reader, const void *text, int size));
ORIG(0x4BEFC0, void, Reader_Free, (ScriptReader *reader));
ORIG(0x4BFC70, int, Lexer_Next, (ScriptLexer *lex, void *token));
ORIG(0x4BF1F0, int, Parse_Block, (ScriptLexer *lex, void **out));
ORIG(0x4BFA80, void, Block_Exec, (void *stmt));
ORIG(0x4BF8F0, void, Block_Free, (void *stmt));
ORIG(0x4BD430, void, Qvm_Init, (void));
ORIG(0x4BC920, void, QscGen_Init, (void));
ORIG(0x4BBC80, void, QvmAsm_Init, (void));
ORIG(0x4C0250, void, QvmDis_Init, (void));
ORIG(0x4C1780, void, Debug_Log, (void));

extern int gScriptWarningSp;
extern int gScriptWarningStack[];
extern int gScriptLineSize;
extern int gScriptUnused0;
extern int gScriptUnused1;
extern void *gScriptErrorContext;
extern int gScriptUnused2;
extern void (*gScriptErrorHandler)(const char *msg, void *data);
extern int gScriptUnused3;

static void Path_SetExtension(char *path, const char *ext)
{
    memcpy(Path_GetExtension(NULL, 0, path), ext, 5);
}

static Script *Script_New(void)
{
    Script *s = Mem_Alloc(sizeof(Script), 4);

    memset(s, 0, sizeof(*s));
    return s;
}

Script *Script_Load(const char *path)
{
    char qvmPath[128], qasPath[128];
    char qvmTime[8], srcTime[8];
    void *qvm = NULL;
    void *source = NULL;
    int size;
    Script *s;

    if (AppContext_GetCompileScripts()) {
        strcpy(qvmPath, path);
        Path_SetExtension(qvmPath, ".qvm");
        if (AppContext_GetQvmOnly()) {
            if (!Resource_Exists(qvmPath))
                return NULL;
            qvm = Qvm_Load(qvmPath, 0, 0);
            if (!qvm)
                return NULL;
            goto have;
        }
        if (Resource_Exists(qvmPath)) {
            Resource_GetTime(qvmTime, qvmPath);
            Resource_GetTime(srcTime, path);
            if (Resource_CompareTime(qvmTime, srcTime) >= 0 && (qvm = Qvm_Load(qvmPath, 0, 0)) != NULL)
                goto have;
        }
    }
    if (!Resource_Exists(path))
        return NULL;
    source = Resource_Load(path, &size);
    if (!source)
        return NULL;
have:
    s = Script_New();
    Script_SetBreakEnabled(0);
    if (!qvm) {
        s->source = source;
        s->size = size;
        s->unk88 = 0;
        strcpy(s->name, path);
        if (!AppContext_GetCompileScripts())
            return s;
        strcpy(qasPath, path);
        Path_SetExtension(qasPath, ".qas");
        File_Delete(qvmPath);
        File_Delete(qasPath);
        if (QscGen_Compile(qasPath, s)) {
            Script_Free(s);
            Script_Error("An error occured when compiling file: %s", path);
            return NULL;
        }
        if (QvmAsm_Assemble(qvmPath, qasPath)) {
            Script_Error("An error occured when compiling file: %s", path);
            return s;
        }
        File_Delete(qasPath);
        Script_Free(s);
        s = Script_New();
        qvm = Qvm_Load(qvmPath, 0, 0);
        if (!qvm) {
            Script_Error("Failed to load QVM file: %s", qvmPath);
            Mem_Free(s);
            return NULL;
        }
    }
    s->isQvm = 1;
    s->qvm = qvm;
    strcpy(s->name, qvmPath);
    return s;
}
HOOK(0x4B80B0, Script_Load);

void Script_Free(Script *s)
{
    if (s->qvm) {
        Qvm_Free(s->qvm);
        Mem_Free(s);
        return;
    }
    Resource_Unload(s->name);
    Mem_Free(s);
}
HOOK(0x4B83D0, Script_Free);

/* compiles a .qsc file to .qvm without running it */
void Script_Compile(const char *path)
{
    char qvmPath[128], qasPath[128];
    int size;
    void *source;
    Script *s;

    if (!Resource_Exists(path))
        return;
    source = Resource_Load(path, &size);
    s = Script_New();
    s->source = source;
    s->size = size;
    s->unk88 = 0;
    strcpy(s->name, path);
    strcpy(qvmPath, path);
    Path_SetExtension(qvmPath, ".qvm");
    strcpy(qasPath, path);
    Path_SetExtension(qasPath, ".qas");
    File_Delete(qvmPath);
    File_Delete(qasPath);
    if (QscGen_Compile(qasPath, s)) {
        Script_Free(s);
        Script_Error("An error occured when compiling file: %s", path);
        return;
    }
    if (QvmAsm_Assemble(qvmPath, qasPath)) {
        Script_Error("An error occured when compiling file: %s", path);
        return;
    }
    File_Delete(qasPath);
    Script_Free(s);
}
HOOK(0x4B8410, Script_Compile);

void Script_Run(Script *s)
{
    char buf[0x800];
    ScriptLexer lex;
    ScriptReader reader;
    void *stmt;

    gScriptBreak = 0;
    gScriptError = 0;
    if (s->qvm) {
        if (Qvm_Run(s->qvm)) {
            Script_Error("Error in QVM program: %s\r\n%s", s->name, (char *)s->qvm + 0x268);
            gScriptBreak = 0;
            return;
        }
    } else {
        Lexer_Init(&lex, &reader, buf, sizeof(buf));
        Reader_InitMem(&reader, s->source, s->size);
        for (;;) {
            if (gScriptBreakEnabled && gScriptBreak)
                goto close;
            stmt = NULL;
            if (!Lexer_Next(&lex, &lex.pending))
                goto close;
            if (!Parse_Block(&lex, &stmt))
                break;
            Block_Exec(stmt);
            Block_Free(stmt);
            if (gScriptError)
                break;
        }
        if (gScriptWarningLevel)
            Script_Error("An error occured in script file: %s", s->name);
        gScriptError = 0;
close:
        Reader_Free(&reader);
    }
    gScriptBreak = 0;
}
HOOK(0x4B85B0, Script_Run);

Expr *Script_ParseExpr(const char *text)
{
    char buf[0x800];
    ScriptLexer lex;
    ScriptReader reader;
    Expr *e;

    Lexer_Init(&lex, &reader, buf, sizeof(buf));
    Reader_InitMem(&reader, text, strlen(text));
    if (Expr_Parse(&lex, &e)) {
        Reader_Free(&reader);
        return e;
    }
    Reader_Free(&reader);
    return NULL;
}
HOOK(0x4B86E0, Script_ParseExpr);

int Script_EvalExpr(const char *text)
{
    char buf[0x800];
    ScriptLexer lex;
    ScriptReader reader;
    Expr *e;
    int v;

    Lexer_Init(&lex, &reader, buf, sizeof(buf));
    Reader_InitMem(&reader, text, strlen(text));
    if (Expr_Parse(&lex, &e)) {
        v = Expr_EvalInt(e);
        Expr_Free(e);
        Reader_Free(&reader);
        return v;
    }
    Reader_Free(&reader);
    return 0;
}
HOOK(0x4B8770, Script_EvalExpr);

void Script_SetWarningLevel(int level)
{
    gScriptWarningLevel = level;
}
HOOK(0x4B8970, Script_SetWarningLevel);

void Script_PushWarningLevel(int level)
{
    if (gScriptWarningSp == 0) {
        Error_Show("Script: Stack underflow.");
        for (;;)
            ;
    }
    gScriptWarningSp--;
    gScriptWarningStack[gScriptWarningSp] = gScriptWarningLevel;
    Script_SetWarningLevel(level);
}
HOOK(0x4B8980, Script_PushWarningLevel);

void Script_PopWarningLevel(void)
{
    if (gScriptWarningSp >= 0x100) {
        Error_Show("Script: Stack overflow.");
        for (;;)
            ;
    }
    gScriptWarningLevel = gScriptWarningStack[gScriptWarningSp];
    gScriptWarningSp++;
}
HOOK(0x4B89C0, Script_PopWarningLevel);

void Script_SetMaxArgs(int count)
{
    gScriptMaxArgs = count;
}
HOOK(0x4B89F0, Script_SetMaxArgs);

void Script_SetLineSize(int size)
{
    gScriptLineSize = size;
}
HOOK(0x4B8A00, Script_SetLineSize);

void Script_SetBreakEnabled(int on)
{
    gScriptBreakEnabled = on;
}
HOOK(0x4B8A10, Script_SetBreakEnabled);

int Script_GetArgInt(ScriptArgs *args, int i)
{
    if (args->vm)
        return Qvm_GetIntArg(args, i);
    return Expr_EvalInt(args->argv[i]);
}
HOOK(0x4B8A20, Script_GetArgInt);

long double Script_GetArgReal(ScriptArgs *args, int i)
{
    if (args->vm)
        return Qvm_GetRealArg(args, i);
    return Expr_EvalReal(args->argv[i]);
}
HOOK(0x4B8A50, Script_GetArgReal);

char *Script_GetArgString(ScriptArgs *args, int i, char *buf, int size)
{
    if (args->vm)
        return Qvm_GetStringArg(args, i, buf, size);
    return Expr_EvalString(args->argv[i], buf, size);
}
HOOK(0x4B8A80, Script_GetArgString);

int Script_GetArgLine(ScriptArgs *args, int i)
{
    if (args->vm)
        return Qvm_GetArgLine(args, i);
    return args->argv[i]->line;
}
HOOK(0x4B8AD0, Script_GetArgLine);

int Script_HasArg(ScriptArgs *args, int i)
{
    return i < args->count;
}
HOOK(0x4B8B00, Script_HasArg);

void Script_Init(void)
{
    gScriptErrorHandler = NULL;
    gScriptUnused3 = 0;
    gScriptUnused1 = 0;
    gScriptErrorContext = NULL;
    gScriptUnused2 = 0;
    gScriptUnused0 = 0;
    Script_InitSymbols();
    Script_SetWarningLevel(1);
    Script_RegisterConstant("TRUE", 1);
    Script_RegisterConstant("ON", 1);
    Script_RegisterConstant("FALSE", 0);
    Script_RegisterConstant("OFF", 0);
    Script_RegisterConstant("SCRIPT_WARNINGLEVEL_NO_WARNINGS", 0);
    Script_RegisterConstant("SCRIPT_WARNINGLEVEL_NORMAL", 1);
    Script_RegisterConstant("SCRIPT_WARNINGLEVEL_HIGH", 2);
    Script_RegisterFunction("_Script_SetWarningLevel", FN(0x4B8C60, Script_SetWarningLevelCmd), NULL, 1);
    Script_RegisterFunction("BreakScript", FN(0x4B8C10, BreakScript), NULL, 0);
    Script_SetLineSize(0x800);
    Script_SetMaxArgs(0x2000);
    gScriptWarningSp = 0x100;
    Qvm_Init();
    QscGen_Init();
    QvmAsm_Init();
    QvmDis_Init();
}
HOOK(0x4B8B20, Script_Init);

ScriptValue *BreakScript(ScriptValue *out)
{
    gScriptBreak = 1;
    out->i = 0;
    out->d = 0;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4B8C10, BreakScript);

ScriptValue *Script_SetWarningLevelCmd(ScriptValue *out, void *data, ScriptArgs *args)
{
    Script_SetWarningLevel(Script_GetArgInt(args, 0));
    out->i = 0;
    out->d = 0;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4B8C60, Script_SetWarningLevelCmd);

void Script_Shutdown(void)
{
    Debug_Log();
    Debug_Log();
    Debug_Log();
    Debug_Log();
    Script_FreeSymbols();
}
HOOK(0x4B8CC0, Script_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A43E60) int gScriptUnused0;
BSS(A43E60) int gScriptUnused1;
BSS(A43E60) void *gScriptErrorContext;
BSS(A43E60) int gScriptUnused2;
BSS(A43E74) int gScriptUnused3;
BSS(A96598) int gScriptWarningLevel;
BSS(A96598) int gScriptWarningSp;
BSS(A96598) int gScriptBreakEnabled;
BSS(A96598) int gScriptBreak;
BSS(A96598) int gScriptError;
PAD(igb, A96598, A965AC, 0x14);
STORE(igb, A96598, 4) int store_A965C0[256];
ALIAS(gScriptWarningStack, store_A965C0, 0);
