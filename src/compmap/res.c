#include "hook.h"
#include "compmap/compmap.h"

ORIG(0x4B7000, Sprite *, Sprite_New, (const char *name));
ORIG(0x4B7030, void, Sprite_Delete, (Sprite *s));
ORIG(0x4B6720, int, Sprite_Load, (SpriteSlot *s, const char *name));
ORIG(0x4B66A0, void, Sprite_Free, (SpriteSlot *s));
ORIG(0x4B63D0, void, Resource_Flush, (const void *res));

static const char *const kArrowNames[4][3] = {
    {"COMPUTER:arrow1_1.spr", "COMPUTER:arrow1_2.spr", "COMPUTER:arrow1_3.spr"},
    {"COMPUTER:arrow2_1.spr", "COMPUTER:arrow2_2.spr", "COMPUTER:arrow2_3.spr"},
    {"COMPUTER:arrow3_1.spr", "COMPUTER:arrow3_2.spr", "COMPUTER:arrow3_3.spr"},
    {"COMPUTER:arrow4_1.spr", "COMPUTER:arrow4_2.spr", "COMPUTER:arrow4_3.spr"},
};

static const char *const kIconNames[19] = {
    "COMPUTER:h_player.spr", "COMPUTER:h_guard.spr", "COMPUTER:h_camera.spr", "COMPUTER:h_computer.spr",
    "COMPUTER:h_cabinet.spr", "COMPUTER:h_mine.spr", "COMPUTER:h_0.spr", "COMPUTER:h_1.spr",
    "COMPUTER:h_2.spr", "COMPUTER:h_3.spr", "COMPUTER:h_4.spr", "COMPUTER:h_5.spr",
    "COMPUTER:h_6.spr", "COMPUTER:h_7.spr", "COMPUTER:h_8.spr", "COMPUTER:h_9.spr",
    "COMPUTER:h_sentrygun.spr", "COMPUTER:h_scamera1.spr", "COMPUTER:h_scamera2.spr",
};

void ComputerRes_Load(void)
{
    ComputerRes *r = ComputerRes_Get();
    int i, j;

    r->computerRes = (void *)Resource_Load("COMPUTER:computer.res", 0);
    r->languageRes = (void *)Resource_Load("LANGUAGE:computer.res", 0);
    r->objectivesRes = (void *)Resource_Load("LANGUAGE:objectives.res", 0);
    r->font1 = Sprite_New("LOCAL:computer/font1.fnt");
    r->font3 = Sprite_New("LOCAL:computer/font3.fnt");
    r->font4 = Sprite_New("LOCAL:computer/font4.fnt");
    Sprite_Load(&r->pointer, "COMPUTER:pointer.spr");
    for (i = 0; i < 4; i++)
        for (j = 0; j < 3; j++)
            Sprite_Load(&r->arrow[i][j], kArrowNames[i][j]);
    Sprite_Load(&r->arrowUp[0], "COMPUTER:arrowup_1.spr");
    Sprite_Load(&r->arrowUp[1], "COMPUTER:arrowup_2.spr");
    Sprite_Load(&r->arrowUp[2], "COMPUTER:arrowup_3.spr");
    Sprite_Load(&r->arrowDown[0], "COMPUTER:arrowdown_1.spr");
    Sprite_Load(&r->arrowDown[1], "COMPUTER:arrowdown_2.spr");
    Sprite_Load(&r->arrowDown[2], "COMPUTER:arrowdown_3.spr");
    Sprite_Load(&r->plus[0], "COMPUTER:plus_1.spr");
    Sprite_Load(&r->plus[1], "COMPUTER:plus_2.spr");
    Sprite_Load(&r->plus[2], "COMPUTER:plus_3.spr");
    Sprite_Load(&r->minus[0], "COMPUTER:minus_1.spr");
    Sprite_Load(&r->minus[1], "COMPUTER:minus_2.spr");
    Sprite_Load(&r->minus[2], "COMPUTER:minus_3.spr");
    Sprite_Load(&r->sphere[0], "COMPUTER:sphere_1.spr");
    Sprite_Load(&r->sphere[1], "COMPUTER:sphere_2.spr");
    Sprite_Load(&r->sphere[2], "COMPUTER:sphere_3.spr");
    Sprite_Load(&r->objective[0], "COMPUTER:objective_1.spr");
    Sprite_Load(&r->objective[1], "COMPUTER:objective_2.spr");
    for (i = 0; i < 19; i++) {
        Sprite_Load(&r->icon[i], kIconNames[i]);
        r->iconName[i] = kIconNames[i];
    }
}
HOOK(0x46B070, ComputerRes_Load);

void ComputerRes_Free(void)
{
    ComputerRes *r = ComputerRes_Get();
    int i, j;

    for (i = 0; i < 19; i++)
        Sprite_Free(&r->icon[i]);
    Sprite_Free(&r->pointer);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 3; j++)
            Sprite_Free(&r->arrow[i][j]);
    for (i = 0; i < 3; i++)
        Sprite_Free(&r->arrowUp[i]);
    for (i = 0; i < 3; i++)
        Sprite_Free(&r->arrowDown[i]);
    for (i = 0; i < 3; i++)
        Sprite_Free(&r->plus[i]);
    for (i = 0; i < 3; i++)
        Sprite_Free(&r->minus[i]);
    for (i = 0; i < 3; i++)
        Sprite_Free(&r->sphere[i]);
    Sprite_Free(&r->objective[0]);
    Sprite_Free(&r->objective[1]);
    Sprite_Delete(r->font1);
    Sprite_Delete(r->font3);
    Sprite_Delete(r->font4);
    Resource_Flush(r->languageRes);
    Resource_Flush(r->objectivesRes);
    Resource_Flush(r->computerRes);
}
HOOK(0x46B310, ComputerRes_Free);

ComputerRes *ComputerRes_Get(void)
{
    return &gComputerRes;
}
HOOK(0x46B4D0, ComputerRes_Get);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BDDB0) ComputerRes gComputerRes;
