#ifndef TASK_TASKVARS_H
#define TASK_TASKVARS_H

#include "task/tasktypes.h"

extern int gTaskHandlerChanges;
extern signed char gTaskSlotEvent[];
extern void (*gTaskHandlers[][TASK_MAX_TYPES])();
extern TaskType gTaskTypes[];
extern Task *gTaskStack[];
extern int gTaskStackTop;

#endif
