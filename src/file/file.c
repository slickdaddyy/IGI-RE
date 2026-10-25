#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "file/file.h"

_Static_assert(sizeof(Device) == 0x8C, "Device size");
_Static_assert(sizeof(LoadedFile) == 0x1C, "LoadedFile size");

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4AF8F0, void, List_AddTail, (void *list, void *node));
ORIG(0x4BADA0, void, Script_FreeRefBlock, (void *node));
ORIG(0x4B1020, int, File_ResolvePath, (char *out, const char *path, int index));
ORIG(0x4B11A0, int, Device_Find, (const char *path));

extern Device gDevices[];
extern char gOpenPath[];
extern char gDeletePath[];
extern File gNullFile[];
extern char gExpandedPath[];
extern LoadedFile *gLoadedFiles;
extern const char *gModeNames[];
extern const int gModeValues[];

/* The part of an expanded path after "DEVICE:". */
static char *AfterDevice(char *path)
{
    int i = 0;

    while (path[i] && path[i] != ':')
        i++;
    return path + i + 1;
}

static void CheckDevice(int dev, const char *path)
{
    if (dev < 1 || dev > 4) {
        Error_Show("Illegal device name in path: \"%s\".", path);
        for (;;)
            ;
    }
    if (!gDevices[dev].present) {
        Error_Show("Device not present: %s", Device_GetName(dev));
        for (;;)
            ;
    }
}

int File_ParseMode(const char *mode);

File *File_Open(const char *path, const char *mode)
{
    int m = File_ParseMode(mode);
    int next = File_ResolvePath(gOpenPath, path, 0);
    int dev = Device_Find(gOpenPath);
    File *f;

    for (;;) {
        CheckDevice(dev, path);
        f = gDevices[dev].open(dev, AfterDevice(gOpenPath), m);
        if (f)
            break;
        if (!next)
            return NULL;
        next = File_ResolvePath(gOpenPath, path, next);
        dev = Device_Find(gOpenPath);
    }
    f->device = dev;
    strcpy(f->name, path);
    return f;
}
HOOK(0x4B1510, File_Open);

int File_ParseMode(const char *mode)
{
    int i;

    for (i = 0; gModeNames[i]; i++)
        if (strcmp(mode, gModeNames[i]) == 0)
            return gModeValues[i];
    return 0;
}
HOOK(0x4B1620, File_ParseMode);

void File_Close(File *f)
{
    if (f == gNullFile) {
        gNullFile->unk04[1] = 0;
        return;
    }
    gDevices[f->device].close(f->device, f);
}
HOOK(0x4B1690, File_Close);

int File_Read(File *f, void *buf, int size)
{
    return gDevices[f->device].read(f->device, f, buf, size);
}
HOOK(0x4B16D0, File_Read);

int File_Write(File *f, const void *buf, int size)
{
    return gDevices[f->device].write(f->device, f, buf, size);
}
HOOK(0x4B1700, File_Write);

int File_Seek(File *f, int offset, int whence)
{
    return gDevices[f->device].seek(f->device, f, offset, whence);
}
HOOK(0x4B1730, File_Seek);

int File_Eof(File *f)
{
    return gDevices[f->device].eof(f->device, f);
}
HOOK(0x4B1760, File_Eof);

int File_GetSize(File *f)
{
    return gDevices[f->device].size(f->device, f);
}
HOOK(0x4B1780, File_GetSize);

int File_GetError(File *f)
{
    return gDevices[f->device].error(f->device, f);
}
HOOK(0x4B17A0, File_GetError);

int File_Flush(File *f)
{
    return gDevices[f->device].control(f->device, f, 0);
}
HOOK(0x4B17C0, File_Flush);

int File_Control(File *f, int arg)
{
    return gDevices[f->device].control(f->device, f, arg);
}
HOOK(0x4B17F0, File_Control);

int File_GetTime(void *filetime, const char *path)
{
    char buf[512];
    int next = 0;
    int dev;

    for (;;) {
        next = File_ResolvePath(buf, path, next);
        dev = Device_Find(buf);
        if (gDevices[dev].gettime(dev, filetime, AfterDevice(buf)) == 0)
            return 0;
        if (!next)
            return -1;
    }
}
HOOK(0x4B1820, File_GetTime);

LoadedFile *File_AddLoaded(const char *name);

void *File_Load(const char *path, int *size_out)
{
    File *f = File_Open(path, "r");
    LoadedFile *lf;
    void *data;
    int size;

    if (!f)
        return NULL;
    if (gDevices[f->device].load) {
        lf = File_AddLoaded(path);
        lf->device = f->device;
        data = gDevices[f->device].load(f->device, f, &size, lf);
        File_Close(f);
        if (size_out)
            *size_out = size;
        return data;
    }
    size = File_GetSize(f);
    data = Mem_Alloc(size, 4);
    lf = File_AddLoaded(path);
    lf->device = f->device;
    lf->data = data;
    lf->size = size;
    File_Read(f, data, size);
    File_Control(f, 0);
    if (!File_GetError(f)) {
        if (size_out)
            *size_out = size;
        File_Close(f);
        return data;
    }
    Script_FreeRefBlock(lf);
    Mem_Free(data);
    File_Close(f);
    if (size_out)
        *size_out = 0;
    return NULL;
}
HOOK(0x4B18B0, File_Load);

LoadedFile *File_AddLoaded(const char *name)
{
    LoadedFile *lf = Mem_Alloc(strlen(name) + 1 + sizeof(LoadedFile), 4);

    memset(lf, 0, sizeof(*lf));
    List_AddTail(&gLoadedFiles, lf);
    lf->name = lf->text;
    strcpy(lf->name, name);
    return lf;
}
HOOK(0x4B19C0, File_AddLoaded);

LoadedFile *File_FindLoaded(void *data);

void File_Unload(void *data)
{
    LoadedFile *lf = File_FindLoaded(data);

    if (gDevices[lf->device].unload) {
        gDevices[lf->device].unload(lf->device, lf);
        Script_FreeRefBlock(lf);
        return;
    }
    Mem_Free(data);
    Script_FreeRefBlock(lf);
}
HOOK(0x4B1A30, File_Unload);

LoadedFile *File_FindLoaded(void *data)
{
    LoadedFile *lf;

    for (lf = gLoadedFiles; lf->next; lf = lf->next)
        if (lf->data == data)
            return lf;
    Error_Show("No files loaded at address: %p", data);
    for (;;)
        ;
}
HOOK(0x4B1A90, File_FindLoaded);

void File_Delete(const char *path)
{
    int next = File_ResolvePath(gDeletePath, path, 0);
    int dev = Device_Find(gDeletePath);

    for (;;) {
        CheckDevice(dev, gDeletePath);
        if (gDevices[dev].remove)
            gDevices[dev].remove(dev, AfterDevice(gDeletePath));
        if (!next)
            return;
        next = File_ResolvePath(gDeletePath, path, next);
        dev = Device_Find(gDeletePath);
    }
}
HOOK(0x4B1AC0, File_Delete);

int Device_Register(const char *name, int (*init)(const char *name))
{
    int dev = init(name);

    if (dev)
        gDevices[dev].present = 1;
    return dev;
}
HOOK(0x4B1B90, Device_Register);

void Device_Shutdown(int dev)
{
    gDevices[dev].shutdown(dev);
}
HOOK(0x4B1BC0, Device_Shutdown);

void Device_SetInit(int dev, void *fn)
{
    *(void **)&gDevices[dev].init = fn;
}
HOOK(0x4B1BE0, Device_SetInit);

void Device_SetShutdown(int dev, void *fn)
{
    *(void **)&gDevices[dev].shutdown = fn;
}
HOOK(0x4B1C00, Device_SetShutdown);

void Device_SetOpen(int dev, void *fn)
{
    *(void **)&gDevices[dev].open = fn;
}
HOOK(0x4B1C20, Device_SetOpen);

void Device_SetClose(int dev, void *fn)
{
    *(void **)&gDevices[dev].close = fn;
}
HOOK(0x4B1C40, Device_SetClose);

void Device_SetRead(int dev, void *fn)
{
    *(void **)&gDevices[dev].read = fn;
}
HOOK(0x4B1C60, Device_SetRead);

void Device_SetWrite(int dev, void *fn)
{
    *(void **)&gDevices[dev].write = fn;
}
HOOK(0x4B1C80, Device_SetWrite);

void Device_SetSeek(int dev, void *fn)
{
    *(void **)&gDevices[dev].seek = fn;
}
HOOK(0x4B1CA0, Device_SetSeek);

void Device_SetTell(int dev, void *fn)
{
    *(void **)&gDevices[dev].tell = fn;
}
HOOK(0x4B1CC0, Device_SetTell);

void Device_SetSize(int dev, void *fn)
{
    *(void **)&gDevices[dev].size = fn;
}
HOOK(0x4B1CE0, Device_SetSize);

void Device_SetError(int dev, void *fn)
{
    *(void **)&gDevices[dev].error = fn;
}
HOOK(0x4B1D00, Device_SetError);

void Device_SetUnused(int dev, void *fn)
{
    *(void **)&gDevices[dev].unused = fn;
}
HOOK(0x4B1D20, Device_SetUnused);

void Device_SetEof(int dev, void *fn)
{
    *(void **)&gDevices[dev].eof = fn;
}
HOOK(0x4B1D40, Device_SetEof);

void Device_SetControl(int dev, void *fn)
{
    *(void **)&gDevices[dev].control = fn;
}
HOOK(0x4B1D60, Device_SetControl);

void Device_SetDelete(int dev, void *fn)
{
    *(void **)&gDevices[dev].remove = fn;
}
HOOK(0x4B1D80, Device_SetDelete);

void Device_SetEnumDir(int dev, void *fn)
{
    *(void **)&gDevices[dev].enumdir = fn;
}
HOOK(0x4B1DA0, Device_SetEnumDir);

void Device_SetGetTime(int dev, void *fn)
{
    *(void **)&gDevices[dev].gettime = fn;
}
HOOK(0x4B1DC0, Device_SetGetTime);

const char *Device_GetName(int dev)
{
    return gDevices[dev].name;
}
HOOK(0x4B1DE0, Device_GetName);

char *File_GetExpandedPath(void)
{
    return gExpandedPath;
}
HOOK(0x4B1FB0, File_GetExpandedPath);

int File_Exists(const char *path)
{
    File *f = File_Open(path, "r");

    if (f) {
        File_Close(f);
        return 1;
    }
    return 0;
}
HOOK(0x4B1FC0, File_Exists);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(str_538354);
REF_EXTERN(str_53B604);
REF_EXTERN(str_543A60);
REF_EXTERN(str_546F18);
STORE(igd, 534A70, 4) const int store_534A70[16] INIT({1, 3, 2, 3, 0, 1054086093, 1050852240, -1222130260, 925353388, 1065353300, 1413754139, 1073291771, -1305752457, 986255323, 0, 1055916032});
ALIAS(gModeValues, store_534A70, 0);
DATA(538354) char str_538354[2] INIT("w");
PAD(igd, 538354, 538356, 0x2);
DATA(53B604) char str_53B604[2] INIT("r");
PAD(igd, 53B604, 53B606, 0x2);
DATA(543A60) char str_543A60[3] INIT("w+");
PAD(igd, 543A60, 543A63, 0x1);
STORE(igd, 546F04, 4) const char *store_546F04[5] INIT({(__typeof__(store_546F04[0]))ref_str_53B604, (__typeof__(store_546F04[1]))ref_str_546F18, (__typeof__(store_546F04[2]))ref_str_538354, (__typeof__(store_546F04[3]))ref_str_543A60});
ALIAS(gModeNames, store_546F04, 0);
DATA(546F04) char str_546F18[3] INIT("r+");
PAD(igd, 546F04, 546F1B, 0x1);
STORE(igb, 9435B0, 1) char store_9435B0[256];
ALIAS(gOpenPath, store_9435B0, 0);
STORE(igb, 9435B0, 4) __typeof__(gDevices[0]) store_9436B0[5];
ALIAS(gDevices, store_9436B0, 0);
STORE(igb, 943980, 4) __typeof__(gNullFile[0]) store_943980[1];
ALIAS(gNullFile, store_943980, 0);
STORE(igb, 943A98, 1) char store_943A98[136];
ALIAS(gDeletePath, store_943A98, 0);
