#ifndef HUMANSTATE_HUMANSTATETYPES_H
#define HUMANSTATE_HUMANSTATETYPES_H

struct Human;

typedef struct HumanState {
    int unk0;
    void (**enter)(struct HumanState *s, struct Human *h);
    unsigned input;
    int prev;
    int state;
    void (*update)(struct HumanState *s, struct Human *h);
} HumanState;

#endif
