import random
import struct

import pytest

from emu import Emu
from test_lod_util import (CTX, RENDER_FUNCS, SCENE, TASK_HANDLERS, Fakes, check, check_error, f32, f64, put,
                           set_func, u32)

e = Emu()
fk = Fakes(e)
byte0 = lambda em, a: (a[0] & 0xFF,)
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]), name="alloc")
fk.add(0x4B0D10, 1, name="free")
fk.add(0x401860, 2, name="event_set")
fk.add(0x401D30, 2, fmt=lambda em, a: (a[0] & 0xFF, a[1]), name="map_slot")
fk.add(0x401D50, 1, fmt=byte0, name="unmap_slot")
fk.add(0x4017C0, 1, ret=lambda em, a: 0x40 + len(fk.log), name="event_alloc")
fk.add(0x4018C0, 1, fmt=byte0, name="event_free")
fk.add(0x4B0B00, 4, name="pool_init")
fk.add(0x4B0B70, 1, name="pool_free")
fk.add(0x5116B0, 1, name="5116b0")
fk.add(0x4CE390, 5, fmt=lambda em, a: (a[0], a[1] & 0xFF, a[2], a[3], a[4]), name="send_all")
fk.add(0x4B32F0, 2, st0="d", fmt=lambda em, a: (em.read(a[0], 24), em.read(a[1], 24)), name="dist",
       ret=lambda em, a: sum((x - y) ** 2 for x, y in zip(struct.unpack("<3d", em.read(a[0], 24)),
                                                          struct.unpack("<3d", em.read(a[1], 24)))))
fk.add(0x497520, 3, deref={1: 8, 2: 8}, name="set_bounds")
fk.add(0x4D9800, 1, ret=lambda em, a: a[0] + 1, name="task_arg")
for a in (0x4DA2F0, 0x4DA8B0, 0x4E5CB0, 0x50FE60, 0x511500):
    fk.add(a, 0)
fk.add(0x50FAE0, 2)
fk.add(0x50FD00, 1)

LIST, OUT, COUNT = 0xB16C98, 0xB12C58, 0xB12C20
ENCLOSING, RADIUS, BOX, DRAW_OUTSIDE = 0xA538D0, 0xB04264, 0xB16C80, 0x54843C
CALLBACKS, SLOTS = 0xA538E4, 0xA4EEA8
NEAR_EVENT, ENCL_EVENT, NEAR_ON = 0x548440, 0x548441, 0x543A64
SPECIAL_TYPE = 0xC30064


def handler_log(em, a):
    return (a[0], a[1])


def task(em, type_, pos=(0, 0, 0)):
    t = em.alloc(0x60)
    em.write(t + 0x1C, struct.pack("<H", type_))
    f64(em, t + 0x20, *pos)
    u32(em, t + 0x48, 0x4800 + type_)
    return t


def set_handler(em, slot, type_, fn):
    set_func(em, TASK_HANDLERS, slot * 0x180 + type_, fn)


H = [fk.func(2, fmt=handler_log, name="h%d" % i) for i in range(4)]


def entries(em, rng, n, special=True):
    lst = em.alloc(8 * max(n, 1))
    u32(em, LIST, lst)
    u32(em, COUNT, n)
    u32(em, 0xC30064, 0x55 if special else 0x99)
    tasks = []
    for i in range(n):
        type_ = rng.choice([0x50, 0x51, 0x55])
        t = task(em, type_, [rng.uniform(-100, 100) for _ in range(3)])
        slot = rng.choice([3, 4])
        em.write(lst + 8 * i, struct.pack("<BxxxI", slot, t))
        set_handler(em, slot, type_, H[slot - 3])
        tasks.append(t)
    return lst, tasks


CB = [fk.func(1, deref={0: 8}, name="cb%d" % i) for i in range(3)]


def callbacks(em, n):
    head = 0
    for i in range(n):
        c = em.alloc(8)
        u32(em, c, head)
        u32(em, c + 4, CB[i])
        head = c
    u32(em, CALLBACKS, head)


def world(em):
    w = em.alloc(0xDF0)
    em.write(w + 0xDE0, struct.pack("<3i", 1000, -2000, 3000))
    return w


def test_init():
    check(e, fk, 0x4D0EF0, "QCube_Init", check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4D11C0, "QCube_Reset"), (0x4D1220, "QCube_Clear")])
def test_reset_clear(addr, name):
    check(e, fk, addr, name, check_eax=False)


def test_shutdown():
    def setup(em):
        em.write(NEAR_EVENT, bytes([0x21, 0x22]))
        u32(em, LIST, 0x1234)
    check(e, fk, 0x4D1260, "QCube_Shutdown", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("enclosing,outside", [(False, 1), (True, 1), (True, 0)])
@pytest.mark.parametrize("seed", range(3))
def test_draw_objects(seed, enclosing, outside, cw):
    def setup(em):
        rng = random.Random(seed)
        entries(em, rng, 12)
        f64(em, CTX + 0x28, 1, 2, 3)
        f32(em, SCENE + 0x18, 10, 20, 300, 400)
        u32(em, ENCLOSING, task(em, 0x60) if enclosing else 0)
        f64(em, BOX, 5, -5, 10)
        f32(em, RADIUS, 90)
        u32(em, DRAW_OUTSIDE, outside)
        callbacks(em, 2)
        return (world(em),)
    check(e, fk, 0x4D12C0, "QCube_DrawObjects", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("n", [0, 3])
def test_run_callbacks(n):
    def setup(em):
        callbacks(em, n)
        return em.u32(CALLBACKS), put(em, b"argument")
    check(e, fk, 0x4D14A0, "QCube_RunCallbacks", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name,var", [(0x4D14C0, "QCube_GetEnclosingEvent", ENCL_EVENT),
                                           (0x4D14D0, "QCube_GetNearClipEvent", NEAR_EVENT)])
def test_events(addr, name, var):
    check(e, fk, addr, name, setup=lambda em: em.write(NEAR_EVENT - 1, bytes([0x91, 0x92, 0x93, 0x94])), eax_mask=0xFF)


def fill_walk(em, a):
    """The faked QCube_Walk collects two objects."""
    rng = random.Random(9)
    lst = em.u32(LIST)
    for i in range(2):
        t = task(em, 0x50 + i, [rng.uniform(-100, 100) for _ in range(3)])
        em.write(lst + 8 * i, struct.pack("<BxxxI", 4, t))
        set_handler(em, 4, 0x50 + i, H[i])
    u32(em, COUNT, 2)
    return 0


fk.add(0x4D14E0, 1, ret=fill_walk, name="walk")


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("fov,detail", [(1.2, 0x73), (1.0, 0x73), (0.6, 0x60), (0.3, 0xA3), (1.5, 0x10), (0.9, 0x40)])
def test_begin(fov, detail, cw):
    def setup(em):
        u32(em, LIST, em.alloc(64))
        f32(em, CTX + 0x40, fov)
        u32(em, 0x54844C, detail)
        return (world(em),)
    check(e, fk, 0x4D2540, "QCube_Begin", setup=setup, check_eax=False, cw=cw)


def near_method(em, a):
    u32(em, a[0] + 4, NEAR_STATE["z"])
    return 0


NEAR_STATE = {"z": 1}
NEAR_METHOD = fk.func(1, ret=near_method, deref={0: 8}, name="near_method")
fk.add(0x498070, 0, ret=6, name="near_clip_method")


def near_handler(em, a):
    z = struct.unpack("<f", em.read(a[1] + 8, 4))[0]
    f32(em, a[1] + 8, z * 0.75)
    return 0


NEAR_H = fk.func(2, ret=near_handler, deref={1: 12}, name="near_h")
fk.add(0x50F770, 1, deref={0: 12}, name="50f770")
fk.add(0x4A53B3, 4, fmt=lambda em, a: (a[0], em.read_cstr(a[1]), a[2], a[3]), name="sprintf")


def task_list(em, types):
    head = 0
    for t_ in reversed(types):
        t = task(em, t_)
        u32(em, t + 0x58, head)
        head = t
    return head


def near_world(em, flags):
    rng = random.Random(5)
    w = world(em)
    entries(em, rng, 6)
    u32(em, w + 0xD18, task_list(em, [0x60, 0x61]))
    for i in range(8):
        u32(em, w + 0xD1C + 4 * i, task_list(em, [0x62 + i % 2]) if i % 3 == 0 else 0)
    em.write(NEAR_EVENT, bytes([7]))
    for t_ in (0x50, 0x51, 0x60, 0x62):
        set_handler(em, 7, t_, NEAR_H)
    set_func(em, RENDER_FUNCS, 6, NEAR_METHOD)
    return w, put(em, bytes(flags))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("z,on", [(1, 1), (0, 1), (1, 0)])
@pytest.mark.parametrize("flags", [[1] * 9, [0, 1, 0, 0, 1, 0, 0, 1, 0], [0] * 9])
def test_update_near_plane(flags, z, on, cw):
    def setup(em):
        NEAR_STATE["z"] = z
        u32(em, NEAR_ON, on)
        return near_world(em, flags)
    check(e, fk, 0x4D2840, "QCube_UpdateNearPlane", setup=setup, check_eax=False, cw=cw)


def test_update_near_plane_no_method():
    def setup(em):
        w, f = near_world(em, [1] * 9)
        set_func(em, RENDER_FUNCS, 6, 0)
        return w, f
    check(e, fk, 0x4D2840, "QCube_UpdateNearPlane", setup=setup, check_eax=False)


def test_run_task_list():
    def setup(em):
        head = task_list(em, [0x60, 0x61, 0x62])
        for t_, h in ((0x60, H[0]), (0x62, H[1])):
            set_handler(em, 4, t_, h)
        for t_, h in ((0x61, H[2]), (0x62, H[3])):
            set_handler(em, 5, t_, h)
        return (head,)
    check(e, fk, 0x4D27F0, "QCube_RunTaskList", setup=setup, check_eax=False)


def send_event(em, a):
    q = a[2]
    if ENCL_STATE["found"]:
        t = task(em, 0x70)
        u32(em, q + 8, 0x31)
        u32(em, q + 0xC, t)
        f64(em, q + 0x10, 4, 5, 6)
        f32(em, q + 0x28, 77)
    else:
        u32(em, q + 8, 0)
        u32(em, q + 0xC, 0)
        f64(em, q + 0x10, 0, 0, 0)
        f32(em, q + 0x28, 0)
    return 0


ENCL_STATE = {"found": False}
fk.add(0x4CE090, 6, ret=send_event,
       fmt=lambda em, a: (a[0], a[1] & 0xFF, em.read(a[2], 16), a[3], a[4], a[5]), name="send_event")


@pytest.mark.parametrize("found", [False, True])
@pytest.mark.parametrize("terrain", [0, 1])
@pytest.mark.parametrize("full", [0, 1])
@pytest.mark.parametrize("ready,active", [(0, 1), (1, 1), (1, 0)])
def test_render(ready, active, full, terrain, found):
    def setup(em):
        ENCL_STATE["found"] = found
        NEAR_STATE["z"] = 1
        u32(em, NEAR_ON, 1)
        w, f = near_world(em, [1, 0, 1, 0, 1, 1, 0, 0, 1])
        u32(em, w + 0xDC8, ready)
        u32(em, w + 0xC, terrain)
        u32(em, 0xBCAB38, active)
        u32(em, 0xB16C68, 0x66)
        em.write(ENCL_EVENT, bytes([9]))
        set_handler(em, 4, 0x70, H[3])
        set_handler(em, 4, 0x60, H[0])
        set_handler(em, 5, 0x62, H[1])
        f32(em, CTX + 0x40, 0.8)
        u32(em, 0x54844C, 0x73)
        callbacks(em, 1)
        return w, f, full
    check(e, fk, 0x4D2650, "QCube_Render", setup=setup, check_eax=False)


def pool(em, base, size, stride, count, used):
    data = em.alloc(stride * count)
    free = em.alloc(4 * count)
    perm = list(range(count))
    random.Random(base).shuffle(perm)
    for slot, idx in enumerate(perm):
        u32(em, free + 4 * slot, idx)
        u32(em, data + idx * stride + size, slot)
    em.write(base, struct.pack("<6I", data, free, used, count, stride, size))
    return data, perm


@pytest.mark.parametrize("seed", range(3))
def test_flush_cache(seed):
    def setup(em):
        rng = random.Random(seed)
        pools = [pool(em, b, 0x20 + 8 * i, 0x40, 8, 6) for i, b in enumerate((0xB12B60, 0xB12C40, 0xB16CC0))]
        cache = 0xB04340
        used = [list(p[1][:6]) for p in pools]
        for i in range(0x800):
            u32(em, cache + 0x1C * i + 0x10, i)
        for i in rng.sample(range(0x800), 9):
            kind = rng.randrange(3)
            if not used[kind]:
                continue
            idx = used[kind].pop(rng.randrange(len(used[kind])))
            em.write(cache + 0x1C * i + 0x14, struct.pack("<iI", kind, pools[kind][0] + idx * 0x40))
    check(e, fk, 0x4D2A20, "QCube_FlushCache", setup=setup, check_eax=False)


def test_flush_cache_bad_kind():
    def setup(em):
        em.write(0xB04340 + 0x1C * 5 + 0x14, struct.pack("<iI", 3, 0x1234))
        return ()
    check_error(e, fk, 0x4D2A20, "QCube_FlushCache", setup)


@pytest.mark.parametrize("used", [0, 3, 9])
def test_add_callback(used):
    def setup(em):
        for i in range(used):
            u32(em, SLOTS + 8 * i + 4, 0x100 + i)
        u32(em, CALLBACKS, 0x4321)
        return (0x777,)
    check(e, fk, 0x4D2B80, "QCube_AddCallback", setup=setup, check_eax=False)


def test_add_callback_full():
    def setup(em):
        for i in range(10):
            u32(em, SLOTS + 8 * i + 4, 0x100 + i)
        return (0x777,)
    check_error(e, fk, 0x4D2B80, "QCube_AddCallback", setup)


def three_callbacks(em):
    head = 0
    for i in range(3):
        c = SLOTS + 8 * i
        u32(em, c, head)
        u32(em, c + 4, 0x100 + i)
        head = c
    u32(em, CALLBACKS, head)


@pytest.mark.parametrize("fn", [0x100, 0x101, 0x102])
def test_remove_callback(fn):
    def setup(em):
        three_callbacks(em)
        return (fn,)
    check(e, fk, 0x4D2BD0, "QCube_RemoveCallback", setup=setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 3])
def test_remove_callback_missing(n):
    def setup(em):
        if n:
            three_callbacks(em)
        return (0x555,)
    check_error(e, fk, 0x4D2BD0, "QCube_RemoveCallback", setup)


def test_set_detail():
    check(e, fk, 0x4D2C20, "QCube_SetDetail", args=(0xA3,), check_eax=False)
