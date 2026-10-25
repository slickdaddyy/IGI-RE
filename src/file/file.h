#ifndef FILE_FILE_H
#define FILE_FILE_H

#include "file/filetypes.h"

File *File_Open(const char *path, const char *mode);
void File_Close(File *f);
int File_Read(File *f, void *buf, int size);
int File_GetSize(File *f);
int File_GetError(File *f);
int File_Control(File *f, int arg);
const char *Device_GetName(int dev);

#endif
