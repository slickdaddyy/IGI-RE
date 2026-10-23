#include <string.h>

#include "hook.h"
#include "graphnode/graphnode.h"

ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, void *node));
ORIG(0x4AF960, void, List_Remove, (void *node));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B16D0, int, File_Read, (void *f, void *buf, int size));
ORIG(0x4B1700, int, File_Write, (void *f, const void *buf, int size));
ORIG(0x4B1730, int, File_Seek, (void *f, int pos, int whence));
ORIG(0x4B1760, int, File_Eof, (void *f));
ORIG(0x4B17C0, void, File_Flush, (void *f));
ORIG(0x4C1780, void, Debug_Log, (void *task, int level));
ORIG(0x4CE400, void *, World_GetMaterial, (void *world, int index));
ORIG(0x4FAA10, int, GraphStream_PeekByte, (GraphStream *s));

#define gStreamHandlers VAR(List, 0xA774A0)
#define gStreamHandlersFree VAR(List, 0xA76D18)
#define gCurrentWorld VAR(void *, 0xA44344)

#define GRAPH_MAGIC 0xFFEEDDCC

/* Values in graph files are a tag byte followed by the data. */
enum {
    TAG_UBYTE,
    TAG_BYTE,
    TAG_USHORT,
    TAG_SHORT,
    TAG_UINT,
    TAG_INT,
    TAG_FLOAT,
    TAG_DOUBLE,
    TAG_VEC3D,
    TAG_STRING,  /* length byte, text with its NUL */
    TAG_BLOB,    /* written as a UBYTE 10, an INT size and the data */
};

static void stream_write(GraphStream *s, const void *buf, int size)
{
    File_Flush(s->file);
    File_Write(s->file, buf, size);
}

static void stream_read(GraphStream *s, void *buf, int size)
{
    File_Flush(s->file);
    File_Read(s->file, buf, size);
}

char GraphStream_ReadShort(GraphStream *s, uint16_t *out);
char GraphStream_ReadDouble(GraphStream *s, double *out);
char GraphStream_ReadBlob(GraphStream *s, void *out, int size);

/* reads the value a GraphStream handler was called for */
void GraphStream_ReadValue(GraphStreamValue *v, void *out, int size)
{
    GraphStream_PeekByte(v->stream);
    switch (v->type) {
    case TAG_UBYTE:
    case TAG_BYTE:
        GraphStream_ReadByte(v->stream, out);
        break;
    case TAG_USHORT:
    case TAG_SHORT:
        GraphStream_ReadShort(v->stream, out);
        break;
    case TAG_UINT:
    case TAG_INT:
        GraphStream_ReadInt(v->stream, out);
        break;
    case TAG_FLOAT:
        GraphStream_ReadFloat(v->stream, out);
        break;
    case TAG_DOUBLE:
        GraphStream_ReadDouble(v->stream, out);
        break;
    case TAG_VEC3D:
        GraphStream_ReadVec3d(v->stream, out);
        break;
    case TAG_STRING:
        GraphStream_ReadString(v->stream, out, size);
        break;
    case TAG_BLOB:
        GraphStream_ReadBlob(v->stream, out, size);
        break;
    }
}
HOOK(0x4F9CD0, GraphStream_ReadValue);

int GraphStream_Hash(const char *name);

/* fn is called with a GraphStreamValue when a value with this name is read */
void GraphStream_AddHandler(const char *name, int type, void (*fn)(GraphStreamValue *v))
{
    int h = GraphStream_Hash(name);
    Node *n, *next;
    GraphStreamHandler *e;

    for (n = gStreamHandlers.head; (next = n->next); n = next) {
        if (((GraphStreamHandler *)n)->hash == h) {
            Warning_Show("GraphStream - collision in hash for %s", name);
            return;
        }
    }
    e = (GraphStreamHandler *)gStreamHandlersFree.head;
    if (!e->node.next)
        return;
    e->hash = h;
    e->type = type;
    e->fn = fn;
    List_Remove(e);
    e->node.prev = NULL;
    e->node.next = NULL;
    List_AddTail(&gStreamHandlers, e);
}
HOOK(0x4F9DF0, GraphStream_AddHandler);

int GraphStream_Hash(const char *name)
{
    int h = 0;
    unsigned int m = 1;
    char c;

    while ((c = *name++)) {
        if (c >= 'a' && c <= 'z')
            c -= 0x20;
        h += c * m;
        m = (m + 0x13D) & 0xFFFF;
    }
    return h;
}
HOOK(0x4F9E70, GraphStream_Hash);

char GraphStream_WriteByte(GraphStream *s, int8_t v);
char GraphStream_WriteUShort(GraphStream *s, uint16_t v);
char GraphStream_WriteShort(GraphStream *s, int16_t v);
char GraphStream_WriteUInt(GraphStream *s, uint32_t v);
char GraphStream_WriteFloat(GraphStream *s, float v);
char GraphStream_WriteDouble(GraphStream *s, double v);
char GraphStream_WriteVec3d(GraphStream *s, const Vec3d *v);
char GraphStream_WriteString(GraphStream *s, const char *v);
char GraphStream_WriteBlob(GraphStream *s, const void *data, int size);

/* the name hash, the type and the value */
void GraphStream_WriteValue(GraphStream *s, const char *name, int type, const void *value, int size)
{
    GraphStream_WriteUInt(s, GraphStream_Hash(name));
    GraphStream_WriteUByte(s, type);
    switch (type) {
    case TAG_UBYTE:
        GraphStream_WriteUByte(s, *(const uint8_t *)value);
        break;
    case TAG_BYTE:
        GraphStream_WriteByte(s, *(const int8_t *)value);
        break;
    case TAG_USHORT:
        GraphStream_WriteUShort(s, *(const uint16_t *)value);
        break;
    case TAG_SHORT:
        GraphStream_WriteShort(s, *(const int16_t *)value);
        break;
    case TAG_UINT:
        GraphStream_WriteUInt(s, *(const uint32_t *)value);
        break;
    case TAG_INT:
        GraphStream_WriteInt(s, *(const int *)value);
        break;
    case TAG_FLOAT:
        GraphStream_WriteFloat(s, *(const float *)value);
        break;
    case TAG_DOUBLE:
        GraphStream_WriteDouble(s, *(const double *)value);
        break;
    case TAG_VEC3D:
        GraphStream_WriteVec3d(s, value);
        break;
    case TAG_STRING:
        GraphStream_WriteString(s, value);
        break;
    case TAG_BLOB:
        GraphStream_WriteBlob(s, value, size);
        break;
    }
}
HOOK(0x4F9EB0, GraphStream_WriteValue);

GraphStreamHandler *GraphStream_Find(int hash);
void Graph_Read(Graph *g, GraphStream *s);
void GraphStream_Skip(GraphStream *s, int count);

/* Old files are a plain node and link list; new ones start with a magic
   number and hold named values for the registered handlers. */
void GraphOpen(Graph *g, const char *path)
{
    GraphStream s;
    GraphStreamValue v;
    uint32_t magic;
    uint8_t type = (uintptr_t)g; /* the original reads into its argument */

    s.file = File_Open(path, "r");
    if (!s.file) {
        Warning_Show("Unable to open %s for reading", path);
        return;
    }
    File_Read(s.file, &magic, 4);
    if (magic == GRAPH_MAGIC) {
        v.stream = &s;
        v.graph = g;
        v.unk8 = 0;
        v.unkC = 0;
        while (!File_Eof(s.file)) {
            int hash;
            GraphStreamHandler *h;

            GraphStream_ReadInt(&s, &hash);
            h = GraphStream_Find(hash);
            if (h) {
                GraphStream_ReadByte(&s, &type);
                v.type = type;
                h->fn(&v);
            } else {
                GraphStream_Skip(&s, 2);
            }
        }
        Debug_Log(g, 0);
    } else {
        File_Flush(s.file);
        File_Seek(s.file, 0, 0);
        Graph_Read(g, &s);
    }
    File_Flush(s.file);
    File_Close(s.file);
}
HOOK(0x4F9FF0, GraphOpen);

GraphStreamHandler *GraphStream_Find(int hash)
{
    Node *n, *next;

    for (n = gStreamHandlers.head; (next = n->next); n = next) {
        if (((GraphStreamHandler *)n)->hash == hash)
            return (GraphStreamHandler *)n;
    }
    return NULL;
}
HOOK(0x4FA140, GraphStream_Find);

/* the old format: counts, nodes, then links */
void Graph_Read(Graph *g, GraphStream *s)
{
    int nodes, links, i;
    void *material = NULL;
    TaskHandler fn;

    GraphStream_ReadInt(s, &nodes);
    GraphStream_ReadInt(s, &g->capacity);
    GraphStream_ReadInt(s, &links);
    if (g->nodes) {
        Mem_Free(g->nodes);
        g->nodes = NULL;
    }
    g->nodes = Mem_Alloc(Graph_GetNodeSize(g) * g->capacity, 4);
    Graph_ClearNodes(g, g->capacity);
    fn = TASK_HANDLER(Graph_GetAllocEvent(), g->task.type);
    if (fn)
        fn(g, NULL);
    for (i = 0; i < nodes; i++) {
        int id, mat = -1;
        Vec3d pos;
        float gamma, radius, unused;
        char criteria[0x100];
        GraphNode *n;

        GraphStream_ReadInt(s, &id);
        GraphStream_ReadVec3d(s, &pos);
        GraphStream_ReadFloat(s, &gamma);
        GraphStream_ReadFloat(s, &radius);
        if (GraphStream_PeekByte(s) == TAG_FLOAT)
            GraphStream_ReadFloat(s, &unused);
        if (GraphStream_PeekByte(s) == TAG_INT)
            GraphStream_ReadInt(s, &mat);
        if (mat != -1)
            material = World_GetMaterial(gCurrentWorld, mat);
        GraphStream_ReadString(s, criteria, sizeof(criteria));
        n = Graph_AddNode(g, id, &pos, gamma, radius, material, criteria);
        if (n) {
            struct {
                GraphNode *node;
                GraphStream *stream;
            } arg = {n, s};

            fn = TASK_HANDLER(Graph_GetNodeLoadEvent(), g->task.type);
            if (fn)
                fn(g, &arg);
        }
    }
    for (i = 0; i < links; i++) {
        int a, b, cost;

        GraphStream_ReadInt(s, &a);
        GraphStream_ReadInt(s, &b);
        GraphStream_ReadInt(s, &cost);
        Graph_AddLink(g, a, b, cost);
    }
    g->linkCount = links;
    g->nodeCount = nodes;
    fn = TASK_HANDLER(Graph_GetLoadEvent(), g->task.type);
    if (fn)
        fn(g, &s);
}
HOOK(0x4FA160, Graph_Read);

/* skips count values of any type */
void GraphStream_Skip(GraphStream *s, int count)
{
    /* the original reads these into its arguments, so they keep old values */
    uint8_t tag = count, len = (uintptr_t)s;
    int blob = 0;

    while (count--) {
        int size;

        if (File_Eof(s->file))
            return;
        stream_read(s, &tag, 1);
        switch (tag) {
        case TAG_UBYTE:
        case TAG_BYTE:
            size = 1;
            break;
        case TAG_USHORT:
        case TAG_SHORT:
            size = 2;
            break;
        case TAG_UINT:
        case TAG_INT:
        case TAG_FLOAT:
            size = 4;
            break;
        case TAG_DOUBLE:
            size = 8;
            break;
        case TAG_VEC3D:
            size = sizeof(Vec3d);
            break;
        case TAG_STRING:
            stream_read(s, &len, 1);
            size = len;
            break;
        case TAG_BLOB:
            stream_read(s, &blob, 4);
            size = blob;
            break;
        default:
            continue;
        }
        if (size) {
            File_Flush(s->file);
            File_Seek(s->file, size, 1);
        }
    }
}
HOOK(0x4FA3B0, GraphStream_Skip);

char GraphStream_WriteUByte(GraphStream *s, uint8_t v)
{
    uint8_t buf[2] = {TAG_UBYTE, v};

    stream_write(s, buf, 2);
    return 1;
}
HOOK(0x4FA4E0, GraphStream_WriteUByte);

char GraphStream_WriteByte(GraphStream *s, int8_t v)
{
    uint8_t buf[2] = {TAG_BYTE, v};

    stream_write(s, buf, 2);
    return 1;
}
HOOK(0x4FA520, GraphStream_WriteByte);

static char write_tagged(GraphStream *s, int tag, const void *v, int size)
{
    uint8_t buf[0x19];

    buf[0] = tag;
    memcpy(buf + 1, v, size);
    stream_write(s, buf, size + 1);
    return 1;
}

char GraphStream_WriteUShort(GraphStream *s, uint16_t v)
{
    return write_tagged(s, TAG_USHORT, &v, 2);
}
HOOK(0x4FA560, GraphStream_WriteUShort);

char GraphStream_WriteShort(GraphStream *s, int16_t v)
{
    return write_tagged(s, TAG_SHORT, &v, 2);
}
HOOK(0x4FA5A0, GraphStream_WriteShort);

char GraphStream_WriteUInt(GraphStream *s, uint32_t v)
{
    return write_tagged(s, TAG_UINT, &v, 4);
}
HOOK(0x4FA5E0, GraphStream_WriteUInt);

char GraphStream_WriteInt(GraphStream *s, int v)
{
    return write_tagged(s, TAG_INT, &v, 4);
}
HOOK(0x4FA620, GraphStream_WriteInt);

char GraphStream_WriteFloat(GraphStream *s, float v)
{
    return write_tagged(s, TAG_FLOAT, &v, 4);
}
HOOK(0x4FA660, GraphStream_WriteFloat);

char GraphStream_WriteDouble(GraphStream *s, double v)
{
    return write_tagged(s, TAG_DOUBLE, &v, 8);
}
HOOK(0x4FA6A0, GraphStream_WriteDouble);

char GraphStream_WriteVec3d(GraphStream *s, const Vec3d *v)
{
    return write_tagged(s, TAG_VEC3D, v, sizeof(Vec3d));
}
HOOK(0x4FA6E0, GraphStream_WriteVec3d);

char GraphStream_WriteString(GraphStream *s, const char *v)
{
    uint8_t buf[0x104];
    int len = strlen(v) + 1;

    buf[0] = TAG_STRING;
    buf[1] = len;
    memcpy(buf + 2, v, len);
    stream_write(s, buf, len + 2);
    return 1;
}
HOOK(0x4FA730, GraphStream_WriteString);

char GraphStream_WriteBlob(GraphStream *s, const void *data, int size)
{
    GraphStream_WriteUByte(s, TAG_BLOB);
    GraphStream_WriteInt(s, size);
    stream_write(s, data, size);
    return 1;
}
HOOK(0x4FA7A0, GraphStream_WriteBlob);

char GraphStream_ReadByte(GraphStream *s, uint8_t *out)
{
    uint8_t buf[2];

    stream_read(s, buf, 2);
    *out = buf[1];
    return 1;
}
HOOK(0x4FA7E0, GraphStream_ReadByte);

char GraphStream_ReadShort(GraphStream *s, uint16_t *out)
{
    uint8_t buf[3];

    stream_read(s, buf, 3);
    memcpy(out, buf + 1, 2);
    return 1;
}
HOOK(0x4FA810, GraphStream_ReadShort);

char GraphStream_ReadInt(GraphStream *s, int *out)
{
    uint8_t buf[5];

    stream_read(s, buf, 5);
    memcpy(out, buf + 1, 4);
    return 1;
}
HOOK(0x4FA850, GraphStream_ReadInt);

/* loads and stores through the FPU like the original */
char GraphStream_ReadFloat(GraphStream *s, float *out)
{
    uint8_t buf[5];
    float v;

    stream_read(s, buf, 5);
    memcpy(&v, buf + 1, 4);
    __asm__("flds %1\n\tfstps %0" : "=m"(*out) : "m"(v));
    return 1;
}
HOOK(0x4FA890, GraphStream_ReadFloat);

char GraphStream_ReadDouble(GraphStream *s, double *out)
{
    uint8_t buf[9];
    double v;

    stream_read(s, buf, 9);
    memcpy(&v, buf + 1, 8);
    __asm__("fldl %1\n\tfstpl %0" : "=m"(*out) : "m"(v));
    return 1;
}
HOOK(0x4FA8D0, GraphStream_ReadDouble);

char GraphStream_ReadVec3d(GraphStream *s, Vec3d *out)
{
    uint8_t buf[0x19];

    stream_read(s, buf, 0x19);
    memcpy(out, buf + 1, sizeof(Vec3d));
    return 1;
}
HOOK(0x4FA910, GraphStream_ReadVec3d);

/* max is not checked; the text is copied without adding a NUL */
char GraphStream_ReadString(GraphStream *s, char *out, int max)
{
    uint8_t buf[0x102];
    int len;

    stream_read(s, buf, 2);
    len = buf[1];
    stream_read(s, buf + 2, len);
    memcpy(out, buf + 2, len);
    return 1;
}
HOOK(0x4FA950, GraphStream_ReadString);

/* the stored size is skipped, size bytes are read */
char GraphStream_ReadBlob(GraphStream *s, void *out, int size)
{
    uint8_t tag;
    int stored;

    GraphStream_ReadByte(s, &tag);
    GraphStream_ReadInt(s, &stored);
    stream_read(s, out, size);
    return 1;
}
HOOK(0x4FA9D0, GraphStream_ReadBlob);
