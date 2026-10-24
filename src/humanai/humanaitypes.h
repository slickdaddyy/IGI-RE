#ifndef HUMANAI_HUMANAITYPES_H
#define HUMANAI_HUMANAITYPES_H

#include "types.h"
#include "task/tasktypes.h"

/* a point of the walk queue */
typedef struct MoveItem {
    Node node;
    Vec3d pos;
    int door;
    int id;
} MoveItem;

/* an enemy or friend an AIDetection knows of */
typedef struct AISeen {
    Node node;
    Task **ref;
    char unkC[0x1C];
    char unk28;
    char visible;
    char valid;
    char unk2B[5];
} AISeen;
_Static_assert(sizeof(AISeen) == 0x30, "AISeen");

typedef struct AIDetection {
    char unk0[0x18];
    List enemies;       /* AISeen */
    List friends;
    char unk30[0x2400];
} AIDetection;
_Static_assert(sizeof(AIDetection) == 0x2430, "AIDetection");

/* a message to the AI around: a noise, a shot, a call */
typedef struct AIMessage {
    int type;
    int team;
    unsigned flags;
    float radius;
    Vec3d *pos;
    Task *from;
    Task *about;
} AIMessage;
_Static_assert(sizeof(AIMessage) == 0x1C, "AIMessage");

typedef struct NearQuery {
    Vec3d pos;
    double best;
    Task *found;
} NearQuery;

typedef struct HumanModel {
    char unk0[4];
    unsigned flags;
    char unk8[0x10];
    Vec3f bones[1];     /* 0x84 bytes apart */
    char unk24[0x288];
    float eye[3];
    float matrix[10];
} HumanModel;
_Static_assert(sizeof(HumanModel) == 0x2E0, "HumanModel");

/* the AIAction_Combat parameter block, after its list node */
typedef struct CombatData {
    List *events;
    int unk4;
    double dist;
    int unk10;
    int timer;
    int count;
    int frame;
    char first;
    char spoke;
} CombatData;

typedef struct AIParam {
    Node node;
    CombatData d;
} AIParam;

typedef struct AIEvent {
    Node node;
    int type;
    int team;
    Vec3d pos;
    Task **from;
    Task **about;
} AIEvent;
_Static_assert(sizeof(AIEvent) == 0x30, "AIEvent");

typedef struct AITarget {
    Node node;
    char used;
    char unk09[3];
    Task **ref;
    Task *humans[32];
    int states[32];
    Vec3d pos;
    int count;
    int removed;
} AITarget;
_Static_assert(sizeof(AITarget) == 0x130, "AITarget");

#endif
