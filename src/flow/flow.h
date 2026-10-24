#ifndef FLOW_H
#define FLOW_H

#include "task/task.h"
#include "script/scripttypes.h"

enum {
    FLOW_EVENT_QUIT = 1,
    FLOW_EVENT_INTRO,
    FLOW_EVENT_MAINMENU,
    FLOW_EVENT_GAME,
    FLOW_EVENT_RESTART_GAME,
    FLOW_EVENT_LEAVE_GAME,
    FLOW_EVENT_MOVIE_A,
    FLOW_EVENT_MOVIE_B,
};

typedef struct Flow {
    Task task;
    void *unk20;
    void *unk24;
    int event;
    int request;
    int start_time;
    int frames;
    int renders;
    int fps;
    int ticks_since_render;
    char need_render;
    char unk45[3];
    Task *root;
    int time_adjust;
    int paused_time;
} Flow;

void Flow_Open(void);
void Flow_CreateHandler(Flow *flow);
void Flow_FocusChanged(int active);
void Flow_RunHandler(Flow *flow);
void Flow_DeleteHandler(Flow *flow);
ScriptValue *Flow_RequestEvent(ScriptValue *ret, void *unused, void *args);
void Flow_Close(void);
uint16_t Flow_GetType(void);
void Flow_New(Task *parent);
void FramesSet(int fps);
int Flow_GetFps(void);
Flow *Flow_Get(void);
void *Flow_GetUnk20(void);
void Flow_SetRequest(int event);
int Flow_GetFrames(void);
int Flow_GetRenders(void);
void Flow_PauseBegin(void);
void Flow_PauseEnd(void);
void Flow_CheckCD(void);

#endif
