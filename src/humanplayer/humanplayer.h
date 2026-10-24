#ifndef HUMANPLAYER_H
#define HUMANPLAYER_H

#include <stddef.h>
#include <stdint.h>

#include "task/tasktypes.h"
#include "script/scripttypes.h"

typedef struct StateMachine StateMachine;
typedef struct HumanPlayer HumanPlayer;
typedef void (*StateFn)(StateMachine *sm, HumanPlayer *h);

/* State machine used for the player's body and weapon. update() turns the
   controls into input bits, then the current state's run function is called. */
struct StateMachine {
    StateFn update;
    StateFn *states;
    uint32_t input;
    int prev;
    int state;
    StateFn run;
    int combo_count;
    int combo;
    void *combos;
    uint8_t combo_progress[0x28];
};

enum {
    SM_ANIM_DONE = 0x1,
    SM_ANIM_EVENT = 0x2,
    SM_HIT = 0x10000,
    SM_DEAD = 0x20000,
};

/* controls, indices into HumanPlayer.input */
enum {
    INPUT_LEFT = 2,
    INPUT_RIGHT = 3,
    INPUT_FORWARD = 4,
    INPUT_BACK = 5,
    INPUT_FIRE = 6,
    INPUT_ALT_FIRE = 7,
    INPUT_RELOAD = 8,
    INPUT_USE = 9,
    INPUT_LEAN = 12,
    INPUT_WEAPON = 13,
    INPUT_TOGGLE_D45 = 20,
    INPUT_NEXT_WEAPON = 21,
    INPUT_PREV_WEAPON = 22,
    INPUT_MAP = 32,
    INPUT_FLY = 33,
    INPUT_BINOCULARS = 34,
    INPUT_COUNT = 40,
};

typedef struct HumanMotion {
    int unk0;
    uint32_t flags;
    char unk8[0x44];
    int unk4C[5];
} HumanMotion;

typedef struct HumanView {
    char unk0[0x68];
    char anim[8];
    HumanMotion *motion;
    char unk74[0xC4];
    char unk138;
    char unk139[0x63];
    float lean[3];
    float lean_speed[3];
    char unk1B4[0x54];
    int unk208[7];
} HumanView;

typedef struct WeaponInfo {
    int id;
    char unk4[0x60C];
    int kind;
    char unk614[8];
    char unk61C[0x28];
    int unk644;
    int clip_size;
    char unk64C[0x30];
    int anim67C;
    int unk680;
    int anim684;
    int anim688;
    int anim68C;
    char unk690[0x54];
    float *heal;
} WeaponInfo;

typedef struct WeaponAmmo {
    int clip;
    int reserve;
} WeaponAmmo;

typedef struct AmmoEntry {
    int id;
    int count;
} AmmoEntry;

typedef struct HitAnims {
    int unk0;
    int hit;
    int death[2];
} HitAnims;

struct HumanPlayer {
    Task task;
    double pos[3];
    char unk38[0x30];
    char anim[8];
    HumanMotion *motion;
    char unk74[4];
    float matrix[9];
    char unk9C[4];
    int unkA0;
    char unkA4[0x28];
    int unkCC;
    char unkD0[8];
    char unkD8;
    char unkD9[0x37];
    float prev_input[INPUT_COUNT];
    float input[INPUT_COUNT];
    char input_locked;
    char unk251[3];
    float damage;
    float max_damage;
    char unk25C[0x8C];
    int hit_zone;
    StateMachine body;
    void *anim_slots;
    char unk33C;
    char crouch;
    char unk33E[2];
    int ammo[0x61];
    Task *weapons[3];
    char unk4D0;
    char fire;
    char unk4D2;
    char unk4D3;
    char unk4D4[4];
    char grenade_ticking;
    char unk4D9[3];
    int grenade_fuse;
    int grenade_ticks;
    int grenade_sound;
    int melee_count;
    HumanView *view;
    char unk4F0[0x24];
    char unk514[0xC];
    float delta[3];
    char delta_scaled;
    char unk52D[0x123];
    int unk650[5];
    float unk664[3];
    float unk670[3];
    char unk67C[0xC];
    float unk688[3];
    char unk694;
    char unk695;
    char unk696;
    char unk697;
    char unk698;
    char unk699[0x77];
    double ground_distance;
    float ground_angle;
    char unk71C[0xC];
    float move_pos[3];
    char unk734[0x28];
    float move_angle[3];
    char unk768[4];
    Task *attach;
    char unk770[0x49];
    char unk7B9;
    char unk7BA[0x53C];
    char unkCF6;
    char unkCF7;
    StateMachine weapon;
    char weapon_lock;
    char unkD45;
    char unkD46[2];
    int cycle_index;
    int next_weapon;
    int unkD50;
    int unkD54;
    int weapon_anim;
    int weapon_anim_req;
    char unkD60[8];
    char unkD68;
    char unkD69[3];
    int medipack_delay;
    char unkD70[8];
    char unkD78;
    char unkD79;
    char unkD7A;
    char unkD7B[0x11];
    int unkD8C;
    char unkD90;
    char unkD91;
    char unkD92;
    char unkD93;
    char unkD94;
    char unkD95[7];
    int unkD9C;
    char unkDA0;
    char unkDA1;
    char unkDA2[0x1A];
    Task **unkDBC;
    int unkDC0[9];
    Task *saved_weapon;
    Task **unkDE8;
    char unkDEC[4];
};

_Static_assert(offsetof(HumanPlayer, motion) == 0x70, "motion");
_Static_assert(offsetof(HumanPlayer, prev_input) == 0x110, "prev_input");
_Static_assert(offsetof(HumanPlayer, input) == 0x1B0, "input");
_Static_assert(offsetof(HumanPlayer, damage) == 0x254, "damage");
_Static_assert(offsetof(HumanPlayer, hit_zone) == 0x2E8, "hit_zone");
_Static_assert(offsetof(HumanPlayer, body) == 0x2EC, "body");
_Static_assert(offsetof(HumanPlayer, crouch) == 0x33D, "crouch");
_Static_assert(offsetof(HumanPlayer, weapons) == 0x4C4, "weapons");
_Static_assert(offsetof(HumanPlayer, melee_count) == 0x4E8, "melee_count");
_Static_assert(offsetof(HumanPlayer, delta) == 0x520, "delta");
_Static_assert(offsetof(HumanPlayer, unk650) == 0x650, "unk650");
_Static_assert(offsetof(HumanPlayer, unk698) == 0x698, "unk698");
_Static_assert(offsetof(HumanPlayer, ground_distance) == 0x710, "ground_distance");
_Static_assert(offsetof(HumanPlayer, move_pos) == 0x728, "move_pos");
_Static_assert(offsetof(HumanPlayer, move_angle) == 0x75C, "move_angle");
_Static_assert(offsetof(HumanPlayer, attach) == 0x76C, "attach");
_Static_assert(offsetof(HumanPlayer, unk7B9) == 0x7B9, "unk7B9");
_Static_assert(offsetof(HumanPlayer, unkCF6) == 0xCF6, "unkCF6");
_Static_assert(offsetof(HumanPlayer, weapon) == 0xCF8, "weapon");
_Static_assert(offsetof(HumanPlayer, weapon_lock) == 0xD44, "weapon_lock");
_Static_assert(offsetof(HumanPlayer, medipack_delay) == 0xD6C, "medipack_delay");
_Static_assert(offsetof(HumanPlayer, unkD8C) == 0xD8C, "unkD8C");
_Static_assert(offsetof(HumanPlayer, unkD9C) == 0xD9C, "unkD9C");
_Static_assert(offsetof(HumanPlayer, unkDBC) == 0xDBC, "unkDBC");
_Static_assert(offsetof(HumanPlayer, saved_weapon) == 0xDE4, "saved_weapon");
_Static_assert(sizeof(HumanPlayer) == 0xDF0, "HumanPlayer size");
_Static_assert(offsetof(HumanView, lean) == 0x19C, "lean");
_Static_assert(offsetof(HumanView, unk208) == 0x208, "unk208");
_Static_assert(offsetof(WeaponInfo, kind) == 0x610, "kind");
_Static_assert(offsetof(WeaponInfo, clip_size) == 0x648, "clip_size");
_Static_assert(offsetof(WeaponInfo, anim67C) == 0x67C, "anim67C");
_Static_assert(offsetof(WeaponInfo, heal) == 0x6E4, "heal");

#endif
