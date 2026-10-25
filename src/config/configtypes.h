#ifndef CONFIG_CONFIGTYPES_H
#define CONFIG_CONFIGTYPES_H

#define CONFIG_ACTION_COUNT 27
#define CONFIG_DEVICE_COUNT 3
#define CONFIG_KEY_COUNT 127

/* name/value pairs registered as script constants: actions, input devices, keys */
typedef struct ConfigConstant {
    char name[0x20];
    int value;
} ConfigConstant;

typedef struct ConfigDisplayMode {
    int width;
    int height;
    int depth;
} ConfigDisplayMode;

/* One render device found at startup and the screen modes it offers. */
typedef struct ConfigDevice {
    char name[0x80];
    char desc[0x80];
    ConfigDisplayMode modes[64];
    int mode_count;
} ConfigDevice;
_Static_assert(sizeof(ConfigDevice) == 0x404, "ConfigDevice");

typedef struct GraphicOptions {
    ConfigDisplayMode display;
    int perf_flags;
    char transparency;
    char unk11;
    char unk12;
    char device[0x100];
    char device_desc[0x100];
    char pad213;
    float gamma;
} GraphicOptions;

typedef struct SoundOptions {
    float fx_volume;
    float music_volume;
    float speech_volume;
    char fx;
    char music;
    char speech;
    char reverse_stereo;
} SoundOptions;

typedef struct InputRemap {
    int action;
    int device;
    int key;
} InputRemap;

/* one player of config.qsc, written back by Config_Write */
typedef struct PlayerProfile {
    int difficulty;
    int language;
    char blood;
    char pad9[3];
    GraphicOptions gfx;
    SoundOptions sound;
    InputRemap remaps[64];
    int remap_count;
    int invert_mouse;
    float mouse_sensitivity;
    char name[32];
    int active_mission;
} PlayerProfile;
_Static_assert(sizeof(PlayerProfile) == 0x564, "PlayerProfile");

#endif
