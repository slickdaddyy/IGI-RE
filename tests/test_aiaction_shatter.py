import math
import random
import struct

import pytest

from emu import Emu
from test_aiaction_util import Fakes, check, f32, f64, u32

e = Emu()
fk = Fakes(e)
fk.add(0x4F0EE0, 0, ret=0x777)
fk.add(0x4012A0, 3, ret=lambda em, a: G, fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2]))
fk.add(0x44A670, 3)   # GlassShatter_AddTri
fk.add(0x416920, 0, ret=0x999)
fk.add(0x49A450, 11, ret=0x4321)
fk.add(0x44A630, 1)   # GlassShatter_PlaySound

G = e.alloc(0x2B048)
e.baseline = e.snapshot()


def model(em, rng, ntris):
    verts = em.alloc(16 * 12)
    for i in range(12):
        em.write(verts + 16 * i, f32(*[rng.uniform(-300, 300) for _ in range(3)], 0.0))
    tris = em.alloc(8 * max(ntris, 1))
    for i in range(ntris):
        em.write(tris + 8 * i, struct.pack("<4H", *[rng.randrange(12) for _ in range(3)], 0))
    mesh = em.alloc(0x20)
    em.write(mesh, u32(ntris))
    em.write(mesh + 0x10, u32(tris) + u32(verts))
    info = em.alloc(0x30)
    em.write(info + 0x2C, u32(mesh))
    y = em.alloc(0x10)
    em.write(y + 0xC, u32(info))
    x = em.alloc(0x10)
    em.write(x + 0xC, u32(y))
    m = em.alloc(4)
    em.write(m, u32(x))
    return m


def rot(rng):
    a, b = rng.uniform(0, 6.3), rng.uniform(0, 6.3)
    ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
    return f32(ca, -sa * cb, sa * sb, sa, ca * cb, -ca * sb, 0.0, sb, cb) + u32(5)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_spawn_from(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        em.write(G, bytes(rng.randrange(256) for _ in range(0x100)))
        mdl = model(em, rng, [0, 1, 5, 20][seed % 4])
        mat = em.alloc(40)
        em.write(mat, rot(rng))
        pos, frm = em.alloc(24), em.alloc(24)
        p = [rng.uniform(-5000, 5000) for _ in range(3)]
        d = [rng.uniform(-1, 1) * [0.3, 2000.0, 0.0][seed % 3] for _ in range(3)]
        em.write(pos, f64(*p))
        em.write(frm, f64(*[p[i] - d[i] for i in range(3)]))
        force = struct.unpack("<I", f32(12288.0))[0]
        scale = struct.unpack("<I", f32(rng.choice([204.8, 409.6])))[0]
        return mdl, mat, pos, frm, force, scale
    check(e, fk, 0x44BD70, "GlassShatter_SpawnFrom", setup=setup, cw=cw, check_eax=False)
