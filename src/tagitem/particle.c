#include "hook.h"
#include "igi.h"

/* The last helpers of "Particle2DDynCubeObjTask" (made by 0x4FDF00). */

typedef struct Particle2D {
    double pos[3];
    char unk18[0x18];
    int level;
    char unk34[0x24];
} Particle2D;

typedef struct Particle2DTask {
    char unk0[0x6C];
    Particle2D **particles;
    char unk70[0x80];
    int count;
    char unkF4[9];
    char autoDelete;
} Particle2DTask;

_Static_assert(sizeof(Particle2D) == 0x58, "Particle2D");

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
        ps->particles[i]->level = level;
}
HOOK(0x4FE5E0, Particle2D_SetLevel);

void Particle2D_Translate(Particle2DTask *ps, const double *delta)
{
    int i;

    for (i = 0; i < ps->count; i++) {
        Particle2D *p = ps->particles[i];

        p->pos[0] = p->pos[0] + delta[0];
        p->pos[1] = delta[1] + p->pos[1];
        p->pos[2] = p->pos[2] + delta[2];
    }
}
HOOK(0x4FE610, Particle2D_Translate);
