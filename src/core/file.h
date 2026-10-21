#ifndef FILE_H
#define FILE_H

#define FILE_ALIAS_MAX 32
#define FILE_DEVICE_MAX 5

/* "MISSION:" -> "LOCAL:missions/location0/level1/", several paths split by ';' */
typedef struct FileAlias {
    char name[16];
    char path[0x80];
    int device;
} FileAlias;

/* a file system driver such as "LOCAL:", slot 0 is never used */
typedef struct FileDevice {
    int (*init)(const char *name);
    void (*shutdown)(int device);
    void *(*open)(int device, const char *path, int mode);
    void *ops[12];
    void (*scanDir)(int device, const char *path, void *fn, void *user);
    void *ops2[5];
    int id;
    int present;
    int unk5C[8];
    char name[16];
} FileDevice;

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
