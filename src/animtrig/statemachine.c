#include <string.h>

#include "hook.h"
#include "igi.h"

typedef struct StateMachine StateMachine;
typedef void (*StateFn)(StateMachine *sm, void *owner);

/* one step of a combo: mode 0 needs none of mask's bits, otherwise all of
   them; timeout is how many updates the step after it may take */
typedef struct ComboStep {
    int mode;
    uint8_t timeout;
    uint32_t mask;
} ComboStep;

typedef struct Combo {
    uint8_t length;
    ComboStep *steps;
} Combo;

typedef struct ComboProgress {
    uint8_t step;
    uint8_t timer;
} ComboProgress;

/* input() turns the controls into bits, then run() is called. Combos are
   input bit sequences; the last one completed this update is in combo. */
struct StateMachine {
    StateFn input;
    StateFn *states;
    uint32_t bits;
    int prev;
    int state;
    StateFn run;
    int comboCount;
    int combo;
    Combo *combos;
    ComboProgress progress[20];
};

_Static_assert(sizeof(StateMachine) == 0x4C, "StateMachine");

void StateMachine_Init(StateMachine *sm, StateFn input, StateFn *states, int state, int comboCount, Combo *combos)
{
    sm->input = input;
    sm->states = states;
    sm->bits = 0;
    sm->prev = -1;
    sm->state = state;
    sm->run = states[state];
    sm->comboCount = comboCount;
    sm->combos = combos;
    sm->combo = -1;
    memset(sm->progress, 0, sizeof(sm->progress));
}
HOOK(0x4EC190, StateMachine_Init);

/* the combos look at the bits from before input() runs */
void StateMachine_Update(StateMachine *sm, void *owner)
{
    uint32_t bits = sm->bits;
    int count = sm->comboCount;
    int i;

    sm->input(sm, owner);
    sm->combo = -1;
    for (i = 0; i < count; i++) {
        ComboProgress *p = &sm->progress[i];
        Combo *c = &sm->combos[i];
        ComboStep *s = &c->steps[p->step];
        int ok = s->mode ? (s->mask & bits) == s->mask : (s->mask & bits) == 0;

        if (ok) {
            p->step++;
            if (p->step == c->length) {
                sm->combo = i;
                p->step = 0;
                p->timer = 0;
            } else {
                p->timer = c->steps[p->step].timeout;
            }
        } else if (p->step && --p->timer == 0) {
            p->step = 0;
            p->timer = 0;
        }
    }
    sm->run(sm, owner);
}
HOOK(0x4EC1E0, StateMachine_Update);
