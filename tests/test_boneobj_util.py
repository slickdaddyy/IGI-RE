"""Helpers shared by the BoneObj tests: emulator setup, x87 control word entry points,
memory builders for bones, blend trees and BOBJ files."""
import math
import random
import struct

from emu import compare, Emu

TICKS = 0xA54658
SMOOTH = 0xA54678
ROOT_QUAT, ROOT_MATRIX = 0xA538E8, 0xA538EC
SKELS, SETS = 0xA542F0, 0xA538F0
SRC_POOL, ORIENT_POOL, INST_POOL, FADE_POOL, NODE_POOL = 0xA545F0, 0xA54608, 0xA54620, 0xA54640, 0xA54660

ALLOC_NODE, ALLOC_SOURCE, ALLOC_FADE = 0x4D35B0, 0x4D4710, 0x4D5F90
NEW_NODE, NEW_PARENT_SOURCE, NEW_ANIM_SOURCE, NEW_ORIENT_SOURCE = 0x4D3560, 0x4D46E0, 0x4D47D0, 0x4D5700
FADE_WEIGHT, FADE_FREE, FADE_SELECT = 0x4D63A0, 0x4D63F0, 0x4D63B0
GET_ORIENT_CB, PARENT_ORIENT_CB = 0x4D5820, 0x4D4780
ANIM_POS_CB, ANIM_ORIENT_CB, ANIM_EVENTS_CB, ANIM_FREE_CB = 0x4D4840, 0x4D5180, 0x4D4B60, 0x4D5080
BONE = 0x84


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def i32(*v):
    return struct.pack("<%di" % len(v), *v)


def u32(e, p):
    return struct.unpack("<I", e.read(p, 4))[0]


def new_emu():
    e = Emu()
    e.stub("OutputDebugStringA", lambda em: em.ret(0, 4))
    e.call(0x4B0BA0)  # Mem_Init
    e.call(0x4D2C30)  # the five BoneObj pools
    e.write(TICKS, f32(160.0))
    e.baseline = e.snapshot()
    return e


def with_cw(e, target, cw, cache={}):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (id(e), addr, cw)
    if key not in cache:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<H", cw) + b"\0\0")
        e.write(p + 4, b"\xd9\x2d" + struct.pack("<I", p) + b"\xe9" + struct.pack("<i", addr - (p + 15)))
        cache[key] = p + 4
    return cache[key]


def check(e, orig, ours, setup=None, cw=None, normalized=True, **kw):
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return compare(e, orig, ours, setup=setup, normalized=normalized, **kw)


def rquat(r, count=None):
    v = [r.uniform(-1, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in v)) * r.uniform(0.98, 1.02)
    return [x / n for x in v] + [r.randrange(0, 1000) if count is None else count]


def quat(q):
    return f32(*q[:4]) + i32(q[4])


def put(e, data):
    p = e.alloc(len(data))
    e.write(p, data)
    return p


def bone_bytes(index, flags=0, offset=(0, 0, 0), parent=0, child=0, nxt=0, blend=0, world=None, r=None):
    r = r or random.Random(index)
    world = world or rquat(r)
    pos = [r.uniform(-5, 5) for _ in range(3)]
    mat = [r.uniform(-1, 1) for _ in range(9)]
    return (i32(index) + struct.pack("<I", flags) + f32(*offset) + f32(r.uniform(0, 1)) + f32(*pos) + f32(*mat) +
            i32(r.randrange(1000)) + quat(world) + quat(rquat(r)) + struct.pack("<4I", parent, child, nxt, blend))


def make_bones(e, parents, offsets=None, seed=0, flags=0):
    """Contiguous bones linked like BoneObj_Create does: parent, first child, next sibling."""
    r = random.Random(seed)
    n = len(parents)
    base = e.alloc(BONE * n)
    offsets = offsets or [[r.uniform(-1, 1) for _ in range(3)] for _ in range(n)]
    for i in range(n):
        kids = [j for j in range(n) if parents[j] == i]
        sibs = [j for j in range(n) if parents[j] == parents[i] and j > i]
        e.write(base + BONE * i, bone_bytes(i, flags, offsets[i], base + BONE * parents[i] if parents[i] >= 0 else 0,
                                            base + BONE * kids[0] if kids else 0,
                                            base + BONE * sibs[0] if sibs else 0, 0, r=r))
    return base


def set_blend(e, bone, node):
    e.write(bone + 0x80, struct.pack("<I", node))


def orient_source(e, bone_index, mode, q):
    qp = put(e, quat(q))
    return e.call(NEW_ORIENT_SOURCE, bone_index, mode, qp)[0]


def node(e, t0, p0, t1, p1, blend=0, weight=0, free=0, select=0, data=0):
    return e.call(NEW_NODE, t0, p0, t1, p1, 0, blend, weight, free, select, data)[0]


def fade(e, duration, result, time=0.0, weight=1.0):
    p = e.call(ALLOC_FADE)[0]
    e.write(p, f32(time, duration, weight) + i32(result))
    return p


def fade_node(e, t0, p0, t1, p1, f):
    return node(e, t0, p0, t1, p1, 0, FADE_WEIGHT, FADE_FREE, FADE_SELECT, f)


# --- BOBJ files ---------------------------------------------------------------

def chunk(cid, data):
    out = cid.encode() + struct.pack(">I", len(data)) + data
    return out + (b"\0" if len(data) & 1 else b"")


def form(ftype, *kids):
    body = ftype.encode() + b"".join(kids)
    return b"FORM" + struct.pack(">I", len(body)) + body


def rot_keys(r, n, length, near=False):
    if n == 1:
        return quat(rquat(r, 0))
    base = [rquat(r) for _ in range(3)]

    def q():
        if not near:
            return rquat(r)[:4]
        b = base[0]
        return [x + r.uniform(-0.002, 0.002) for x in b[:4]]
    out = b""
    for k in range(n):
        out += f32(*q()) + f32(length * k / n) + f32(*q()) + f32(*q())
    return out


def pos_keys(r, n, length):
    if n == 1:
        return f32(*[r.uniform(-3, 3) for _ in range(3)]) + f32(0)
    out = b""
    for k in range(n):
        out += f32(*[r.uniform(-3, 3) for _ in range(3)]) + f32(length * k / (n - 1))
        out += f32(*[r.uniform(-1, 1) for _ in range(6)])
    return out


def anim(r, aid, bones, length, flags=0, pos=3, rots=None, events=(), near=False):
    """rots: key count per bone. events: (id, time, x, y, z)."""
    rots = rots or [3] * bones
    kids = [chunk("BOAH", f32(length) + struct.pack("<Ii", flags, aid)), chunk("BOTH", i32(pos))]
    if pos > 0:
        kids.append(chunk("BOTD", pos_keys(r, pos, length)))
    for n in rots:
        kids += [chunk("BORH", i32(n)), chunk("BORD", rot_keys(r, n, length, near))]
    kids.append(chunk("BOEH", i32(len(events))))
    if events:
        kids.append(chunk("BOED", b"".join(i32(ev[0], 0) + f32(*ev[1:]) for ev in events)))
    return form("BOAN", *kids)


def bobj(skel, parents, offsets, anims, total=None):
    head = form("BOBH", chunk("BOSH", i32(skel, len(parents))), chunk("PLST", i32(*parents)),
                chunk("TLST", b"".join(f32(*o) for o in offsets)))
    total = total if total is not None else len(anims) + 2
    return form("BOBJ", head, form("BOAL", chunk("BALH", i32(len(anims), total)), *anims))
