#include "hook.h"
#include "editcam/editcam.h"

ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));

extern uint8_t gControllerEvent;

/* the state a controller handler fills each update */
typedef struct Controller {
    void (*handler)(struct Controller *c, void *arg);
    float x;
    float y;
    float throttle;
    float rudder;
    uint32_t pressed;   /* bits that went down this update */
    uint32_t last;
    uint32_t bits;
    uint16_t held[32];  /* updates each bit has been down */
} Controller;

_Static_assert(sizeof(Controller) == 0x60, "Controller");

void Controller_Init(void)
{
    gControllerEvent = TaskEvent_Alloc(0);
}
HOOK(0x507E70, Controller_Init);

void Controller_Shutdown(void)
{
    TaskEvent_Free(gControllerEvent);
}
HOOK(0x507E80, Controller_Shutdown);

void Controller_Setup(Controller *c, void (*handler)(Controller *c, void *arg))
{
    int i;

    c->handler = handler;
    c->x = 0;
    c->y = 0;
    c->throttle = 0;
    c->rudder = 0;
    c->last = 0;
    c->bits = 0;
    c->pressed = 0;
    for (i = 0; i < 16; i++)
        ((uint32_t *)c->held)[i] = 0;
}
HOOK(0x507E90, Controller_Setup);

void Controller_Update(Controller *c, void *arg)
{
    uint32_t bits;
    int i;

    c->last = c->bits;
    if (c->handler)
        c->handler(c, arg);
    bits = c->bits;
    c->pressed = ~c->last & bits;
    for (i = 0; i < 32; i++) {
        if (bits & (1u << i))
            c->held[i]++;
        else
            c->held[i] = 0;
    }
}
HOOK(0x507EC0, Controller_Update);
