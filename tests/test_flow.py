import struct

import pytest

from emu import Emu, ErrorShown, compare

emu = Emu()
emu.stub("GetDriveTypeA", lambda e: e.ret(3, 4))

FAKES = {  # address: (nargs, return value)
    0x4010A0: (0, 0), 0x405850: (1, 0), 0x405BD0: (1, 0), 0x406DF0: (1, 0), 0x4061C0: (13, 0x55),
    0x4061E0: (1, 0x66), 0x4071E0: (2, 0), 0x408350: (0, 0), 0x4137A0: (0, 0), 0x413B70: (0, 0),
    0x415020: (1, 0), 0x4154B0: (3, 0), 0x416900: (1, 0), 0x416D40: (0, 0), 0x416FE0: (0, 0),
    0x418B00: (5, 0), 0x48F180: (2, 0), 0x490370: (0, 1000), 0x491220: (0, 0), 0x4B8A20: (2, 3),
    0x4C4930: (1, 0), 0x4E6030: (1, 0), 0x4E6530: (1, 0), 0x4E7340: (1, 0), 0x4E7530: (0, 5),
    0x4E7580: (1, 0x7777), 0x4E7B80: (2, 0), 0x4E7BB0: (0, 0), 0x4EC0C0: (1, 0), 0x4ED5C0: (1, 0x8888),
    0x4ED5E0: (16, 0), 0x4EDCB0: (1, 0), 0x4EE030: (1, 0), 0x4EE3B0: (0, 0), 0x4EE940: (0, 0),
    0x4A5350: (2, 0x9000), 0x4A52A4: (3, 0), 0x4A528A: (1, 0x3A), 0x4A5234: (1, 0),
    0x4B8810: (2, 0), 0x4B8890: (4, 0),
}
for addr, (nargs, ret) in FAKES.items():
    emu.fake(addr, ret, nargs)

PROFILE = emu.alloc(0x600)
emu.fake(0x406220, PROFILE)

TASK_INIT, TYPE_NEW, SET_HANDLER, TASK_NEW, TASK_ALLOC = 0x4010D0, 0x401900, 0x401400, 0x4012A0, 0x4012F0
FLOW_OPEN = 0x401FC0
G_FLOW = 0x567C8C


def logger(e, log):
    code = (b"\x8b\x44\x24\x04" + b"\x8b\x0d" + struct.pack("<I", log)
            + b"\x89\x04\x8d" + struct.pack("<I", log + 4) + b"\x41"
            + b"\x89\x0d" + struct.pack("<I", log) + b"\xc3")
    p = e.alloc(len(code))
    e.write(p, code)
    return p


class State:
    """Task system and a Flow task created by the original code, flow handlers not run."""

    def __init__(self, e, **fields):
        e.call(TASK_INIT)
        e.call(FLOW_OPEN)
        self.flow_type = e.u32(0x567C7C) & 0xFFFF
        root_type = e.call(0x4018F0)[0] & 0xFFFF
        self.log = e.alloc(4 * 64)
        handler = logger(e, self.log)
        kid = e.call(TYPE_NEW, 0x536274, 0x20, root_type, 0)[0] & 0xFFFF
        e.call(SET_HANDLER, kid, 1, handler)
        e.call(SET_HANDLER, kid, 5, handler)
        self.flow = e.call(TASK_ALLOC, 0, self.flow_type)[0]
        self.root = e.call(TASK_NEW, 0, root_type, 0)[0]
        self.spare = e.call(TASK_NEW, 0, root_type, 0)[0]
        e.set_fake_ret(0x416D40, self.spare)
        for parent in (self.flow, self.root, self.flow):
            e.call(TASK_NEW, parent, kid, 0)
        e.write(G_FLOW, struct.pack("<I", self.flow))
        f = dict(fps=30, frames=5, renders=3, start_time=100, paused_time=50, need_render=1,
                 ticks_since_render=0, request=0, event=0, time_adjust=7)
        f.update(fields)
        offsets = dict(event=0x28, request=0x2C, start_time=0x30, frames=0x34, renders=0x38, fps=0x3C,
                       ticks_since_render=0x40, time_adjust=0x4C, paused_time=0x50)
        for k, off in offsets.items():
            e.write(self.flow + off, struct.pack("<i", f[k]))
        e.write(self.flow + 0x44, bytes([f["need_render"]]))
        e.write(self.flow + 0x48, struct.pack("<I", self.root))
        e.write(self.flow + 0x20, struct.pack("<I", 0x7777))


def with_fakes(values, fn):
    def setup(e):
        for addr, v in values.items():
            e.set_fake_ret(addr, v)
        r = fn(e)
        e.write(0x30080000, b"\0\0\0\0")
        return r
    return setup


def reset_fakes():
    for addr, (nargs, ret) in FAKES.items():
        emu.set_fake_ret(addr, ret)


def test_open():
    compare(emu, FLOW_OPEN, "Flow_Open", setup=with_fakes({}, lambda e: e.call(TASK_INIT) and None),
            check_eax=False, normalized=True)


@pytest.mark.parametrize("c28c61,c28f80", [(0, 0), (1, 0), (1, 1)])
def test_create_handler(c28c61, c28f80):
    def setup(e):
        s = State(e)
        e.write(0xC28C61, bytes([c28c61]))
        e.write(0xC28F80, bytes([c28f80]))
        return (s.flow,)
    compare(emu, 0x402070, "Flow_CreateHandler", setup=with_fakes({}, setup), check_eax=False, normalized=True)


@pytest.mark.parametrize("fopen,fgetc", [(0, 0x3A), (0x9000, 0x3B)])
def test_create_handler_disc_check_fails(fopen, fgetc):
    for target in (0x402070, "Flow_CreateHandler"):
        emu.reset()
        s = State(emu)
        emu.set_fake_ret(0x4A5350, fopen)
        emu.set_fake_ret(0x4A528A, fgetc)
        with pytest.raises(ErrorShown, match="EXCEPTION FAULT"):
            emu.call(target, s.flow)
    reset_fakes()


RUN_CASES = [
    dict(now=1000), dict(now=50), dict(now=1000, frames=1), dict(now=1000, frames=2, need_render=0),
    dict(now=1000, frames=3, need_render=0), dict(now=1000, ticks_since_render=11), dict(now=10, need_render=0),
    dict(now=10, need_render=0, renders=1), dict(now=1000, fps=25), dict(now=1000, paused_time=900),
    dict(now=1000, minimized=1), dict(now=1000, minimized=1, busy=1), dict(now=1000, flag=1),
]


@pytest.mark.parametrize("case", RUN_CASES)
@pytest.mark.parametrize("request_", [0, 2, 3, 4, 5, 6, 7, 8, 9])
def test_run_handler(case, request_):
    case = dict(case)
    now = case.pop("now")
    fakes = {0x490370: now, 0x491220: case.pop("minimized", 0), 0x4EE940: case.pop("busy", 0)}
    flag = case.pop("flag", 0)

    def setup(e):
        s = State(e, request=request_, **case)
        e.write(0x5C8E00, bytes([flag]))
        return (s.flow,)
    compare(emu, 0x402260, "Flow_RunHandler", setup=with_fakes(fakes, setup), check_eax=False, normalized=True)
    reset_fakes()


def test_run_handler_quit_and_key():
    def setup(e):
        return (State(e, request=1).flow,)
    compare(emu, 0x402260, "Flow_RunHandler", setup=with_fakes({}, setup), check_eax=False, normalized=True)
    compare(emu, 0x402260, "Flow_RunHandler", setup=with_fakes({0x415020: 1}, setup), check_eax=False, normalized=True)
    reset_fakes()


def test_delete_handler():
    compare(emu, 0x402740, "Flow_DeleteHandler", setup=with_fakes({}, lambda e: (State(e).flow,)),
            check_eax=False, normalized=True)


def test_request_event():
    def setup(e):
        State(e)
        return e.alloc(24), 0, 0x1234
    # bytes 4 and 20 of the result come from uninitialised stack in the original
    compare(emu, 0x402780, "Flow_RequestEvent", setup=with_fakes({}, setup), check_eax=False, normalized=True,
            ignore=[(0x20000000, 0x24000000)])
    for target in (0x402780, "Flow_RequestEvent"):
        emu.reset()
        State(emu)
        ret = emu.alloc(24)
        emu.call(target, ret, 0, 0x1234)
        assert struct.unpack("<i", emu.read(ret, 4))[0] == 1
        assert struct.unpack("<d", emu.read(ret + 8, 8))[0] == 1.0
        assert emu.u32(ret + 16) == 0x567C74
        assert struct.unpack("<i", emu.read(emu.u32(G_FLOW) + 0x2C, 4))[0] == 3


@pytest.mark.parametrize("orig,ours,args", [
    (0x4027E0, "Flow_Close", ()), (0x4027F0, "Flow_GetType", ()), (0x402870, "Flow_GetFps", ()),
    (0x402880, "Flow_Get", ()), (0x402890, "Flow_GetUnk20", ()), (0x4028A0, "Flow_SetRequest", (4,)),
    (0x4028B0, "Flow_GetFrames", ()), (0x4028C0, "Flow_GetRenders", ()), (0x402820, "FramesSet", (25,)),
    (0x4028D0, "Flow_PauseBegin", ()), (0x4028F0, "Flow_PauseEnd", ()),
    (0x402240, "Flow_FocusChanged", (0,)), (0x402240, "Flow_FocusChanged", (1,)),
])
@pytest.mark.parametrize("depth", [0, 1, 2])
def test_small(orig, ours, args, depth):
    def setup(e):
        State(e)
        e.write(0x567C80, struct.pack("<i", depth))
        e.write(0x567C78, struct.pack("<i", 400))
        return args
    mask = 0xFFFF if ours == "Flow_GetType" else 0xFFFFFFFF
    returns = ours.startswith(("Flow_Get", "Flow_GetType"))
    compare(emu, orig, ours, setup=with_fakes({}, setup), normalized=True, eax_mask=mask, check_eax=returns)


def test_new():
    def setup(e):
        s = State(e)
        return (s.root,)
    compare(emu, 0x402800, "Flow_New", setup=with_fakes({}, setup), check_eax=False, normalized=True)


@pytest.mark.parametrize("fopen,fgetc", [(0x9000, 0x40), (0x9000, 0x41), (0, 0)])
def test_check_cd(fopen, fgetc):
    if fgetc != 0x40:
        for target in (0x402F50, "Flow_CheckCD"):
            emu.reset()
            emu.set_fake_ret(0x4A5350, fopen)
            emu.set_fake_ret(0x4A528A, fgetc)
            with pytest.raises(ErrorShown, match="CD not found"):
                emu.call(target)
    else:
        compare(emu, 0x402F50, "Flow_CheckCD", setup=with_fakes({0x4A528A: fgetc}, lambda e: None),
                check_eax=False, normalized=True)
    reset_fakes()
