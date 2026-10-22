#include "hook.h"
#include "igi.h"

/* the alarm state of the previous tick (AlarmControl, 0x350 bytes) */
char AlarmControl_IsLastAlarm(char *ac)
{
    return ac[0x2A2];
}
HOOK(0x440C10, AlarmControl_IsLastAlarm);
