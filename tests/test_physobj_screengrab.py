import itertools

import pytest

from emu import Emu
from test_physobj_util import Fakes, check, cstr, u32

TYPE, STEP, HEADER, SINGLE, RECORDING, PARAMS, FRAME = 0x54DCB8, 0x54DCBC, 0xA70C48, 0xA70C5A, 0xA70C5B, 0xA70C60, \
    0xA70C64
KEYS, PREV = 0xC28C62, 0xC28D62

e = Emu()
fk = Fakes(e)
EXIST = set()
STATE = {"fail_w": None, "alloc": 1}
BUF = e.alloc(64)
PIX = e.alloc(64)
DISP = e.alloc(12)
u32(e, DISP, 0, 640, 480)


def file_open(em, a):
    path, mode = em.read_cstr(a[0]), em.read_cstr(a[1])
    if mode == b"r":
        return 0x5000 + len(path) if path in EXIST else 0
    return 0 if path == STATE["fail_w"] else 0x6000


def write_fmt(em, a):
    return (a[0], em.read(a[1], 18) if a[2] == 18 else em.normalize(a[1]), a[2])


fk.add(0x4018F0, 0, ret=0x10)                                   # Task_GetRootType
fk.add(0x401900, 5, ret=0xABCD0123, fmt=lambda em, a: (em.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))
fk.add(0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})                 # TaskType_SetHandler
fk.add(0x401530, 3, masks={0: 0xFFFF})                          # TaskType_SetHandler2
fk.add(0x401A20, 1, masks={0: 0xFFFF})                          # TaskType_Free
fk.add(0x4C1830, 1)                                             # QTaskList_Free
fk.add(0x4E7530, 0, ret=0x2C)                                   # Screen_GetDrawEvent
fk.add(0x4F1A70, 0, ret=0x33)                                   # Param_GetChangedEvent
fk.add(0x4F1A60, 0, ret=0x32)                                   # Param_GetTableEvent
fk.add(0x4F1950, 1, ret=0x2000B000)                             # ParamTable_New
fk.add(0x4F0FC0, 0, ret=0x111)                                  # Param_VarString
fk.add(0x4F1000, 0, ret=0x222)                                  # Param_Bool8
fk.add(0x4F1A80, 6, fmt=lambda em, a: (a[0], a[2] - a[1], a[3], a[4], a[5]))  # ParamTable_Add
fk.add(0x4B0C60, 2, ret=lambda em, a: BUF if a[0] == 11 else (PIX if STATE["alloc"] else 0))  # Mem_Alloc
fk.add(0x4B0D10, 1)                                             # Mem_Free
fk.add(0x4B0DB0, 0, ret=0)                                      # Mem_GetMode
fk.add(0x4B0D40, 0)                                             # Mem_SetDefaultMode
fk.add(0x4B0D50, 0)                                             # Mem_ClearMode
fk.add(0x491CF0, 0, ret=DISP)                                   # Display_Get
fk.add(0x491D80, 1)                                             # Display_ReadPixels
fk.add(0x4B1510, 2, ret=file_open, fmt=lambda em, a: (em.read_cstr(a[0]), em.read_cstr(a[1])))  # File_Open
fk.add(0x4B1690, 1)                                             # File_Close
fk.add(0x4B1700, 3, fmt=write_fmt)                              # File_Write
fk.add(0x4B17C0, 1)                                             # File_Flush
fk.add(0x4AF810, 2, fmt=lambda em, a: (em.read_cstr(a[0]), em.read_cstr(a[1])))  # Warning_Show
e.baseline = e.snapshot()


def grab(em, dir_=0, on=0, was=0):
    g = em.alloc(0x28)
    u32(em, g + 0x10, 0x77)
    u32(em, g + 0x20, dir_)
    em.write(g + 0x24, bytes([on, was, 0x5A, 0x5B]))
    return g


def test_open():
    check(e, fk, 0x4EE410, "ScreenGrab_Open", check_eax=False)


def test_create():
    check(e, fk, 0x4EE530, "ScreenGrab_Create", lambda em: (grab(em, 0x1234, 1, 1),), check_eax=False)


@pytest.mark.parametrize("bits", range(1 << 7))
@pytest.mark.parametrize("recording", [0, 1])
def test_run(bits, recording):
    addrs = [KEYS + 0x1D, KEYS + 0xB5, PREV + 0xB5, KEYS + 0x46, PREV + 0x46, KEYS + 0x38, KEYS + 0x2A]

    def setup(em):
        for i, a in enumerate(addrs):
            em.write(a, bytes([0x80 if bits >> i & 1 else 0x7F]))
        em.write(RECORDING, bytes([recording]))
        em.write(SINGLE, bytes([0]))
        u32(em, STEP, 5)
    check(e, fk, 0x4EE540, "ScreenGrab_Run", setup, check_eax=False)


@pytest.mark.parametrize("single,recording", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_draw(single, recording):
    def setup(em):
        EXIST.clear()
        em.write(SINGLE, bytes([single, recording]))
        return (grab(em, cstr(em, b"LOCAL:shots")),)
    check(e, fk, 0x4EE5E0, "ScreenGrab_Draw", setup, check_eax=False)


@pytest.mark.parametrize("has_dir,on,was,recording", list(itertools.product([0, 1], repeat=4)))
def test_params_changed(has_dir, on, was, recording):
    def setup(em):
        em.write(RECORDING, bytes([recording]))
        u32(em, STEP, 5)
        return (grab(em, cstr(em, b"LOCAL:x") if has_dir else 0, on * 3, was),)
    check(e, fk, 0x4EE600, "ScreenGrab_ParamsChanged", setup, check_eax=False)


def test_get_params():
    def setup(em):
        u32(em, PARAMS, 0x12345678)
        out = em.alloc(4)
        return 0, out
    check(e, fk, 0x4EE680, "ScreenGrab_GetParams", setup, check_eax=False)


def test_close():
    def setup(em):
        u32(em, PARAMS, 0x12345678)
        u32(em, TYPE, 0xEEEE0042)
    check(e, fk, 0x4EE690, "ScreenGrab_Close", setup, check_eax=False)


SAVE_CASES = [
    # dir, single, recording, step, frame, existing frames, mode, alloc ok, fail second open
    (b"LOCAL:shots", 1, 0, 100000, 123456, 0, 0, 1, 0),
    (b"LOCAL:shots", 0, 1, 1, 7, 3, 1, 1, 0),
    (b"LOCAL:shots/", 0, 1, 100000, 299999, 2, 0, 1, 0),
    (b"LOCAL:shots\\", 1, 1, 100000, -150001, 0, 0, 1, 0),
    (None, 1, 0, 1, 0, 1, 0, 1, 0),
    (b"LOCAL:shots", 1, 0, 1, 0, 0, 0, 0, 0),
    (b"LOCAL:shots", 1, 0, 1, 0, 0, 1, 0, 0),
    (b"LOCAL:shots", 1, 0, 1, 0, 0, 0, 1, 1),
    (b"LOCAL:missing", 1, 0, 1, 0, 0, 0, 1, 0),
    (b"LOCAL:shots", 0, 0, 1, 0, 0, 0, 1, 0),
    (b"", 1, 0, 1, 0, 0, 0, 1, 0),
]


@pytest.mark.parametrize("case", SAVE_CASES)
def test_save(case):
    dir_, single, recording, step, frame, existing, mode, alloc, fail2 = case

    def setup(em):
        EXIST.clear()
        start = frame // 100000 * 100000 if frame >= 0 else -(-frame // 100000 * 100000)
        if step != 100000:
            start = frame
        for i in range(existing):
            EXIST.add(b"%s/A%07d.tga" % (dir_ or b"LOCAL:grab", start + i))
        STATE["fail_w"] = b"%s/empty.dat" % (dir_ or b"") if dir_ == b"LOCAL:missing" else None
        if fail2:
            STATE["fail_w"] = b"%s/A%07d.tga" % (dir_, frame)
        STATE["alloc"] = alloc
        fk.rets[0x4B0DB0] = mode
        em.write(SINGLE, bytes([single, recording]))
        u32(em, STEP, step)
        u32(em, FRAME, frame)
        return (cstr(em, dir_) if dir_ is not None else 0,)
    try:
        check(e, fk, 0x4EE6B0, "ScreenGrab_Save", setup, check_eax=False)
    finally:
        STATE.update(fail_w=None, alloc=1)
        fk.rets[0x4B0DB0] = 0


@pytest.mark.parametrize("f", [0, 0x6000])
@pytest.mark.parametrize("w,h", [(640, 480), (0x12345, 0xFFFF0258)])
def test_tga_header(f, w, h):
    def setup(em):
        em.write(HEADER, bytes(range(0x30, 0x42)))
        return f, HEADER, w, h
    check(e, fk, 0x4EE8E0, "Tga_WriteHeader", setup, check_eax=False)


@pytest.mark.parametrize("single,recording", [(0, 0), (1, 0), (0, 1), (7, 9)])
def test_is_active(single, recording):
    check(e, fk, 0x4EE940, "ScreenGrab_IsActive", lambda em: em.write(SINGLE, bytes([single, recording])))


@pytest.mark.parametrize("v", [0, 250, 251, 0x20001234, 0xFFFFFFFF, 0x80000000])
def test_task_ref_hash(v):
    check(e, fk, 0x4EE960, "TaskRef_Hash", args=(v,))
