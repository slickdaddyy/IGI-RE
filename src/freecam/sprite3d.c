#pragma GCC optimize("excess-precision=standard")

#include "freecam/freecam.h"

/* Sprite3D: a camera facing sprite at a world position. */

typedef struct Sprite3D {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    float size[2];
    char unk70[0xC];
    int frame;
    void *sprite;
    int flags;
} Sprite3D;

_Static_assert(sizeof(Sprite3D) == 0x88, "Sprite3D");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, int align));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x497F20, void, TransContext_Project, (float *screen, float *depth, const Vec3d *pos));
ORIG(0x498040, void, TransContext_ScaleSize, (float *out, const float *in, float depth));
ORIG(0x499FF0, int, Float_Log2, (float f));
ORIG(0x4B51F0, void, QVertex_DrawQuad, (void *sprite, float x0, float y0, float x1, float y1, float x2, float y2,
                                        float x3, float y3, float r, float g, float b, int frame, int flags, int mip));

extern uint16_t gSprite3DType;
extern Pool gSprite3DPool;

void Sprite3D_Draw(Sprite3D *s)
{
    float screen[2], half[2], size[2], depth, x0, y0, x1, y1;

    size[0] = s->size[0];
    size[1] = s->size[1];
    TransContext_Project(screen, &depth, &s->pos);
    if (depth > kZero) {
        TransContext_ScaleSize(half, size, depth);
        y1 = half[1] + screen[1];
        x1 = half[0] + screen[0];
        x0 = screen[0] - half[0];
        y0 = screen[1] - half[1];
        QVertex_DrawQuad(s->sprite, x0, y0, x1, y0, x0, y1, x1, y1, 255.0f, 255.0f, 255.0f, s->frame, s->flags,
                         Float_Log2(depth));
    }
}
HOOK(0x531E10, Sprite3D_Draw);

void Sprite3D_Init(void)
{
    gSprite3DType = TaskType_New("Sprite3D", sizeof(Sprite3D), gDynCubeObjType, 0, 0);
    TaskType_SetHandler(gSprite3DType, 4, Sprite3D_Draw);
    Pool_Init(&gSprite3DPool, sizeof(Sprite3D), 100, 4);
}
HOOK(0x531EF0, Sprite3D_Init);

void Sprite3D_Shutdown(void)
{
    Pool_Free(&gSprite3DPool);
    TaskType_Free(gSprite3DType);
}
HOOK(0x531F40, Sprite3D_Shutdown);
