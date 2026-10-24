import random
import struct

import pytest

from emu import Emu
from test_animtrig_util import CWS, add_stubs, check_cw, logfn
from test_menu_util import RET, check as _check, cstr, f32, logfake, u16, u32

PORTS, INDEX = 0xBC20A0, 0xA5EF9C
JOY, KEYS, MOUSE = 0xBCAA08, 0xC28C62, 0xC28F84

e = Emu()
add_stubs(e)
for addr, n in ((0x490230, 0), (0x48FC20, 0), (0x509CF0, 0), (0x507E90, 2), (0x507EC0, 2), (0x401A20, 1),
                (0x4012A0, 3), (0x401530, 3), (0x401400, 3), (0x4018F0, 0), (0x401900, 4), (0x4F1950, 1),
                (0x4F0F50, 0), (0x4F0FD0, 0), (0x4F1A60, 0), (0x4F1A70, 0), (0x4C1830, 1),
                (0x4CEC10, 1), (0x4B8930, 2), (0x4B80B0, 1), (0x4F0E50, 4), (0x4B85B0, 1), (0x4B83D0, 1)):
    kw = {}
    if addr in (0x401A20,):
        kw["masks"] = {0: 0xFFFF}
    if addr in (0x4012A0, 0x401530):
        kw["masks"] = {1 if addr == 0x4012A0 else 0: 0xFFFF}
    if addr == 0x401400:
        kw["masks"] = {0: 0xFFFF, 1: 0xFF}
    if addr == 0x401900:
        kw["masks"] = {2: 0xFFFF}
        kw["strings"] = (0,)
    if addr in (0x4CEC10, 0x4B80B0):
        kw["strings"] = (0,)
    if addr == 0x4B8930:
        kw["strings"] = (0,)
    if addr == 0x4F0E50:
        kw["strings"] = (0,)
    logfake(e, addr, n, **kw)
# ParamTable_Add: the template is a stack local, log the field offset
logfn(e, 0x4F1A80, lambda em, a: [a[0], a[2] - a[1], a[3], a[4], a[5]], 6)


def check(orig, ours, setup=None, cw=None, **kw):
    kw.setdefault("check_eax", False)
    if cw is None:
        return _check(e, orig, ours, setup, **kw)
    return check_cw(e, orig, ours, cw, setup, **kw)


def test_unk():
    check(0x4ED000, "InputPort_ResetUnused", lambda em: u32(em, 0xA5EF98, 5))


def port_setup(em, rng, port, analog, axes, bind_bits):
    p = PORTS + 0xB0 * port
    binds = em.alloc(12 * 6)
    devs = [(1, 0x1E), (1, 0x20), (2, 0x4), (3, 0x2), (2, 0x10), (4, 1)]
    bits = [0x10, 0x20, 0x40, 0x80, 0x01, 0x02]
    for i, (d, k) in enumerate(devs):
        u32(em, binds + 12 * i, bits[i], d, k)
    u32(em, p, 1, 6, binds, analog)
    f32(em, p + 0x10, *axes)
    f32(em, p + 0x20, 0.8, 0.5, 1.0, 1.0)
    f32(em, p + 0x30, 0.15, 0.25, 0.1, 0.1)
    f32(em, p + 0x40, 0.3, 0.07, 0.2, 0.2)
    # which bindings are down
    em.write(KEYS + 0x1E, bytes([0x80 if bind_bits & 0x10 else 0x7F]))
    em.write(KEYS + 0x20, bytes([0x80 if bind_bits & 0x20 else 0]))
    u32(em, JOY + 0x28 * port + 0x18, (0x4 if bind_bits & 0x40 else 0) | (0x10 if bind_bits & 1 else 0))
    u32(em, MOUSE + 8, 0x2 if bind_bits & 0x80 else 0x1)
    f32(em, JOY + 0x28 * port, *(rng.uniform(-1.2, 1.2) for _ in range(2)))
    f32(em, JOY + 0x28 * port + 0x10, *(rng.uniform(-0.5, 1.5) for _ in range(2)))
    f32(em, MOUSE, *(rng.uniform(-1.5, 1.5) for _ in range(2)))
    u32(em, INDEX, port)
    out = em.alloc(0x20)
    return (out,)


AXES = [(0, 0, 0, 0), (0.75, -0.3, 0.5, 0.1), (-0.9, 0.9, 1.2, -0.4), (0.8, -0.8, -0.3, 0.0), (0.05, -0.05, 0.9, 0.3),
        (-0.8, 0.2, 0.2, 0.2), (2.0, -2.0, 0.0, 0.0)]


@pytest.mark.parametrize("bind_bits", [0, 0x10, 0x20, 0x30, 0x40, 0x80, 0xC0, 0x51, 0xA0])
@pytest.mark.parametrize("axes", range(len(AXES)))
@pytest.mark.parametrize("analog", [0, 2, 3])
@pytest.mark.parametrize("cw", CWS[:2])
def test_input_handler(bind_bits, axes, analog, cw):
    def setup(em):
        rng = random.Random(axes * 7 + analog)
        return port_setup(em, rng, (axes + analog) % 2, analog, AXES[axes], bind_bits)
    check(0x4ED010, "InputPort_InputHandler", setup, cw)


def test_init():
    check(0x4ED510, "InputPort_Init", lambda em: [u32(em, PORTS + 0xB0 * i, 1) for i in range(4)] and None,
          normalized=True)


@pytest.mark.parametrize("active", [(1, 0, 1, 1), (0, 0, 0, 0)])
def test_run(active):
    def setup(em):
        for i, a in enumerate(active):
            u32(em, PORTS + 0xB0 * i, a)
        u32(em, INDEX, 7)
        return (0x5000,)
    check(0x4ED560, "InputPort_Run", setup)


def test_shutdown_new():
    check(0x4ED5B0, "InputPort_Shutdown", lambda em: u16(em, 0xA5EFA0, 0x45))
    RET[0x4012A0] = 0x1234
    check(0x4ED5C0, "InputPort_New", lambda em: (u16(em, 0xA5EFA0, 0x45), (0x5000,))[1], check_eax=True)


@pytest.mark.parametrize("port", [0, 3])
def test_setup(port):
    def setup(em):
        u32(em, PORTS + 0xB0 * port + 0x1C, 0xDEAD)
        return (port, 4, 0x9000, 2) + tuple(0x3F000000 + i for i in range(12))
    check(0x4ED5E0, "InputPort_Setup", setup, normalized=True)


# --- LOD settings ---

def test_lod_init():
    def setup(em):
        RET[0x4018F0] = 0x11
        RET[0x401900] = 0x60
        RET[0x4F1950] = 0x7700
        RET[0x4F0F50] = 0x8800
        RET[0x4F0FD0] = 0x8900
        RET[0x4F1A60] = 0x2A
        RET[0x4F1A70] = 0x2B
    check(0x4ED6B0, "LODSettings_Init", setup, normalized=True)


def test_lod_create():
    def setup(em):
        t = em.alloc(0x40)
        em.write(t, b"\xAA" * 0x40)
        return (t,)
    check(0x4EDA70, "LODSettings_Create", setup)


@pytest.mark.parametrize("cw", CWS[:2])
def test_lod_changed(cw):
    def setup(em):
        t = em.alloc(0x40)
        f32(em, t + 0x20, 50.0, 33.3, 71.7, 120.0, 9.99, 1234.5, 0.6, 2.7)
        return (t,)
    check(0x4EDAA0, "LODSettings_Changed", setup, cw)


def test_lod_get_params():
    def setup(em):
        u32(em, 0xA5EFC8, 0x7700, 0x7800)
        out = em.alloc(4)
        return 0x1000, out
    check(0x4EDB20, "LODSettings_GetParams", setup)
    check(0x4EDBF0, "ModelLODSettings_GetParams", setup)


def test_model_create():
    def setup(em):
        t = em.alloc(0x44)
        em.write(t, b"\xAA" * 0x44)
        return (t,)
    check(0x4EDB30, "ModelLODSettings_Create", setup)


@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("dist", [(1, 2, 3, 4, 5), (5, 4, 6, 1, 7.5), (0, 0, 0, 0, 0), (9, 1, 1, 1, 1)])
@pytest.mark.parametrize("cw", CWS[:2])
def test_model_changed(found, dist, cw):
    def setup(em):
        t = em.alloc(0x44)
        em.write(t + 0x20, b"tree\0")
        f32(em, t + 0x30, *dist)
        RET[0x4CEC10] = em.alloc(0x40) if found else 0
        return (t,)
    check(0x4EDB60, "ModelLODSettings_Changed", setup, cw)


@pytest.mark.parametrize("found", [0, 1])
def test_model_delete(found):
    def setup(em):
        t = em.alloc(0x44)
        em.write(t + 0x20, b"tree\0")
        RET[0x4CEC10] = em.alloc(0x40) if found else 0
        if found:
            u32(em, RET[0x4CEC10] + 0x24, 1)
        return (t,)
    check(0x4EDBD0, "ModelLODSettings_Delete", setup)


@pytest.mark.parametrize("script", [0, 0x4321])
def test_container_create(script):
    def setup(em):
        RET[0x4B80B0] = script
        return (0x6000,)
    check(0x4EDC00, "ModelLODSettingsContainer_Create", setup)


def test_container_predelete():
    check(0x4EDC50, "ModelLODSettingsContainer_PreDelete", lambda em: u32(em, 0xA5EFD4, 0x6000))


def test_lod_shutdown():
    def setup(em):
        u32(em, 0xA5EFC8, 0x7700, 0x7800)
        u16(em, 0x54DBDC, 0x60, 0x61, 0x62)
    check(0x4EDC60, "LODSettings_Shutdown", setup)


def test_container_type():
    check(0x4EDCB0, "ModelLODSettingsContainer_GetType", lambda em: u16(em, 0x54DBE0, 0x62), check_eax=True,
          eax_mask=0xFFFF)
