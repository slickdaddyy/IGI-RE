#ifndef FREECAM_LIGHT_H
#define FREECAM_LIGHT_H

#include "freecam/freecam.h"

typedef struct PointLightObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    float color[3];
    float range;
    void *poolItem;
    int unk7C;
} PointLightObj;

void PointLightObj_Delete(void *world, PointLightObj *l);
void PointLightObj_SetRange(void *world, PointLightObj *l, const Vec3d *pos, float range);

#endif
