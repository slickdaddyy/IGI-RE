"""CamTask, FreeCamera, Sprite3D, FlashTask, particle systems and lights (0x530220 - 0x531F60)."""
import math
import random
import struct

import pytest

from test_freecam_util import CWS, FISTP_CWS, d64, f32, logfake, logger, new_emu, run, task, trap, u16, u32

NAN = float("nan")
e = new_emu()
STATE = {}


def task_new(em, a):
    return 0x40 + (a[2] & 0x0F)


def project(em, a):
    f32(em, a[0], *STATE["screen"])
    f32(em, a[1], STATE["depth"])


def scale_size(em, a):
    w, h = struct.unpack("<2f", em.read(a[1], 8))
    f32(em, a[0], w * 0.5 + 1.25, h * 0.75 - 2.5)


H = {0xFFFF: 0}
for addr, n, kw in [
    (0x401900, 5, dict(strings=(0,), masks={2: 0xFFFF}, effect=task_new)),  # TaskType_New
    (0x401400, 3, dict(masks={0: 0xFFFF, 1: 0xFF})),  # TaskType_SetHandler
    (0x401530, 3, dict(masks={0: 0xFFFF})),  # TaskType_SetHandler2
    (0x401A20, 1, dict(masks={0: 0xFFFF})),  # TaskType_Free
    (0x4018F0, 0, dict(ret=0x11)),  # Task_GetRootType
    (0x4015F0, 1, {}),  # Task_Delete
    (0x4B0B00, 4, {}),  # Pool_Init
    (0x4B0B70, 1, {}),  # Pool_Free
    (0x4E75A0, 0, dict(ret=77)),  # Screen_GetFrame
    (0x4D9870, 10, dict(derefs={1: 24, 2: 40})),  # Camera_Set
    (0x497F20, 3, dict(derefs={2: 24}, masks={0: 0, 1: 0}, effect=project)),  # TransContext_Project
    (0x498040, 3, dict(derefs={1: 8}, masks={0: 0}, effect=scale_size)),  # TransContext_ScaleSize
    (0x499FF0, 1, dict(ret=3)),  # Float_Log2
    (0x4B51F0, 15, {}),  # QVertex_DrawQuad
    (0x4B4720, 1, {}),  # Random_Init
    (0x4B0D10, 1, {}),  # Mem_Free
    (0x4C6840, 3, {}),  # DynCube_ListRemove
    (0x4C6940, 4, dict(derefs={2: 24})),  # DynCube_Move
    (0x4C7560, 2, {}),  # DynCube_Remove
    (0x4DA8D0, 3, dict(derefs={0: 24, 1: 12})),  # Light_PushPoint
    (0x4DA950, 5, dict(derefs={0: 24, 1: 12, 3: 12})),  # Light_PushSpot
    (0x4DAA00, 0, {}),  # Light_Pop
]:
    logfake(e, addr, n, **kw)
SPAWN = trap(e, "spawn", logger(e, "spawn", 2, derefs={1: 24}))
ON_MOVE = trap(e, "on_move", logger(e, "on_move", 1))
e.baseline = e.snapshot()


def rnd_bytes(em, p, n, rng):
    em.write(p, bytes(rng.randrange(256) for _ in range(n)))


# --- CamTask, FreeCamera --------------------------------------------------------

@pytest.mark.parametrize("orig,ours", [(0x530220, "CamTask_Init"), (0x530250, "CamTask_Shutdown"),
                                       (0x530260, "CamTask_GetType"), (0x5310E0, "FreeCamera_Init"),
                                       (0x531180, "FreeCamera_Shutdown"), (0x5310C0, "ParticleSystem_Shutdown"),
                                       (0x5310D0, "ParticleSystem_GetType"), (0x531190, "SpriteParticleSystem_Init"),
                                       (0x5311C0, "SpriteParticleSystem_Shutdown"), (0x530C80, "ParticleSystem_Init"),
                                       (0x530A80, "FlashTask_Init"), (0x530C60, "FlashTask_Shutdown"),
                                       (0x531EF0, "Sprite3D_Init"), (0x531F40, "Sprite3D_Shutdown"),
                                       (0x531300, "LightObj_Init"), (0x5314F0, "LightObj_Shutdown"),
                                       (0x531470, "LightObj_Pop")])
def test_type_setup(orig, ours):
    def setup(em):
        for cell in (0x54F768, 0x54F7D0, 0x54F7B0, 0xA84608, 0x54F7A0, 0xC28F98, 0x54F800, 0x54F802):
            u16(em, cell, 0x55 + (cell & 0xF))
        u16(em, 0x54875C, 0x33)
        u32(em, 0xA84610, 7)
        em.write(0xA845EC, b"\x05")
    run(e, orig, ours, setup, check_eax=ours.endswith("GetType"), eax_mask=0xFFFF)


@pytest.mark.parametrize("active", [0, 1, 0x80])
def test_free_camera_run(active):
    def setup(em):
        rng = random.Random(active)
        c = task(em, 0x90, 0x44)
        rnd_bytes(em, c + 0x20, 0x70, rng)
        em.write(c + 0x84, bytes([active]))
        return (c,)
    run(e, 0x531130, "FreeCamera_Run", setup)


# --- Sprite3D --------------------------------------------------------------------

@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("depth", [5.0, 0.001, 0.0, -0.0, -3.0, NAN, 1e30])
def test_sprite3d_draw(depth, cw):
    def setup(em):
        rng = random.Random(int(depth) if depth == depth and abs(depth) < 1e9 else 9)
        STATE["screen"] = (rng.uniform(-100, 700), rng.uniform(-100, 500))
        STATE["depth"] = depth
        s = task(em, 0x88, 0x45)
        rnd_bytes(em, s + 0x20, 0x68, rng)
        d64(em, s + 0x20, rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(-50, 50))
        f32(em, s + 0x68, rng.uniform(0, 40), rng.uniform(0, 40))
        return (s,)
    run(e, 0x531E10, "Sprite3D_Draw", setup, cw=cw)


# --- FlashTask ---------------------------------------------------------------------

def flash(em, rng, key, count, frame, looped, last_frames=5):
    keys = em.alloc(20 * (count + 2)) + 20
    for i in range(-1, count + 1):
        f32(em, keys + 20 * i, *[rng.uniform(0, 255) for _ in range(3)])
        u32(em, keys + 20 * i + 12, rng.choice([1, 3, 8]))
        f32(em, keys + 20 * i + 16, rng.uniform(1, 100))
    u32(em, keys + 20 * (key - 1) + 12, last_frames)
    light = task(em, 0x80, 0x46)
    rnd_bytes(em, light + 0x20, 0x60, rng)
    t = task(em, 0x88, 0x47)
    rnd_bytes(em, t + 0x20, 0x68, rng)
    u32(em, t + 0x20, light, 0x7777)
    d64(em, t + 0x28, rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(-50, 50))
    u32(em, t + 0x40, keys, count, key, frame)
    em.write(t + 0x50, bytes([looped]))
    u32(em, t + 0x80, 0x8888)
    return t, keys


CASES = [(1, 3, 0, 0, 5), (1, 3, 4, 0, 5), (1, 3, 2, 1, 5), (1, 3, 5, 0, 5), (2, 3, 9, 0, 5), (3, 4, 9, 1, 2),
         (2, 3, 9, 0, -1), (0, 3, 0, 0, 5)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(CASES)))
def test_flash_run(case, cw):
    key, count, frame, looped, last = CASES[case]

    def setup(em):
        rng = random.Random(case)
        t, keys = flash(em, rng, key, count, frame, looped, last)
        if case == 6:
            u32(em, keys + 20 * key + 12, 0xFFFFFFFF)
        return (t,)
    run(e, 0x530AD0, "FlashTask_Run", setup, cw=cw)


# --- point light pool ------------------------------------------------------------

def light_pool(em, rng, used, slot):
    data = em.alloc(4 * 10)
    slots = em.alloc(4 * 10)
    order = list(range(10))
    rng.shuffle(order)
    u32(em, slots, *order)
    for i in range(10):
        u32(em, data + 4 * i, order.index(i))
    u32(em, 0xAFA800, data, slots, used, 10, 4, 0)
    light = task(em, 0x80, 0x46)
    u32(em, light + 0x78, data + 4 * order[slot])
    return light


@pytest.mark.parametrize("used,slot", [(5, 4), (5, 0), (5, 2), (1, 0), (10, 9), (10, 3)])
def test_point_light_delete(used, slot):
    def setup(em):
        return 0x9999, light_pool(em, random.Random(used * 10 + slot), used, slot)
    run(e, 0x531250, "PointLightObj_Delete", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(3))
def test_flash_delete_and_range(case, cw):
    def setup(em):
        rng = random.Random(case + 20)
        t, keys = flash(em, rng, 1, 3, 0, 0)
        light = light_pool(em, rng, 6, case)
        u32(em, t + 0x20, light)
        return (t,)
    run(e, 0x530C40, "FlashTask_Delete", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("rng_seed", range(3))
def test_point_light_set_range(rng_seed, cw):
    def setup(em):
        rng = random.Random(rng_seed)
        light = task(em, 0x80, 0x46)
        pos = em.alloc(24)
        d64(em, pos, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
        return 0x9999, light, pos, struct.unpack("<I", struct.pack("<f", rng.uniform(0, 300)))[0]
    run(e, 0x5312D0, "PointLightObj_SetRange", setup, cw=cw)


@pytest.mark.parametrize("orig,ours,size", [(0x531200, "PointLightObj_Push", 0x80),
                                            (0x531220, "SpotLightObj_Push", 0x90)])
def test_light_push(orig, ours, size):
    def setup(em):
        rng = random.Random(size)
        light = task(em, size, 0x46)
        rnd_bytes(em, light + 0x20, size - 0x20, rng)
        return (light,)
    run(e, orig, ours, setup)


@pytest.mark.parametrize("named", [0, 1])
@pytest.mark.parametrize("orig,ours,size", [(0x5313F0, "PointLightObj_GetInfo", 0x80),
                                            (0x531480, "SpotLightObj_GetInfo", 0x90)])
def test_light_get_info(orig, ours, size, named):
    def setup(em):
        rng = random.Random(size + named)
        light = task(em, size, 0x46)
        rnd_bytes(em, light + 0x20, size - 0x20, rng)
        out = em.alloc(0x54)
        rnd_bytes(em, out, 0x50, rng)
        name = em.alloc(16)
        em.write(name, b"\xee" * 16)
        u32(em, out + 0x50, name if named else 0)
        return light, out
    run(e, orig, ours, setup)


# --- particle systems ----------------------------------------------------------------

def cam(em, rng, scale=1.0):
    d64(em, 0xBCAB08, *[rng.uniform(-3000, 3000) * scale for _ in range(3)])
    a, b = rng.uniform(0, 6.283), rng.uniform(-1.5, 1.5)
    m = [math.cos(a), 0.0, -math.sin(a), -math.sin(a) * math.sin(b), math.cos(b), -math.cos(a) * math.sin(b),
         math.sin(a) * math.cos(b), math.sin(b), math.cos(a) * math.cos(b)]
    f32(em, 0xBCAAE0, *m)
    f32(em, 0xBCAB20, rng.uniform(0.3, 1.5), rng.uniform(0.3, 1.2))


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("seed", range(40))
def test_particle_is_visible(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        cam(em, rng)
        p = em.alloc(24)
        cp = struct.unpack("<3d", em.read(0xBCAB08, 24))
        d64(em, p, *[c + rng.uniform(-400, 400) for c in cp])
        r = rng.choice([0.0, 5.0, 50.0, 200.0, NAN])
        return p, struct.unpack("<I", struct.pack("<f", r))[0]
    run(e, 0x530F20, "ParticleSystem_IsVisible", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("seed", range(12))
def test_particle_draw(seed, cw):
    def setup(em):
        rng = random.Random(seed + 100)
        cam(em, rng)
        ps = task(em, 0xB8, 0x48)
        rnd_bytes(em, ps + 0x20, 0x98, rng)
        u32(em, ps + 0x6C, rng.choice([0, 1, 2, 3]), rng.choice([0x3F, 0xFF, 0x1FF]), 0x7474,
            rng.choice([64, 128, 255]))
        u32(em, ps + 0x7C, 0x7C7C, 0x8080)
        f32(em, ps + 0x84, rng.choice([10.0, 100.0, 1000.0]))
        u32(em, ps + 0xAC, ON_MOVE if rng.random() < 0.5 else 0, SPAWN)
        em.write(ps + 0xB4, bytes([rng.choice([0, 1])]))
        return (ps,)
    run(e, 0x530CD0, "ParticleSystem_Draw", setup, cw=cw)


def test_particle_delete():
    def setup(em):
        ps = task(em, 0xB8, 0x48)
        u32(em, ps + 0x78, 0x5678, 0x9ABC)
        return (ps,)
    run(e, 0x531090, "ParticleSystem_Delete", setup)


# --- TransparencyList ------------------------------------------------------------

from test_freecam_util import LOG  # noqa: E402

te = new_emu()


def frame_alloc(em, a):
    return em.alloc(a[0])


def prim_register(em, a):
    return 0x60 + len(em.read_cstr(a[0]))


for addr, n, kw in [
    (0x401900, 5, dict(strings=(0,), masks={2: 0xFFFF}, effect=task_new)),
    (0x401400, 3, dict(masks={0: 0xFFFF, 1: 0xFF})),
    (0x401530, 3, dict(masks={0: 0xFFFF})),
    (0x401A20, 1, dict(masks={0: 0xFFFF})),
    (0x4B0D10, 1, {}),  # Mem_Free
    (0x4C6840, 3, {}),  # DynCube_ListRemove
    (0x4978D0, 1, dict(strings=(0,), effect=prim_register)),  # Primitive_Register
    (0x4978E0, 1, {}),  # Primitive_Unregister
    (0x4978F0, 2, {}),  # Primitive_SetHandler
    (0x4950E0, 0, dict(ret=2)),  # Direct3D_GetRenderMode
    (0x4B0EC0, 3, {}),  # RenderMode_SetMethod
    (0x497800, 1, dict(effect=frame_alloc)),  # Frame_Alloc
    (0x4978B0, 2, dict(derefs={1: 16})),  # RenderQueue_Add
]:
    logfake(te, addr, n, **kw)


def draw_primitive(em):
    a = [em.arg(i) for i in range(6)]
    if LOG:
        LOG[-1].append(("draw",) + tuple(a))
    em.ret(0, 24)


DRAW = trap(te, "draw_primitive", draw_primitive)
VTBL = te.alloc(0x68)
u32(te, VTBL + 0x64, DRAW)
DEVICE = te.alloc(4)
u32(te, DEVICE, VTBL)
te.baseline = te.snapshot()


def trans_list(em, rng, count):
    l = task(em, 0x80, 0x49)
    rnd_bytes(em, l + 0x20, 0x48, rng)
    entries = em.alloc(8 * max(count, 1))
    for i in range(count):
        tris = rng.choice([0, 1, 2, 3])
        stride = rng.choice([16, 24, 32])
        b = em.alloc(0x20 + stride * 3 * tris)
        u32(em, b, 0x1111, tris)
        u32(em, b + 0x18, stride, rng.choice([0x142, 0x1C4]))
        depth = rng.choice([1.0, 2.5, 2.5, -4.0, 100.0, rng.uniform(-10, 10)])
        f32(em, entries + 8 * i, depth)
        u32(em, entries + 8 * i + 4, b)
    u32(em, l + 0x68, 0x6868, count, rng.randrange(0, 0x400), em.alloc(0x400), entries)
    return l


def trans_node(em, l):
    n = em.alloc(16)
    u32(em, n, 0, 0x55, 0, l)
    return n


def trans_globals(em):
    u16(em, 0x54F884, 0x4A)
    u16(em, 0x54875C, 0x33)
    u32(em, 0xC28F90, 0x61, 0x62)
    u32(em, 0xBCABE0, 9)
    u32(em, 0x5CA114, DEVICE)


@pytest.mark.parametrize("orig,ours", [(0x531FA0, "TransparencyList_Init"), (0x5321C0, "TransparencyList_Shutdown")])
def test_trans_setup(orig, ours):
    run(te, orig, ours, trans_globals)


def test_trans_delete():
    def setup(em):
        return (trans_list(em, random.Random(1), 2),)
    run(te, 0x531F60, "TransparencyList_Delete", setup)


@pytest.mark.parametrize("a,b", [(1.0, 2.0), (2.0, 1.0), (1.0, 1.0), (-0.0, 0.0), (NAN, 1.0), (1.0, NAN), (NAN, NAN)])
def test_trans_compare(a, b):
    def setup(em):
        p, q = em.alloc(8), em.alloc(8)
        f32(em, p, a)
        f32(em, q, b)
        return p, q
    run(te, 0x5320D0, "TransparencyList_Compare", setup, check_eax=True)


@pytest.mark.parametrize("count", [0, 1, 2, 5, 9, 20])
def test_trans_draw(count):
    def setup(em):
        trans_globals(em)
        u32(em, 0xA84620, 0x1234)
        return (trans_node(em, trans_list(em, random.Random(count), count)),)
    run(te, 0x532030, "TransparencyList_Draw", setup)


@pytest.mark.parametrize("orig,ours", [(0x532100, "TransparencyList_Start"), (0x532140, "TransparencyList_Render")])
def test_trans_start_render(orig, ours):
    def setup(em):
        trans_globals(em)
        l = trans_list(em, random.Random(5), 3)
        return (trans_node(em, l) if ours.endswith("Start") else l,)
    run(te, orig, ours, setup)
