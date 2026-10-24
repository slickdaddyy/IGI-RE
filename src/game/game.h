#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"

typedef struct GameNode {
    struct GameNode *next;
    struct GameNode *prev;
} GameNode;

typedef struct Task {
    GameNode node;
    GameNode *head;
    void *tail;
    GameNode *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)(void *task, void *arg);
typedef void (*TaskFn1)(void *task);

/* Same layout as the script engine's value. Bytes 4 and 20 are never set. */
typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *type;
    int unk14;
} ScriptValue;

/* The in-mission game task, made from the "Game" task type. */
typedef struct Game {
    Task task;
    void *world;
    void *models;
    int unk28;
    Random rand1;
    Random rand2;
    Task *unk44;
    Task *unk48;
    void *unk4C;
    int unk50;
    Task **ref54;
    int unk58;
    void *unk5C;
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

_Static_assert(offsetof(Game, players) == 0xC0, "Game.players");
_Static_assert(offsetof(Game, music) == 0xF0, "Game.music");
_Static_assert(sizeof(Game) == 0x158, "Game");

extern Game *gGame;
extern TaskFn gTaskHandlers[][0x180];
extern TaskFn1 gTaskHandlers1[][0x180];
extern GameNode *gTaskStack[];
extern int gTaskStackTop;
extern const char gScriptEmptyString[];
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];

/* what every script function in the game returns: the number 1 */
static inline ScriptValue *Game_ReturnOne(ScriptValue *out, int i, double d)
{
    out->i = i;
    out->d = d;
    out->type = gScriptEmptyString;
    return out;
}

void *Game_GetWorld(void);
Game *Game_Get(void);
Task *Game_GetPlayer(int i);
uint8_t Game_GetEvent13(void);
int Game_CheatKeyDown(int key);
int Game_CheatKeyPressed(int key);
long double Vec3d_PitchAngle(Vec3d v);
long double Vec3d_YawAngle(Vec3d v);

#endif
