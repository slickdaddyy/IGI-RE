#ifndef HELI_HELITYPES_H
#define HELI_HELITYPES_H

#include "types.h"
#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "humanai/humanaitypes.h"
#include "vehbase/vehbasetypes.h"

typedef struct CarContact {
    char active;
    char hit;
    char pad[2];
    Vec3f normal;       /* 0x04 */
    Vec3f pos;          /* 0x10 */
} CarContact;

typedef struct Car {
    Vehicle v;
    Task *engine_sound; /* 0x748 */
    float *sound_params; /* 0x74C */
    int wheel_slots;    /* 0x750 */
    int steered_count;  /* 0x754 */
    int driven_count;   /* 0x758 */
    CarContact contacts[16]; /* 0x75C */
    char exploding;     /* 0x91C */
    char unk91D;
    char pad91E[2];
    int explode_timer;
    int last_tick;
    VarString vars[3];  /* 0x928 */
    Vec3d unkA18;
    Matrix unkA30;
    double light_cfg[3]; /* 0xA58 */
    char unkA70[0x18];
    char unkA88;
    char unkA89[7];
    AIDetection light;  /* 0xA90 */
    float light_params[3]; /* 0x2EC0: two angles in degrees, a distance */
    char unk2ECC;
    char pad2ECD[3];
    Task *attacker;     /* 0x2ED0 */
    char registered;    /* 0x2ED4 */
    char exploded;      /* 0x2ED5 */
    char pad2ED6[2];
} Car;
_Static_assert(sizeof(Car) == 0x2ED8, "Car");

/* the extra config of "Heli" */
typedef struct HeliConfig {
    VehicleConfig v;
    int count1;         /* 0x118 */
    int list[13];       /* 0x11C, the second list (explode points) starts at list[3] */
    int count2;         /* 0x150 */
} HeliConfig;

typedef struct Heli {
    Vehicle v;
    float *sound_params; /* 0x748 */
    Task **rotor_sound; /* 0x74C */
    char exploding;     /* 0x750 */
    char lights;        /* 0x751 */
    char pad752[2];
    int explode_timer;  /* 0x754 */
    VarString rotor_expr; /* 0x758 */
    VarString light_expr; /* 0x7A8 */
    VarString engine_expr; /* 0x7F8 */
    Vec3d light_pos;    /* 0x848 */
    Matrix light_matrix; /* 0x860 */
    double light_cfg[3]; /* 0x888 */
    char unk8A0[0x18];
    char unk8B8;
    char pad8B9[7];
    AIDetection light;  /* 0x8C0 */
    float light_params[3]; /* 0x2CF0 */
    char unk2CFC[4];
} Heli;
_Static_assert(sizeof(Heli) == 0x2D00, "Heli");

#endif
