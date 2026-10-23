"""SplinePathNodeQTask and SplinePathDynCubeObj (0x5199D0 - 0x51ACB0)."""
import struct

import pytest

from emu import Emu
from test_nodereg_util import CWS, FISTP_CWS, NODE, PATH, d64, f32, logfake, node, path, rnd, run, task, u16, u32

OTHER = 0x53

emu = Emu()
logfake(emu, 0x4AF810, 1, strings=(0,))  # Warning_Show
u16(emu, 0x54ED4C, NODE)
u16(emu, 0x54ED4E, PATH)
emu.baseline = emu.snapshot()


def test_node_get_info():
    for name in (0, 1):
        def setup(e):
            info = e.alloc(0x54)
            u32(e, info + 0x50, e.alloc(16) if name else 0)
            return node(e, rnd(1)), info
        run(emu, 0x5199D0, "SplinePathNode_GetInfo", setup)


@pytest.mark.parametrize("ptype", [PATH, OTHER])
def test_node_create(ptype):
    run(emu, 0x519AD0, "SplinePathNode_Create", lambda e: (node(e, rnd(2), task(e, 0x40, ptype)),))


@pytest.mark.parametrize("cw", CWS)
def test_update_matrix(cw):
    cases = [(0.0, 0.0, 0.0), (100.0, 0.0, 0.0), (0.0, 0.0, 5.0), (0.0, 0.0, -3.0), (3.0, -4.0, 12.0),
             (1e-30, 0.0, 0.0), (-7.5, 2.25, 0.0)]
    ups = [(0.0, 0.0, 1.0), (0.0, 0.0, 0.0), (0.3, -0.2, 0.9), (1.0, 0.0, 0.0)]
    rng = rnd(3)
    cases += [tuple(rng.uniform(-100, 100) for _ in range(3)) for _ in range(6)]
    for t in cases:
        for up in ups:
            def setup(e):
                n = node(e, rnd(4), tangent=t)
                f32(e, n + 0x40, up[0])
                f32(e, n + 0x4C, up[1])
                f32(e, n + 0x58, up[2])
                return (n,)
            run(emu, 0x519BD0, "SplinePathNode_UpdateMatrix", setup, cw=cw)


@pytest.mark.parametrize("cw", FISTP_CWS)
def test_step_scale(cw):
    for u in (-0.5, 0.0, 0.05, 0.15, 0.349, 0.95, 0.96, 1.2, float("nan")):
        def setup(e):
            return node(e, rnd(5)), struct.unpack("<I", struct.pack("<f", u))[0]
        run(emu, 0x51A0C0, "SplinePathNode_GetStepScale", setup, cw=cw, check_st0=True)


@pytest.mark.parametrize("n,extra", [(0, ()), (1, ()), (4, (OTHER, NODE)), (101, ())])
def test_collect(n, extra):
    def setup(e):
        p, _ = path(e, rnd(6), n, extra=extra)
        u32(e, p + 0x1F8, 77)
        return (p,)
    run(emu, 0x51A110, "SplinePath_CollectNodes", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("n,which", [(0, 0), (3, 5), (3, 0), (4, 1), (4, 3), (5, 2)])
def test_remove(n, which, cw):
    def setup(e):
        p, nodes = path(e, rnd(7 + n), n)
        return p, nodes[which] if which < n else 0x1234
    run(emu, 0x51A160, "SplinePath_RemoveNode", setup, cw=cw)


def test_path_create():
    def setup(e):
        p = task(e, 0x200, PATH)
        u32(e, p + 0x68, *range(1, 101))
        u32(e, p + 0x1F8, 5, 6)
        return (p,)
    run(emu, 0x51A240, "SplinePath_Create", setup)


US = (0.0, 0.25, 0.5, 0.731, 1.0, -0.2, 1.3)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("orig,ours", [(0x51A280, "SplinePath_GetPos"), (0x51A370, "SplinePath_GetTangent")])
def test_hermite(orig, ours, cw):
    for u in US:
        def setup(e):
            p, _ = path(e, rnd(8), 3)
            return e.alloc(24), p, 1, struct.unpack("<I", struct.pack("<f", u))[0]
        run(emu, orig, ours, setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
def test_get_matrix(seed, cw):
    for u in US:
        def setup(e):
            rng = rnd(9 + seed)
            p, nodes = path(e, rng, 3)
            if seed == 1:
                f32(e, nodes[1] + 0x60, *struct.unpack("<4f", e.read(nodes[0] + 0x60, 16)))
            if seed == 2:
                q = struct.unpack("<4f", e.read(nodes[0] + 0x60, 16))
                f32(e, nodes[1] + 0x60, *[-x for x in q])
            if seed == 3:
                q = struct.unpack("<4f", e.read(nodes[0] + 0x60, 16))
                f32(e, nodes[1] + 0x60, q[0] + 0.001, q[1], q[2], q[3])
            if seed == 4:
                d64(e, nodes[0] + 0x78, 0.0, 0.0, 0.0)
                d64(e, nodes[1] + 0x78, 0.0, 0.0, 0.0)
                d64(e, nodes[1] + 0x20, *struct.unpack("<3d", e.read(nodes[0] + 0x20, 24)))
            return p, e.alloc(0x28), 0, struct.unpack("<I", struct.pack("<f", u))[0]
        run(emu, 0x51A4A0, "SplinePath_GetMatrix", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("n,autos,speeds", [(2, None, None), (3, None, None), (4, (0, 1, 0, 1), None),
                                            (5, (1, 0, 1, 1, 0), (10.0, 0.0, 0.0, 5.0, 2.0)),
                                            (3, (0, 0, 0), (-1.0, 1.0, float("nan")))])
def test_update_lengths(n, autos, speeds, cw):
    def setup(e):
        p, _ = path(e, rnd(10 + n), n, autos, speeds=speeds)
        return (p,)
    run(emu, 0x51A8C0, "SplinePath_UpdateLengths", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
def test_update_rotations(seed, cw):
    tangents = [(1.0, 0.0, 0.0), (0.0, -1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0),
                (0.0, 0.0, -1.0), (0.0, 0.0, 0.0), (-3.0, -4.0, 0.5), (2.0, -1.0, -7.0)]

    def setup(e):
        rng = rnd(20 + seed)
        p, nodes = path(e, rng, len(tangents))
        for k, t in zip(nodes, tangents):
            d64(e, k + 0x78, *t)
            if seed & 1:
                f32(e, k + 0x40, 0.0)
                f32(e, k + 0x4C, 0.0)
                f32(e, k + 0x58, 1.0)
            if seed & 2:
                f32(e, k + 0x40, -0.6)
                f32(e, k + 0x4C, 0.0)
                f32(e, k + 0x58, -0.8)
        return (p,)
    run(emu, 0x51ACB0, "SplinePath_UpdateRotations", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("ptype,speed", [(PATH, 3.5), (PATH, -2.0), (OTHER, 1.0), (PATH, float("nan"))])
def test_node_params_changed(ptype, speed, cw):
    def setup(e):
        p, nodes = path(e, rnd(30), 4, extra=(OTHER,))
        u16(e, p + 0x1C, ptype)
        f32(e, nodes[2] + 0x90, speed)
        return (nodes[2],)
    run(emu, 0x519A60, "SplinePathNode_ParamsChanged", setup, cw=cw)


@pytest.mark.parametrize("which", [0, 2])
def test_node_delete(which):
    def setup(e):
        p, nodes = path(e, rnd(31), 4)
        return (nodes[which],)
    run(emu, 0x519B80, "SplinePathNode_Delete", setup)
