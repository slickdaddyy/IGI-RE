import struct

import pytest

from emu import Emu
from test_lod_util import RENDER_FUNCS, Fakes, check, cstr, put, u32

e = Emu()
fk = Fakes(e)
CHUNKS = {}
fk.add(0x4CEAA0, 3, ret=lambda em, a: CHUNKS.get(a[1], 0), fmt=lambda em, a: (a[0], a[1], a[2]), name="find_chunk")
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]), name="alloc")
fk.add(0x4B7D50, 2, deref={1: "s"}, name="palette_load")
fk.add(0x4B7D30, 1, name="palette_free")
fk.add(0x4B0D10, 1, name="free")
fk.add(0x4B22F0, 1, name="texture_free")
fk.add(0x4B63D0, 1, name="flush")
fk.add(0x4CF050, 0, name="init_lods")
fk.add(0x4D67A0, 1, deref={0: "s"}, name="anim_load")
UNLOAD = fk.func(1, deref={0: 8}, name="unload")

MODEL_DIR, ANIM_DIR, UNLOAD_METHOD = 0xA4ED6C, 0xA4EDF8, 0x54820C


def names(*ss):
    return struct.pack("<i", len(ss)) + b"".join(s + b"\0" for s in ss)


def new_list(em, p):
    em.write(p, struct.pack("<III", p + 4, 0, p))


def add_tail(em, lst, node):
    last = em.u32(lst + 8)
    em.write(node, struct.pack("<II", lst + 4, last))
    u32(em, last, node)
    u32(em, lst + 8, node)


def model_file(em):
    f = em.alloc(0x4C)
    for off in (0x10, 0x1C, 0x28):
        new_list(em, f + off)
    u32(em, f + 8, 0x7000)
    u32(em, f + 0xC, 0x7100)
    return f


@pytest.mark.parametrize("n", [0, 3])
def test_load_palettes(n):
    def setup(em):
        em.write(MODEL_DIR, b"LOCAL:\0")
        f = model_file(em)
        CHUNKS.clear()
        CHUNKS[0x50414C46] = put(em, names(*[b"pal%d" % i for i in range(n)]))
        return (f,)
    check(e, fk, 0x4CF870, "ModelFile_LoadPalettes", setup=setup, check_eax=False)


@pytest.mark.parametrize("case", ["none", "empty", "dir", "animdir"])
def test_load_anims(case):
    def setup(em):
        em.write(MODEL_DIR, b"LOCAL:\0")
        em.write(ANIM_DIR, b"COMMON:\0" if case == "animdir" else b"\0")
        CHUNKS.clear()
        if case != "none":
            CHUNKS[0x42414E4D] = put(em, names() if case == "empty" else names(b"walk", b"run"))
        return (model_file(em),)
    check(e, fk, 0x4CF910, "ModelFile_LoadAnims", setup=setup, check_eax=False)


def mesh(em, anims):
    m = em.alloc(0x18)
    u32(em, m + 8, 0x8000 + anims)
    u32(em, m + 0xC, 0x8100 + anims)
    u32(em, m + 0x10, anims)
    u32(em, m + 0x14, 0x8200 + anims)
    return m


def coll(em, i):
    m = em.alloc(0x14)
    u32(em, m + 0x10, 0x9000 + i)
    return m


@pytest.mark.parametrize("handler", [True, False])
@pytest.mark.parametrize("shared", [0, 1])
@pytest.mark.parametrize("textures", [0, 3])
def test_free(textures, shared, handler):
    def setup(em):
        f = model_file(em)
        lst = em.alloc(12)
        new_list(em, lst)
        add_tail(em, lst, f)
        u32(em, f + 0x48, shared)
        u32(em, f + 0x44, 2)
        u32(em, f + 0x3C, em.alloc(0x18))
        u32(em, f + 0x40, textures)
        if textures:
            u32(em, f + 0x34, em.alloc(textures * 0x24))
            info = em.alloc(textures * 0xC)
            u32(em, info + 4, 1)
            u32(em, info + 0x1C, 1)
            u32(em, f + 0x38, info)
        for i in (0, 2):
            add_tail(em, f + 0x10, mesh(em, i))
        for i in range(2):
            add_tail(em, f + 0x1C, coll(em, i))
        for i in range(3):
            add_tail(em, f + 0x28, em.alloc(0x1C))
        u32(em, UNLOAD_METHOD, 5)
        u32(em, RENDER_FUNCS + 20, UNLOAD if handler else 0)
        return (f,)
    check(e, fk, 0x4CF9D0, "ModelFile_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("anims", [0, 2])
def test_mesh_free(anims):
    def setup(em):
        lst = em.alloc(12)
        new_list(em, lst)
        m = mesh(em, anims)
        add_tail(em, lst, m)
        u32(em, UNLOAD_METHOD, 5)
        u32(em, RENDER_FUNCS + 20, UNLOAD)
        return (m,)
    check(e, fk, 0x4CFB30, "Mesh_Free", setup=setup, check_eax=False)


def test_coll_mesh_free():
    def setup(em):
        lst = em.alloc(12)
        new_list(em, lst)
        m = coll(em, 7)
        add_tail(em, lst, m)
        return (m,)
    check(e, fk, 0x4CFBB0, "CollMesh_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4CFBD0, "Model_SetDir"), (0x4CFC00, "Model_SetAnimDir")])
@pytest.mark.parametrize("s", [b"", b"LOCAL:", b"LOCAL:missions/location0/level1/"])
def test_set_dir(addr, name, s):
    check(e, fk, addr, name, setup=lambda em: (cstr(em, s),), check_eax=False)
