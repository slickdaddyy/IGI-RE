#include <string.h>

#include "igi.h"
#include "hook.h"
#include "level/level.h"
#include "physobj/physobjvars.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B8A20, int, Script_GetArgInt, (ScriptArgs *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (ScriptArgs *args, int i, char *buf, int size));
ORIG(0x4F1C20, void, ParamCursor_Need, (ParamCursor *c, int count));

/* Operations of the parameter types made by the level init: parse from
   script arguments, format as text, copy the value out to *arg, paste it
   back from *arg, and get its size. */

void ParamInt16_Copy(int16_t *field, int16_t **dst)
{
    **dst = *field;
}
HOOK(0x4F0170, ParamInt16_Copy);

void ParamInt16_Paste(int16_t *field, int16_t **src)
{
    *field = **src;
}
HOOK(0x4F0190, ParamInt16_Paste);

void ParamInt16_Size(int16_t *field, int *size)
{
    *size = 2;
}
HOOK(0x4F01B0, ParamInt16_Size);

void ParamVarString_Parse(VarString *v, ParamCursor *c)
{
    char buf[0x400];

    if (v->text) {
        Mem_Free(v->text);
        v->text = NULL;
    }
    ParamCursor_Need(c, 1);
    Script_GetArgString(c->args, c->index, buf, sizeof(buf));
    if (strlen(buf) != 0) {
        v->text = Mem_Alloc(strlen(buf) + 1, 4);
        strcpy(v->text, buf);
    }
    c->index++;
}
HOOK(0x4F01C0, ParamVarString_Parse);

void ParamVarString_Copy(VarString *v, char **dst)
{
    if (!v->text) {
        **dst = 0;
        return;
    }
    strcpy(*dst, v->text);
}
HOOK(0x4F0270, ParamVarString_Copy);

void ParamVarString_Paste(VarString *v, char **src)
{
    if (v->text) {
        Mem_Free(v->text);
        v->text = NULL;
    }
    if (strlen(*src) != 0) {
        v->text = Mem_Alloc(strlen(*src) + 1, 4);
        strcpy(v->text, *src);
    }
}
HOOK(0x4F02B0, ParamVarString_Paste);

void ParamVarString_Size(VarString *v, int *size)
{
    if (!v->text) {
        *size = 1;
        return;
    }
    *size = strlen(v->text) + 1;
}
HOOK(0x4F0320, ParamVarString_Size);

void ParamString16_Parse(char *field, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    Script_GetArgString(c->args, c->index, field, 0x10);
    c->index++;
}
HOOK(0x4F0350, ParamString16_Parse);

void ParamString_Size(char *field, int *size)
{
    *size = strlen(field) + 1;
}
HOOK(0x4F0380, ParamString_Size);

void ParamString32_Parse(char *field, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    Script_GetArgString(c->args, c->index, field, 0x20);
    c->index++;
}
HOOK(0x4F03A0, ParamString32_Parse);

void ParamString_Paste(char *field, char **src)
{
    strcpy(field, *src);
}
HOOK(0x4F03D0, ParamString_Paste);

void ParamString256_Parse(char *field, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    Script_GetArgString(c->args, c->index, field, 0x100);
    c->index++;
}
HOOK(0x4F0400, ParamString256_Parse);

void ParamString_Format(char *field, ParamText *t)
{
    crt_sprintf(t->buf + strlen(t->buf), "%s", field);
}
HOOK(0x4F0430, ParamString_Format);

/* the buffer must already hold a string as long as the expression can be */
void ParamExpression_Parse(char **field, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    Script_GetArgString(c->args, c->index, *field, strlen(*field));
    c->index++;
}
HOOK(0x4F0460, ParamExpression_Parse);

void ParamExpression_Format(char **field, ParamText *t)
{
    crt_sprintf(t->buf + strlen(t->buf), "%s", *field);
}
HOOK(0x4F04A0, ParamExpression_Format);

void ParamString_Copy(char *field, char **dst)
{
    strcpy(*dst, field);
}
HOOK(0x4F04D0, ParamString_Copy);

void ParamBool8_Parse(char *field, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    *field = Script_GetArgInt(c->args, c->index);
    c->index++;
}
HOOK(0x4F0500, ParamBool8_Parse);

void ParamBool8_Format(char *field, ParamText *t)
{
    crt_sprintf(t->buf + strlen(t->buf), "%d", *field);
}
HOOK(0x4F0530, ParamBool8_Format);

void ParamBool8_Copy(char *field, char **dst)
{
    **dst = *field;
}
HOOK(0x4F0560, ParamBool8_Copy);

void ParamBool8_Paste(char *field, char **src)
{
    *field = **src;
}
HOOK(0x4F0570, ParamBool8_Paste);

/* parameter type ids for the task parameter tables */

int Param_PushButton(void) { return gParamPushButton; }
HOOK(0x4F0EF0, Param_PushButton);

int Param_DropDownCombo(void) { return gParamDropDownCombo; }
HOOK(0x4F0F00, Param_DropDownCombo);

int Param_ScriptFile(void) { return gParamScriptFile; }
HOOK(0x4F0F10, Param_ScriptFile);

int Param_Angle(void) { return gParamAngle; }
HOOK(0x4F0F20, Param_Angle);

int Param_Degrees(void) { return gParamDegrees; }
HOOK(0x4F0F30, Param_Degrees);

int Param_RGB(void) { return gParamRGB; }
HOOK(0x4F0F40, Param_RGB);

int Param_Real32(void) { return gParamReal32; }
HOOK(0x4F0F50, Param_Real32);

int Param_Real32x3(void) { return gParamReal32x3; }
HOOK(0x4F0F60, Param_Real32x3);

int Param_Real32x9(void) { return gParamReal32x9; }
HOOK(0x4F0F70, Param_Real32x9);

int Param_Real64(void) { return gParamReal64; }
HOOK(0x4F0F80, Param_Real64);

int Param_Real64x3(void) { return gParamReal64x3; }
HOOK(0x4F0F90, Param_Real64x3);

int Param_Int32(void) { return gParamInt32; }
HOOK(0x4F0FA0, Param_Int32);

int Param_Int16(void) { return gParamInt16; }
HOOK(0x4F0FB0, Param_Int16);

int Param_VarString(void) { return gParamVarString; }
HOOK(0x4F0FC0, Param_VarString);

int Param_String16(void) { return gParamString16; }
HOOK(0x4F0FD0, Param_String16);

int Param_String32(void) { return gParamString32; }
HOOK(0x4F0FE0, Param_String32);

int Param_String256(void) { return gParamString256; }
HOOK(0x4F0FF0, Param_String256);

int Param_Bool8(void) { return gParamBool8; }
HOOK(0x4F1000, Param_Bool8);

int Param_ObjectPos(void) { return gParamObjectPos; }
HOOK(0x4F1010, Param_ObjectPos);

int Param_Graph(void) { return gParamGraph; }
HOOK(0x4F1020, Param_Graph);
