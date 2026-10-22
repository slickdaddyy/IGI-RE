/* Models: .mtp files, their meshes and textures, and the VirModel LOD table. */
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "model/model.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail_, (List *list, Node *node));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int method));
ORIG(0x4B24C0, void, Texture_LoadFile, (void *tex, void *info, const char *name));
ORIG(0x4B5F00, void *, Resource_Load, (const char *path, int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *path));
ORIG(0x4B7470, void *, Chunk_Next, (void *c));
ORIG(0x4B7490, void *, Chunk_FirstChild, (void *c));
ORIG(0x4B74D0, void *, Chunk_Data, (void *c, void *parent));
ORIG(0x4B7590, void *, Chunk_Find, (void *c, unsigned int id));
ORIG(0x4CF870, void, sub_4CF870, (ModelFile *f));
ORIG(0x4CF910, void, sub_4CF910, (ModelFile *f));
ORIG(0x4CF9D0, void, ModelFile_Free, (ModelFile *f));

#define gModelInitMethod VAR(int, 0x548200)
#define gModelDeinitMethod VAR(int, 0x548204)
#define gModelLoadMethod VAR(int, 0x548208)
#define gModelUnloadMethod VAR(int, 0x54820C)
#define gVirModels ((VirModel *)0xA443A0)
#define gModelFiles VAR(List, 0xA4EC60)
#define gModelPath ((char *)0xA4EC6C)
#define gModelDir ((char *)0xA4ED6C)
#define gVirModelCount VAR(int, 0xA4EDEC)
#define gVirModelSaved VAR(int, 0xA4EDF0)
#define gRenderFuncs ((void (**)(void *))0xA94E84)

_Static_assert(sizeof(VirModel) == 0x48, "VirModel");
_Static_assert(sizeof(ModelFile) == 0x4C, "ModelFile");
_Static_assert(sizeof(ModelLod) == 0x1C, "ModelLod");

static void list_init(List *l)
{
    l->tailpred = (Node *)l;
    l->head = (Node *)&l->tail;
    l->tail = NULL;
}

int Model_GetInitMethod(void)
{
    return gModelInitMethod;
}
HOOK(0x4CEA60, Model_GetInitMethod);

int Model_GetDeinitMethod(void)
{
    return gModelDeinitMethod;
}
HOOK(0x4CEA70, Model_GetDeinitMethod);

int Model_GetLoadMethod(void)
{
    return gModelLoadMethod;
}
HOOK(0x4CEA80, Model_GetLoadMethod);

int Model_GetUnloadMethod(void)
{
    return gModelUnloadMethod;
}
HOOK(0x4CEA90, Model_GetUnloadMethod);

/* The .mtp file is IFF style: big endian ids and sizes, chunks padded to 4. */
void *Mtp_FindChunk(const void *file, unsigned int id, unsigned int *size)
{
    const char *end = (const char *)file + __builtin_bswap32(((const uint32_t *)file)[1]) + 8;
    const uint32_t *c = (const uint32_t *)((const char *)file + 0xC);

    while (__builtin_bswap32(c[0]) != id) {
        c = (const uint32_t *)((const char *)c + ((__builtin_bswap32(c[1]) + 3) & ~3u) + 8);
        if ((const char *)c >= end)
            return NULL;
    }
    if (size)
        *size = __builtin_bswap32(c[1]);
    return (void *)(c + 2);
}
HOOK(0x4CEAA0, Mtp_FindChunk);

VirModel *VirModel_Add(const char *name)
{
    VirModel *m;

    if (!name)
        return NULL;
    if (gVirModelCount >= VIRMODEL_MAX) {
        Error_Show("MTP_nLODVirModel >= %d", VIRMODEL_MAX);
        for (;;)
            ;
    }
    m = &gVirModels[gVirModelCount];
    strcpy(m->name, name);
    gVirModelCount++;
    return m;
}
HOOK(0x4CEBA0, VirModel_Add);

/* Unknown names fall back to the "missing" model. */
VirModel *VirModel_Find(const char *name)
{
    VirModel *m = VirModel_Get(name);

    if (m)
        return m;
    if (!*name)
        return NULL;
    return VirModel_Get("missing");
}
HOOK(0x4CEC10, VirModel_Find);

/* Finds or adds the model; NULL when it has no LODs. */
VirModel *VirModel_Get(const char *name)
{
    VirModel *m;
    int i;

    crt_strstr(name, "johan");
    for (i = 0; i < gVirModelCount; i++) {
        if (strcmp(name, gVirModels[i].name) == 0)
            return gVirModels[i].lod_count ? &gVirModels[i] : NULL;
    }
    if (gVirModelCount >= VIRMODEL_MAX) {
        Error_Show("MTP_nLODVirModel >= %d", VIRMODEL_MAX);
        for (;;)
            ;
    }
    m = &gVirModels[gVirModelCount];
    strcpy(m->name, name);
    VirModel_InitLods(&gVirModels[gVirModelCount]);
    if (!gVirModels[gVirModelCount].lod_count)
        return NULL;
    gVirModelCount++;
    return m;
}
HOOK(0x4CEC40, VirModel_Get);

void VirModel_InitLods(VirModel *m)
{
    char name[32];
    int i, n = 0;

    m->unk24 = 0;
    strcpy(name, m->name);
    crt_strstr(name, "johan");
    for (i = 1; i <= VIRMODEL_LODS; i++) {
        m->lod_dist[i - 1] = (uint16_t)(100 / (i * i));
        m->lods[i - 1] = ModelLod_Find(name);
        if (!m->lods[i - 1])
            break;
        name[strlen(name) - 1]++;
        n++;
    }
    m->lod_count = (uint16_t)n;
}
HOOK(0x4CED50, VirModel_InitLods);

ModelLod *ModelLod_Find(const char *name)
{
    Node *f, *l;

    for (f = gModelFiles.head; f->next; f = f->next) {
        for (l = ((ModelFile *)f)->lods.head; l->next; l = l->next) {
            if (strcmp(name, ((ModelLod *)l)->name) == 0)
                return (ModelLod *)l;
        }
    }
    return NULL;
}
HOOK(0x4CEE30, ModelLod_Find);

void VirModel_RestoreCount(void)
{
    gVirModelCount = gVirModelSaved;
}
HOOK(0x4CEEA0, VirModel_RestoreCount);

void Model_Init(void)
{
    memset(gVirModels, 0, sizeof(VirModel) * VIRMODEL_MAX);
    list_init(&gModelFiles);
    gModelDir[0] = 0;
    gVirModelCount = 0;
    gVirModelSaved = 0;
    gModelInitMethod = RenderMode_CreateMethod("InitModels");
    gModelDeinitMethod = RenderMode_CreateMethod("DeinitModels");
    gModelLoadMethod = RenderMode_CreateMethod("LoadModel");
    gModelUnloadMethod = RenderMode_CreateMethod("UnloadModel");
}
HOOK(0x4CEEB0, Model_Init);

void VirModel_SaveCount(void)
{
    gVirModelSaved = gVirModelCount;
}
HOOK(0x4CEF30, VirModel_SaveCount);

void Model_Shutdown(void)
{
    Node *n, *prev;

    RenderMode_DeleteMethod(gModelUnloadMethod);
    RenderMode_DeleteMethod(gModelLoadMethod);
    RenderMode_DeleteMethod(gModelDeinitMethod);
    RenderMode_DeleteMethod(gModelInitMethod);
    n = gModelFiles.tailpred;
    for (prev = n->prev; prev; prev = prev->prev) {
        ModelFile_Free((ModelFile *)n);
        n = prev;
    }
}
HOOK(0x4CEF40, Model_Shutdown);

ModelFile *ModelFile_Load(const char *name)
{
    char path[64];
    ModelFile *f = Mem_Alloc(sizeof(ModelFile), 4);

    f->node.prev = NULL;
    f->node.next = NULL;
    f->unk48 = 0;
    List_AddTail_(&gModelFiles, &f->node);
    list_init(&f->meshes);
    list_init(&f->lods);
    list_init(&f->coll);
    crt_sprintf(path, "%s%s.mtp", gModelDir, name);
    f->mtp = Resource_Load(path, 0);
    ModelFile_Setup(f, name, 1);
    VirModel_InitAllLods();
    return f;
}
HOOK(0x4CEFA0, ModelFile_Load);

void VirModel_InitAllLods(void)
{
    int i;

    for (i = 0; i < VIRMODEL_MAX; i++)
        VirModel_InitLods(&gVirModels[i]);
}
HOOK(0x4CF050, VirModel_InitAllLods);

/* Texture and texinfo references of 0x2000 and up are into the first file. */
void ModelFile_Setup(ModelFile *f, const char *name, int textures)
{
    ModelFile *first = (ModelFile *)gModelFiles.head;
    char path[256];
    const int *inst;
    const char *names;
    int count;

    sub_4CF910(f);
    if (textures) {
        crt_sprintf(path, "%stextures/%s.res", gModelDir, name);
        Resource_Load(path, 0);
        ModelFile_LoadTextures(f);
        sub_4CF870(f);
        Resource_Unload(path);
    }
    crt_sprintf(path, "%smodels/%s.res", gModelDir, name);
    f->res = Resource_Load(path, 0);
    ModelFile_LoadMeshes(f);
    ModelFile_LoadCollMeshes(f);
    inst = Mtp_FindChunk(f->mtp, MTP_VNAM, 0);
    count = *inst;
    names = (const char *)(inst + count + 1);
    inst = Mtp_FindChunk(f->mtp, MTP_INST, 0);
    for (; count > 0; count--) {
        int idx = *inst++, n, i;
        Mesh *mesh;
        ModelLod *l;
        const int *gtt;

        if (idx >= 0x2000)
            mesh = (Mesh *)List_GetNth(&first->meshes, idx - 0x2000);
        else
            mesh = (Mesh *)List_GetNth(&f->meshes, idx);
        n = *inst++;
        l = Mem_Alloc(n * 8 + sizeof(ModelLod), 4);
        l->textures = (void **)(l + 1);
        l->texinfo = (void **)(l + 1) + n;
        l->count = n;
        l->node.prev = NULL;
        l->node.next = NULL;
        List_AddTail_(&f->lods, &l->node);
        l->mesh = mesh;
        l->name = names;
        names += strlen(names) + 1;
        gtt = (const int *)Mtp_FindChunk(f->mtp, MTP_GTT, 0) + 1;
        for (i = 0; i < n; i++) {
            int g = *inst++;
            int t = gtt[g * 2], u = gtt[g * 2 + 1];
            char *tex, *info;

            if (t >= 0x2000)
                tex = first->textures ? first->textures + (t - 0x2000) * 0x24 : NULL;
            else
                tex = f->textures ? f->textures + t * 0x24 : NULL;
            if (u == -1)
                info = f->texinfo ? f->texinfo - 0xC : NULL;
            else if (u >= 0x2000)
                info = first->texinfo ? first->texinfo + (u - 0x2000) * 0xC : NULL;
            else
                info = f->unk3C ? f->unk3C + u * 0xC : NULL;
            l->textures[i] = tex;
            l->texinfo[i] = info;
        }
    }
}
HOOK(0x4CF080, ModelFile_Setup);

Node *List_GetNth(List *list, int n)
{
    Node *node = list->head, *next;
    int i = 0;

    for (next = node->next; next; next = next->next) {
        if (i == n)
            return node;
        node = next;
        i++;
    }
    return NULL;
}
HOOK(0x4CF300, List_GetNth);

void ModelFile_LoadMeshes(ModelFile *f)
{
    const int *p = Mtp_FindChunk(f->mtp, MTP_MODS, 0);
    const char *s = (const char *)(p + 1);
    int n;

    for (n = *p; n > 0; n--) {
        List_AddTail_(&f->meshes, &Mesh_Load(s)->node);
        s += strlen(s) + 1;
    }
}
HOOK(0x4CF330, ModelFile_LoadMeshes);

static void *mef_chunk(void *res, unsigned int id)
{
    return Chunk_Data(Chunk_Find(Chunk_FirstChild(res), id), res);
}

Mesh *Mesh_Load(const char *name)
{
    Mesh *m = Mem_Alloc(sizeof(Mesh), 4);
    struct {
        float *header;
        void *res;
    } arg;

    m->node.prev = NULL;
    m->node.next = NULL;
    m->anim_count = 0;
    m->anims = NULL;
    crt_sprintf(gModelPath, "LOCAL:models/%s.mef", name);
    m->res = Resource_Load(gModelPath, 0);
    m->header = mef_chunk(m->res, MEF_MESH);
    if (*(uint32_t *)m->header != 0x3DCCCCCD) {
        Error_Show("Mesh \"%s\" and source code are incompatible. (Mesh \"%s\" is version %0.2f, source code is "
                   "version %0.2f.)",
                   name, name, (double)*m->header, (double)0.1f);
        for (;;)
            ;
    }
    *(void **)((char *)m->header + 0x8C) = mef_chunk(m->res, MEF_ATTA);
    *(void **)((char *)m->header + 0x88) = mef_chunk(m->res, MEF_MVTX);
    *(void **)((char *)m->header + 0x98) = mef_chunk(m->res, MEF_PORT);
    *(void **)((char *)m->header + 0x90) = mef_chunk(m->res, MEF_PTFC);
    *(void **)((char *)m->header + 0x94) = mef_chunk(m->res, MEF_PTVX);
    if (Chunk_Find(Chunk_FirstChild(m->res), MEF_NAXT)) {
        void *c = Chunk_Find(Chunk_FirstChild(m->res), MEF_NAXT), *x;
        int n = 1, i;

        for (x = Chunk_Next(c); x && (x = Chunk_Find(x, MEF_NAXT)); x = Chunk_Next(x))
            n++;
        m->anims = Mem_Alloc(n * sizeof(MeshAnim), 4);
        m->anim_count = n;
        for (i = 0; n; n--, i++) {
            int *d = Chunk_Data(c, m->res);

            m->anims[i].data = d + 2;
            m->anims[i].unk8 = d[0];
            m->anims[i].unk0 = d[1];
            m->anims[i].unk4 = 0;
            c = Chunk_Next(c);
            if (c)
                c = Chunk_Find(c, MEF_NAXT);
        }
    }
    arg.header = m->header;
    arg.res = m->res;
    if (gRenderFuncs[Model_GetLoadMethod()])
        gRenderFuncs[Model_GetLoadMethod()](&arg);
    return m;
}
HOOK(0x4CF390, Mesh_Load);

void ModelFile_LoadCollMeshes(ModelFile *f)
{
    const int *p = Mtp_FindChunk(f->mtp, MTP_SVOL, 0);
    const char *s;
    int n;

    if (!p)
        return;
    s = (const char *)(p + 1);
    for (n = *p; n > 0; n--) {
        List_AddTail_(&f->coll, &CollMesh_Load(s)->node);
        s += strlen(s) + 1;
    }
}
HOOK(0x4CF630, ModelFile_LoadCollMeshes);

CollMesh *CollMesh_Load(const char *name)
{
    CollMesh *m = Mem_Alloc(sizeof(CollMesh), 4);
    unsigned int size;
    char *faces, *verts, *edges;
    int i;

    m->node.prev = NULL;
    m->node.next = NULL;
    crt_sprintf(gModelPath, "%smodels/%s.mef", gModelDir, name);
    m->res = Resource_Load(gModelPath, 0);
    m->parts = Mtp_FindChunk(m->res, MTP_SMES, &size);
    m->count = size / 0x1C;
    faces = Mtp_FindChunk(m->res, MTP_SFAC, 0);
    verts = Mtp_FindChunk(m->res, MTP_SVTX, 0);
    edges = Mtp_FindChunk(m->res, MTP_EDGE, 0);
    for (i = 0; i < m->count; i++) {
        int *p = m->parts + i * 7;

        p[0] = (int)(faces + p[0] * 0x1C);
        p[1] = (int)(verts + p[1] * 0xC);
        p[2] = (int)(edges + p[2] * 8);
    }
    return m;
}
HOOK(0x4CF690, CollMesh_Load);

void ModelFile_LoadTextures(ModelFile *f)
{
    const int *p = Mtp_FindChunk(f->mtp, MTP_TEXF, 0);
    const char *s;
    int n = *p, i;

    f->texture_count = n;
    if (n <= 0) {
        f->textures = NULL;
        f->texinfo = NULL;
        return;
    }
    f->textures = Mem_Alloc(n * 0x24, 4);
    f->texinfo = Mem_Alloc(n * 0xC, 4);
    s = (const char *)(p + 1);
    for (i = 0; i < n; i++) {
        ((int *)(f->texinfo + i * 0xC))[1] = 0;
        crt_sprintf(gModelPath, "LOCAL:textures/%s.tex", s);
        Texture_LoadFile(f->textures + i * 0x24, f->texinfo + i * 0xC, gModelPath);
        s += strlen(s) + 1;
    }
}
HOOK(0x4CF790, ModelFile_LoadTextures);
