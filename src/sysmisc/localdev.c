#include <string.h>
#include <windows.h>

#include "hook.h"
#include "igi.h"
#include "sysmisc/sysmisctypes.h"

/* The "LOCAL:" device: files of the host file system through the C
   runtime, 16 open files at a time. */

typedef struct File {
    int device;
    int unk04[5];
    char name[128];
} File;

typedef struct LocalFile {
    File file;
    void *fp;
    int error;          /* a read or write came up short */
    int unkA0;
    int mode;
    int used;
} LocalFile;

#define LOCAL_FILE_MAX 16

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A5350, void *, crt_fopen, (const char *name, const char *mode));
ORIG(0x4A5234, int, crt_fclose, (void *fp));
ORIG(0x516C91, int, crt_fread, (void *buf, int size, int count, void *fp));
ORIG(0x516D79, int, crt_fwrite, (const void *buf, int size, int count, void *fp));
ORIG(0x4A52A4, int, crt_fseek, (void *fp, long offset, int whence));
ORIG(0x4A6E17, long, crt_ftell, (void *fp));
ORIG(0x516C67, int, crt_remove, (const char *path));
ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4B1180, int, Device_New, (void));
ORIG(0x4B11A0, int, Device_Find, (const char *path));
ORIG(0x4B1290, void, Device_SetName, (int dev, const char *name));
ORIG(0x4B1B90, int, Device_Register, (const char *name, int (*init)(const char *name)));
ORIG(0x4B1BC0, void, Device_Shutdown, (int dev));
ORIG(0x4B1BE0, void, Device_SetInit, (int dev, void *fn));
ORIG(0x4B1C00, void, Device_SetShutdown, (int dev, void *fn));
ORIG(0x4B1C20, void, Device_SetOpen, (int dev, void *fn));
ORIG(0x4B1C40, void, Device_SetClose, (int dev, void *fn));
ORIG(0x4B1C60, void, Device_SetRead, (int dev, void *fn));
ORIG(0x4B1C80, void, Device_SetWrite, (int dev, void *fn));
ORIG(0x4B1CA0, void, Device_SetSeek, (int dev, void *fn));
ORIG(0x4B1CC0, void, Device_SetTell, (int dev, void *fn));
ORIG(0x4B1CE0, void, Device_SetSize, (int dev, void *fn));
ORIG(0x4B1D00, void, Device_SetError, (int dev, void *fn));
ORIG(0x4B1D20, void, Device_SetUnk2C, (int dev, void *fn));
ORIG(0x4B1D40, void, Device_SetEof, (int dev, void *fn));
ORIG(0x4B1D60, void, Device_SetUnk34, (int dev, void *fn));
ORIG(0x4B1D80, void, Device_SetDelete, (int dev, void *fn));
ORIG(0x4B1DA0, void, Device_SetEnumDir, (int dev, void *fn));
ORIG(0x4B1DC0, void, Device_SetGetTime, (int dev, void *fn));
ORIG(0x4B1DE0, const char *, Device_GetName, (int dev));

extern LocalFile gLocalFiles[];

int LocalDevice_Init(const char *name);
void LocalDevice_EnumDir(int dev, const char *path, void (*fn)(DirEntry *e, void *user), void *user);
int LocalDevice_Delete(int dev, const char *path);
LocalFile *LocalDevice_Open(int dev, const char *path, int mode);
int LocalDevice_Close(int dev, LocalFile *f);
int LocalDevice_Read(int dev, LocalFile *f, void *buf, int size);
int LocalDevice_Write(int dev, LocalFile *f, const void *buf, int size);
int LocalDevice_Seek(int dev, LocalFile *f, long offset, int whence);
int LocalDevice_Size(int dev, LocalFile *f);
int LocalDevice_Tell(int dev, LocalFile *f);
int LocalDevice_Eof(int dev, LocalFile *f);
int LocalDevice_GetUnk2C(int dev, LocalFile *f);
int LocalDevice_GetError(int dev, LocalFile *f);
int LocalDevice_Unk34(int dev, LocalFile *f, int arg);
int LocalDevice_GetTime(int dev, FILETIME *time, const char *path);

void LocalDevice_Register(void)
{
    Device_Register("LOCAL:", LocalDevice_Init);
}
HOOK(0x5098B0, LocalDevice_Register);

void LocalDevice_Unregister(void)
{
    Device_Shutdown(Device_Find("LOCAL:"));
}
HOOK(0x5098D0, LocalDevice_Unregister);

LONG File_CompareTime(const FILETIME *a, const FILETIME *b)
{
    return CompareFileTime(a, b);
}
HOOK(0x5098F0, File_CompareTime);

int LocalDevice_Init(const char *name)
{
    int dev = Device_New();
    int i;

    Device_SetInit(dev, LocalDevice_Init);
    Device_SetShutdown(dev, Debug_Log);
    Device_SetOpen(dev, LocalDevice_Open);
    Device_SetClose(dev, LocalDevice_Close);
    Device_SetRead(dev, LocalDevice_Read);
    Device_SetWrite(dev, LocalDevice_Write);
    Device_SetSeek(dev, LocalDevice_Seek);
    Device_SetTell(dev, LocalDevice_Tell);
    Device_SetSize(dev, LocalDevice_Size);
    Device_SetEof(dev, LocalDevice_Eof);
    Device_SetUnk2C(dev, LocalDevice_GetUnk2C);
    Device_SetError(dev, LocalDevice_GetError);
    Device_SetUnk34(dev, LocalDevice_Unk34);
    Device_SetEnumDir(dev, LocalDevice_EnumDir);
    Device_SetGetTime(dev, LocalDevice_GetTime);
    Device_SetDelete(dev, LocalDevice_Delete);
    Device_SetName(dev, name);
    for (i = 0; i < LOCAL_FILE_MAX; i++)
        gLocalFiles[i].used = 0;
    return dev;
}
HOOK(0x509F50, LocalDevice_Init);

int LocalDevice_Delete(int dev, const char *path)
{
    return crt_remove(path);
}
HOOK(0x50A040, LocalDevice_Delete);

/* Calls fn for every file in the directory path except names starting
   with '.'; the entry name is the full path with the device prefix. */
void LocalDevice_EnumDir(int dev, const char *path, void (*fn)(DirEntry *e, void *user), void *user)
{
    char name[260];
    char pattern[260];
    WIN32_FIND_DATAA fd;
    DirEntry entry;
    const char *sep;
    HANDLE h;

    strcpy(pattern, path);
    if (path[strlen(path) - 1] == '/') {
        sep = "";
    } else {
        strcat(pattern, "/");
        sep = "/";
    }
    strcat(pattern, "*.*");
    h = FindFirstFileA(pattern, &fd);
    entry.name = name;
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        strcpy(name, Device_GetName(dev));
        strcat(name, path);
        strcat(name, sep);
        strcat(name, fd.cFileName);
        entry.size = fd.nFileSizeLow;
        entry.isDir = (fd.dwFileAttributes >> 4) & 1;
        if (fd.cFileName[0] != '.')
            fn(&entry, user);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
HOOK(0x50A050, LocalDevice_EnumDir);

LocalFile *LocalDevice_Open(int dev, const char *path, int mode)
{
    LocalFile *f = NULL;
    const char *how;
    void *fp;
    int i;

    for (i = 0; i < LOCAL_FILE_MAX; i++) {
        if (!gLocalFiles[i].used) {
            f = &gLocalFiles[i];
            break;
        }
    }
    /* with no free record the path buffer itself is used as one */
    if (i == LOCAL_FILE_MAX)
        f = (LocalFile *)path;
    if (!f)
        return NULL;
    switch (mode) {
    case 1:
        how = "rb";
        break;
    case 2:
        how = "wb";
        break;
    case 3:
        how = "w+b";
        break;
    default:
        Error_Show("Illegal openmode (%X) for file %s.", mode, path);
        for (;;)
            ;
    }
    fp = crt_fopen(path, how);
    if (!fp)
        return NULL;
    f->fp = fp;
    f->mode = mode;
    f->used = 1;
    f->error = 0;
    f->unkA0 = 0;
    return f;
}
HOOK(0x50A370, LocalDevice_Open);

int LocalDevice_Close(int dev, LocalFile *f)
{
    f->used = 0;
    return crt_fclose(f->fp);
}
HOOK(0x50A430, LocalDevice_Close);

int LocalDevice_Read(int dev, LocalFile *f, void *buf, int size)
{
    int n = crt_fread(buf, 1, size, f->fp);

    if (n < size)
        f->error = 1;
    return n;
}
HOOK(0x50A450, LocalDevice_Read);

int LocalDevice_Write(int dev, LocalFile *f, const void *buf, int size)
{
    int n = crt_fwrite(buf, 1, size, f->fp);

    if (n < size)
        f->error = 1;
    return n;
}
HOOK(0x50A490, LocalDevice_Write);

int LocalDevice_Seek(int dev, LocalFile *f, long offset, int whence)
{
    int how;

    switch (whence) {
    case 0:
        how = SEEK_SET;
        break;
    case 1:
        how = SEEK_CUR;
        break;
    case 2:
        how = SEEK_END;
        break;
    default:
        return whence - 2;  /* what the original leaves in eax */
    }
    return crt_fseek(f->fp, offset, how);
}
HOOK(0x50A4D0, LocalDevice_Seek);

int LocalDevice_Size(int dev, LocalFile *f)
{
    void *fp = f->fp;
    long pos = crt_ftell(fp);
    long size;

    crt_fseek(fp, 0, SEEK_END);
    size = crt_ftell(fp);
    crt_fseek(fp, pos, SEEK_SET);
    return size;
}
HOOK(0x50A510, LocalDevice_Size);

int LocalDevice_Tell(int dev, LocalFile *f)
{
    return crt_ftell(f->fp);
}
HOOK(0x50A550, LocalDevice_Tell);

int LocalDevice_Eof(int dev, LocalFile *f)
{
    int pos = LocalDevice_Tell(dev, f);

    return pos >= LocalDevice_Size(dev, f);
}
HOOK(0x50A570, LocalDevice_Eof);

int LocalDevice_GetUnk2C(int dev, LocalFile *f)
{
    return f->unkA0;
}
HOOK(0x50A5A0, LocalDevice_GetUnk2C);

int LocalDevice_GetError(int dev, LocalFile *f)
{
    return f->error;
}
HOOK(0x50A5B0, LocalDevice_GetError);

int LocalDevice_Unk34(int dev, LocalFile *f, int arg)
{
    return 0;
}
HOOK(0x50A5C0, LocalDevice_Unk34);

int LocalDevice_GetTime(int dev, FILETIME *time, const char *path)
{
    HANDLE h;

    time->dwLowDateTime = 0;
    time->dwHighDateTime = 0;
    h = CreateFileA(path, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    GetFileTime(h, NULL, NULL, time);
    CloseHandle(h);
    return 0;
}
HOOK(0x50A5D0, LocalDevice_GetTime);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, A7B640, A7B640, 0xAE8);
ALIAS(gLocalFiles, pad_A7B640, 0);
