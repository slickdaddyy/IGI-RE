#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "shadow/shadow.h"

ORIG(0x497F20, void, TransContext_Project, (float *screen, float *depth, Vec3d *pos));

typedef struct QVertex {
    float x, y, z;
    float u, v;
    float a, r, g, b;
} QVertex;

typedef struct QVertexStrip {
    int untextured;
    unsigned int flags;
    int unk8;
    int count;
    QVertex *verts;
} QVertexStrip;

#define gRenderFuncs ((void (**)(QVertexStrip *))0xA94E84)
#define gQVertexTexture VAR(void *, 0xA84A50)
#define gQVertexStrip VAR(int, 0xA84A54)
#define gQVertexPalette VAR(void *, 0xA84A58)
#define gViewDir ((float *)0xBCAAF8)
#define gViewPos VAR(Vec3d, 0xBCAB08)
#define kNearPlane VAR(float, 0x5338D0)

static inline int is_zero(float f)
{
    return !(f < kZero || f > kZero);
}

static inline int is_one(float f)
{
    return !(f < kOne || f > kOne);
}

static inline float view_depth(const Vec3d *p)
{
    long double dx = (long double)p->x - gViewPos.x;
    long double dy = (long double)p->y - gViewPos.y;
    long double dz = (long double)p->z - gViewPos.z;

    return gViewDir[2] * dz + gViewDir[1] * dy + gViewDir[0] * dx;
}

static void line_draw(const Vec3d *pa, const Vec3d *pb, const float *ca, float aa, const float *cb, float ab, int unk8)
{
    Vec3d a, b;
    float da, db, sa[2], sb[2];
    QVertex v[2];
    QVertexStrip strip;

    if (is_zero(aa) && is_zero(ab))
        return;
    a = *pa;
    b = *pb;
    da = view_depth(&a);
    db = view_depth(&b);
    /* clip against the near plane */
    if (da > kNearPlane) {
        if (!(db > kNearPlane)) {
            long double t = ((long double)kNearPlane - db) / ((long double)da - db);

            b.x = (a.x - (long double)b.x) * t + b.x;
            b.y = (a.y - (long double)b.y) * t + b.y;
            b.z = (a.z - (long double)b.z) * t + b.z;
        }
    } else {
        long double t;

        if (!(db > kNearPlane))
            return;
        t = ((long double)kNearPlane - da) / ((long double)db - da);
        a.x = (b.x - (long double)a.x) * t + a.x;
        a.y = (b.y - (long double)a.y) * t + a.y;
        a.z = (b.z - (long double)a.z) * t + a.z;
    }
    TransContext_Project(sa, &da, &a);
    TransContext_Project(sb, &db, &b);
    v[0].x = sa[0];
    v[0].y = sa[1];
    v[0].z = da;
    v[0].u = 0;
    v[0].v = 0;
    v[0].a = aa;
    v[0].r = ca[0];
    v[0].g = ca[1];
    v[0].b = ca[2];
    v[1].x = sb[0];
    v[1].y = sb[1];
    v[1].z = db;
    v[1].u = 0;
    v[1].v = 0;
    v[1].a = ab;
    v[1].r = cb[0];
    v[1].g = cb[1];
    v[1].b = cb[2];
    strip.flags = is_one(aa) && is_one(ab) ? 0x40060000 : 0xC0000;
    strip.unk8 = unk8;
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    strip.untextured = 1;
    strip.count = 2;
    strip.verts = v;
    gRenderFuncs[gQVertexStrip](&strip);
}

/* a 3D line with a colour and alpha at each end */
void Line3D_Draw(const Vec3d *a, const Vec3d *b, const float *ca, float aa, const float *cb, float ab)
{
    line_draw(a, b, ca, aa, cb, ab, 0);
}
HOOK(0x4E0E20, Line3D_Draw);

/* the same with the strip's unk8 set to -1 */
void Line3D_Draw2(const Vec3d *a, const Vec3d *b, const float *ca, float aa, const float *cb, float ab)
{
    line_draw(a, b, ca, aa, cb, ab, -1);
}
HOOK(0x4E1130, Line3D_Draw2);
