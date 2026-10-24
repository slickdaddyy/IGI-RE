#include <string.h>

#include "hook.h"
#include "igi.h"
#include "core/error.h"
#include "core/file.h"
#include "core/list.h"

ORIG(0x4AAD10, int, crt_strncmp, (const char *a, const char *b, size_t n));
ORIG(0x4AA0A0, char *, crt_strchr, (const char *s, int c));
ORIG(0x5098B0, void, LocalDevice_Register, (void));
ORIG(0x5098D0, void, LocalDevice_Unregister, (void));
ORIG(0x4BADA0, void, Script_FreeRefBlock, (Node *file));
ORIG(0x4B1DE0, const char *, Device_GetName, (int device));

extern FileAlias gAliases[];
extern FileDevice gDevices[];
extern int gDeviceCount;
extern List gLoadedFiles;
extern int gUnk943988;
extern char gExpandedPath[];

/* Expands an alias at the start of path into its index'th path. Returns the
   next index to try, or 0 when this was the last one. */
int File_ResolvePath(char *out, const char *path, int index)
{
    int i;

    for (i = 0; i < FILE_ALIAS_MAX; i++) {
        FileAlias *a = &gAliases[i];
        const char *p;
        char *o = out;
        size_t len;
        int next, n;

        if (!a->device)
            continue;
        len = strlen(a->name);
        if (crt_strncmp(path, a->name, len) != 0)
            continue;
        p = a->path;
        for (n = index; n; n--) {
            p = crt_strchr(p, ';');
            if (!p) {
                Error_Show("Error in alias path: %s", a->path);
                for (;;)
                    ;
            }
            p++;
        }
        next = crt_strchr(p, ';') ? index + 1 : 0;
        while (*p != ';' && *p)
            *o++ = *p++;
        strcpy(o, path + len);
        strcpy(gExpandedPath, out);
        return next;
    }
    strcpy(out, path);
    strcpy(gExpandedPath, path);
    return 0;
}
HOOK(0x4B1020, File_ResolvePath);

int Device_New(void)
{
    int i = gDeviceCount;

    gDevices[i].id = i;
    gDeviceCount = i + 1;
    return i;
}
HOOK(0x4B1180, Device_New);

int Device_Find(const char *path)
{
    int i;

    for (i = 1; i < FILE_DEVICE_MAX; i++) {
        FileDevice *d = &gDevices[i];

        if (d->present && crt_strncmp(d->name, path, strlen(d->name)) == 0)
            return i;
    }
    return 0;
}
HOOK(0x4B11A0, Device_Find);

void File_Init(void)
{
    Alias_Clear();
    gDeviceCount = 1;
    memset(gDevices, 0, sizeof(FileDevice) * FILE_DEVICE_MAX);
    gExpandedPath[0] = 0;
    LocalDevice_Register();
    gUnk943988 = 0;
    gLoadedFiles.head = (Node *)&gLoadedFiles.tail;
    gLoadedFiles.tailpred = (Node *)&gLoadedFiles;
    gLoadedFiles.tail = NULL;
}
HOOK(0x4B1200, File_Init);

void File_Shutdown(void)
{
    Node *n = gLoadedFiles.head, *next;

    for (next = n->next; next; next = next->next) {
        Script_FreeRefBlock(n);
        n = next;
    }
    LocalDevice_Unregister();
}
HOOK(0x4B1260, File_Shutdown);

void Device_SetName(int device, const char *name)
{
    strcpy(gDevices[device].name, name);
}
HOOK(0x4B1290, Device_SetName);

void Alias_Clear(void)
{
    int i;

    for (i = 0; i < FILE_ALIAS_MAX; i++)
        gAliases[i].device = 0;
}
HOOK(0x4B12D0, Alias_Clear);

void Alias_Set(const char *name, const char *path)
{
    int device = Device_Find(path);
    int i;

    if (!device)
        return;
    Alias_Remove(name);
    for (i = 0; i < FILE_ALIAS_MAX; i++) {
        if (!gAliases[i].device) {
            strcpy(gAliases[i].name, name);
            strcpy(gAliases[i].path, path);
            gAliases[i].device = device;
            return;
        }
    }
}
HOOK(0x4B12F0, Alias_Set);

void Alias_Remove(const char *name)
{
    int i;

    for (i = 0; i < FILE_ALIAS_MAX; i++) {
        if (gAliases[i].device && strcmp(gAliases[i].name, name) == 0) {
            gAliases[i].device = 0;
            return;
        }
    }
}
HOOK(0x4B13A0, Alias_Remove);

/* Calls the device's directory scan for every path the alias expands to. */
void File_ScanDir(const char *path, void *fn, void *user)
{
    char buf[128];
    int next = File_ResolvePath(buf, path, 0);
    int device = Device_Find(buf);

    for (;;) {
        int i;

        if (device <= 0 || device >= FILE_DEVICE_MAX) {
            Error_Show("Illegal device name in path: \"%s\".", path);
            for (;;)
                ;
        }
        if (!gDevices[device].present) {
            Error_Show("Device not present: %s", Device_GetName(device));
            for (;;)
                ;
        }
        for (i = 0; buf[i] && buf[i] != ':'; i++)
            ;
        gDevices[device].scanDir(device, buf + i + 1, fn, user);
        if (!next)
            return;
        next = File_ResolvePath(buf, path, next);
        device = Device_Find(buf);
    }
}
HOOK(0x4B1420, File_ScanDir);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, 942330, 942330, 0x1280);
ALIAS(gAliases, pad_942330, 0);
BSS(94396C) int gDeviceCount;
BSS(94396C) List gLoadedFiles;
BSS(943988) int gUnk943988;
PAD(igb, 943A18, 943A18, 0x80);
ALIAS(gExpandedPath, pad_943A18, 0);
