import random
import struct

import pytest

from emu import Emu
from test_lod_util import (CTX, RENDER_FUNCS, SCENE, Fakes, check, f32, f64, mesh_header, model_lod, put, set_view,
                           u32, vir_model)

e = Emu()
fk = Fakes(e)
fk.add(0x4AF810, 3, deref={0: "s", 2: "s"}, name="warning")
fk.add(0x497EC0, 0, ret=0, name="select_count")
fk.add(0x497670, 0, ret=0, name="scene_version")
fk.add(0x4CEC10, 1, ret=0, deref={0: "s"}, name="virmodel_find")
fk.add(0x4017C0, 1, ret=lambda em, a: 0x30 + len(fk.log), name="event_alloc")
fk.add(0x401810, 1, ret=0x77, name="event_alloc2")
fk.add(0x4018C0, 1, fmt=lambda em, a: (a[0] & 0xFF,), name="event_free")
fk.add(0x4018E0, 1, name="event_free2")
fk.add(0x4B0E70, 1, ret=lambda em, a: 0x100 + len(fk.log), deref={0: "s"}, name="create_method")
fk.add(0x4B0EB0, 1, name="delete_method")
fk.add(0x49A340, 0, name="49a340")
fk.add(0x4B0DB0, 0, ret=0, name="get_mode")
fk.add(0x4B0D40, 0, name="default_mode")
fk.add(0x4B0D50, 0, name="clear_mode")
fk.add(0x4B0D10, 1, name="free")

EXCLUDE_METHOD = 0xA4EE78


def exclude_handler(em, a):
    u32(em, a[0] + 12, 0x5A)
    return 0


def log_exclude(em, a):
    p = a[0]
    return (em.read(em.u32(p), 12), em.read(p + 4, 8))


EXCLUDE = fk.func(1, ret=exclude_handler, fmt=log_exclude, name="exclude")


def lod_settings(e, rng):
    f32(e, 0x548370, rng.choice([1.0, 1.5, 2.0]))
    f32(e, 0x54835C, rng.uniform(0, 1))
    f32(e, 0xA4EE98, rng.uniform(0, 3000))
    for a in (0xA4EE80, 0xA4EE84, 0xA4EE88):
        f32(e, a, rng.uniform(0.05, 3))
    for a in (0xA4EE8C, 0xA4EE90, 0xA4EE94):
        f32(e, a, rng.uniform(1, 40))


def random_model(e, rng, table):
    n = rng.randint(1, 5)
    lods = []
    c0 = rng.randint(100, 600)
    for i in range(n):
        c = c0 if i == 0 else rng.randint(20, 700)
        h = mesh_header(e, center=[rng.uniform(-50, 50) for _ in range(3)], sphere=rng.uniform(1, 200),
                        complexity=c)
        lods.append(model_lod(e, b"model_%d" % i, h))
    tab = None
    if table:
        tab = sorted(rng.uniform(0, 3000) for _ in range(5))
        if rng.random() < 0.4:
            tab[0] = 0.0
    return vir_model(e, lods, tab)


def obj_at(e, rng, size, model_off, model):
    o = e.alloc(size)
    cam = struct.unpack("<3d", e.read(CTX + 0x28, 24))
    f64(e, o + 0x20, *[c + rng.uniform(-4000, 4000) for c in cam])
    u32(e, o + model_off, model)
    return o


SEEDS = range(40)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", SEEDS)
def test_modelobj_get_lod(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        set_view(em, rng)
        lod_settings(em, rng)
        o = obj_at(em, rng, 0xA8, 0x6C, random_model(em, rng, seed % 3 == 0))
        f32(em, o + 0x98, *[rng.uniform(0.2, 3) for _ in range(3)])
        return (o,)
    check(e, fk, 0x4CFCB0, "ModelObj_GetLOD", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", SEEDS)
def test_bone_mesh_lod(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        set_view(em, rng)
        lod_settings(em, rng)
        o = obj_at(em, rng, 0xA8, 0x74, random_model(em, rng, seed % 3 == 0))
        f32(em, o + 0xA0, rng.uniform(1, 200))
        return (o,)
    check(e, fk, 0x4CFF70, "Mesh3D_GetBoneMeshLOD", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", SEEDS)
def test_spline_mesh_lod(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        set_view(em, rng)
        lod_settings(em, rng)
        o = obj_at(em, rng, 0x5A8, 0x6C, random_model(em, rng, seed % 3 == 0))
        f32(em, o + 0x44, rng.uniform(1, 200))
        if seed % 2:
            u32(em, o + 0x5A0, put(em, struct.pack("<4f", *[rng.uniform(-50, 50) for _ in range(3)],
                                                  rng.uniform(1, 200))))
        return (o,)
    check(e, fk, 0x4D0180, "Mesh3D_GetSplineMeshLOD", setup=setup, cw=cw)


@pytest.mark.parametrize("addr,name", [(0x4CFCB0, "ModelObj_GetLOD"), (0x4CFF70, "Mesh3D_GetBoneMeshLOD"),
                                       (0x4D0180, "Mesh3D_GetSplineMeshLOD")])
def test_lod_no_model(addr, name):
    check(e, fk, addr, name, setup=lambda em: (em.alloc(0x5A8),))


def test_lod_results_cover_branches():
    """The random cases above reach every return path."""
    seen = set()
    for seed in SEEDS:
        for name, size, off in (("ModelObj_GetLOD", 0xA8, 0x6C), ("Mesh3D_GetBoneMeshLOD", 0xA8, 0x74)):
            e.reset()
            rng = random.Random(seed)
            set_view(e, rng)
            lod_settings(e, rng)
            o = obj_at(e, rng, size, off, random_model(e, rng, seed % 3 == 0))
            f32(e, o + 0x98, *[rng.uniform(0.2, 3) for _ in range(3)])
            f32(e, o + 0xA0, rng.uniform(1, 200))
            fk.log = []
            r = e.call(name, o)[0]
            seen.add((seed % 3 == 0, r if r in (0, 0xFFFFFFFF) else 1))
            if fk.log:
                seen.add("warning")
    assert {(False, 0), (False, 1), (False, 0xFFFFFFFF), (True, 0), (True, 1), (True, 0xFFFFFFFF),
            "warning"} <= seen, seen


def exclude_world(em, rng, same):
    set_view(em, rng)
    f32(em, SCENE + 0x18, *[rng.uniform(0, 300) for _ in range(2)], *[rng.uniform(100, 600) for _ in range(8)])
    fk.rets[0x497EC0] = 11
    fk.rets[0x497670] = 22
    u32(em, 0xA4EEA0, 11 if same > 0 else 5)
    u32(em, 0xA4EE9C, 22 if same > 1 else 6)
    u32(em, EXCLUDE_METHOD, 9)
    u32(em, RENDER_FUNCS + 4 * 9, EXCLUDE)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("same", [0, 1, 2])
@pytest.mark.parametrize("seed", range(4))
def test_exclude(seed, same, cw):
    def setup(em):
        rng = random.Random(seed)
        exclude_world(em, rng, same)
        return put(em, struct.pack("<3f", 1, 2, 3)), struct.unpack("<I", struct.pack("<f", 12.5))[0], 0x1234
    check(e, fk, 0x4D03D0, "Mesh3D_Exclude", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_exclude_rigid(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        exclude_world(em, rng, 2)
        o = obj_at(em, rng, 0xA8, 0x6C, random_model(em, rng, False))
        f32(em, o + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
        f32(em, o + 0x98, *[rng.uniform(0.2, 3) for _ in range(3)])
        return o, 0x777
    check(e, fk, 0x4D0510, "Mesh3D_ExcludeRigid", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_exclude_bone(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        exclude_world(em, rng, 2)
        o = obj_at(em, rng, 0xA8, 0x74, random_model(em, rng, False))
        return o, 0x777
    check(e, fk, 0x4D06C0, "Mesh3D_ExcludeBone", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_exclude_spline(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        exclude_world(em, rng, 2)
        o = obj_at(em, rng, 0x5A8, 0x6C, random_model(em, rng, False))
        f32(em, o + 0x44, rng.uniform(1, 200))
        if seed % 2:
            u32(em, o + 0x5A0, put(em, struct.pack("<4f", *[rng.uniform(-50, 50) for _ in range(4)])))
        return o, 0x777
    check(e, fk, 0x4D0790, "Mesh3D_ExcludeSpline", setup=setup, cw=cw)


@pytest.mark.parametrize("addr,name,var,mask", [
    (0x4CFC30, "Mesh3D_GetDrawRigidMethod", 0xB46D14, 0xFFFFFFFF),
    (0x4CFC40, "Mesh3D_GetDrawBoneMethod", 0xB81880, 0xFFFFFFFF),
    (0x4CFC50, "Mesh3D_GetDrawSplineMethod", 0xB46CE0, 0xFFFFFFFF),
    (0x4CFC60, "Mesh3D_GetExcludeMethod", 0xA4EE78, 0xFFFFFFFF),
    (0x4CFC70, "Mesh3D_GetEvent1", 0x548367, 0xFF),
    (0x4CFC80, "Mesh3D_GetEvent2", 0x548366, 0xFF),
    (0x4CFC90, "Mesh3D_GetEvent3", 0x548365, 0xFF),
    (0x4CFCA0, "Mesh3D_GetEvent4", 0x548368, 0xFF),
    (0x4D0D80, "Mesh3D_GetEvent5", 0x54836C, 0xFFFFFFFF)])
def test_getters(addr, name, var, mask):
    check(e, fk, addr, name, setup=lambda em: em.write(var - 3, bytes([0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97])),
          eax_mask=mask)


def test_bone_world_vertex():
    check(e, fk, 0x4D0930, "Mesh3D_GetBoneWorldVertex", setup=lambda em: (put(em, b"\xAA" * 32),), check_eax=False)


@pytest.mark.parametrize("which", ["none", "nolod", "ok"])
def test_get_radius(which):
    def setup(em):
        if which == "none":
            return (0,)
        if which == "nolod":
            return (vir_model(em, []),)
        return (vir_model(em, [model_lod(em, header=mesh_header(em, radius=37.25))]),)
    check(e, fk, 0x4D0950, "VirModel_GetRadius", setup=setup, check_eax=False, check_st0=True)


VERTS = [(1.5, -2.25, 3.0), (10.1, 20.2, -30.3), (-0.7, 0.3, 123.456)]


def vertex_obj(em, which, rng):
    o = em.alloc(0xA8)
    f32(em, o + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
    f32(em, o + 0x98, *[rng.uniform(0.2, 3) for _ in range(3)])
    vm = 0
    if which != "none":
        vm = vir_model(em, [model_lod(em, header=mesh_header(em, verts=VERTS))] if which == "ok" else [])
    u32(em, o + 0x6C, vm)
    return o, vm


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("which", ["none", "nolod", "ok"])
def test_get_vertex(which, cw):
    def setup(em):
        o, _ = vertex_obj(em, which, random.Random(3))
        return em.alloc(12), o, 2
    check(e, fk, 0x4D0970, "Mesh3D_GetVertex", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("which", ["none", "nolod", "ok"])
def test_get_model_vertex(which, cw):
    def setup(em):
        o, vm = vertex_obj(em, which, random.Random(4))
        fk.rets[0x4CEC10] = vm
        return em.alloc(12), o, 1, put(em, b"tree\0")
    check(e, fk, 0x4D09E0, "Mesh3D_GetModelVertex", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("i", [0, 1, 2])
@pytest.mark.parametrize("seed", range(4))
def test_get_world_vertex(seed, i, cw):
    def setup(em):
        o, _ = vertex_obj(em, "ok", random.Random(seed))
        return em.alloc(24), o, i
    check(e, fk, 0x4D0A50, "Mesh3D_GetWorldVertex", setup=setup, check_eax=False, cw=cw)


def test_get_world_vertex_no_model():
    check(e, fk, 0x4D0A50, "Mesh3D_GetWorldVertex", setup=lambda em: (em.alloc(24), em.alloc(0xA8), 0),
          check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_init(cw):
    check(e, fk, 0x4D0B30, "Mesh3D_Init", check_eax=False, cw=cw)


def test_shutdown():
    def setup(em):
        em.write(0x548364, bytes([1, 4, 3, 2, 5]))
        u32(em, 0x54836C, 6)
        for i, a in enumerate((0xB817A0, 0xB8170C, 0xB46D14, 0xB81880, 0xB46CE0, 0xB818DC, 0xA4EE78)):
            u32(em, a, 0x200 + i)
    check(e, fk, 0x4D0C50, "Mesh3D_Shutdown", setup=setup, check_eax=False)


def anim_lod(em):
    return model_lod(em, header=mesh_header(em, verts=VERTS, atta=3), textures=6,
                     anims=[(4, 2, 0, 0x99), (7, 5, 3, 0x98), (2, 1, 5, 0x97)])


@pytest.mark.parametrize("addr,name,args", [
    (0x4D0CF0, "ModelLod_GetAttach", (2,)),
    (0x4D0D10, "ModelLod_GetAttachCount", ()),
    (0x4D0D20, "ModelLod_GetVertexCount", ()),
    (0x4D0D90, "ModelLod_GetTexAnimCount", ()),
    (0x4D0DA0, "ModelLod_GetTexAnimFrames", (1,)),
    (0x4D0DC0, "ModelLod_SetTexAnimFrame", (2, 0x1234)),
    (0x4D0DE0, "ModelLod_ApplyTexAnims", ()),
    (0x4D0E20, "ModelLod_RestoreTexAnims", ()),
    (0x4D0E50, "ModelLod_GetTexAnimStateSize", ())])
def test_lod_accessors(addr, name, args):
    check(e, fk, addr, name, setup=lambda em: (anim_lod(em),) + args, normalized=False,
          check_eax=name not in ("ModelLod_SetTexAnimFrame", "ModelLod_ApplyTexAnims", "ModelLod_RestoreTexAnims"))


@pytest.mark.parametrize("addr,name", [(0x4D0DE0, "ModelLod_ApplyTexAnims"),
                                       (0x4D0E20, "ModelLod_RestoreTexAnims")])
def test_tex_anims_none(addr, name):
    check(e, fk, addr, name, setup=lambda em: (model_lod(em, textures=2),), check_eax=False)


@pytest.mark.parametrize("buf", [True, False])
def test_save_tex_anim_state(buf):
    check(e, fk, 0x4D0E60, "ModelLod_SaveTexAnimState",
          setup=lambda em: (anim_lod(em), em.alloc(12) if buf else 0), check_eax=False)


@pytest.mark.parametrize("buf", [True, False])
def test_load_tex_anim_state(buf):
    check(e, fk, 0x4D0EA0, "ModelLod_LoadTexAnimState",
          setup=lambda em: (anim_lod(em), put(em, struct.pack("<3i", 9, 8, 7)) if buf else 0), check_eax=False)


@pytest.mark.parametrize("mode", [0, 1])
def test_free_lod_data(mode):
    def setup(em):
        fk.rets[0x4B0DB0] = mode
        return (put(em, struct.pack("<5I", 0x111, 0, 0x333, 0x444, 0)),)
    check(e, fk, 0x4D0D30, "ModelObj_FreeLodData", setup=setup, check_eax=False)


def test_set_lod_scale():
    check(e, fk, 0x4D0EE0, "Mesh3D_SetLodScale", args=(0x40000000,), check_eax=False)
