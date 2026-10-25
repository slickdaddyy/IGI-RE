#include "hook.h"
#include "igi.h"
#include "widget/widget.h"
#include "script/scripttypes.h"
#include "config/configtypes.h"
#include "win/wintypes.h"
#include "config/configvars.h"

ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4061B0, struct InputRemap *, Config_GetDefaultKeys, (void));
ORIG(0x4061C0, struct InputRemap *, Profile_GetKeys, (void));
ORIG(0x4061E0, int, Profile_GetKeyCount, (void));
ORIG(0x406240, void, Config_GetKey, (int i, int *value, const char **name));
ORIG(0x406270, int, Config_GetKeyCount, (void));
ORIG(0x406280, void, Config_GetAction, (int i, int *value, const char **name));
ORIG(0x4062B0, int, Config_GetActionCount, (void));
ORIG(0x418E40, Sprite *, MenuManager_FindFont, (void *mm, const char *name));
ORIG(0x418EA0, const char *, MenuManager_GetString, (void *mm, const char *name));
ORIG(0x418FC0, void, MenuManager_Pause, (void *mm));
ORIG(0x418FD0, void, MenuManager_Resume, (void *mm));
ORIG(0x4B4720, void, Random_Init, (Random *r));
ORIG(0x4B4740, unsigned int, Random_Next, (Random *r));
ORIG(0x4E7180, void, Sound_PlayName, (const char *name, int flags, void *params));
ORIG(0x48A8C0, void, Keys_Clear, (void));
ORIG(0x4B6E90, void, Text_SetString, (Text *t, const char *s));
ORIG(0x4B6EC0, Text *, Text_New, (Sprite *s, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (Text *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));

#define ROWS 64
#define ROW_HEIGHT 20
#define VISIBLE_ROWS 10

/* the key binding list: action names and the key bound to each, 10 rows shown */
typedef struct ControlsMenu {
    MenuWindow w;
    Text *names[ROWS];
    Text *keys[ROWS];
    int bindings[ROWS]; /* index into the config key table */
    int offset;         /* first row shown, the script variable <name>.nOffset */
    char registered;
    char waiting;       /* waiting for a key for the selected row */
    char reset;         /* <name>.isReset: load the default keys */
    char pad35B;
    int selected;
    int unk360;
} ControlsMenu;

_Static_assert(sizeof(ControlsMenu) == 0x364, "ControlsMenu");

extern uint16_t gControlsMenuType;
extern void *gControlsMenuParams;
extern Random gControlsRandom;
extern uint8_t gKeys[];
extern Mouse gMouse;
extern const float gHalf;

#define KEY_ESCAPE 1
#define KEY_LWIN 0xDB
#define KEY_RWIN 0xDC

void ControlsMenu_Run(ControlsMenu *cm);
void ControlsMenu_Draw(ControlsMenu *cm);
void ControlsMenu_Delete(ControlsMenu *cm);
void ControlsMenu_Create(ControlsMenu *cm, void *arg);
void ControlsMenu_GetParams(void *task, void **out);
void ControlsMenu_Changed(ControlsMenu *cm, void *arg);
void ControlsMenu_Activate(ControlsMenu *cm);
void ControlsMenu_Deactivate(ControlsMenu *cm);
void ControlsMenu_Load(ControlsMenu *cm);
void ControlsMenu_Save(ControlsMenu *cm);
ScriptValue *ControlsMenu_ConvertFloatFromOffset(ScriptValue *out, void *data, void *args);
ScriptValue *ControlsMenu_ConvertOffsetFromFloat(ScriptValue *out, void *data, void *args);

void ControlsMenu_Init(void)
{
    gControlsMenuType = TaskType_New("ControlsMenu", sizeof(ControlsMenu), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gControlsMenuType, 1, ControlsMenu_Run);
    TaskType_SetHandler2(gControlsMenuType, Screen_GetDrawEvent(), ControlsMenu_Draw);
    TaskType_SetHandler2(gControlsMenuType, 2, ControlsMenu_Delete);
    TaskType_SetHandler(gControlsMenuType, 0, ControlsMenu_Create);
    TaskType_SetHandler(gControlsMenuType, Param_GetTableEvent(), ControlsMenu_GetParams);
    TaskType_SetHandler(gControlsMenuType, Param_GetChangedEvent(), ControlsMenu_Changed);
    TaskType_SetHandler2(gControlsMenuType, Level_GetActivateEvent(), ControlsMenu_Activate);
    TaskType_SetHandler2(gControlsMenuType, Level_GetDeactivateEvent(), ControlsMenu_Deactivate);
    TaskType_SetHandler2(gControlsMenuType, MenuWindow_GetLoadEvent(), ControlsMenu_Load);
    TaskType_SetHandler2(gControlsMenuType, MenuWindow_GetSaveEvent(), ControlsMenu_Save);
    Script_RegisterFunction("ControlsMenu_ConvertFloatFromOffset", ControlsMenu_ConvertFloatFromOffset, NULL, 1);
    Script_RegisterFunction("ControlsMenu_ConvertOffsetFromFloat", ControlsMenu_ConvertOffsetFromFloat, NULL, 1);
}
HOOK(0x4230A0, ControlsMenu_Init);

/* the value Config_GetKey has for an index of the key table */
int ControlsMenu_GetKeyValue(int index)
{
    int value;

    Config_GetKey(index, &value, NULL);
    return value;
}
HOOK(0x423080, ControlsMenu_GetKeyValue);

/* the scroll offset as 0..1 for a scroll bar */
ScriptValue *ControlsMenu_ConvertFloatFromOffset(ScriptValue *out, void *data, void *args)
{
    int offset = Script_GetArgInt(args, 0);
    long double f = (long double)offset / (Config_GetActionCount() - 10);
    long long i;

    __asm__("fistpll %0" : "=m"(i) : "t"(f) : "st");
    out->i = (int)i;
    out->d = f;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4231E0, ControlsMenu_ConvertFloatFromOffset);

ScriptValue *ControlsMenu_ConvertOffsetFromFloat(ScriptValue *out, void *data, void *args)
{
    float pos = Script_GetArgReal(args, 0);
    long double f = (long double)(Config_GetActionCount() - 10) * pos + gHalf;
    long long i;

    __asm__("fistpll %0" : "=m"(i) : "t"(f) : "st");
    out->i = (int)i;
    out->d = f;
    out->s = gEmptyString;
    return out;
}
HOOK(0x423240, ControlsMenu_ConvertOffsetFromFloat);

void ControlsMenu_Create(ControlsMenu *cm, void *arg)
{
    int n = Config_GetActionCount();
    int i;

    CALL_BASE(0, cm, arg);
    cm->w.x = 300;
    cm->w.y = 230;
    cm->w.w = 360;
    cm->w.h = 200;
    cm->w.centreX = 1;
    cm->w.centreY = 1;
    for (i = 0; i < n; i++) {
        cm->names[i] = NULL;
        cm->keys[i] = NULL;
    }
    cm->offset = 0;
    cm->registered = 0;
    cm->selected = -1;
    cm->waiting = 0;
    cm->reset = 0;
    Random_Init(&gControlsRandom);
}
HOOK(0x4232B0, ControlsMenu_Create);

/* a macro so the ORIG calls are not made through an inline function */
#define FREE_TEXTS(cm, n)                                               \
    do {                                                                \
        int i_;                                                         \
        for (i_ = 0; i_ < (n); i_++) {                                  \
            if ((cm)->names[i_]) {                                      \
                Text_Free((cm)->names[i_]);                             \
                (cm)->names[i_] = NULL;                                 \
            }                                                           \
            if ((cm)->keys[i_]) {                                       \
                Text_Free((cm)->keys[i_]);                              \
                (cm)->keys[i_] = NULL;                                  \
            }                                                           \
        }                                                               \
    } while (0)

void ControlsMenu_Delete(ControlsMenu *cm)
{
    int n = Config_GetActionCount();

    FREE_TEXTS(cm, n);
    ((void (*)(void *))HANDLER(Level_GetDeactivateEvent(), cm->w.task.type))(cm);
}
HOOK(0x423350, ControlsMenu_Delete);

void ControlsMenu_Changed(ControlsMenu *cm, void *arg)
{
    int n = Config_GetActionCount();
    Sprite *font;
    int i;

    FREE_TEXTS(cm, n);
    /* without a menu the task itself is taken for the font */
    font = cm->w.menu ? MenuManager_FindFont(cm->w.menu, "font1.fnt") : (Sprite *)cm;
    if (font) {
        for (i = 0; i < n; i++) {
            const char *name;

            Config_GetAction(i, NULL, &name);
            cm->names[i] = Text_New(font, 0.0f, 0.0f, MenuManager_GetString(cm->w.menu, name));
            cm->names[i]->unk20 = 0;
            cm->keys[i] = Text_New(font, 0.0f, 0.0f, gEmptyString);
            cm->keys[i]->unk20 = 0;
        }
    }
    CALL_BASE(Param_GetChangedEvent(), cm, arg);
}
HOOK(0x4233D0, ControlsMenu_Changed);

void ControlsMenu_SetBinding(ControlsMenu *cm, int row, int key);
void ControlsMenu_LoadDefaults(ControlsMenu *cm);

/* click a row, then press a key or joystick button for it (Esc cancels).
   A key already used by another row is swapped with this row's old key. */
void ControlsMenu_Run(ControlsMenu *cm)
{
    if (cm->reset) {
        ControlsMenu_LoadDefaults(cm);
        cm->reset = 0;
    }
    if (!cm->waiting) {
        int mx, my;

        MenuWindow_HitTest(&cm->w, cm->w.x, cm->w.y, cm->w.w, cm->w.h, &mx, &my);
        if (cm->w.clicked) {
            Text *t;

            cm->selected = (my - cm->w.y) / ROW_HEIGHT + cm->offset;
            t = cm->keys[cm->selected];
            if (t)
                Text_SetString(t, MenuManager_GetString(cm->w.menu, "Select a key or button"));
            MenuManager_Pause(cm->w.menu);
            cm->waiting = 1;
            Sound_PlayName("menu_sel2", 0, NULL);
        }
    } else {
        int joy = gMouse.buttons;
        char found = 0;

        if (gKeys[KEY_ESCAPE] & 0x80) {
            ControlsMenu_SetBinding(cm, cm->selected, cm->bindings[cm->selected]);
            Sound_PlayName("menu_cancel", 0, NULL);
            goto done;
        } else {
            int n = Config_GetKeyCount();
            int sel = cm->selected;
            int i, value;

            for (i = 0; i < n && !found; i++) {
                unsigned int code;

                Config_GetKey(i, &value, NULL);
                code = value & 0x3FFFFFFF;
                switch ((value >> 30) & 3) {
                case 1:
                    if (gKeys[code] & 0x80)
                        found = 1;
                    break;
                case 3:
                    if ((unsigned int)joy == code)
                        found = 1;
                    break;
                }
                if (code == KEY_LWIN || code == KEY_RWIN)
                    found = 0;
            }
            if (found) {
                int rows = Config_GetActionCount();
                char sound[20];
                unsigned int r = Random_Next(&gControlsRandom);
                unsigned int m = (unsigned int)((int)r >> 31);
                int old, key;

                crt_sprintf(sound, "menu_k0%.1d", (int)((r ^ m) - m) % 7 + 1);
                Sound_PlayName(sound, 0, NULL);
                old = cm->bindings[sel];
                key = i - 1;
                ControlsMenu_SetBinding(cm, sel, key);
                for (i = 0; i < rows; i++)
                    if (sel != i && cm->bindings[i] == key)
                        ControlsMenu_SetBinding(cm, i, old);
                goto done;
            }
        }
    }
    goto run;
done:
    Keys_Clear();
    MenuManager_Resume(cm->w.menu);
    cm->waiting = 0;
    cm->selected = -1;
run:
    CALL_BASE(1, cm);
}
HOOK(0x4234E0, ControlsMenu_Run);

void ControlsMenu_SetBinding(ControlsMenu *cm, int row, int key)
{
    Text *t = cm->keys[row];

    cm->bindings[row] = key;
    if (t) {
        const char *name;

        Config_GetKey(key, NULL, &name);
        Text_SetString(t, MenuManager_GetString(cm->w.menu, name));
    }
}
HOOK(0x423770, ControlsMenu_SetBinding);

int ControlsMenu_FindBinding(int action);
int ControlsMenu_FindKey(int device, int key);

/* the defaults are indexed like the profile's bindings */
void ControlsMenu_LoadDefaults(ControlsMenu *cm)
{
    InputRemap *defaults = Config_GetDefaultKeys();
    int n = Config_GetActionCount();
    int i;

    for (i = 0; i < n; i++) {
        InputRemap *k = &defaults[ControlsMenu_FindBinding(i)];

        ControlsMenu_SetBinding(cm, i, ControlsMenu_FindKey(k->device, k->key));
    }
}
HOOK(0x4237C0, ControlsMenu_LoadDefaults);

/* the index of the active profile's binding for the i'th action, or -1 */
int ControlsMenu_FindBinding(int action)
{
    int n = Profile_GetKeyCount();
    InputRemap *k = Profile_GetKeys();
    int value, i;

    Config_GetAction(action, &value, NULL);
    for (i = 0; i < n; i++, k++)
        if (k->action == value)
            return i;
    return -1;
}
HOOK(0x423810, ControlsMenu_FindBinding);

/* the index in the config key table, or -1 */
int ControlsMenu_FindKey(int device, int key)
{
    int code = device << 30 | key;
    int n = Config_GetKeyCount();
    int i, value;

    for (i = 0; i < n; i++) {
        Config_GetKey(i, &value, NULL);
        if (value == code)
            break;
    }
    return i == n ? -1 : i;
}
HOOK(0x423860, ControlsMenu_FindKey);

void ControlsMenu_DrawFrame(ControlsMenu *cm, float alpha);
void ControlsMenu_DrawList(ControlsMenu *cm, float alpha);

void ControlsMenu_Draw(ControlsMenu *cm)
{
    float alpha;

    UI_ALPHA(cm->w.menu, alpha);
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), MenuWindow_GetType()))(cm);
    ControlsMenu_DrawFrame(cm, alpha);
    ControlsMenu_DrawList(cm, alpha);
}
HOOK(0x4238B0, ControlsMenu_Draw);

/* a green outline and the line between the two columns */
void ControlsMenu_DrawFrame(ControlsMenu *cm, float alpha)
{
    static const float column = 210.0f;
    int x = SCREEN_X(&cm->w) + 1;
    int h = cm->w.h;
    int w = cm->w.w;
    int y = SCREEN_Y(&cm->w) + 1;
    QVertex v[5];
    QVertexStrip strip;
    float fx, fy;
    int i;

    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    for (i = 0; i < 4; i++) {
        v[i].r = 0.0f;
        v[i].g = 1.0f;
        v[i].b = 0.0f;
        v[i].a = alpha;
        v[i].u = 0.0f;
        v[i].v = 0.0f;
        v[i].z = 1.0f;
    }
    fx = x;
    fy = y;
    v[0].x = fx;
    v[0].y = fy;
    v[1].x = x + w - 1;
    v[1].y = fy;
    v[2].x = v[1].x;
    v[2].y = h + y - 1;
    v[3].x = fx;
    v[3].y = v[2].y;
    v[4] = v[0];
    strip.lines = 1;
    strip.flags = 0x80000;
    strip.queue = 0;
    strip.count = 5;
    strip.verts = v;
    gRenderModeCurrent.methods[gQVertexStrip](&strip);
    v[0].x = fx + column;
    v[1].x = v[0].x;
    v[1].y = (long double)h + fy - 1.0f;
    strip.count = 2;
    strip.verts = v;
    gRenderModeCurrent.methods[gQVertexStrip](&strip);
}
HOOK(0x423930, ControlsMenu_DrawFrame);

/* rows offset .. offset + 9: names in green, keys in orange, on bars of
   alternating green */
void ControlsMenu_DrawList(ControlsMenu *cm, float alpha)
{
    static const float nameX = 4.0f, keyX = 214.0f, textY = 5.0f, column = 210.0f;
    int x = SCREEN_X(&cm->w);
    int y = SCREEN_Y(&cm->w);
    int row = cm->offset;
    int end = row + VISIBLE_ROWS;

    if (row < end) {
        float bar = alpha * gHalf;
        float fx = x;
        float fx2 = fx + column;

        for (; row < end; row++, y += ROW_HEIGHT) {
            Text *name = cm->names[row];
            Text *key = cm->keys[row];
            float fy;

            if (name && key) {
                float ty;

                name->color[0] = 0.0f;
                name->color[2] = 0.0f;
                name->x = fx + nameX;
                name->color[1] = 128.0f;
                name->flags |= 0x80000;
                name->alpha = alpha;
                ty = (float)y + textY;
                name->y = ty;
                Text_DrawAlt(name);
                key->flags |= 0x80000;
                key->color[0] = 128.0f;
                key->color[1] = 85.0f;
                key->x = fx + keyX;
                key->color[2] = 0.0f;
                key->alpha = alpha;
                key->y = ty;
                Text_DrawAlt(key);
            }
            fy = y;
            Draw_RectA(fx, fy, 210.0f, 20.0f, 0.0f, row & 1 ? 16.0f : 32.0f, 0.0f, bar, 0x80000, 0);
            Draw_RectA(fx2, fy, 150.0f, 20.0f, 0.0f, row & 1 ? 32.0f : 48.0f, 0.0f, bar, 0x80000, 0);
        }
    }
}
HOOK(0x423AC0, ControlsMenu_DrawList);

void ControlsMenu_GetParams(void *task, void **out)
{
    *out = gControlsMenuParams;
}
HOOK(0x423CA0, ControlsMenu_GetParams);

void ControlsMenu_Activate(ControlsMenu *cm)
{
    char name[64], buf[128];

    if (!cm->registered) {
        Task_GetName(name, cm);
        crt_sprintf(buf, "%s.nOffset", name);
        Script_RegisterInt2(buf, &cm->offset);
        crt_sprintf(buf, "%s.isReset", name);
        Script_RegisterBool8(buf, &cm->reset);
        cm->registered = 1;
    }
}
HOOK(0x423CB0, ControlsMenu_Activate);

void ControlsMenu_Deactivate(ControlsMenu *cm)
{
    char name[64], buf[128];

    if (cm->registered) {
        Task_GetName(name, cm);
        crt_sprintf(buf, "%s.nOffset", name);
        Script_RemoveSymbol(buf);
        crt_sprintf(buf, "%s.isReset", name);
        Script_RemoveSymbol(buf);
        cm->registered = 0;
    }
}
HOOK(0x423D30, ControlsMenu_Deactivate);

/* shows the active profile's bindings */
void ControlsMenu_Load(ControlsMenu *cm)
{
    InputRemap *keys = Profile_GetKeys();
    int n = Config_GetActionCount();
    int i;

    for (i = 0; i < n; i++) {
        int j = ControlsMenu_FindBinding(i);
        int key = ControlsMenu_FindKey(keys[j].device, keys[j].key);
        const char *name;

        Config_GetAction(i, NULL, &name);
        Text_SetString(cm->names[i], MenuManager_GetString(cm->w.menu, name));
        ControlsMenu_SetBinding(cm, i, key);
    }
    CALL_CHILDREN(cm, MenuWindow_GetLoadEvent());
}
HOOK(0x423DB0, ControlsMenu_Load);

/* writes the bindings back to the active profile */
void ControlsMenu_Save(ControlsMenu *cm)
{
    InputRemap *keys = Profile_GetKeys();
    int n = Config_GetActionCount();
    int i;

    for (i = 0; i < n; i++) {
        unsigned int value = ControlsMenu_GetKeyValue(cm->bindings[i]);
        InputRemap *k = &keys[ControlsMenu_FindBinding(i)];

        k->device = (int)value >> 30 & 3;
        k->key = value & 0x3FFFFFFF;
    }
    CALL_CHILDREN(cm, MenuWindow_GetSaveEvent());
}
HOOK(0x423EB0, ControlsMenu_Save);

void ControlsMenu_Shutdown(void)
{
    Script_UnregisterSymbol("ControlsMenu_ConvertFloatFromOffset");
    Script_UnregisterSymbol("ControlsMenu_ConvertOffsetFromFloat");
    TaskType_Free(gControlsMenuType);
    gControlsMenuType = 0x181;
}
HOOK(0x423F90, ControlsMenu_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B874) uint16_t gControlsMenuType INIT(385);
PAD(igd, 53B874, 53B876, 0x2);
BSS(57BC40) void *gControlsMenuParams;
PAD(igb, 57BC40, 57BC44, 0x4);
BSS(57BC40) Random gControlsRandom;
