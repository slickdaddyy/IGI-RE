"""Helpers shared by the test_sector tests."""
import struct

from test_bonemesh_util import make_emu, with_cw  # noqa: F401
from test_menu_util import (LOG, RET, add_child, check, f32, handler, logfake, logger, set_handler,  # noqa: F401
                            task, trap, u16, u32)

MAGIC, SECTOROBJ, RIGID = 0x50, 0x51, 0x52
# (type, base) pairs TaskType_IsA answers yes to
ISA = {(MAGIC, MAGIC), (MAGIC, RIGID), (SECTOROBJ, SECTOROBJ), (SECTOROBJ, RIGID), (RIGID, RIGID)}

VIEW = 0xBCAAE0
AREA = 0xBCABD0


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def new_emu():
    e = make_emu()
    e.call(0x4B0BA0)
    logfake(e, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF},
            effect=lambda e, a: int((a[0] & 0xFFFF, a[1] & 0xFFFF) in ISA))
    logfake(e, 0x4D9FA0, 0, ret=MAGIC)
    logfake(e, 0x4DEE80, 0, ret=SECTOROBJ)
    logfake(e, 0x4C48C0, 0, ret=RIGID)
    e.baseline = e.snapshot()
    return e


def set_view(e, scale=(320.0, -320.0), center=(320.0, 240.0), area=(0.0, 0.0, 640.0, 480.0),
             matrix=(1, 0, 0, 0, 0, 1, 0, 1, 0), pos=(0.0, 0.0, 0.0)):
    f32(e, VIEW, *matrix)
    d64(e, VIEW + 0x28, *pos)
    f32(e, VIEW + 0x48, *scale, *center)
    f32(e, AREA, *area)


def mesh(e, fmt=3, center=(0.0, 0.0, 0.0), radius=100.0, radius2=150.0, verts=(), tris=(), portals=()):
    m = e.alloc(0xA0)
    u32(e, m + 0x20, fmt)
    f32(e, m + 0x50, *center, radius)
    f32(e, m + 0x78, radius2)
    u16(e, m + 0x84, len(portals))
    if tris:
        t = e.alloc(12 * len(tris))
        for i, tr in enumerate(tris):
            u32(e, t + 12 * i, *tr)
        u32(e, m + 0x90, t)
    if verts:
        v = e.alloc(12 * len(verts))
        for i, vv in enumerate(verts):
            f32(e, v + 12 * i, *vv)
        u32(e, m + 0x94, v)
    if portals:
        p = e.alloc(0x14 * len(portals))
        for i, pp in enumerate(portals):
            u32(e, p + 0x14 * i, *pp)
        u32(e, m + 0x98, p)
    return m


def model(e, meshes):
    """A VirModel with one LOD per mesh."""
    vm = e.alloc(0x40)
    for i, m in enumerate(meshes):
        lod = e.alloc(0x10)
        mid = e.alloc(0x10)
        u32(e, lod + 0xC, mid)
        u32(e, mid + 0xC, m)
        u32(e, vm + 4 * i, lod)
    u16(e, vm + 0x3C, len(meshes))
    return vm


def obj(e, size, type_, vm=0, pos=(0.0, 0.0, 0.0), matrix=(1, 0, 0, 0, 1, 0, 0, 0, 1), scale=(1, 1, 1),
        attached=None):
    o = task(e, size, type_)
    d64(e, o + 0x20, *pos)
    u32(e, o + 0x6C, vm)
    f32(e, o + 0x70, *matrix)
    f32(e, o + 0x98, *scale)
    u32(e, o + 0xD8, 0x1234 + o % 97)
    if attached is not None:
        u32(e, o + 0xE8, attached)
    return o


def sector_object(e, vm, sectors=(), **kw):
    """A SectorObject with a task holding its sectors at 0x148, all portals open."""
    so = obj(e, 0x1A0, SECTOROBJ, vm, attached=task(e, 0x20), **kw)
    holder = task(e, 0x20)
    for s in sectors or ():
        add_child(e, holder, s)
    u32(e, so + 0x148, holder if sectors is not None else 0)
    e.write(so + 0x14C, b"\x01" * 0x44)
    return so


def pool(e, p, size, count):
    e.call(0x4B0B00, p, size, count, 0)
