/* LightmapInfo: the task that builds and loads the lightmaps of a sector object. */
#pragma GCC optimize("excess-precision=standard")
#pragma GCC optimize("rounding-math")

#include "hook.h"
#include "dirlight/dirlight.h"
#include "dirlight/dirlightvars.h"

ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, unsigned int align));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B2320, void, Texture_Alloc, (void *tex, void *palette, int pitch, int width, int height, int format, int unk,
                                     unsigned int flags));
ORIG(0x4B2450, void, Texture_Download, (void *tex, void *palette, int pitch, int width, int height, int format,
                                        void *pixels));
ORIG(0x4B2610, void, Texture_SetActiveTMU, (int tmu));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B33A0, void, Matrix_RotateEuler, (Matrix *out, const Matrix *m, float ax, float ay, float az));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4D3710, void, Task_Nothing2, (void *task, int arg));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4CFC70, uint8_t, Mesh3D_GetEvent1, (void));
ORIG(0x4CFC90, uint8_t, Mesh3D_GetEvent3, (void));
ORIG(0x4CFCA0, uint8_t, Mesh3D_GetEvent4, (void));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x48F230, int, AppContext_GetLightmapsUsed, (void));
ORIG(0x4BD070, VertexLights *, Qvm_GetArgLine, (LightmapInfo *info));
ORIG(0x497570, void, Scene_Init, (void *scene, int queues));
ORIG(0x497680, void, Scene_SetRect, (void *scene, const float *rect));
ORIG(0x4975F0, void, Scene_Free, (void *scene));
ORIG(0x521040, void, Lightmap_FreeBuffers, (void));
ORIG(0x523110, void *, Hemicube_New, (int unk));
ORIG(0x5233A0, void, Hemicube_Free, (void *p));
ORIG(0x523450, void, LightmapInfo_Create, (LightmapInfo *info));
ORIG(0x523670, void, VertexLights_Free, (VertexLights *vl));
ORIG(0x5236C0, void, LightmapInfo_Delete, (LightmapInfo *info));
ORIG(0x5237E0, void, Lightmaps_Free, (Lightmaps *lm));
ORIG(0x5274C0, uint16_t, Dirlight_GetType, (void));
/* Lightmap_BuildLod, called through the exe so tests can replace it */
ORIG(0x523D40, Lightmaps *, BuildLod, (void *lod, int lodIndex, float scale, const char *name, int index, int full,
                                       int flags, int unk));

extern uint16_t gLightmapInfoType;
extern void *gLightmapInfoParams;
extern char gLightmapRandom[];
extern Matrix gLightmapRotations[];
extern char gLightmapScene[];
extern uint8_t gLightSourceIsEvent;
extern uint8_t gLightSourceTraceEvent;
extern uint8_t gLightSourceVertexEvent;

extern char kScaleRange[];

static const float kTexelScale = 4096.0f;
static const float kInvTexelScale = 0x1p-12f;
static const float kHalf = 0.5f;
static const float kHalfPi = 0x1.921fb6p+0f;

static inline uint32_t float_bits(const float *f)
{
    uint32_t u;

    memcpy(&u, f, 4);
    return u;
}

void LightmapTex_Free(LightmapTex *t)
{
    Mem_PushMode(1);
    Texture_Free(t);
    memset(t, 0, sizeof(t->texture));
    Mem_PopMode();
}
HOOK(0x523900, LightmapTex_Free);

void LightmapInfo_Changed(LightmapInfo *info)
{
    Task *parent = info->task.parent;
    int stale = 0, missing = 0, edited = 0;
    LightmapSet *set, *loaded;
    void *match[2];
    int i, j;

    info->scale = 1.0f;
    if (!info->unkCC && float_bits(&info->lastScale) != 0x3F800000)
        stale = 1;
    if (info->unk70 || info->unk6F || info->unk71) {
        set = info->set;
        edited = 1;
        if (set)
            for (i = 0; i < set->count; i++)
                if (set->refs[i].lightmaps && !set->refs[i].lightmaps->info)
                    missing = 1;
    }
    info->unkCC = 0;
    stale |= missing;
    match[0] = info->set;
    TASK_HANDLER(Mesh3D_GetEvent3(), parent->type)(parent, match);
    if (match[1] && feq(info->lastScale, info->scale) && !stale && !info->unkDC)
        goto done;

    if (info->set)
        for (i = 0; i < info->set->count; i++) {
            LightmapRef *r = &info->set->refs[i];

            if (r->lightmaps) {
                Lightmaps_Free(r->lightmaps);
                r->lightmaps = NULL;
            }
        }
    if (info->vertexLights) {
        VertexLights_Free(info->vertexLights);
        info->vertexLights = NULL;
    }
    loaded = info->set;
    if (AppContext_GetLightmapsUsed())
        TASK_HANDLER(Mesh3D_GetEvent1(), parent->type)(parent, &loaded);
    info->set = loaded;
    if (!loaded)
        goto done;

    info->texelsA8 = 0;
    info->texelsAC = 0;
    gLightmapTexelCount = 0;
    gLightmapFlaggedTexelCount = 0;
    for (i = 0; i < info->set->count; i++) {
        LightmapRef *r = &info->set->refs[i];
        int full = stale || info->unkDC || edited;
        Lightmaps *lm = BuildLod(r->lod, r->lodIndex, (1 << r->lodIndex) * info->scale * kTexelScale,
                                          info->name, i, full, edited, (int)float_bits(&info->unk94));

        r->lightmaps = lm;
        r->uploaded = 0;
        if (lm)
            for (j = 0; j < r->lightmaps->count; j++) {
                LightmapTex *t = &r->lightmaps->tex[j];

                info->texelsAC += t->height * t->width;
                info->texelsA8 += t->height * t->width;
            }
    }
    info->unkC0 = gLightmapTexelCount;
    info->unkC4 = gLightmapFlaggedTexelCount;
    for (i = 0; i < info->set->count; i++) {
        Task *obj = (Task *)info->set->refs[i].obj;

        TASK_HANDLER(Mesh3D_GetEvent4(), obj->type)(obj, NULL);
    }
    Task_Nothing(info->set);

done:
    info->lastScale = info->scale;
    info->unkDC = 0;
    if (!info->set)
        return;
    if (info->unk6F || info->unk70) {
        VertexLights *vl;

        info->unkBC = Hemicube_New(info->unkA4);
        if (info->vertexLights) {
            VertexLights_Free(info->vertexLights);
            info->vertexLights = NULL;
        }
        vl = Qvm_GetArgLine(info);
        info->vertexLights = vl;
        if (!vl) {
            info->vertexLightSize = 0;
        } else {
            info->vertexLightSize = vl->count * 12 + 4;
            for (i = 0; i < vl->count; i++)
                info->vertexLightSize += vl->lights[i].count * 8 + 4;
        }
        Hemicube_Free(info->unkBC);
        info->unkBC = NULL;
    }
    if (info->unk71)
        Task_Nothing2(info, info->unkB4);
}
HOOK(0x523930, LightmapInfo_Changed);

/* the crossing point of the lines a0-a1 and b0-b1; a shared end point is
   returned as it is */
void Line2_Intersect(float *out, const float *a0, const float *a1, const float *b0, const float *b1)
{
    long double ay, ax, bx;
    float by, ca, cb, det;

    if (feq(a0[0], b0[0]) && feq(a0[1], b0[1])) {
        memcpy(out, a0, 8);
        return;
    }
    if (feq(a0[0], b1[0]) && feq(a0[1], b1[1])) {
        memcpy(out, a0, 8);
        return;
    }
    if (feq(a1[0], b0[0]) && feq(a1[1], b0[1])) {
        memcpy(out, a1, 8);
        return;
    }
    if (feq(a1[0], b1[0]) && feq(a1[1], b1[1])) {
        memcpy(out, a1, 8);
        return;
    }
    ay = -(a1[1] - a0[1]);
    ax = a1[0] - a0[0];
    by = -(b1[1] - b0[1]);
    bx = b1[0] - b0[0];
    ca = -(ay * a0[0]) - ax * a0[1];
    cb = -(by * b0[0]) - bx * b0[1];
    det = bx * ay - by * ax;
    out[0] = -((ca * bx - cb * ax) / det);
    out[1] = -((cb * ay - ca * by) / det);
}
HOOK(0x5251D0, Line2_Intersect);

void LightmapTex_Alloc(LightmapTex *t)
{
    Mem_PushMode(1);
    Texture_SetActiveTMU(1);
    Texture_Alloc(t, NULL, t->width * 4, t->width, t->height, 3, 0, 0xA808003C);
    Texture_SetActiveTMU(-1);
    Mem_PopMode();
}
HOOK(0x525320, LightmapTex_Alloc);

void LightmapInfo_GetParams(LightmapInfo *info, void **out)
{
    *out = gLightmapInfoParams;
}
HOOK(0x525370, LightmapInfo_GetParams);

void LightmapTex_Upload(LightmapTex *t);
void Lightmap_UploadRef(LightmapRef *r);

/* first use of a lightmapped object: send all its lightmaps to the card */
void Lightmap_UploadObject(char *obj)
{
    LightmapRef *refs = *(LightmapRef **)(obj + 0xDC);
    int i;

    if (refs->uploaded)
        return;
    for (i = 0; i < *(short *)(*(char **)(obj + 0x6C) + 0x3C); i++)
        Lightmap_UploadRef(&refs[i]);
    refs->uploaded = 1;
}
HOOK(0x525380, Lightmap_UploadObject);

void Lightmap_UploadRef(LightmapRef *r)
{
    Lightmaps *lm = r->lightmaps;
    int i;

    if (!lm)
        return;
    for (i = 0; i < lm->count; i++)
        LightmapTex_Upload(&lm->tex[i]);
}
HOOK(0x5253D0, Lightmap_UploadRef);

void LightmapTex_Upload(LightmapTex *t)
{
    Mem_PushMode(1);
    Texture_Download(t, NULL, t->width * 4, t->width, t->height, 3, t->pixels);
    Mem_PopMode();
}
HOOK(0x525410, LightmapTex_Upload);

void Dirlight_IsLightSource(void *dl, int *out)
{
    *out = 1;
}
HOOK(0x525450, Dirlight_IsLightSource);

typedef struct VertexLightArg {
    int index;
    LightmapInfo *info;
    float **colorsA;
    float **colorsB;
} VertexLightArg;

/* adds the light's colour, weighted per vertex, to two vertex colour lists */
void Dirlight_AddVertexLight(Dirlight *dl, VertexLightArg *arg)
{
    VertexLight *vl = &arg->info->vertexLights->lights[arg->index];
    float c[3];
    long double s;
    int j, n;

    c[0] = dl->color.x;
    c[1] = dl->color.y;
    c[2] = dl->color.z;
    s = arg->info->set->radius * kInvTexelScale;
    s = s + s;
    s = s * s;
    c[0] = s * c[0];
    c[1] = s * c[1];
    c[2] = s * c[2];
    n = vl->count;
    for (j = 0; j < n; j++) {
        long double x = c[0] * vl->weights[j];
        float y = c[1] * vl->weights[j];
        float z = c[2] * vl->weights[j];
        int v = vl->verts[j];
        float *p = arg->colorsA[v];
        float *q = arg->colorsB[v];
        float x2 = x + x;
        long double xh;
        float ax, ay, az, by, bz;

        y = y + y;
        z = z + z;
        xh = x2 * kHalf;
        ax = xh;
        ay = y * kHalf;
        az = z * kHalf;
        by = y * kHalf;
        bz = z * kHalf;
        p[0] = ax + p[0];
        p[1] = ay + p[1];
        p[2] = az + p[2];
        q[0] = xh + q[0];
        q[1] = by + q[1];
        q[2] = bz + q[2];
    }
}
HOOK(0x526150, Dirlight_AddVertexLight);

void LightmapInfo_Init(void)
{
    LightmapInfo t;
    float rect[4];
    uint16_t type;
    uint8_t ev;
    Matrix *m;
    int i;

    type = TaskType_New("LightmapInfo", sizeof(LightmapInfo), Task_GetRootType(), 0, 0);
    gLightmapInfoType = type;
    TaskType_SetHandler(type, 0, LightmapInfo_Create);
    TaskType_SetHandler2(gLightmapInfoType, 3, LightmapInfo_Delete);
    TaskType_SetHandler(gLightmapInfoType, Param_GetChangedEvent(), LightmapInfo_Changed);
    TaskType_SetHandler(gLightmapInfoType, Param_GetTableEvent(), LightmapInfo_GetParams);
    gLightmapInfoParams = ParamTable_New(0x15);
    ParamTable_Add(gLightmapInfoParams, &t, &t.scale, Param_Real32(), 7, kScaleRange);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unkB4, Param_Int32(), 7, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unkA4, Param_Int32(), 7, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unkA0, Param_Int32(), 7, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk90, Param_Real32(), 7, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk94, Param_Real32(), 7, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unkD0, Param_RGB(), 7, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.name, Param_String16(), 3, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk70, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk6F, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk71, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk6D, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk6C, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk6E, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk6A, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk6B, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk69, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.unk68, Param_PushButton(), 6, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.texelsA8, Param_Int32(), 2, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.texelsAC, Param_Int32(), 2, NULL);
    ParamTable_Add(gLightmapInfoParams, &t, &t.vertexLightSize, Param_Int32(), 2, NULL);
    Pool_Init(&gLightmapInfoPool, 4, 1000, 0);
    Pool_Init(&gLightSourcePool, 4, 100, 0);
    Random_Init(gLightmapRandom);
    for (i = 0, m = gLightmapRotations; m < gLightmapRotations + 4; m++, i++) {
        Matrix_SetRotX(m, kHalfPi);
        Matrix_RotateEuler(m, m, 0, i * kHalfPi, 0);
    }
    gLightSourceIsEvent = TaskEvent_Alloc(0x181);
    gLightSourceTraceEvent = TaskEvent_Alloc(0x181);
    gLightSourceVertexEvent = TaskEvent_Alloc(0x181);
    TaskType_SetHandler(Dirlight_GetType(), gLightSourceIsEvent, Dirlight_IsLightSource);
    TaskType_SetHandler(Dirlight_GetType(), gLightSourceTraceEvent, Dirlight_TraceVertices);
    TaskType_SetHandler(Dirlight_GetType(), gLightSourceVertexEvent, Dirlight_AddVertexLight);
    ev = Mesh3D_GetEvent4();
    TaskType_SetHandler(RigidDynCubeObj_GetType(), ev, Lightmap_UploadObject);
    rect[0] = 0;
    rect[1] = 0;
    rect[2] = 220.0f;
    rect[3] = 120.0f;
    Scene_Init(gLightmapScene, 0x100);
    Scene_SetRect(gLightmapScene, rect);
    gLightmapBuffer = Mem_Alloc(0x40000, 4);
    for (i = 0; i < 0x10000; i++)
        gLightmapBuffer[i] = -1;
}
HOOK(0x526330, LightmapInfo_Init);

void LightmapInfo_Shutdown(void)
{
    Lightmap_FreeBuffers();
    Mem_Free(gLightmapBuffer);
    gLightmapBuffer = NULL;
    Scene_Free(gLightmapScene);
    TaskEvent_Free(gLightSourceVertexEvent);
    TaskEvent_Free(gLightSourceTraceEvent);
    TaskEvent_Free(gLightSourceIsEvent);
    Pool_Free(&gLightSourcePool);
    Pool_Free(&gLightmapInfoPool);
    QTaskList_Free(gLightmapInfoParams);
    TaskType_Free(gLightmapInfoType);
}
HOOK(0x526860, LightmapInfo_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54F040) uint16_t gLightmapInfoType INIT(385);
PAD(igd, 54F040, 54F042, 0xE) INIT({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F});
STORE(igd, 54F040, 1) char store_54F050[328] INIT({0, 0, 0, 63, 0, 0, 32, 65, 77, 73, 83, 83, 73, 79, 78, 58, 37, 115, 0, 0, 108, 105, 103, 104, 116, 109, 97, 112, 115, 0, 0, 0, 77, 73, 83, 83, 73, 79, 78, 58, 37, 115, 47, 37, 115, 0, 0, 0, 108, 105, 103, 104, 116, 109, 97, 112, 115, 46, 114, 101, 115, 0, 0, 0, 37, 115, 37, 48, 53, 100, 0, 0, 111, 98, 106, 0, 37, 115, 47, 37, 115, 95, 37, 48, 53, 100, 46, 111, 108, 109, 0, 0, 76, 105, 103, 104, 116, 109, 97, 112, 73, 110, 102, 111, 0, 0, 0, 0, 76, 105, 103, 104, 116, 115, 111, 117, 114, 99, 101, 32, 110, 111, 116, 32, 102, 111, 117, 110, 100, 0, 0, 0, 68, 105, 114, 108, 105, 103, 104, 116, 75, 101, 121, 102, 114, 97, 109, 101, 0, 0, 0, 0, 68, 105, 114, 108, 105, 103, 104, 116, 0, 0, 0, 0, 76, 105, 110, 107, 32, 115, 101, 116, 116, 105, 110, 103, 32, 37, 115, 0, 83, 99, 97, 108, 101, 32, 66, 32, 37, 115, 0, 0, 83, 99, 97, 108, 101, 32, 71, 32, 37, 115, 0, 0, 83, 99, 97, 108, 101, 32, 82, 32, 37, 115, 0, 0, 70, 111, 103, 32, 100, 101, 110, 115, 105, 116, 121, 32, 37, 115, 0, 0, 70, 111, 103, 32, 99, 111, 108, 111, 114, 32, 37, 115, 0, 0, 0, 0, 65, 109, 98, 105, 101, 110, 116, 32, 99, 111, 108, 111, 114, 32, 37, 115, 0, 0, 0, 0, 111, 98, 106, 101, 99, 116, 32, 99, 97, 116, 101, 103, 111, 114, 121, 32, 37, 100, 0, 0, 116, 101, 114, 114, 97, 105, 110, 0, 71, 108, 111, 98, 97, 108, 76, 105, 103, 104, 116, 75, 101, 121, 102, 114, 97, 109, 101, 0, 71, 108, 111, 98, 97, 108, 76, 105, 103, 104, 116});
ALIAS(kScaleRange, store_54F050, 0);
BSS(A8397C) uint8_t gLightSourceTraceEvent;
PAD(igb, A8397C, A8397D, 0x3);
STORE(igb, A8397C, 1) char store_A83980[76];
ALIAS(gLightmapScene, store_A83980, 0);
BSS(A839F4) int gLightmapFlaggedTexelCount;
BSS(A83A18) uint8_t gLightSourceIsEvent;
PAD(igb, A83A18, A83A19, 0x3);
BSS(A83B28) int *gLightmapBuffer;
STORE(igb, A83B30, 1) char store_A83B30[12];
ALIAS(gLightmapRandom, store_A83B30, 0);
BSS(A83B30) void *gLightmapInfoParams;
BSS(A83B44) uint8_t gLightSourceVertexEvent;
PAD(igb, A83B44, A83B45, 0x3);
STORE(igb, A83C58, 4) __typeof__(gLightmapRotations[0]) store_A83C58[4];
ALIAS(gLightmapRotations, store_A83C58, 0);
