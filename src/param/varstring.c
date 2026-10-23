#include <string.h>

#include "hook.h"
#include "param/param.h"

ORIG(0x401AE0, Task **, TaskRef_Add, (Task *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B86E0, void *, Script_ParseExpr, (const char *text));
ORIG(0x4B8980, void, Script_PushWarningLevel, (int level));
ORIG(0x4B89C0, void, Script_PopWarningLevel, (void));
ORIG(0x4B9660, long double, Expr_EvalReal, (void *expr));
ORIG(0x4B9BE0, void, Expr_Free, (void *expr));
/* reads one word, number or quoted string; *end gets the position after it */
ORIG(0x517260, char, Lex_ReadToken, (const char *s, const char **end, char *out, int size));
ORIG(0x4F1030, Task *, Task_FindById, (int id));

void Task_GetName(char *out, Task *task);
int VarString_ParseId(const char *s);

void VarString_Init(VarString *v)
{
    v->text = NULL;
    v->expanded = NULL;
    v->expr = NULL;
    v->refCount = 0;
}
HOOK(0x4F1320, VarString_Init);

void VarString_Free(VarString *v)
{
    int i;

    for (i = 0; i < v->refCount; i++)
        TaskRef_Remove(v->refs[i]);
    v->refCount = 0;
    if (v->text) {
        Mem_Free(v->text);
        v->text = NULL;
    }
    if (v->expanded) {
        Mem_Free(v->expanded);
        v->expanded = NULL;
    }
    if (v->expr) {
        Expr_Free(v->expr);
        v->expr = NULL;
    }
}
HOOK(0x4F1340, VarString_Free);

void VarString_AddRef(VarString *v, Task *task)
{
    int i;

    for (i = 0; i < v->refCount; i++)
        if (*v->refs[i] == task)
            return;
    v->refs[v->refCount] = TaskRef_Add(task);
    v->refCount++;
}
HOOK(0x4F13C0, VarString_AddRef);

/* Expand the text for task and compile it: "<word>_<id>.x" adds a
   reference to level task id (or becomes 0 when there is none),
   "this.x" becomes "<task name>.x", anything with ".isExists" becomes 1. */
void VarString_Bind(VarString *v, Task *task)
{
    /* the id parser may read the byte before the token */
    struct {
        const char *src;
        char tok[128];
    } s;
    char rest[128];
    char name[128];
    char out[1024];
    char *o = out;
    int i;

    s.src = v->text;
    out[0] = 0;
    for (i = 0; i < v->refCount; i++) {
        TaskRef_Remove(v->refs[i]);
        v->refs[i] = NULL;
    }
    if (v->expr)
        Expr_Free(v->expr);
    v->refCount = 0;
    while (s.src && *s.src) {
        if (!Lex_ReadToken(s.src, &s.src, s.tok, 0x7F)) {
            o[1] = 0;
            o[0] = *s.src;
            if (*s.src) {
                o++;
                s.src++;
            }
            continue;
        }
        i = VarString_ParseId(s.tok);
        if (i >= 0 && (s.tok[0] < '0' || s.tok[0] > '9')) {
            Task *t = Task_FindById(i);

            if (t)
                VarString_AddRef(v, t);
            else
                memcpy(s.tok, "0", 2);
        }
        if (s.tok[0] == 't' && s.tok[1] == 'h' && s.tok[2] == 'i' && s.tok[3] == 's' && s.tok[4] == '.') {
            strcpy(rest, s.tok + 5);
            Task_GetName(name, task);
            crt_sprintf(s.tok, "%s.%s", name, rest);
        }
        if (crt_strstr(s.tok, ".isExists"))
            memcpy(s.tok, "1", 2);
        strcpy(o, s.tok);
        o += strlen(o);
    }
    o[0] = ';';
    o[1] = 0;
    if (v->expanded) {
        Mem_Free(v->expanded);
        v->expanded = NULL;
    }
    if (Mem_GetMode()) {
        Script_PushWarningLevel(0);
        v->expanded = Mem_Alloc(strlen(out) + 1, 4);
        v->expr = Script_ParseExpr(out);
        Script_PopWarningLevel();
    } else {
        Mem_SetDefaultMode();
        Script_PushWarningLevel(0);
        v->expanded = Mem_Alloc(strlen(out) + 1, 4);
        v->expr = Script_ParseExpr(out);
        Script_PopWarningLevel();
        Mem_ClearMode();
    }
    strcpy(v->expanded, out);
}
HOOK(0x4F1400, VarString_Bind);

char VarString_HasExpr(VarString *v)
{
    if (!v->text)
        return 0;
    if (!*v->expanded)
        return 0;
    return v->expr != NULL;
}
HOOK(0x4F16C0, VarString_HasExpr);

void VarString_CheckRefs(VarString *v);

/* binds on first use or after a referenced task died */
long double VarString_EvalReal(VarString *v, Task *task)
{
    if (!v->expanded || (VarString_CheckRefs(v), !v->expanded))
        VarString_Bind(v, task);
    if (v->expr)
        return Expr_EvalReal(v->expr);
    return 0.0;
}
HOOK(0x4F16E0, VarString_EvalReal);

/* drops the expansion when a referenced task was deleted */
void VarString_CheckRefs(VarString *v)
{
    int alive = 1;
    int i;

    if (v->refCount <= 0)
        return;
    for (i = 0; i < v->refCount; i++)
        if (!*v->refs[i])
            alive = 0;
    if (alive)
        return;
    for (i = 0; i < v->refCount; i++)
        TaskRef_Remove(v->refs[i]);
    v->refCount = 0;
    Mem_Free(v->expanded);
    v->expanded = NULL;
    if (v->expr) {
        Expr_Free(v->expr);
        v->expr = NULL;
    }
}
HOOK(0x4F1740, VarString_CheckRefs);

/* the decimal number just before the first '.', -1 if there is none */
int VarString_ParseId(const char *s)
{
    const char *p = s;
    int value = 0;
    int mul = 1;
    char found = 0;
    char c = *p;

    while (c != '.' && c)
        c = *++p;
    for (;;) {
        c = *--p;
        if (c < '0' || c > '9')
            break;
        found = 1;
        value += (c - '0') * mul;
        mul *= 10;
        if (p < s)
            return -1;
    }
    if (!found)
        return -1;
    return value;
}
HOOK(0x4F17D0, VarString_ParseId);
