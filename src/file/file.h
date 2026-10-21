#ifndef FILE_FILE_H
#define FILE_FILE_H

/* An open file. Each device allocates its own, larger, record. */
typedef struct File {
    int device;
    int unk04[5];
    char name[128];
} File;

/* A whole file loaded into memory by File_Load. */
typedef struct LoadedFile {
    struct LoadedFile *next;
    struct LoadedFile *prev;
    char *name;
    int device;
    int size;
    int unk14;
    void *data;
    char text[];
} LoadedFile;

/* A file system driver ("LOCAL:" is the only one in the game). */
typedef struct Device {
    int (*init)(const char *name);
    void (*shutdown)(int dev);
    File *(*open)(int dev, const char *path, int mode);
    void (*close)(int dev, File *f);
    int (*read)(int dev, File *f, void *buf, int size);
    int (*write)(int dev, File *f, const void *buf, int size);
    int (*seek)(int dev, File *f, int offset, int whence);
    int (*tell)(int dev, File *f);
    int (*size)(int dev, File *f);
    int (*eof)(int dev, File *f);
    void *unk28;
    int (*unk2C)(int dev, File *f);
    int (*error)(int dev, File *f);
    int (*unk34)(int dev, File *f, int arg);
    void *(*load)(int dev, File *f, int *size, LoadedFile *lf);
    void (*enumdir)(int dev, const char *path, void *fn, void *data);
    int (*gettime)(int dev, void *filetime, const char *path);
    int unk44[2];
    void (*remove)(int dev, const char *path);
    void (*unload)(int dev, LoadedFile *lf);
    int index;
    int present;
    char unk5C[32];
    char name[16];
} Device;

File *File_Open(const char *path, const char *mode);
void File_Close(File *f);
int File_Read(File *f, void *buf, int size);
int File_GetSize(File *f);
int File_GetError(File *f);
int sub_4B17F0(File *f, int arg);
const char *Device_GetName(int dev);

#endif
