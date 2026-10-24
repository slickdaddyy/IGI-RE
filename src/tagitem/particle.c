#include <string.h>

#include "hook.h"
#include "igi.h"
#include "graphtask/graphtasktypes.h"

/* The last helpers of "Particle2DDynCubeObjTask" (made by 0x4FDF00). */

void Particle2D_SetAutoDelete(Particle2DTask *ps, char on)
{
    ps->autoDelete = on;
}
HOOK(0x4FE5D0, Particle2D_SetAutoDelete);

/* level is a float, copied as raw bits */
void Particle2D_SetLevel(Particle2DTask *ps, int level)
{
    int i;

    for (i = 0; i < ps->count; i++)
        memcpy(&ps->list[i]->level, &level, sizeof(level));
}
HOOK(0x4FE5E0, Particle2D_SetLevel);

void Particle2D_Translate(Particle2DTask *ps, const double *delta)
{
    int i;

    for (i = 0; i < ps->count; i++) {
        Particle2D *p = ps->list[i];

        p->pos.x = p->pos.x + delta[0];
        p->pos.y = delta[1] + p->pos.y;
        p->pos.z = p->pos.z + delta[2];
    }
}
HOOK(0x4FE610, Particle2D_Translate);
