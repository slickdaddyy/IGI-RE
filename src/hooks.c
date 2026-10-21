#include "hook.h"
#include "core/list.h"
#include "core/mem.h"
#include "script/symbol.h"
#include "script/symtab.h"

const Hook hooks[] = {
    {0x4AF8F0, List_AddTail},
    {0x4AF910, List_AddHead},
    {0x4AF960, List_Remove},
    {0x4B0BA0, Mem_Init},
    {0x4B0D40, Mem_SetDefaultMode},
    {0x4B0C60, Mem_Alloc},
    {0x4B0D10, Mem_Free},
    {0x4B0D60, Mem_PushMode},
    {0x4B0D90, Mem_PopMode},
    {0x4C0530, SymbolTable_Hash},
    {0x4C04B0, SymbolTable_Find},
    {0x4C0560, SymbolTable_Add},
    {0x4C0620, SymbolTable_Lookup},
    {0x4BAAC0, Script_RegisterSymbol},
    {0x4BAB80, Script_FindSymbol},
    {0x4B8930, Script_SetSymbolContext},
    {0x4B7E80, Script_RegisterInt},
    {0x4B7FA0, Script_RegisterInt2},
    {0x4B7EE0, Script_RegisterBool8},
    {0x4B7FC0, Script_RegisterReal},
    {0x4B8810, Script_RegisterConstant},
    {0x4B8890, Script_RegisterFunction},
    {0x4B88F0, Script_RegisterFunctionEx},
    {0, 0},
};
