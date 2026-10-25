#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "litmap/litmap.h"
#include "litmap/litmaptypes.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x4B1420, void, File_ScanDir, (const char *path, void *fn, void *user));
ORIG(0x4B5B60, int, Resource_Exists, (const char *path));
ORIG(0x4B5F00, void *, Resource_Load, (const char *path, int *size));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));
ORIG(0x4B6380, void, Resource_Unload, (const char *path));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4CFC80, uint8_t, Mesh3D_GetEvent2, (void));

/* fixed size slots with a table of slot indices; the index of a used slot
   in the table is stored right after its data */
/* delta form factors of a hemicube with size x size pixels on top and
   size x size/2 on each side */
typedef struct Hemicube {
    int size;
    int half;
    void *top;
    void *side[4];
    void *top2;
    void *side2[4];
    float *topFactor;
    float *sideFactor;
    char unk38[0xB4];
} Hemicube;

_Static_assert(sizeof(Hemicube) == 0xEC, "Hemicube");

ORIG(0x523900, void, LightmapTex_Free, (LightmapTex *t));

/* what File_ScanDir passes to its callback */
typedef struct ScanEntry {
    const char *name;
    int size;
    char isDir;
} ScanEntry;

extern void *gLightmapBufA;
extern void *gLightmapBufB;
extern void *gLightmapBufC;
extern void *gLightmapBufD;
extern int gLightmapBufCount;
extern int gLightmapBuilt;
extern char gLightmapDir[];
extern char gLightmapResPath[];
extern Pool gLightmapInfoPool;
extern const char *gLightmapScanName;
extern int gLightmapFileFound;
extern float gHemicubeTopMax;
extern float gHemicubeSideMax;

static const float kInfoUnk94 = 100.0f;

void Lightmap_FreeBuffers(void)
{
    if (gLightmapBufA) {
        Mem_Free(gLightmapBufA);
        gLightmapBufA = NULL;
    }
    if (gLightmapBufB) {
        Mem_Free(gLightmapBufB);
        gLightmapBufB = NULL;
    }
    if (gLightmapBufC) {
        Mem_Free(gLightmapBufC);
        gLightmapBufC = NULL;
    }
    if (gLightmapBufD) {
        Mem_Free(gLightmapBufD);
        gLightmapBufD = NULL;
    }
    gLightmapBufCount = 0;
    gLightmapBuilt = 0;
}
HOOK(0x521040, Lightmap_FreeBuffers);

char *Lightmap_GetDir(void)
{
    crt_sprintf(gLightmapDir, "MISSION:%s", "lightmaps");
    return gLightmapDir;
}
HOOK(0x5210C0, Lightmap_GetDir);

char *Lightmap_GetResPath(void)
{
    crt_sprintf(gLightmapResPath, "MISSION:%s/%s", "lightmaps", "lightmaps.res");
    return gLightmapResPath;
}
HOOK(0x5210E0, Lightmap_GetResPath);

Hemicube *Hemicube_New(int size)
{
    Hemicube *h;
    int bytes, i, x, y, n;

    Mem_PushMode(1);
    h = Mem_Alloc(sizeof(Hemicube), 4);
    bytes = size * size * 4;
    h->size = size;
    h->half = size >> 1;
    h->top = Mem_Alloc(bytes, 4);
    h->top2 = Mem_Alloc(bytes, 4);
    h->topFactor = Mem_Alloc(bytes, 4);
    bytes = (size >> 1) * size * 4;
    h->sideFactor = Mem_Alloc(bytes, 4);
    for (i = 0; i < 4; i++) {
        h->side[i] = Mem_Alloc(bytes, 4);
        h->side2[i] = Mem_Alloc(bytes, 4);
    }
    for (y = 0; y < h->size; y++) {
        for (x = 0; x < h->size; x++) {
            long double fn, c, s, d, v;
            float fx, fy;

            n = h->size;
            fn = n;
            c = fn * kHalf;
            fx = ((long double)x - c + kHalf) / c;
            fy = ((long double)y - c + kHalf) / c;
            s = kTwo / fn;
            d = (long double)fy * fy + (long double)fx * fx + kOne;
            v = s * s * (kOne / (d * d * kPi));
            h->topFactor[n * y + x] = v;
            if (!(gHemicubeTopMax > v))
                gHemicubeTopMax = v;
        }
    }
    for (y = 0; y < h->size / 2; y++) {
        for (x = 0; x < h->size; x++) {
            long double fn, c, s, d, v, ay;
            float fx, fy;

            n = h->size;
            fn = n;
            c = fn * kHalf;
            fx = ((long double)x - c + kHalf) / c;
            fy = ((long double)y - c + kHalf) / c;
            s = kTwo / fn;
            ay = fy < kZero ? -(long double)fy : fy;
            d = (long double)fy * fy + (long double)fx * fx + kOne;
            v = s * s * (ay / (d * d * kPi));
            h->sideFactor[n * y + x] = v;
            if (!(gHemicubeSideMax > v))
                gHemicubeSideMax = v;
        }
    }
    Mem_PopMode();
    return h;
}
HOOK(0x523110, Hemicube_New);

void Hemicube_Free(Hemicube *h)
{
    int i;

    Mem_Free(h->sideFactor);
    h->sideFactor = NULL;
    Mem_Free(h->topFactor);
    h->topFactor = NULL;
    Mem_Free(h->top2);
    h->top2 = NULL;
    Mem_Free(h->top);
    h->top = NULL;
    for (i = 0; i < 4; i++) {
        Mem_Free(h->side2[i]);
        h->side2[i] = NULL;
        Mem_Free(h->side[i]);
        h->side[i] = NULL;
    }
    Mem_Free(h);
}
HOOK(0x5233A0, Hemicube_Free);

void LightmapInfo_ScanFile(ScanEntry *e)
{
    if (!e->isDir && crt_strstr(e->name, gLightmapScanName))
        gLightmapFileFound = 1;
}
HOOK(0x523420, LightmapInfo_ScanFile);

void LightmapInfo_MakeName(LightmapInfo *li);

void LightmapInfo_Create(LightmapInfo *li)
{
    Pool *p = &gLightmapInfoPool;
    char *item;

    if (!p->used && Resource_Exists(Lightmap_GetResPath()))
        Resource_Load(Lightmap_GetResPath(), NULL);
    item = p->data + p->free[p->used] * p->stride;
    *(int *)(item + p->size) = p->used;
    p->used++;
    li->slot = item;
    *(LightmapInfo **)item = li;
    li->unk68 = li->unk6F = li->unk70 = li->unk71 = 0;
    li->unk69 = li->unk6A = li->unk6B = li->unk6C = 0;
    li->unk6D = li->unk6E = li->unk72[0] = li->unk72[1] = 0;
    crt_sprintf(li->name, "");
    li->scale = li->lastScale = li->unk90 = li->unk9C = 1.0f;
    li->unkA4 = 150;
    li->unkA0 = 1000;
    li->texelsA8 = li->texelsAC = li->vertexLightSize = 0;
    li->unk94 = kInfoUnk94;
    li->unkB4 = 1;
    li->unkD0 = li->unkD4 = li->unkD8 = 0;
    li->owner = li->task.parent;
    li->set = NULL;
    li->vertexLights = NULL;
    li->unkBC = 0;
    li->unkCC = 1;
    li->unkDC = 0;
    DynCube_ListAdd(gCurrentWorld, li, 0);
    LightmapInfo_MakeName(li);
}
HOOK(0x523450, LightmapInfo_Create);

/* the first "obj%05d" that no file in the lightmap folder and no other
   LightmapInfo uses */
void LightmapInfo_MakeName(LightmapInfo *li)
{
    char name[256];
    int n, i, used;

    for (n = 0;; n++) {
        Pool *p = &gLightmapInfoPool;

        used = 0;
        crt_sprintf(name, "%s%05d", "obj", n);
        gLightmapFileFound = 0;
        gLightmapScanName = name;
        File_ScanDir(Lightmap_GetDir(), LightmapInfo_ScanFile, NULL);
        for (i = 0; i < p->used; i++) {
            LightmapInfo *o = *(LightmapInfo **)(p->data + p->free[i] * p->stride);

            if (crt_strstr(o->name, name)) {
                used = 1;
                break;
            }
        }
        if (!gLightmapFileFound && !used)
            break;
    }
    crt_sprintf(li->name, "%s%05d", "obj", n);
}
HOOK(0x5235A0, LightmapInfo_MakeName);

void VertexLights_Free(VertexLights *vl)
{
    int i;

    for (i = 0; i < vl->count; i++) {
        if (vl->lights[i].weights) {
            Mem_Free(vl->lights[i].weights);
            vl->lights[i].weights = NULL;
            Mem_Free(vl->lights[i].verts);
            vl->lights[i].verts = NULL;
        }
    }
    Mem_Free(vl);
}
HOOK(0x523670, VertexLights_Free);

void Lightmaps_Free(Lightmaps *lm);

void LightmapInfo_Delete(LightmapInfo *li)
{
    Pool *p = &gLightmapInfoPool;
    LightmapSet *set = li->set;
    Task *owner;
    int i, idx;

    DynCube_ListRemove(gCurrentWorld, li, 0);
    if (li->vertexLights) {
        VertexLights_Free(li->vertexLights);
        li->vertexLights = NULL;
    }
    owner = li->task.parent;
    if (set) {
        void *arg;
        void (*fn)(void *, void *);

        for (i = 0; i < set->count; i++) {
            if (set->refs[i].lightmaps) {
                Lightmaps_Free(set->refs[i].lightmaps);
                set->refs[i].lightmaps = NULL;
            }
        }
        arg = set;
        fn = TASK_HANDLER(Mesh3D_GetEvent2(), owner->type);
        fn(owner, &arg);
    }
    idx = *(int *)(li->slot + p->size);
    p->used--;
    if (idx != p->used) {
        char *last = p->data + p->free[p->used] * p->stride;
        int t = p->free[idx];
        int k = *(int *)(last + p->size);

        p->free[idx] = p->free[k];
        p->free[k] = t;
        *(int *)(last + p->size) = idx;
    }
    if (!p->used && Resource_Find(Lightmap_GetResPath()))
        Resource_Unload(Lightmap_GetResPath());
}
HOOK(0x5236C0, LightmapInfo_Delete);

void Lightmaps_Free(Lightmaps *o)
{
    int i;

    if (o->info) {
        for (i = 0; i < o->count; i++) {
            LightmapTexInfo *r = &o->info[i];

            Mem_Free(r->unk18);
            r->unk18 = NULL;
            Mem_Free(r->unk14);
            r->unk14 = NULL;
            Mem_Free(r->positions);
            r->positions = NULL;
            Mem_Free(r->normals);
            r->normals = NULL;
            Mem_Free(r->texelIds);
            r->texelIds = NULL;
            Mem_Free(r->weights);
            r->weights = NULL;
            Mem_Free(r->unkC);
            r->unkC = NULL;
            Mem_Free(r->unk8);
            r->unk8 = NULL;
        }
        Mem_Free(o->info);
        o->info = NULL;
    }
    for (i = 0; i < o->count; i++)
        LightmapTex_Free(&o->tex[i]);
    if (o->fromResource) {
        Resource_Flush(o);
        return;
    }
    for (i = 0; i < o->count; i++) {
        Mem_Free(o->tex[i].pixels);
        o->tex[i].pixels = NULL;
    }
    Mem_Free(o);
}
HOOK(0x5237E0, Lightmaps_Free);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A83978) int gLightmapFileFound;
BSS(A839CC) void *gLightmapBufD;
BSS(A839CC) void *gLightmapBufA;
PAD(igb, A839CC, A839D4, 0x1C);
BSS(A839CC) void *gLightmapBufB;
BSS(A83A00) Pool gLightmapInfoPool;
STORE(igb, A83A20, 1) char store_A83A20[260];
ALIAS(gLightmapDir, store_A83A20, 0);
BSS(A83A20) const char *gLightmapScanName;
BSS(A83B2C) float gHemicubeSideMax;
BSS(A83B40) float gHemicubeTopMax;
STORE(igb, A83B54, 1) char store_A83B54[256];
ALIAS(gLightmapResPath, store_A83B54, 0);
BSS(A83B54) void *gLightmapBufC;
BSS(A83D24) int gLightmapBufCount;
BSS(A83D24) int gLightmapBuilt;
