#ifndef TAGITEM_TAGITEM_H
#define TAGITEM_TAGITEM_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "editobj/editobjtypes.h"
#include "sound/soundtypes.h"

typedef void (*TaskFn)(void *task, void *arg);

/* TagItem (0x124), TagItemInt32 / Real32 (0x128), String16 (0x134), String256 (0x224) */
typedef struct TagItem {
    Task task;
    char name[0x100];
    void *paramType;
    union {
        int i;
        float f;
        char s[0x100];
    } v;
} TagItem;

_Static_assert(sizeof(TagItem) == 0x224, "TagItem");

#endif
