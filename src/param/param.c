#include <string.h>

#include "hook.h"
#include "orig.h"
#include "param/param.h"

FN_EXTERN(ParamEnum_ForEachConstant);

FN_EXTERN(ParamEnum_VisitConstant);

ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, Node *node));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4AF9D0, int, Regex_Compile, (void *prog, void *data, const char *pattern));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B8B00, int, Script_HasArg, (ScriptArgs *args, int i));
ORIG(0x4BABA0, void *, Script_GetSymbolTable, (void));
ORIG(0x4C02C0, void, SymbolTable_ForEachFiltered, (void *table, void *fn, void *data));
ORIG(0x4F0F50, int, Param_Real32, (void));
ORIG(0x4F0FA0, int, Param_Int32, (void));
ORIG(0x4F0FD0, int, Param_String16, (void));
ORIG(0x4F0FE0, int, Param_String32, (void));
ORIG(0x4F0FF0, int, Param_String256, (void));

extern char gParamEventUsed[];
extern ParamType gParamTypes[];

extern uint8_t gParamTableEvent;
extern uint8_t gParamChangedEvent;
extern char gParamOpArgCount;
extern char gParamOpCopy;
extern char gParamOpPaste;
extern char gParamOpSize;
extern char gParamOpParse;
extern char gParamOpFormat;

extern int gParamEnumReal32;
extern int gParamEnumString16;
extern int gParamEnumString32;
extern int gParamEnumString256;
extern int gParamEnumInt32;

extern ParamRange gParamRange01;
extern ParamRange gParamRange0100;
extern ParamRange gParamRange02;

/* the enum list callback keeps the original address, hooked below */
#define ParamEnumForEachConstant ((void (*)(void *))FN(0x4F1CF0, ParamEnum_ForEachConstant))
#define ParamEnumVisitConstant (FN(0x4F1D10, ParamEnum_VisitConstant))

char ParamEvent_Alloc(void);
void ParamEvent_Free(char index);

void Param_Init(void)
{
    memset(gParamEventUsed, 0, 16);
    memset(gParamTypes, 0, sizeof(ParamType) * PARAM_MAX_TYPES);
    gParamTableEvent = TaskEvent_Alloc(0);
    gParamChangedEvent = TaskEvent_Alloc(0);
    gParamOpFormat = ParamEvent_Alloc();
    gParamOpParse = ParamEvent_Alloc();
    gParamOpCopy = ParamEvent_Alloc();
    gParamOpPaste = ParamEvent_Alloc();
    gParamOpSize = ParamEvent_Alloc();
    gParamOpArgCount = ParamEvent_Alloc();
}
HOOK(0x4F1840, Param_Init);

void Param_Close(void)
{
    ParamEvent_Free(gParamOpFormat);
    ParamEvent_Free(gParamOpParse);
    ParamEvent_Free(gParamOpCopy);
    ParamEvent_Free(gParamOpPaste);
    ParamEvent_Free(gParamOpSize);
    ParamEvent_Free(gParamOpArgCount);
    gParamOpFormat = -1;
    gParamOpParse = -1;
    gParamOpCopy = -1;
    gParamOpPaste = -1;
    gParamOpSize = -1;
    gParamOpArgCount = -1;
    TaskEvent_Free(gParamTableEvent);
    TaskEvent_Free(gParamChangedEvent);
    gParamTableEvent = 0xFF;
    gParamChangedEvent = 0xFF;
}
HOOK(0x4F18C0, Param_Close);

ParamTable *ParamTable_New(int max)
{
    ParamTable *t = Mem_Alloc(max * sizeof(ParamEntry) + sizeof(ParamTable), 4);

    t->max = max;
    t->count = 0;
    t->entries = (ParamEntry *)(t + 1);
    return t;
}
HOOK(0x4F1950, ParamTable_New);

/* a copy of base with room for extra more entries */
ParamTable *ParamTable_Extend(ParamTable *base, int extra)
{
    ParamTable *t = ParamTable_New(base->count + extra);

    memcpy(t->entries, base->entries, base->count * sizeof(ParamEntry));
    t->count = base->count;
    return t;
}
HOOK(0x4F1980, ParamTable_Extend);

/* a new type starts with the operations of its parent */
int ParamType_New(const char *name, int parent)
{
    int i;

    for (i = 0; i < PARAM_MAX_TYPES; i++)
        if (!gParamTypes[i].used)
            break;
    if (i == PARAM_MAX_TYPES) {
        Error_Show("Unable to allocate parameter type");
        for (;;);
    }
    if (parent != -1)
        gParamTypes[i] = gParamTypes[parent];
    gParamTypes[i].used = 1;
    gParamTypes[i].name = name;
    gParamTypes[i].parent = parent;
    return i;
}
HOOK(0x4F19C0, ParamType_New);

void ParamType_Free(int type)
{
    gParamTypes[type].used = 0;
}
HOOK(0x4F1A40, ParamType_Free);

char Param_GetTableEvent(void)
{
    return gParamTableEvent;
}
HOOK(0x4F1A60, Param_GetTableEvent);

char Param_GetChangedEvent(void)
{
    return gParamChangedEvent;
}
HOOK(0x4F1A70, Param_GetChangedEvent);

/* Callers push only the first four arguments: flags and extra are the
   arguments of the type getter call before, still on the stack. */
void ParamTable_Add(ParamTable *t, void *base, void *field, int type, char flags, void *extra)
{
    int offset = (char *)field - (char *)base;
    ParamEntry *e;

    if (type == -1) {
        Error_Show("Undefined parameter type.");
        for (;;);
    }
    e = &t->entries[t->count];
    e->offset = offset;
    e->type = type;
    e->extra = extra;
    e->flags = flags;
    t->count++;
}
HOOK(0x4F1A80, ParamTable_Add);

void ParamType_Call(void *field, int type, char op, void *arg)
{
    gParamTypes[type].ops[(int)op](field, arg);
}
HOOK(0x4F1AF0, ParamType_Call);

char ParamEvent_Alloc(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (!gParamEventUsed[i]) {
            gParamEventUsed[i] = 1;
            return i;
        }
    }
    Error_Show("Unable to allocate parameter event");
    for (;;);
}
HOOK(0x4F1B20, ParamEvent_Alloc);

void ParamEvent_Free(char index)
{
    gParamEventUsed[(int)index] = 0;
}
HOOK(0x4F1B50, ParamEvent_Free);

char ParamOp_Parse(void)
{
    return gParamOpParse;
}
HOOK(0x4F1B60, ParamOp_Parse);

char ParamOp_ArgCount(void)
{
    return gParamOpArgCount;
}
HOOK(0x4F1B70, ParamOp_ArgCount);

char ParamOp_Format(void)
{
    return gParamOpFormat;
}
HOOK(0x4F1B80, ParamOp_Format);

char ParamOp_Size(void)
{
    return gParamOpSize;
}
HOOK(0x4F1B90, ParamOp_Size);

char ParamOp_Copy(void)
{
    return gParamOpCopy;
}
HOOK(0x4F1BA0, ParamOp_Copy);

char ParamOp_Paste(void)
{
    return gParamOpPaste;
}
HOOK(0x4F1BB0, ParamOp_Paste);

/* sets an operation and passes it down to all derived types */
void ParamType_SetOp(int type, char op, ParamOpFn fn)
{
    int i;

    gParamTypes[type].ops[(int)op] = fn;
    for (i = 0; i < PARAM_MAX_TYPES; i++)
        if (gParamTypes[i].used && gParamTypes[i].parent == type)
            ParamType_SetOp(i, op, fn);
}
HOOK(0x4F1BC0, ParamType_SetOp);

void ParamCursor_Need(ParamCursor *c, int count)
{
    int start = c->index;
    int i;

    for (i = 0; i < count; i++) {
        if (!Script_HasArg(c->args, start + i)) {
            Error_Show("Too few parameters in line %d of file '%s'", c->line, c->file);
            for (;;);
        }
    }
}
HOOK(0x4F1C20, ParamCursor_Need);

ParamRange *Param_Range01(void)
{
    gParamRange01.min = 0.0f;
    gParamRange01.max = 1.0f;
    return &gParamRange01;
}
HOOK(0x4F1C70, Param_Range01);

ParamRange *Param_Range02(void)
{
    gParamRange02.min = 0.0f;
    gParamRange02.max = 2.0f;
    return &gParamRange02;
}
HOOK(0x4F1C90, Param_Range02);

ParamRange *Param_Range0100(void)
{
    gParamRange0100.min = 0.0f;
    gParamRange0100.max = 100.0f;
    return &gParamRange0100;
}
HOOK(0x4F1CB0, Param_Range0100);

void ParamEnum_Setup(ParamEnum *e, void (*forEach)(void *ctx));

void ParamEnum_Init(ParamEnum *e)
{
    ParamEnum_Setup(e, ParamEnumForEachConstant);
}
HOOK(0x4F1CD0, ParamEnum_Init);

void ParamEnum_ForEachConstant(ParamEnumVisitor *v)
{
    SymbolTable_ForEachFiltered(Script_GetSymbolTable(), ParamEnumVisitConstant, v);
}
HOOK(0x4F1CF0, ParamEnum_ForEachConstant);

/* passes every integer script constant to the visitor as an Int32 */
void ParamEnum_VisitConstant(const char *name, ScriptSymbol *sym, ParamEnumVisitor *v)
{
    ParamEnumValue value;
    int data;

    if (sym->kind == 2 && sym->type == 1) {
        data = (int)sym->data;
        value.type = Param_Int32();
        value.name = name;
        value.value = &data;
        v->add(v, &value);
    }
}
HOOK(0x4F1D10, ParamEnum_VisitConstant);

void ParamEnum_Setup(ParamEnum *e, void (*forEach)(void *ctx))
{
    e->forEach = forEach;
    e->patterns.head = (Node *)&e->patterns.tail;
    e->patterns.tailpred = (Node *)&e->patterns.head;
    e->patterns.tail = NULL;
}
HOOK(0x4F1D60, ParamEnum_Setup);

void ParamEnum_Free(ParamEnum *e)
{
    Node *n = e->patterns.head;
    Node *next;

    for (next = n->next; next; n = next, next = next->next) {
        List_Remove(n);
        Mem_Free(n);
    }
}
HOOK(0x4F1D80, ParamEnum_Free);

void ParamEnum_AddConstants(ParamEnum *e, const char *pattern)
{
    ParamEnumPattern *p = Mem_Alloc(sizeof(ParamEnumPattern), 4);

    p->pattern = pattern;
    p->node.prev = NULL;
    p->node.next = NULL;
    Regex_Compile(p->prog, p->data, pattern);
    List_AddTail(&e->patterns, &p->node);
}
HOOK(0x4F1DB0, ParamEnum_AddConstants);

int Param_EnumInt32(void)
{
    return gParamEnumInt32;
}
HOOK(0x4F1E00, Param_EnumInt32);

int Param_EnumReal32(void)
{
    return gParamEnumReal32;
}
HOOK(0x4F1E10, Param_EnumReal32);

int Param_EnumString32(void)
{
    return gParamEnumString32;
}
HOOK(0x4F1E20, Param_EnumString32);

void ParamEnum_InitTypes(void)
{
    gParamEnumInt32 = ParamType_New("EnumInt32", Param_Int32());
    gParamEnumReal32 = ParamType_New("EnumReal32", Param_Real32());
    gParamEnumString16 = ParamType_New("EnumString16", Param_String16());
    gParamEnumString32 = ParamType_New("EnumString32", Param_String32());
    gParamEnumString256 = ParamType_New("EnumString256", Param_String256());
}
HOOK(0x4F1E30, ParamEnum_InitTypes);

void ParamEnum_FreeTypes(void)
{
    ParamType_Free(gParamEnumInt32);
    ParamType_Free(gParamEnumReal32);
    ParamType_Free(gParamEnumString16);
    ParamType_Free(gParamEnumString32);
    ParamType_Free(gParamEnumString256);
}
HOOK(0x4F1EA0, ParamEnum_FreeTypes);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E0E4) uint8_t gParamTableEvent INIT(255);
DATA(54E0E4) uint8_t gParamChangedEvent INIT(255);
DATA(54E0E4) char gParamOpArgCount INIT(-1);
DATA(54E0E4) char gParamOpCopy INIT(-1);
DATA(54E0E4) char gParamOpPaste INIT(-1);
DATA(54E0E4) char gParamOpSize INIT(-1);
DATA(54E0E4) char gParamOpParse INIT(-1);
DATA(54E0E4) char gParamOpFormat INIT(-1);
DATA(54E150) int gParamEnumReal32 INIT(-1);
DATA(54E150) int gParamEnumString16 INIT(-1);
DATA(54E150) int gParamEnumString32 INIT(-1);
DATA(54E150) int gParamEnumString256 INIT(-1);
DATA(54E150) int gParamEnumInt32 INIT(-1);
PAD(igb, A758C8, A758C8, 0x18);
ALIAS(gParamEventUsed, pad_A758C8, 0);
PAD(igb, A758C8, A758E0, 0x1300);
ALIAS(gParamTypes, pad_A758E0, 0);
BSS(A758C8) ParamRange gParamRange01;
BSS(A758C8) ParamRange gParamRange0100;
BSS(A758C8) ParamRange gParamRange02;
