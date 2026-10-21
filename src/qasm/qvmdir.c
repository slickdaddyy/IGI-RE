#include <string.h>

#include "hook.h"
#include "qasm/qasm.h"

ORIG(0x48F340, int, sub_48F340, (void));
ORIG(0x4A5980, int, crt_stricmp, (const char *a, const char *b));
ORIG(0x4C48E0, void, RigidDynCubeObj_ParseConfigFile, (const char *path));

typedef struct DirEntry {
    char *name;
    int unk04;
    char isDir;
} DirEntry;

/* Directory scan callback: loads every .qvm file, or .qsc when the flag
   read by sub_48F340 is clear. */
void sub_4C2F90(DirEntry *e)
{
    const char *ext;
    size_t len;

    if (e->isDir)
        return;
    ext = sub_48F340() ? "qvm" : "qsc";
    len = strlen(e->name);
    if (len > strlen(ext) && crt_stricmp(e->name + len - strlen(ext), ext) == 0)
        RigidDynCubeObj_ParseConfigFile(e->name);
}
HOOK(0x4C2F90, sub_4C2F90);
