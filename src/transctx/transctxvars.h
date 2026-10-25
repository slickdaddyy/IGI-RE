#ifndef TRANSCTX_TRANSCTXVARS_H
#define TRANSCTX_TRANSCTXVARS_H

#include "transctx/transctxtypes.h"

extern float gMusicVolume;
extern float gMasterVolume;
extern float gSfxVolume;
extern int gVoiceUpdateRate;
extern int gTransContextAutoNear;
extern SoundList gSoundLists[];
extern int gVolumeTable[];
extern struct Voice *gActiveVoices;
extern struct Voice *gFreeVoices;
extern int gTransContextValid;
extern OffscreenBuffer gOffscreenTarget;
extern float gOffscreenClip[];
extern float gLayer0MinZ;
extern float gLayer0MaxZ;
extern float gLayer2MaxZ;
extern int gD3DZ24;
extern float gLayer2Far;
extern float gLayer2Near;
extern float gLayer0Far;
extern float gLayer0Near;
extern float gLayer2MinZ;
extern int gFlowFrames;

#endif
