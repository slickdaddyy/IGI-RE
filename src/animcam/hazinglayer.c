/* The first handler of EditHazinglayer, a Position subtype. */
#include "hook.h"
#include "animcam/animcam.h"

/* "EditHazinglayer" (0x88 bytes) */
typedef struct EditHazinglayer {
    Task task;
    int unk20;
    int unk24;
    Vec3d pos;          /* 0x28 */
    double unk40[6];
    int unk70;          /* 0x70 */
    float unk74;        /* 0x74 */
    int unk78[2];
    int unk80[2];
} EditHazinglayer;

_Static_assert(sizeof(EditHazinglayer) == 0x88, "EditHazinglayer");

void EditHazinglayer_Create(EditHazinglayer *h)
{
    int i;

    h->pos.x = h->pos.y = h->pos.z = 0.0;
    for (i = 0; i < 6; i++)
        h->unk40[i] = 0.0;
    h->unk78[0] = h->unk78[1] = 0;
    h->unk80[0] = h->unk80[1] = 0;
    h->unk74 = 100.0f;
    h->unk70 = 0;
    h->unk20 = 0;
}
HOOK(0x52AFE0, EditHazinglayer_Create);
