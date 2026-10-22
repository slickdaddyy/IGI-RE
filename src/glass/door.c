#include "hook.h"
#include "igi.h"

/* flips the open flag of a Door (0x290) */
void Door_Toggle(char *door)
{
    door[0x290] ^= 1;
}
HOOK(0x447DA0, Door_Toggle);
