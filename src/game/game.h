#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "script/scripttypes.h"
#include "game/gametypes.h"

typedef void (*TaskFn)(void *task, void *arg);
typedef void (*TaskFn1)(void *task);

extern Game *gGame;
extern TaskFn gTaskHandlers[][0x180];
extern TaskFn1 gTaskHandlers1[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern const char gScriptEmptyString[];
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];

/* what every script function in the game returns: the number 1 */
static inline ScriptValue *Game_ReturnOne(ScriptValue *out, int i, double d)
{
    out->i = i;
    out->d = d;
    out->s = gScriptEmptyString;
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
