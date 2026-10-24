#include <string.h>

#include "hook.h"
#include "qasm/qasm.h"

ORIG(0x48F340, int, AppContext_GetQvmOnly, (void));
ORIG(0x4A5980, int, crt_stricmp, (const char *a, const char *b));
ORIG(0x4C48E0, void, RigidDynCubeObj_ParseConfigFile, (const char *path));

typedef struct DirEntry {
    char *name;
    int unk04;
    char isDir;
} DirEntry;

/* Directory scan callback: loads every .qvm file, or .qsc when the flag
   read by AppContext_GetQvmOnly is clear. */
void MagicObj_LoadDirEntry(DirEntry *e)
{
    const char *ext;
    size_t len;

    if (e->isDir)
        return;
    ext = AppContext_GetQvmOnly() ? "qvm" : "qsc";
    len = strlen(e->name);
    if (len > strlen(ext) && crt_stricmp(e->name + len - strlen(ext), ext) == 0)
        RigidDynCubeObj_ParseConfigFile(e->name);
}
HOOK(0x4C2F90, MagicObj_LoadDirEntry);
