/* Standalone build: names the code calls through ORIG that no rewrite defines under that
   name. Each is a single jmp, so any calling convention and varargs pass straight through. */
#include <windows.h>

#define ALIAS(name, target) __asm__("\t.text\n\t.globl _" #name "\n_" #name ":\n\tjmp _" #target)

/* the c runtime the game code calls, bound at startup to msvcrt.dll: the closest thing to the
   static msvc 6 runtime in IGI.exe (same FILE layout). Our own code keeps the toolchain's
   runtime, as in the dll build. terrdraw/crt.c is left out of this build. */
#define CRT_LIST(X) \
    X(crt_acos, acos) \
    X(crt_asin, asin) \
    X(crt_atof, atof) \
    X(crt_atoi, atoi) \
    X(crt_ceil, ceil) \
    X(crt_exit, exit) \
    X(crt_fclose, fclose) \
    X(crt_fgetc, fgetc) \
    X(crt_filbuf, _filbuf) \
    X(crt_floor, floor) \
    X(crt_flsbuf, _flsbuf) \
    X(crt_fopen, fopen) \
    X(crt_fread, fread) \
    X(crt_free, free) \
    X(crt_fseek, fseek) \
    X(crt_ftell, ftell) \
    X(crt_fwrite, fwrite) \
    X(crt_isctype, _isctype) \
    X(crt_malloc, malloc) \
    X(crt_memcpy, memcpy) \
    X(crt_read, _read) \
    X(crt_remove, remove) \
    X(crt_splitpath, _splitpath) \
    X(crt_sprintf, sprintf) \
    X(crt_strchr, strchr) \
    X(crt_strcpy, strcpy) \
    X(crt_stricmp, _stricmp) \
    X(crt_strlen, strlen) \
    X(crt_strlwr, _strlwr) \
    X(crt_strncat, strncat) \
    X(crt_strncmp, strncmp) \
    X(crt_strncpy, strncpy) \
    X(crt_strrchr, strrchr) \
    X(crt_strstr, strstr) \
    X(crt_strtok, strtok) \
    X(crt_toupper, toupper) \
    X(crt_vsprintf, vsprintf) \
    X(crt_write, _write) \
    X(Crt_LongJmp, longjmp)

#define X(name, target) \
    __attribute__((used)) static void *ptr_##name; \
    __asm__("\t.text\n\t.globl _" #name "\n_" #name ":\n\tjmp *_ptr_" #name);
CRT_LIST(X)
#undef X

/* dinput, bound to the system copy: the game folder may hold this project's proxy dinput.dll */
__attribute__((used)) static void *ptr_DirectInputCreateA, *ptr_DirectInputCreateEx;
__asm__("\t.text\n\t.globl _DirectInputCreateA_@16\n_DirectInputCreateA_@16:\n\tjmp *_ptr_DirectInputCreateA\n"
        "\t.globl _DirectInputCreateEx_@20\n_DirectInputCreateEx_@20:\n\tjmp *_ptr_DirectInputCreateEx");

int Link_Bind(HMODULE msvcrt, HMODULE dinput)
{
    if (!(ptr_DirectInputCreateA = (void *)GetProcAddress(dinput, "DirectInputCreateA")) ||
        !(ptr_DirectInputCreateEx = (void *)GetProcAddress(dinput, "DirectInputCreateEx")))
        return 0;
#define X(name, target) \
    if (!(ptr_##name = (void *)GetProcAddress(msvcrt, #target))) \
        return 0;
    CRT_LIST(X)
#undef X
    return 1;
}

/* the exe's import thunks */
ALIAS(AVIFileExit_@0, AVIFileExit@0);
ALIAS(AVIFileGetStream_@16, AVIFileGetStream@16);
ALIAS(AVIFileInfoA_@12, AVIFileInfoA@12);
ALIAS(AVIFileInit_@0, AVIFileInit@0);
ALIAS(AVIFileOpenA_@16, AVIFileOpenA@16);
ALIAS(AVIFileRelease_@4, AVIFileRelease@4);
ALIAS(AVIStreamInfoA_@12, AVIStreamInfoA@12);
ALIAS(AVIStreamRead_@28, AVIStreamRead@28);
ALIAS(AVIStreamReadFormat_@16, AVIStreamReadFormat@16);
ALIAS(AVIStreamRelease_@4, AVIStreamRelease@4);
ALIAS(DirectDrawCreateEx_@16, DirectDrawCreateEx@16);
ALIAS(DirectDrawEnumerateExA_@12, DirectDrawEnumerateExA@12);
ALIAS(DirectSoundCreate_@12, DirectSoundCreate@12);
ALIAS(ICClose_@4, ICClose@4);
ALIAS(ICDecompress_, ICDecompress);
ALIAS(ICLocate_@20, ICLocate@20);
ALIAS(ICSendMessage_@16, ICSendMessage@16);

/* local names for rewritten functions */
ALIAS(BuildLod, Lightmap_BuildLod);
ALIAS(Coll_Descend_, Coll_Descend);
ALIAS(DynCube_GetRoot_, DynCube_GetRoot);
ALIAS(List_AddTail_, List_AddTail);
ALIAS(List_Insert, List_InsertBefore);
ALIAS(List_Remove_, List_Remove);
ALIAS(Model_GetUnloadMethod_, Model_GetUnloadMethod);
ALIAS(Mtp_FindChunk_, Mtp_FindChunk);
ALIAS(Pool_Free_, Pool_Free);
ALIAS(Pool_Init_, Pool_Init);
ALIAS(Quat_SlerpPrepare_, Quat_SlerpPrepare);
ALIAS(RigidDynCubeObj_GetType_, RigidDynCubeObj_GetType);
ALIAS(sub_4B0B90, Debug_Log);
ALIAS(sub_4C1780, Debug_Log);
ALIAS(sub_4C8130, DynCube_SendEventLines);
ALIAS(sub_51A4A0, SplinePath_GetMatrix);
ALIAS(Task_Nothing2, Task_Nothing);
ALIAS(VirModel_Find_, VirModel_Find);
ALIAS(VirModel_GetRadius_, VirModel_GetRadius);
ALIAS(VirModel_GetRadius2, VirModel_GetRadius);
ALIAS(VirModel_InitAllLods_, VirModel_InitAllLods);
ALIAS(Weapon_GetEvent7b, Weapon_GetEvent7);
ALIAS(World_GetEvent0_, World_GetEvent0);
