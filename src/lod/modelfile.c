/* Model files: palettes, animations and freeing a loaded .mtp. */
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "model/model.h"
#include "core/coretypes.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF960, void, List_Remove_, (Node *node));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B63D0, void, Resource_Flush, (void *res));
ORIG(0x4B7D30, void, Palette_Free, (void *pal));
ORIG(0x4B7D50, void, Palette_Load, (void *pal, const char *path));
ORIG(0x4CEA90, int, Model_GetUnloadMethod_, (void));
ORIG(0x4CEAA0, void *, Mtp_FindChunk_, (const void *file, unsigned int id, unsigned int *size));
ORIG(0x4CF050, void, VirModel_InitAllLods_, (void));
ORIG(0x4D67A0, void, BoneObj_Load, (const char *path));

#define MTP_PALF 0x50414C46
#define MTP_BANM 0x42414E4D

extern char gModelPath[];
extern char gModelDir[];
extern char gModelAnimDir[];
extern RenderMode gRenderModeCurrent;

/* Palettes named in the PALF chunk, 0xC bytes each at f->unk3C. */
void ModelFile_LoadPalettes(ModelFile *f)
{
    const int *p = Mtp_FindChunk_(f->mtp, MTP_PALF, 0);
    const char *s;
    int n = *p, i;

    f->unk44 = n;
    if (n <= 0) {
        f->unk3C = NULL;
        return;
    }
    f->unk3C = Mem_Alloc(n * 0xC, 4);
    s = (const char *)(p + 1);
    for (i = 0; i < n; i++) {
        crt_sprintf(gModelPath, "%spalettes/%s.pal", gModelDir, s);
        Palette_Load(f->unk3C + i * 0xC, gModelPath);
        s += strlen(s) + 1;
    }
}
HOOK(0x4CF870, ModelFile_LoadPalettes);

/* Animations named in the BANM chunk, from the anim directory when one is set. */
void ModelFile_LoadAnims(ModelFile *f)
{
    const int *p = Mtp_FindChunk_(f->mtp, MTP_BANM, 0);
    const char *s, *dir;
    char path[64];
    int n;

    if (!p)
        return;
    n = *p;
    s = (const char *)(p + 1);
    dir = gModelAnimDir[0] ? gModelAnimDir : gModelDir;
    for (; n > 0; n--) {
        crt_sprintf(path, "%sanims/%s.iff", dir, s);
        BoneObj_Load(path);
        s += strlen(s) + 1;
    }
}
HOOK(0x4CF910, ModelFile_LoadAnims);

void CollMesh_Free(CollMeshRes *m);
void Mesh_Free(Mesh *m);

void ModelFile_Free(ModelFile *f)
{
    Node *n, *prev;
    int i;

    List_Remove_(&f->node);
    for (i = 0; i < f->unk44; i++)
        Palette_Free(f->unk3C + i * 0xC);
    if (f->unk3C) {
        Mem_Free(f->unk3C);
        f->unk3C = NULL;
    }
    for (i = 0; i < f->texture_count; i++)
        if (((int *)(f->texinfo + i * 0xC))[1])
            Palette_Free(f->texinfo + i * 0xC);
    if (f->texinfo) {
        Mem_Free(f->texinfo);
        f->texinfo = NULL;
    }
    for (i = f->texture_count - 1; i >= 0; i--)
        Texture_Free(f->textures + i * 0x24);
    if (f->textures) {
        Mem_Free(f->textures);
        f->textures = NULL;
    }
    n = f->coll.tailpred;
    for (prev = n->prev; prev; prev = prev->prev) {
        CollMesh_Free((CollMeshRes *)n);
        n = prev;
    }
    n = f->meshes.tailpred;
    for (prev = n->prev; prev; prev = prev->prev) {
        Mesh_Free((Mesh *)n);
        n = prev;
    }
    n = f->lods.tailpred;
    for (prev = n->prev; prev; prev = prev->prev) {
        List_Remove_(n);
        Mem_Free(n);
        n = prev;
    }
    Resource_Flush(f->res);
    if (!f->unk48)
        Resource_Flush(f->mtp);
    Mem_Free(f);
    VirModel_InitAllLods_();
}
HOOK(0x4CF9D0, ModelFile_Free);

void Mesh_Free(Mesh *m)
{
    struct {
        float *header;
        void *res;
    } arg;

    if (m->anim_count) {
        Mem_Free(m->anims);
        m->anims = NULL;
    }
    arg.header = (float *)m->header;
    arg.res = m->res;
    if (gRenderModeCurrent.methods[Model_GetUnloadMethod_()])
        gRenderModeCurrent.methods[Model_GetUnloadMethod_()](&arg);
    Resource_Flush(m->res);
    List_Remove_(&m->node);
    Mem_Free(m);
}
HOOK(0x4CFB30, Mesh_Free);

void CollMesh_Free(CollMeshRes *m)
{
    Resource_Flush(m->res);
    List_Remove_(&m->node);
    Mem_Free(m);
}
HOOK(0x4CFBB0, CollMesh_Free);

void Model_SetDir(const char *dir)
{
    strcpy(gModelDir, dir);
}
HOOK(0x4CFBD0, Model_SetDir);

void Model_SetAnimDir(const char *dir)
{
    strcpy(gModelAnimDir, dir);
}
HOOK(0x4CFC00, Model_SetAnimDir);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, A4EDF8, 1) char store_A4EDF8[128];
ALIAS(gModelAnimDir, store_A4EDF8, 0);
