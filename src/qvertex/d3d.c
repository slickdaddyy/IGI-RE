#pragma GCC optimize("excess-precision=standard")
#include <string.h>
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <dinput.h>

#include "hook.h"
#include "qvertex/qvertex.h"
#include "res/restypes.h"
#include "transctx/transctxtypes.h"

/* The QVertex driver of the Direct3D render mode. */

ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x497800, void *, Frame_Alloc, (int size));
ORIG(0x497860, void, Primitive_Draw, (void *prim));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));
ORIG(0x4B72E0, void *, SortRender_Add, (void (*fn)(void *), int size, uint32_t key));

ORIG(0x4B7430, int, PtrList_Add, (PtrList *list, void *fn));

/* A vertex on its way through the clipper: colours and uv are divided by z. */
typedef struct ClipVert {
    float x, y, w;
    float u, v;
    float a, r, g, b;
    unsigned int code;
} ClipVert;

typedef void (*ClipFn)(ClipVert **v, QVertexStrip *s);

void QVertex_ClipTriangle(ClipVert a, ClipVert b, ClipVert c, float nearZ, ClipFn fn, QVertexStrip *s);

typedef struct StripNode {
    void *next;
    int method;
    int unk08;
    QTexture *texture;
    QPalette *palette;
    unsigned int flags;
    uint8_t alpha;
    int count;
    int lines;
    TLVertex *verts;
    TLVertex v[];
} StripNode;

_Static_assert(sizeof(StripNode) == 0x28, "StripNode");

/* what the render state functions get */
typedef struct StripState {
    unsigned int flags;
    QTexture *texture;
} StripState;

extern IDirect3DDevice7 *gD3DDevice;
extern IDirect3DVertexBuffer7 *gVBTL;
extern int gVBTLSize;
extern int gVBTLUsed;
extern DWORD gD3DZEnable;
extern int gD3DClippingOnIndex;
extern PtrList *gD3DTextureStates;
extern PtrList *gD3DBlendStates;
extern PtrList *gD3DRenderStates;
extern PtrList *gD3DClipStates;
extern float gLayer1MaxZ;
extern float gD3DMinZ;
extern float gLayer1MinZ;
extern float gD3DMaxZ;
extern float gNearPlane;
extern float gWorldScale;
extern TransContext gTransContext;
extern float gSceneMinX;
extern float gSceneMinY;
extern float gSceneMaxX;
extern float gSceneMaxY;
extern int gTransContextValid;
extern int gSortActive;

extern StripNode gQVStripBuffer[];
extern int gQVTextureStatesIndex;
extern int gQVRenderStatesIndex;
extern int gQVBlendStatesIndex;
extern unsigned int gQVLastFlags;
extern QTexture *gQVLastTexture;
extern int gClipDepth;
extern const uint8_t gKeys[];

QV_F32(kZero, 0x00000000);
QV_F32(kOne, 0x3F800000);
QV_F32(kHalf, 0x3F000000);
QV_F32(kQuarter, 0x3E800000);
QV_F32(k255, 0x437F0000);
QV_F32(kMinus255, 0xC37F0000);
QV_F32(kNearMul, 0x3F8147AE);
QV_F32(kTexelOffset, 0x3A800000);
QV_F32(kDebugZScale, 0x3F7D70A4);
QV_F32(kDebugZBias, 0x4083126F);

void D3DQVertex_DrawStrip(StripNode *n);
void D3DQVertex_Emit(QVertexStrip *s);
void D3DQVertex_EmitLine(ClipVert **v, QVertexStrip *s);
void D3DQVertex_EmitTriangle(ClipVert **v, QVertexStrip *s);

/* Clipping against the scene rectangle (outcode bits 1 left, 2 top, 4 right,
   8 bottom). Each edge either moves the two outside corners onto it, or
   moves the one outside corner and hands the extra triangle of the
   resulting quad to a recursive call. The rounding of the cut parameter
   and of the new outcodes differs from edge to edge, as in the original.
   nearZ is only passed on. */

static void ClipVert_Lerp(ClipVert *p, const ClipVert *a, long double t)
{
    p->x = ((long double)p->x - a->x) * t + a->x;
    p->y = ((long double)p->y - a->y) * t + a->y;
    p->w = ((long double)p->w - a->w) * t + a->w;
    p->r = ((long double)p->r - a->r) * t + a->r;
    p->g = ((long double)p->g - a->g) * t + a->g;
    p->b = ((long double)p->b - a->b) * t + a->b;
    p->u = ((long double)p->u - a->u) * t + a->u;
    p->v = ((long double)p->v - a->v) * t + a->v;
    p->a = ((long double)p->a - a->a) * t + a->a;
}

/* Lerp of everything but x and y, which the caller keeps unrounded. */
static void ClipVert_LerpRest(ClipVert *p, const ClipVert *a, long double t)
{
    p->w = ((long double)p->w - a->w) * t + a->w;
    p->r = ((long double)p->r - a->r) * t + a->r;
    p->g = ((long double)p->g - a->g) * t + a->g;
    p->b = ((long double)p->b - a->b) * t + a->b;
    p->u = ((long double)p->u - a->u) * t + a->u;
    p->v = ((long double)p->v - a->v) * t + a->v;
    p->a = ((long double)p->a - a->a) * t + a->a;
}

static long double MaxX(void)
{
    return (long double)gSceneMaxX + kOne.f;
}

static long double MaxY(void)
{
    return (long double)gSceneMaxY + kOne.f;
}

static float CutLeft(const ClipVert *p, const ClipVert *a)
{
    return ((long double)gSceneMinX - a->x) / ((long double)p->x - a->x);
}

static long double CutTop(const ClipVert *p, const ClipVert *a)
{
    return ((long double)gSceneMinY - a->y) / ((long double)p->y - a->y);
}

static float CutRight(const ClipVert *p, const ClipVert *a)
{
    return (MaxX() - a->x) / ((long double)p->x - a->x);
}

static long double CutBottom(const ClipVert *p, const ClipVert *a)
{
    return (MaxY() - a->y) / ((long double)p->y - a->y);
}

/* outcode of a vertex that is now inside the left edge */
static unsigned int CodeAfterLeft(const ClipVert *p)
{
    unsigned int top = !(p->y >= gSceneMinY), right = !(MaxX() >= p->x), bottom = !(MaxY() >= p->y);

    return ((bottom * 2 | right) * 2 | top) * 2;
}

static unsigned int CodeAfterTop(const ClipVert *p)
{
    unsigned int right = !(MaxX() >= p->x), bottom = !(MaxY() >= p->y);

    return (bottom * 2 | right) * 4;
}

static unsigned int CodeAfterRight(const ClipVert *p)
{
    return (unsigned)!(MaxY() >= p->y) * 8;
}

/* The corner that is out: p, the others follow it in order. */
static void PickOne(ClipVert *a, ClipVert *b, ClipVert *c, unsigned int bit, ClipVert **p, ClipVert **n1, ClipVert **n2)
{
    if (a->code & bit) {
        *p = a, *n1 = b, *n2 = c;
    } else if (b->code & bit) {
        *p = b, *n1 = c, *n2 = a;
    } else {
        *p = c, *n1 = a, *n2 = b;
    }
}

/* The corner that is in: in, the others follow it in order. */
static void PickTwo(ClipVert *a, ClipVert *b, ClipVert *c, unsigned int bit, ClipVert **in, ClipVert **p, ClipVert **q)
{
    if (!(a->code & bit)) {
        *in = a, *p = b, *q = c;
    } else if (!(b->code & bit)) {
        *in = b, *p = c, *q = a;
    } else {
        *in = c, *p = a, *q = b;
    }
}

static int CountOut(const ClipVert *a, const ClipVert *b, const ClipVert *c, unsigned int bit)
{
    return ((a->code & bit) + (b->code & bit) + (c->code & bit)) / bit;
}

void QVertex_ClipTriangle(ClipVert a, ClipVert b, ClipVert c, float nearZ, ClipFn fn, QVertexStrip *s)
{
    ClipVert *v[3], *p, *q, *n1, *n2, t;
    unsigned int any, all;
    long double tx, ty, cut;

    gClipDepth++;
    any = a.code | b.code | c.code;
    all = a.code & b.code & c.code;
    if (all || (any & 0x10))
        goto done;

    if (any & 1) {
        if (CountOut(&a, &b, &c, 1) == 2) {
            PickTwo(&a, &b, &c, 1, &n1, &p, &q);
            ClipVert_Lerp(p, n1, CutLeft(p, n1));
            p->code = CodeAfterLeft(p);
            ClipVert_Lerp(q, n1, CutLeft(q, n1));
            q->code = CodeAfterLeft(q);
        } else {
            PickOne(&a, &b, &c, 1, &p, &n1, &n2);
            t = *p;
            ClipVert_Lerp(p, n1, CutLeft(p, n1));
            p->code = CodeAfterLeft(p);
            cut = CutLeft(&t, n2);
            tx = ((long double)t.x - n2->x) * cut + n2->x;
            ty = ((long double)t.y - n2->y) * cut + n2->y;
            t.x = tx;
            t.y = ty;
            ClipVert_LerpRest(&t, n2, cut);
            t.code = (((unsigned)!(MaxY() >= t.y) * 2 | (tx > MaxX())) * 2 | !(ty >= gSceneMinY)) * 2;
            QVertex_ClipTriangle(*n2, t, *p, nearZ, fn, s);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
        if (all)
            goto done;
    }

    if (any & 2) {
        if (CountOut(&a, &b, &c, 2) == 2) {
            PickTwo(&a, &b, &c, 2, &n1, &p, &q);
            ClipVert_Lerp(p, n1, CutTop(p, n1));
            p->code = CodeAfterTop(p);
            ClipVert_Lerp(q, n1, CutTop(q, n1));
            q->code = CodeAfterTop(q);
        } else {
            PickOne(&a, &b, &c, 2, &p, &n1, &n2);
            t = *p;
            ClipVert_Lerp(p, n1, CutTop(p, n1));
            p->code = CodeAfterTop(p);
            cut = (float)(((long double)gSceneMinY - n2->y) / ((long double)t.y - n2->y));
            ty = ((long double)t.y - n2->y) * cut + n2->y;
            t.x = ((long double)t.x - n2->x) * cut + n2->x;
            t.y = ty;
            ClipVert_LerpRest(&t, n2, cut);
            t.code = ((unsigned)(ty > MaxY()) * 2 | !(MaxX() >= t.x)) * 4;
            QVertex_ClipTriangle(*n2, t, *p, nearZ, fn, s);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
        if (all)
            goto done;
    }

    if (any & 4) {
        if (CountOut(&a, &b, &c, 4) == 2) {
            PickTwo(&a, &b, &c, 4, &n1, &p, &q);
            ClipVert_Lerp(p, n1, CutRight(p, n1));
            p->code = CodeAfterRight(p);
            ClipVert_Lerp(q, n1, CutRight(q, n1));
            q->code = CodeAfterRight(q);
        } else {
            PickOne(&a, &b, &c, 4, &p, &n1, &n2);
            t = *p;
            ClipVert_Lerp(p, n1, CutRight(p, n1));
            p->code = CodeAfterRight(p);
            cut = CutRight(&t, n2);
            ty = ((long double)t.y - n2->y) * cut + n2->y;
            t.x = ((long double)t.x - n2->x) * cut + n2->x;
            t.y = ty;
            ClipVert_LerpRest(&t, n2, cut);
            t.code = (unsigned)(ty > MaxY()) * 8;
            QVertex_ClipTriangle(*n2, t, *p, nearZ, fn, s);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
        if (all)
            goto done;
    }

    if (any & 8) {
        if (CountOut(&a, &b, &c, 8) == 2) {
            PickTwo(&a, &b, &c, 8, &n1, &p, &q);
            ClipVert_Lerp(p, n1, CutBottom(p, n1));
            p->code = 0;
            ClipVert_Lerp(q, n1, CutBottom(q, n1));
            q->code = 0;
        } else {
            PickOne(&a, &b, &c, 8, &p, &n1, &n2);
            t = *p;
            ClipVert_Lerp(p, n1, CutBottom(p, n1));
            p->code = 0;
            ClipVert_Lerp(&t, n2, (float)((MaxY() - n2->y) / ((long double)t.y - n2->y)));
            t.code = 0;
            QVertex_ClipTriangle(*n2, t, *p, nearZ, fn, s);
        }
        if (a.code & b.code & c.code)
            goto done;
    }

    v[0] = &a;
    v[1] = &b;
    v[2] = &c;
    fn(v, s);

    if ((gKeys[DIK_LCONTROL] & 0x80) && (gKeys[DIK_LSHIFT] & 0x80) && (gKeys[DIK_6] & 0x80)) {
        /* debug view: outline the clipped triangle */
        QVertexStrip l;
        QVertex buf[4];
        ClipVert *cv[3] = { &a, &b, &c };
        int i;

        a.w = kOne.f / a.w;
        b.w = kOne.f / b.w;
        c.w = kOne.f / c.w;
        for (i = 0; i < 3; i++) {
            buf[i].x = cv[i]->x;
            buf[i].y = cv[i]->y;
            buf[i].z = cv[i]->w * kDebugZScale.f - kDebugZBias.f;
            buf[i].r = kHalf.f;
            buf[i].g = kHalf.f;
            buf[i].b = kOne.f;
            buf[i].a = kOne.f;
            buf[i].u = 0;
            buf[i].v = 0;
        }
        l.verts = buf;
        l.flags = 0x40060000;
        l.lines = 0;
        l.queue = -1;
        l.count = 3;
        gQVertexTexture = NULL;
        D3DQVertex_Emit(&l);
        for (i = 0; i < 3; i++) {
            buf[i].z = buf[i].z * kDebugZScale.f;
            buf[i].r = 0;
            buf[i].g = 0;
            buf[i].b = kOne.f;
        }
        buf[3] = buf[0];
        l.lines = 1;
        l.count = 4;
        D3DQVertex_Emit(&l);
    }
done:
    gClipDepth--;
}
HOOK(0x50BF20, QVertex_ClipTriangle);

/* Turns a strip into a TL vertex node and queues, sorts or draws it. */
void D3DQVertex_Emit(QVertexStrip *s)
{
    StripNode *n;
    int sorted, i;
    uint32_t c;

    if ((s->flags & 0x60000) && gSortActive && !(s->flags & 0x40000000)) {
        sorted = 1;
        n = SortRender_Add((void (*)(void *))D3DQVertex_DrawStrip, s->count * sizeof(TLVertex) + sizeof(StripNode),
                           QV_Bits(&s->verts[0].z));
    } else {
        sorted = 0;
        if (s->queue < 0)
            n = gQVStripBuffer;
        else
            n = Frame_Alloc(s->count * sizeof(TLVertex) + sizeof(StripNode));
    }
    if (n) {
        n->method = gQVertexDrawStrip;
        n->texture = gQVertexTexture;
        n->palette = gQVertexPalette;
        n->flags = s->flags;
        n->count = s->count;
        n->lines = s->lines;
        n->verts = n->v;
        n->alpha = QV_Fist(s->verts[0].a * (long double)k255.f);
        for (i = 0; i < n->count; i++) {
            const QVertex *v = &s->verts[i];
            TLVertex *d = &n->verts[i];
            float a;

            d->x = v->x - kHalf.f;
            d->y = v->y - kHalf.f;
            if (v->z > kZero.f && gTransContextValid) {
                long double lo = (long double)gD3DMinZ * kNearMul.f, z, rhw;
                float t = gWorldScale * v->z;

                if (lo > t)
                    z = lo;
                else if (!(gD3DMaxZ >= t))
                    z = gD3DMaxZ;
                else
                    z = t;
                rhw = kOne.f / z;
                d->rhw = rhw;
                d->z = ((long double)gLayer1MaxZ - gLayer1MinZ) * ((kOne.f - rhw * gD3DMinZ) * gTransContext.proj[10]) + gLayer1MinZ;
            } else {
                d->z = kHalf.f;
                d->rhw = kOne.f / (((long double)gD3DMaxZ - gD3DMinZ) * kHalf.f);
            }
            a = v->a;
            if (n->flags & 4)
                a = kHalf.f;
            else if (n->flags & 0x20)
                a = kQuarter.f;
            if (n->flags & 0x10880024) {
                c = (uint32_t)QV_Fist(a * (long double)kMinus255.f) << 8;
                c = (c + QV_Fist(v->r * (long double)kMinus255.f)) << 8;
                c = (c + QV_Fist(v->g * (long double)kMinus255.f)) << 8;
                c = QV_Fist(v->b * (long double)k255.f) - c;
            } else {
                c = (uint32_t)(0xFF00 - QV_Fist(v->r * (long double)kMinus255.f)) << 8;
                c = (c - QV_Fist(v->g * (long double)kMinus255.f)) << 8;
                c = c - QV_Fist(v->b * (long double)kMinus255.f);
            }
            d->color = c;
            if (n->texture) {
                d->u = n->texture->uScale * v->u + kTexelOffset.f;
                d->v = n->texture->vScale * v->v + kTexelOffset.f;
            }
        }
    }
    if (!sorted) {
        if (s->queue < 0)
            Primitive_Draw(n);
        else
            RenderQueue_Add(s->queue, n);
    }
}
HOOK(0x50D4C0, D3DQVertex_Emit);

static void RunState(PtrList *l, int index, StripState *st)
{
    if (l->current != index) {
        l->current = index;
        ((void (*)(void *))l->items[index])(st);
    }
}

/* Locks room for count vertices; returns the first index or -1. */
static int LockVerts(int count, TLVertex **out)
{
    HRESULT hr;
    int start;
    TLVertex *p;

    if (gVBTLUsed + count > gVBTLSize) {
        hr = gVBTL->lpVtbl->Lock(gVBTL, DDLOCK_DISCARDCONTENTS | DDLOCK_WRITEONLY, (void **)&p, NULL);
        start = 0;
        gVBTLUsed = count;
    } else {
        hr = gVBTL->lpVtbl->Lock(gVBTL, DDLOCK_WRITEONLY, (void **)&p, NULL);
        start = gVBTLUsed;
        p += gVBTLUsed;
        gVBTLUsed += count;
    }
    if (hr || !p)
        return -1;
    *out = p;
    return start;
}

void D3DQVertex_DrawStrip(StripNode *n)
{
    StripState st;
    TLVertex *v = n->verts, *p;
    int start;

    st.flags = n->flags;
    st.texture = n->texture;
    if (((gQVLastFlags & 0x8000003C) == 0) != ((st.flags & 0x8000003C) == 0) || ((gQVLastFlags ^ st.flags) & 0x60000))
        gD3DRenderStates->current = -1;
    if ((gQVLastFlags ^ st.flags) & 0x8000003C)
        gD3DBlendStates->current = -1;
    if ((gQVLastTexture == NULL) != (st.texture == NULL) || ((gQVLastFlags ^ st.flags) & 0x1400000))
        gD3DTextureStates->current = -1;
    gQVLastTexture = st.texture;
    gQVLastFlags = st.flags;
    RunState(gD3DTextureStates, gQVTextureStatesIndex, &st);
    RunState(gD3DBlendStates, gQVBlendStatesIndex, &st);
    RunState(gD3DRenderStates, gQVRenderStatesIndex, &st);
    RunState(gD3DClipStates, gD3DClippingOnIndex, &st);
    if (st.texture)
        gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, n->texture->surface);

    if (n->lines || n->count > 2) {
        start = LockVerts(n->count, &p);
        if (start < 0)
            return;
        memcpy(p, v, n->count * sizeof(TLVertex));
        gVBTL->lpVtbl->Unlock(gVBTL);
        gD3DDevice->lpVtbl->DrawPrimitiveVB(gD3DDevice, n->lines ? D3DPT_LINESTRIP : D3DPT_TRIANGLESTRIP, gVBTL, start,
                                            n->count, 0);
        return;
    }
    /* two corners: a rectangle */
    start = LockVerts(4, &p);
    if (start < 0)
        return;
    p[0] = v[0];
    p[3] = v[1];
    p[1] = v[0];
    p[1].x = v[1].x;
    p[1].u = v[1].u;
    p[2] = v[0];
    p[2].y = v[1].y;
    p[2].v = v[1].v;
    gVBTL->lpVtbl->Unlock(gVBTL);
    gD3DDevice->lpVtbl->DrawPrimitiveVB(gD3DDevice, D3DPT_TRIANGLESTRIP, gVBTL, start, 4, 0);
}
HOOK(0x50D7E0, D3DQVertex_DrawStrip);

void D3DQVertex_SetTextureStates(StripState *st);
void D3DQVertex_SetBlendStates(StripState *st);
void D3DQVertex_SetRenderStates(StripState *st);
void D3DQVertex_Strip(QVertexStrip *s);

void D3DQVertex_Init(void)
{
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gQVertexStrip, D3DQVertex_Strip);
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gQVertexDrawStrip, D3DQVertex_DrawStrip);
    gQVLastFlags = 0xFFFFFFFF;
    gQVLastTexture = NULL;
    gQVTextureStatesIndex = PtrList_Add(gD3DTextureStates, D3DQVertex_SetTextureStates);
    gQVBlendStatesIndex = PtrList_Add(gD3DBlendStates, D3DQVertex_SetBlendStates);
    gQVRenderStatesIndex = PtrList_Add(gD3DRenderStates, D3DQVertex_SetRenderStates);
}
HOOK(0x50DC30, D3DQVertex_Init);

static void SetState(D3DRENDERSTATETYPE s, DWORD v)
{
    gD3DDevice->lpVtbl->SetRenderState(gD3DDevice, s, v);
}

static void SetStage(DWORD stage, D3DTEXTURESTAGESTATETYPE s, DWORD v)
{
    gD3DDevice->lpVtbl->SetTextureStageState(gD3DDevice, stage, s, v);
}

void D3DQVertex_SetRenderStates(StripState *st)
{
    unsigned int flags;

    SetState(47, 0);
    SetState(D3DRENDERSTATE_FOGENABLE, 0);
    SetState(D3DRENDERSTATE_SPECULARENABLE, 0);
    SetState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
    SetState(D3DRENDERSTATE_LIGHTING, 0);
    SetState(D3DRENDERSTATE_DITHERENABLE, 1);
    SetState(D3DRENDERSTATE_COLORVERTEX, 1);
    if (st->flags & 0x8000003C) {
        SetState(D3DRENDERSTATE_ALPHATESTENABLE, 1);
        SetState(D3DRENDERSTATE_ALPHAREF, 10);
        SetState(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER);
    } else {
        SetState(D3DRENDERSTATE_ALPHATESTENABLE, 0);
    }
    flags = st->flags;
    if (flags & 0x20000) {
        SetState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
        SetState(D3DRENDERSTATE_ZENABLE, gD3DZEnable);
        SetState(D3DRENDERSTATE_ZWRITEENABLE, 1);
    } else if (flags & 0x40000) {
        SetState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
        SetState(D3DRENDERSTATE_ZENABLE, gD3DZEnable);
        SetState(D3DRENDERSTATE_ZWRITEENABLE, 0);
    } else {
        SetState(D3DRENDERSTATE_ZENABLE, 0);
    }
}
HOOK(0x50DCC0, D3DQVertex_SetRenderStates);

void D3DQVertex_SetTextureStates(StripState *st)
{
    if (st->texture) {
        if (st->flags & 0x1000000) {
            SetStage(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
            SetStage(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
        } else {
            SetStage(0, D3DTSS_MINFILTER, D3DTFN_POINT);
            SetStage(0, D3DTSS_MAGFILTER, D3DTFG_POINT);
        }
        if (st->flags & 0x400000)
            SetStage(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP);
        else
            SetStage(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
        SetStage(0, D3DTSS_MIPFILTER, D3DTFP_NONE);
        SetStage(0, D3DTSS_TEXCOORDINDEX, 0);
        SetStage(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        SetStage(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        SetStage(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        SetStage(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        SetStage(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        SetStage(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        SetStage(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, st->texture->surface);
        return;
    }
    SetStage(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    SetStage(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    SetStage(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    SetStage(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    SetStage(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    SetStage(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    SetStage(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, NULL);
}
HOOK(0x50DE00, D3DQVertex_SetTextureStates);

static void SetBlend(DWORD src, DWORD dst)
{
    SetState(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
    SetState(D3DRENDERSTATE_SRCBLEND, src);
    SetState(D3DRENDERSTATE_DESTBLEND, dst);
}

void D3DQVertex_SetBlendStates(StripState *st)
{
    unsigned int flags = st->flags;

    if (flags & 8)
        SetBlend(D3DBLEND_SRCALPHA, D3DBLEND_ONE);
    else if (flags & 0x10)
        SetBlend(D3DBLEND_ZERO, D3DBLEND_INVSRCCOLOR);
    else if (flags & 0x80000000)
        SetBlend(D3DBLEND_ZERO, D3DBLEND_SRCCOLOR);
    else
        SetBlend(D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
}
HOOK(0x50E000, D3DQVertex_SetBlendStates);

void D3DQVertex_ClipLines(QVertexStrip *s);
void D3DQVertex_ClipStrip(QVertexStrip *s);
void D3DQVertex_ClipRect(QVertexStrip *s);

void D3DQVertex_Strip(QVertexStrip *s)
{
    unsigned int code = 0;
    int n = s->count;

    if (n > 0) {
        long double maxX = (long double)gSceneMaxX + kOne.f, maxY = (long double)gSceneMaxY + kOne.f;
        const QVertex *v = s->verts;

        do {
            code |= (((unsigned)!(maxY >= v->y) * 2 | (v->x > maxX)) * 2 | !(v->y >= gSceneMinY)) * 2 |
                    !(v->x >= gSceneMinX);
            v++;
        } while (--n);
    }
    if (s->lines) {
        if (code) {
            D3DQVertex_ClipLines(s);
            return;
        }
    } else if (s->count > 2) {
        if (code) {
            D3DQVertex_ClipStrip(s);
            return;
        }
    } else if (code) {
        if (!(s->flags & 0x60000)) {
            s->verts[0].z = kOne.f;
            s->verts[1].z = kOne.f;
        }
        D3DQVertex_ClipRect(s);
        return;
    }
    D3DQVertex_Emit(s);
}
HOOK(0x50E0D0, D3DQVertex_Strip);

/* The outcode of the line and strip clippers. */
static unsigned int Outcode(const QVertex *v)
{
    unsigned int left = !(v->x >= gSceneMinX);
    unsigned int top = !(v->y >= gSceneMinY);
    unsigned int right = !((long double)gSceneMaxX + kOne.f >= v->x);
    unsigned int bottom = !((long double)gSceneMaxY + kOne.f >= v->y);

    return ((bottom * 2 | right) * 2 | top) * 2 | left;
}

/* uv and colours over z; the first product uses the unrounded 1 / z */
static void ClipVert_Set(ClipVert *c, const QVertex *v)
{
    long double w = kOne.f / v->z;

    c->x = v->x;
    c->y = v->y;
    c->w = w;
    c->u = w * v->u;
    c->v = v->v * c->w;
    c->a = v->a * c->w;
    c->r = v->r * c->w;
    c->g = v->g * c->w;
    c->b = v->b * c->w;
}

void D3DQVertex_ClipLines(QVertexStrip *s)
{
    QVertexStrip l = *s;
    QVertex buf[2];
    ClipVert a, b;
    unsigned int code, prev = 0;
    int i;

    l.verts = buf;
    l.count = 2;
    l.lines = 0;
    for (i = 0; i < s->count; i++) {
        const QVertex *v = &s->verts[i];

        code = Outcode(v);
        if (i > 0) {
            if (!(code | prev)) {
                buf[0] = v[-1];
                buf[1] = v[0];
                l.lines = 1;
                D3DQVertex_Emit(&l);
            } else if (!(prev & code)) {
                ClipVert_Set(&a, v - 1);
                ClipVert_Set(&b, v);
                a.code = prev;
                b.code = code;
                l.lines = 0;
                QVertex_ClipTriangle(a, b, a, gNearPlane, D3DQVertex_EmitLine, &l);
            }
        }
        prev = code;
    }
}
HOOK(0x50E1E0, D3DQVertex_ClipLines);

static void ClipVert_Restore(QVertex *d, const ClipVert *c)
{
    d->x = c->x;
    d->y = c->y;
    d->z = kOne.f / c->w;
    d->u = c->u * d->z;
    d->v = c->v * d->z;
    d->a = c->a * d->z;
    d->r = c->r * d->z;
    d->g = c->g * d->z;
    d->b = c->b * d->z;
}

/* Only the first piece of a clipped line is drawn. */
void D3DQVertex_EmitLine(ClipVert **v, QVertexStrip *s)
{
    int i;

    if (s->lines)
        return;
    s->lines = 1;
    for (i = 0; i < 2; i++)
        ClipVert_Restore(&s->verts[i], v[i]);
    D3DQVertex_Emit(s);
}
HOOK(0x50E450, D3DQVertex_EmitLine);

void D3DQVertex_ClipStrip(QVertexStrip *s)
{
    static const int order[5] = { 1, 2, 0, 1, 2 };
    QVertexStrip l = *s;
    QVertex buf[3];
    ClipVert ring[3];
    int i, slot = 0;

    l.verts = buf;
    l.count = 3;
    for (i = 0; i < s->count; i++, slot++) {
        const QVertex *v = &s->verts[i];

        if (slot > 2)
            slot = 0;
        ring[slot].code = Outcode(v);
        ClipVert_Set(&ring[slot], v);
        if (i >= 2)
            QVertex_ClipTriangle(ring[order[slot]], ring[order[slot + 1]], ring[order[slot + 2]], gNearPlane,
                                 D3DQVertex_EmitTriangle, &l);
    }
}
HOOK(0x50E520, D3DQVertex_ClipStrip);

void D3DQVertex_EmitTriangle(ClipVert **v, QVertexStrip *s)
{
    int i;

    for (i = 0; i < 3; i++)
        ClipVert_Restore(&s->verts[i], v[i]);
    D3DQVertex_Emit(s);
}
HOOK(0x50E730, D3DQVertex_EmitTriangle);

/* The two corners become two triangles. */
void D3DQVertex_ClipRect(QVertexStrip *s)
{
    QVertexStrip l = *s;
    QVertex buf[3];
    ClipVert c[4];
    const QVertex *v = s->verts;
    float maxX = gSceneMaxX + kOne.f;
    long double maxY = (long double)gSceneMaxY + kOne.f, w;
    int i;

    l.count = 3;
    l.verts = buf;
    for (i = 0; i < 4; i++) {
        float x = v[i & 1].x, y = v[i >> 1].y;

        c[i].code = (((unsigned)!(maxY >= y) * 2 | (x > maxX)) * 2 | !(y >= gSceneMinY)) * 2 | !(x >= gSceneMinX);
        c[i].x = x;
        c[i].y = y;
    }
    w = kOne.f / v[0].z;
    c[0].w = w;
    c[0].u = w * v[0].u;
    c[0].v = w * v[0].v;
    c[0].a = w * v[0].a;
    c[0].r = w * v[0].r;
    c[0].g = w * v[0].g;
    c[0].b = w * v[0].b;
    c[1].w = w;
    c[1].u = w * v[1].u;
    c[1].v = c[0].v;
    c[2].w = w;
    c[2].u = c[0].u;
    c[2].v = w * v[1].v;
    c[3].w = w;
    c[3].u = c[1].u;
    c[3].v = c[2].v;
    for (i = 1; i < 4; i++) {
        c[i].a = c[0].a;
        c[i].r = c[0].r;
        c[i].g = c[0].g;
        c[i].b = c[0].b;
    }
    QVertex_ClipTriangle(c[0], c[1], c[2], gNearPlane, D3DQVertex_EmitTriangle, &l);
    QVertex_ClipTriangle(c[1], c[2], c[3], gNearPlane, D3DQVertex_EmitTriangle, &l);
}
HOOK(0x50E7F0, D3DQVertex_ClipRect);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A7CED8) int gQVTextureStatesIndex;
STORE(igb, A7CED8, 4) __typeof__(gQVStripBuffer[0]) store_A7CEDC[71];
ALIAS(gQVStripBuffer, store_A7CEDC, 0);
BSS(A7CED8) int gQVRenderStatesIndex;
BSS(A7CED8) int gQVBlendStatesIndex;
PAD(igb, A7CED8, A7D9FC, 0x4);
BSS(A7CED8) unsigned int gQVLastFlags;
BSS(A7CED8) QTexture *gQVLastTexture;
BSS(A7CED8) int gClipDepth;
BSS(BCADA0) float gD3DMinZ;
BSS(BCADC8) float gD3DMaxZ;
