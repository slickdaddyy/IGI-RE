#ifndef AMMOTYPE_AMMOTYPETYPES_H
#define AMMOTYPE_AMMOTYPETYPES_H

#include "task/tasktypes.h"

typedef struct Bullet {
    int used;
    int unk04;
    Vec3d pos;
    Vec3f dir;
    void *type;      /* 0x2C, WeaponType */
    int unk30;
    float speed;     /* 0x34 */
    int unk38;
    float pierce;    /* 0x3C, compared with the material's hardness */
    char ricocheted; /* 0x40 */
    char pierced;    /* 0x41, during the last step */
    char pad42[2];
    int unk44;
} Bullet;

typedef struct Trail {
    Vec3d a, b;
    int used;          /* 0x30 */
    int life;          /* 0x34 */
    float from, to;    /* 0x38, 0x3C: tracer part, divided by len */
    float len;         /* 0x40 */
    float v[3][3];     /* 0x44: corners relative to the first trail */
    int color[3][3];   /* 0x68 */
    float normal[3];   /* 0x8C */
    float uv[3][2];    /* 0x98 */
    float alpha[3];    /* 0xB0 */
    int poly;          /* 0xBC */
    int hasPoly;       /* 0xC0 */
    int unkC4;
} Trail;

/* one triangle handed to the poly object */
typedef struct ShotPoly {
    float *v[3];
    int *color[3];
    float uv[3][2];
    float alpha[3];
    float normal[3];
    int unk48;
    int unk4C;
} ShotPoly;

typedef struct PolyObj {
    Task task;
    char unk20[0x7C];
    int count; /* 0x9C */
} PolyObj;

typedef struct GunShot {
    Task task;
    Vec3d pos;              /* 0x20 */
    char unk38[0x30];
    int released;           /* 0x68 */
    Task **gun;             /* 0x6C */
    Task **ref70;           /* 0x70 */
    int unk74;
    Vec3d unk78;            /* 0x78 */
    int total;              /* 0x90 */
    int active;             /* 0x94 */
    int trailRate;          /* steps until the next trail */
    int trailCount;         /* 0x9C */
    int unkA0;
    Bullet *activeList[15]; /* 0xA4 */
    Bullet bullets[15];     /* 0xE0 */
    Trail trails[5];        /* 0x518 */
    Trail *trailList[5];    /* 0x900 */
    PolyObj *poly;          /* 0x914 */
    ShotPoly polys[10];     /* 0x918 */
} GunShot;
_Static_assert(sizeof(GunShot) == 0xC38, "GunShot");

typedef struct FireArgs {
    Task *weapon;
    int unk04;
    Vec3d *pos;
    int unk0C[2];
    Matrix *matrix;
    int unk18[2];
    char *type;         /* WeaponType */
    int unk24;
    int unk28;
    int unk2C;
} FireArgs;

typedef struct ReloadMsg {
    Task *h;
    int anim[7];
    char done;
    char unlimited;
} ReloadMsg;

typedef struct AmmoEntry {
    int id;
    int count;
    int limit; /* -1: none */
} AmmoEntry;

#endif
