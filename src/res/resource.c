#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "res/resource.h"

ORIG(0x4AF8F0, void, List_AddTail, (List *list, Node *node));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, int align));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4AD73D, int, crt_toupper, (int c));
ORIG(0x4AAD10, int, crt_strncmp, (const char *a, const char *b, size_t n));
ORIG(0x4B1020, int, File_ResolvePath, (char *out, const char *name, int index));
ORIG(0x4B1420, void, File_ScanDir, (const char *name, void (*fn)(), void *user));
ORIG(0x4B1820, int, File_GetTime, (int *time, const char *name));
ORIG(0x4B18B0, void *, File_Load, (const char *name, int *size));
ORIG(0x4B1A30, void, File_Unload, (void *data));
ORIG(0x4B1FB0, char *, File_GetExpandedPath, (void));
ORIG(0x4B1FC0, int, File_Exists, (const char *name));
ORIG(0x5098F0, int, File_CompareTime, (const int *a, const int *b));

extern Pool gResourcePool;
extern List gResourceList;
extern int gResourceUnk;
extern char gMemberName[];

static Resource *Pool_New(void)
{
    Pool *p = &gResourcePool;
    char *obj = p->base + p->order[p->used] * p->stride;

    *(int *)(obj + p->size) = p->used;
    p->used++;
    return (Resource *)obj;
}

static void Pool_Delete(void *obj)
{
    Pool *p = &gResourcePool;
    int pos = *(int *)((char *)obj + p->size);
    char *last;
    int lastpos, t;

    p->used--;
    if (pos == p->used)
        return;
    last = p->base + p->order[p->used] * p->stride;
    lastpos = *(int *)(last + p->size);
    t = p->order[pos];
    p->order[pos] = p->order[lastpos];
    p->order[lastpos] = t;
    *(int *)(last + p->size) = pos;
}

static void Path_FixSlashes(char *s)
{
    for (; *s; s++)
        *s = *s == '\\' ? '/' : *s;
}

void String_ToUpper(char *dst, const char *src);
unsigned int Resource_Hash(const char *name);

Resource *Resource_Create(const char *name, const char *info, void *data, int size, int type, const int *time)
{
    Resource *r = Pool_New();

    String_ToUpper(r->name, name);
    Path_FixSlashes(r->name);
    r->data = data ? data : r;
    r->size = size;
    r->type = type;
    r->unk30 = 1;
    r->used = 0;
    r->info = info;
    r->hash = Resource_Hash(r->name);
    r->node.prev = NULL;
    r->node.next = NULL;
    r->child.prev = NULL;
    r->child.next = NULL;
    r->children.head = (Node *)&r->children.tail;
    r->children.tailpred = (Node *)&r->children;
    r->children.tail = NULL;
    List_AddTail(&gResourceList, &r->node);
    r->self = r;
    if (time) {
        r->time[0] = time[0];
        r->time[1] = time[1];
    } else {
        r->time[0] = 0;
        r->time[1] = 0;
    }
    return r;
}
HOOK(0x4B59F0, Resource_Create);

void String_ToUpper(char *dst, const char *src)
{
    for (; *src; src++)
        *dst++ = (char)crt_toupper(*src);
    *dst = 0;
}
HOOK(0x4B5AF0, String_ToUpper);

unsigned int Resource_Hash(const char *name)
{
    unsigned int h = 1;

    for (; *name; name++)
        h = (h + 1) * *name;
    return h % 0x3037;
}
HOOK(0x4B5B30, Resource_Hash);

int Resource_Exists(const char *name)
{
    if (Resource_IsLoaded(name, NULL))
        return 1;
    return File_Exists(name);
}
HOOK(0x4B5B60, Resource_Exists);

int Resource_IsLoaded(const char *name, Resource **out)
{
    char path[128], upper[128];
    int next = 0;

    do {
        Resource *r;
        unsigned int h;

        next = File_ResolvePath(path, name, next);
        String_ToUpper(upper, path);
        Path_FixSlashes(upper);
        h = Resource_Hash(upper);
        for (r = (Resource *)gResourceList.tailpred; r->node.prev; r = (Resource *)r->node.prev) {
            if (r->hash == h && strcmp(r->name, upper) == 0) {
                if (out)
                    *out = r;
                return 1;
            }
        }
    } while (next);
    return 0;
}
HOOK(0x4B5B90, Resource_IsLoaded);

void Resource_Free(Resource *r)
{
    Node *n, *next;

    switch (r->type) {
    case RES_PACK:
        for (n = r->children.head; (next = n->next) != NULL; n = next)
            Resource_Free(((Resource **)n)[2]);
        List_Remove(&r->node);
        File_Unload(r->data);
        break;
    case RES_MEMBER:
    case RES_PATH:
        List_Remove(&r->child);
        List_Remove(&r->node);
        break;
    case RES_FILE:
        File_Unload(r->data);
        List_Remove(&r->node);
        break;
    case RES_USER:
        List_Remove(&r->node);
        break;
    default:
        return;
    }
    Pool_Delete(r);
}
HOOK(0x4B5C90, Resource_Free);

typedef struct ResourceFindInfo {
    char *name;
    int size;
    char unk8;
} ResourceFindInfo;

void Resource_ForEach(const char *name, void (*fn)(ResourceFindInfo *info, void *user), void *user)
{
    Resource *r, *m;
    ResourceFindInfo info;
    char buf[512];
    const char *s;

    if (!Resource_IsLoaded(name, &r) || r->type != RES_PATH) {
        File_ScanDir(name, (void (*)())fn, user);
        return;
    }
    s = r->data;
    info.name = buf;
    info.unk8 = 0;
    while (*s) {
        int skip = 0, len = 0;

        while (s[skip]) {
            if (s[skip] == ';') {
                skip++;
                break;
            }
            buf[len++] = s[skip++];
        }
        buf[len] = 0;
        if (Resource_IsLoaded(buf, &m)) {
            info.size = m->size;
            fn(&info, user);
        }
        s += skip;
    }
}
HOOK(0x4B5D80, Resource_ForEach);

void Resource_Init(void)
{
    gResourceList.head = (Node *)&gResourceList.tail;
    gResourceList.tailpred = (Node *)&gResourceList;
    gResourceList.tail = NULL;
    Pool_Init(&gResourcePool, sizeof(Resource), 10000, 0);
    gResourceUnk = 0;
}
HOOK(0x4B5E70, Resource_Init);

void Resource_FreeAll(void);

void Resource_Shutdown(void)
{
    Resource_FreeAll();
    Pool_Free(&gResourcePool);
}
HOOK(0x4B5EC0, Resource_Shutdown);

void Resource_FreeAll(void)
{
    while (gResourceList.head->next)
        Resource_Free((Resource *)gResourceList.head);
}
HOOK(0x4B5EE0, Resource_FreeAll);

void *Resource_Add(const char *path, void *data, int size);

void *Resource_Load(const char *name, int *size)
{
    Resource *r;
    void *data;
    int n;

    if (Resource_IsLoaded(name, &r)) {
        if (size)
            *size = r->size;
        r->used = 1;
        return r->data;
    }
    data = File_Load(name, &n);
    if (!data) {
        Error_Show("Failed to load resource: '%s'", name);
        for (;;)
            ;
    }
    if (size)
        *size = n;
    return Resource_Add(File_GetExpandedPath(), data, n);
}
HOOK(0x4B5F00, Resource_Load);

int Resource_AddChunks(const char *path, Resource *pack, Chunk *form, const int *time);

/* The member headers of a "RESOURCE" pack */
typedef struct PackEntry {
    int unk0;
    int size;
    int namelen;
    char name[];
} PackEntry;

void *Resource_Add(const char *path, void *data, int size)
{
    int time[2];
    Resource *r;

    File_GetTime(time, path);
    if (Chunk_Id(data) == CHUNK_ID('I', 'L', 'F', 'F') && Chunk_FormType(data) == CHUNK_ID('I', 'R', 'E', 'S')) {
        r = Resource_Create(path, NULL, data, 0, RES_PACK, time);
        Resource_AddChunks(path, r, data, time);
        r->used = 1;
        return data;
    }
    if ((unsigned int)size >= 8 && crt_strncmp(data, "RESOURCE", 8) == 0) {
        unsigned int count;
        char *p = (char *)data + 0x20;

        r = Resource_Create(path, NULL, data, 0, RES_PACK, time);
        r->used = 1;
        for (count = ((unsigned int *)data)[2]; count; count--) {
            PackEntry *e = (PackEntry *)p;
            int len = e->size;
            char *body;
            Resource *m;

            memcpy(gMemberName, e->name, e->namelen);
            gMemberName[e->namelen] = 0;
            body = p + ((e->namelen + 0x2B) & ~0x1F);
            p = body + ((len + 0x1F) & ~0x1F);
            m = Resource_Create(gMemberName, NULL, body, len, RES_MEMBER, time);
            List_AddTail(&r->children, &m->child);
        }
        return data;
    }
    r = Resource_Create(path, NULL, data, size, RES_FILE, time);
    r->used = 1;
    return data;
}
HOOK(0x4B5F80, Resource_Add);

static Resource *Resource_AddMember(Resource *pack, const char *name, const char *info, void *data, int size, int type, const int *time)
{
    Resource *m = Resource_Create(name, info, data, size, type, time);

    List_AddTail(&pack->children, &m->child);
    return m;
}

int Resource_AddChunks(const char *path, Resource *pack, Chunk *form, const int *time)
{
    Chunk *c;
    unsigned int size, align;

    for (c = Chunk_FirstChild(form); c; c = Chunk_Next(c)) {
        if (Chunk_Id(c) == CHUNK_ID('N', 'A', 'M', 'E')) {
            const char *name = Chunk_Data(c, form);
            const char *info = NULL;
            Chunk *dbg, *s;

            c = Chunk_Next(c);
            if (!c)
                goto error;
            dbg = Chunk_Next(c);
            if (dbg && Chunk_Id(dbg) == CHUNK_ID('I', 'L', 'F', 'F') && Chunk_FormType(dbg) == CHUNK_ID('D', 'B', 'U', 'G')) {
                for (s = Chunk_FirstChild(dbg); s; s = Chunk_Next(s))
                    if (Chunk_Id(s) == CHUNK_ID('C', 'S', 'T', 'R'))
                        info = Chunk_Data(s, dbg);
            }
            if (Chunk_Id(c) == CHUNK_ID('B', 'O', 'D', 'Y')) {
                Chunk_GetInfo(&size, &align, c);
                Resource_AddMember(pack, name, info, Chunk_Data(c, form), size, RES_MEMBER, time);
            } else if (Chunk_Id(c) == CHUNK_ID('I', 'L', 'F', 'F') && Chunk_FormType(c) == CHUNK_ID('I', 'R', 'E', 'S')) {
                Chunk_GetInfo(&size, &align, c);
                Resource_AddMember(pack, name, info, c, 0, RES_MEMBER, time);
                if (!Resource_AddChunks(path, pack, c, time))
                    return 0;
            } else if (Chunk_Id(c) == CHUNK_ID('C', 'S', 'T', 'R')) {
                Chunk_GetInfo(&size, &align, c);
                Resource_AddMember(pack, name, NULL, Chunk_Data(c, form), size, RES_MEMBER, time);
            } else if (Chunk_Id(c) == CHUNK_ID('P', 'A', 'T', 'H')) {
                Chunk_GetInfo(&size, &align, c);
                Resource_AddMember(pack, name, info, Chunk_Data(c, form), size, RES_PATH, time);
            } else {
                goto error;
            }
        } else if (Chunk_Id(c) != CHUNK_ID('I', 'L', 'F', 'F') || Chunk_FormType(c) != CHUNK_ID('D', 'B', 'U', 'G')) {
            goto error;
        }
    }
    return 1;
error:
    Error_Show("Error in resource file: '%s'", path);
    for (;;)
        ;
}
HOOK(0x4B60F0, Resource_AddChunks);

void Resource_Unload(const char *name)
{
    Resource *r;

    if (!Resource_IsLoaded(name, &r)) {
        Error_Show("Resource_Unload() : Resource '%s' is not loaded.", name);
        for (;;)
            ;
    }
    if (r->type != RES_USER && r->type != RES_MEMBER)
        Resource_Free(r);
}
HOOK(0x4B6380, Resource_Unload);

void Resource_Flush(void *data)
{
    Resource *r;

    for (r = (Resource *)gResourceList.head; r->node.next; r = (Resource *)r->node.next) {
        if (r->data == data) {
            if (r->type != RES_USER && r->type != RES_MEMBER)
                Resource_Free(r);
            return;
        }
    }
    Error_Show("Resource_Flush() : Couldn't locate resource at %p.", data);
    for (;;)
        ;
}
HOOK(0x4B63D0, Resource_Flush);

Resource *Resource_Register(const char *name, int size, void *data)
{
    return Resource_Create(name, NULL, data, size, RES_USER, NULL);
}
HOOK(0x4B6420, Resource_Register);

int Resource_GetTime(int *time, const char *name)
{
    Resource *r;

    if (Resource_IsLoaded(name, &r)) {
        time[0] = r->time[0];
        time[1] = r->time[1];
        return 0;
    }
    return File_GetTime(time, name);
}
HOOK(0x4B6440, Resource_GetTime);

int Resource_CompareTime(const int *a, const int *b)
{
    return File_CompareTime(a, b);
}
HOOK(0x4B6490, Resource_CompareTime);

void Resource_Release(const char *name)
{
    Resource *r;

    if (Resource_IsLoaded(name, &r))
        Resource_Free(r);
}
HOOK(0x4B64B0, Resource_Release);

int Resource_Find(const char *name)
{
    return Resource_IsLoaded(name, NULL);
}
HOOK(0x4B64E0, Resource_Find);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(943B90) int gResourceUnk;
PAD(igb, 943C20, 943C20, 0x200);
ALIAS(gMemberName, pad_943C20, 0);
BSS(943C20) Pool gResourcePool;
BSS(943C20) List gResourceList;
