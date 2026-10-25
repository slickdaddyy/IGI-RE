#ifndef HUMANAI_HUMANAIVARS_H
#define HUMANAI_HUMANAIVARS_H

#include <stdint.h>

#include "humanai/humanaitypes.h"
#include "types.h"

extern uint16_t gHumanAIType;
extern const char *gAITypeNames[];
extern AITarget gAITargetPool[AI_TARGETS];
extern List gAITargets;
extern void *gAIScripts[];

#endif
