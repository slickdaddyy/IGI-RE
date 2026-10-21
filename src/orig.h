#ifndef ORIG_H
#define ORIG_H

#include "igi.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AA670, void *, crt_malloc, (size_t size));
ORIG(0x4A6A50, void, crt_free, (void *p));

#endif
