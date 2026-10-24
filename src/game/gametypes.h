#ifndef GAME_GAMETYPES_H
#define GAME_GAMETYPES_H

#include "task/tasktypes.h"

/* The in-mission game task, made from the "Game" task type. */
typedef struct Game {
    Task task;
    void *world;
    void *models;
    int unk28;
    Random rand1;
    Random rand2;
    Task *root;
    Task *unk48;
    void *unk4C;
    int unk50;
    Task **ref54;
    int unk58;
    struct CameraTask *camera;
    char unk60[0x60];
    Task **players[4];
    int unkD0[4];
    char paused;
    char godMode;
    char unkE2[2];
    int unkE4;
    void *sprites;
    void *textures;
    int music[4];
    char unk100[0x58];
} Game;

#endif
