#ifndef FILE_H
#define FILE_H

#include "file/filetypes.h"

#define FILE_ALIAS_MAX 32
#define FILE_DEVICE_MAX 5

/* "MISSION:" -> "LOCAL:missions/location0/level1/", several paths split by ';' */
typedef struct FileAlias {
    char name[16];
    char path[0x80];
    int device;
} FileAlias;

int File_ResolvePath(char *out, const char *path, int index);
int Device_New(void);
int Device_Find(const char *path);
void File_Init(void);
void File_Shutdown(void);
void Device_SetName(int device, const char *name);
void Alias_Clear(void);
void Alias_Set(const char *name, const char *path);
void Alias_Remove(const char *name);
void File_ScanDir(const char *path, void *fn, void *user);

#endif
