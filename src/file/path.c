#include <string.h>

#include "hook.h"
#include "igi.h"

ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));

/* Returns the part of path after "DEVICE:", optionally copying it to out. */
const char *Path_CopyNoDevice(char *out, int size, const char *path)
{
    int i;

    for (i = 0; path[i]; i++) {
        if (path[i] == ':') {
            if (out) {
                crt_strncpy(out, path + i + 1, size);
                out[size - 1] = 0;
            }
            return path + i + 1;
        }
    }
    if (out) {
        crt_strncpy(out, path, size);
        out[size - 1] = 0;
    }
    return path;
}
HOOK(0x4B1E00, Path_CopyNoDevice);

const char *Path_SkipDevice(const char *path)
{
    return Path_CopyNoDevice(NULL, 0, path);
}
HOOK(0x4B1E70, Path_SkipDevice);

/* The extension including the dot, or the whole path when there is none. */
const char *Path_GetExtension(char *out, int size, const char *path)
{
    int i = strlen(path);
    const char *ext;

    while (i && path[i] != '.')
        i--;
    ext = path + i;
    if (out) {
        crt_strncpy(out, ext, size - 1);
        out[size - 1] = 0;
    }
    return ext;
}
HOOK(0x4B1E90, Path_GetExtension);

char *Path_GetDir(char *out, int size, const char *path)
{
    char *slash = NULL;
    int i;

    for (i = 0; i < size && path[i]; i++) {
        if (path[i] == '\\' || path[i] == '/')
            slash = out + i;
        out[i] = path[i];
    }
    if (slash)
        *slash = 0;
    else
        *out = 0;
    return out;
}
HOOK(0x4B1EE0, Path_GetDir);

char *Path_StripExtension(char *out, int size, const char *path)
{
    int i = strlen(path);

    (void)size;
    for (; i >= 0; i--) {
        char c = path[i];

        if (c == '.') {
            out[i--] = 0;
            break;
        }
        if (c == '/' || c == '\\' || c == ':')
            break;
        out[i] = c;
    }
    for (; i >= 0; i--)
        out[i] = path[i];
    return out;
}
HOOK(0x4B1F30, Path_StripExtension);
