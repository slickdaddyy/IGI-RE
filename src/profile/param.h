#ifndef PROFILE_PARAM_H
#define PROFILE_PARAM_H

#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"

/* Task parameters: a type's table lists the fields a script may set,
   with their offset, value type and a range or enum list. */

ORIG(0x4F0CF0, void *, Task_RunScript, (void *parent, const char *path, int unk));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1950, ParamTable *, ParamTable_New, (int max));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
/* the original passes flags and extra by leaving the type getter's
   arguments on the stack */
ORIG(0x4F1A80, void, ParamTable_Add, (ParamTable *t, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F1C70, ParamRange *, Param_Range01, (void));
ORIG(0x4F1C90, ParamRange *, Param_Range02, (void));
ORIG(0x4F1CB0, ParamRange *, Param_Range0100, (void));
ORIG(0x4F1CD0, void, ParamEnum_Init, (void *e));
ORIG(0x4F1D80, void, ParamEnum_Free, (void *e));
ORIG(0x4F1DB0, void, ParamEnum_AddConstants, (void *e, const char *pattern));
ORIG(0x4F1E00, void *, Param_EnumInt32, (void));
ORIG(0x4F1E10, void *, Param_EnumReal32, (void));
ORIG(0x4C1830, void, QTaskList_Free, (void *p));

#endif
