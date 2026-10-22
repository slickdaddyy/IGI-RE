import struct

import pytest

from emu import compare
from test_d3d_util import DSBUFFER, DSOUND, D3DEmu, api, check, check_raises, com, cstr, out, pyfake, u32

emu = D3DEmu()

FAIL = 0x80004005
SOUND_READY, CD_OPEN, SOUND_3D = 0x6840E9, 0x6840E8, 0x6840EA
DSOUND_P, LISTENER, PRIMARY = 0x67A498, 0x67A49C, 0x5CA154
STREAM_BUF, STREAM_FILE, STREAM_HALF, STREAM_END, STREAM_POS, STREAM_HALFSIZE, STREAM_LOOP, STREAM_READY = (
    0x5CA190, 0x5CA194, 0x5CA198, 0x5CA19C, 0x5CA1A0, 0x5CA1A4, 0x5CA1B0, 0x5CA1B4)
MIXERS, MIXER_COUNT, MIXER_SIZE = 0x676E58, 0x5CA150, 0x364


def noise(n, seed=0):
    return bytes((i * 151 + seed * 13 + 7) & 0xFF for i in range(n))


def u8(e, p, v):
    e.write(p, bytes([v]))


# --- small ones ---------------------------------------------------------------

def test_init_lists():
    def setup(e):
        p = e.alloc(101 * 12)
        e.write(p, noise(101 * 12))
        return (p,)
    compare(emu, 0x4955F0, "Sound_InitLists", setup=setup, check_eax=False)


@pytest.mark.parametrize("empty", [True, False])
def test_list_push(empty):
    def setup(e):
        head = e.alloc(4)
        if not empty:
            old = e.alloc(16)
            e.write(old, noise(16, 1))
            u32(e, head, old)
        node = e.alloc(16)
        e.write(node, noise(16, 2))
        return node, head
    compare(emu, 0x495620, "Sound_ListPush", setup=setup, check_eax=False)


@pytest.mark.parametrize("ready", [0, 1])
def test_stream_init(ready):
    def setup(e):
        u8(e, SOUND_READY, ready)
        u8(e, STREAM_READY, 7)
        u32(e, STREAM_FILE, 0x1111)
        u32(e, STREAM_BUF, 0x2222)
    compare(emu, 0x495A60, "Stream_Init", setup=setup, check_eax=False)


def test_getters_setters():
    compare(emu, 0x495CB0, "Sound_IsReady", setup=lambda e: u8(e, SOUND_READY, 1), eax_mask=0xFF)
    compare(emu, 0x495CC0, "sub_495CC0", args=(0x12345678,), check_eax=False)
    compare(emu, 0x495F30, "Music_SetSfxVolume", args=(0x3F400000,), check_eax=False)


# --- CD audio -----------------------------------------------------------------

def mci(ret=0):
    def before(e):
        cmd, parms = e.arg(1), e.arg(3)
        if cmd == 0x803:
            u32(e, parms + 4, 0x77)
            return (ret, e.read_cstr(e.u32(parms + 8)))
        if parms:
            return (ret, e.u32(parms))
        return ret
    return before


@pytest.mark.parametrize("ret", [0, 263])
def test_cd_open(ret):
    def setup(e):
        api(e, "mciSendCommandA", 4, before=mci(ret))
        u8(e, 0x5CA188, 5)
        u8(e, 0x676AB4, 5)
    check(emu, 0x4952A0, "CD_Open", setup=setup, check_eax=False)


def test_cd_stop():
    def setup(e):
        api(e, "mciSendCommandA", 4, before=mci())
        u32(e, 0x5CA130, 0x77)
    check(emu, 0x495F40, "CD_Stop", setup=setup, check_eax=False)


@pytest.mark.parametrize("opened", [0, 1])
def test_cd_close(opened):
    def setup(e):
        api(e, "mciSendCommandA", 4, before=mci())
        u32(e, 0x5CA130, 0x77)
        u8(e, CD_OPEN, opened)
    check(emu, 0x495AB0, "CD_Close", setup=setup, check_eax=False)


# --- mixer --------------------------------------------------------------------

def details_stub(e):
    d = e.arg(1)
    cb, cid, ch, multi, size, pa = struct.unpack("<6I", e.read(d, 24))
    return (None, (cb, cid, ch, multi, size, e.read(pa, 4 * min(ch, 2))))


@pytest.mark.parametrize("channels", [0, 1, 2, 3])
def test_mixer_set_volume(channels):
    def setup(e):
        api(e, "mixerSetControlDetails", 3, 0x55, before=details_stub)
        line = e.alloc(0xA8)
        e.write(line, noise(0xA8, 3))
        u32(e, line + 0x1C, channels)
        ctrl = e.alloc(0x94)
        e.write(ctrl, noise(0x94, 4))
        return 0x500, line, ctrl, 0x1234, 0x5678
    check(emu, 0x495C10, "Mixer_SetVolume", setup=setup)


def mixer_slot(e, i, hmx=0x500, dest=1, wave=(1, 1), cd=1, values=(10, 20, 30, 40), channels=2):
    m = MIXERS + MIXER_SIZE * i
    e.write(m, noise(MIXER_SIZE, i))
    u32(e, m, hmx)
    u32(e, m + 0x34, dest)
    u32(e, m + 0xDC, wave[0])
    u32(e, m + 0xDC + 0x1C, channels)
    u32(e, m + 0x22C, wave[1])
    u32(e, m + 0x184, cd)
    u32(e, m + 0x184 + 0x1C, channels)
    e.write(m + 0x354, struct.pack("<4I", *values))


SLOTS = [dict(), dict(hmx=0), dict(dest=0), dict(wave=(0, 1)), dict(wave=(1, 0)), dict(cd=0),
         dict(values=(0xFFFFFFFF, 0x80000000, 65535, 1), channels=1)]


@pytest.mark.parametrize("count", [0, 1, len(SLOTS)])
def test_mixer_shutdown(count):
    def setup(e):
        api(e, "mixerSetControlDetails", 3, before=details_stub)
        api(e, "mixerClose", 1)
        for i, kw in enumerate(SLOTS):
            mixer_slot(e, i, **kw)
        u32(e, MIXER_COUNT, count)
    check(emu, 0x495B50, "Mixer_Shutdown", setup=setup, check_eax=False)


@pytest.mark.parametrize("vols", [(1.0, 1.0), (0.5, 0.75), (0.3, 0.7), (0.0, 1.0), (1.5, 0.999)])
def test_music_set_volume(vols):
    def setup(e):
        api(e, "mixerSetControlDetails", 3, before=details_stub)
        for i, kw in enumerate(SLOTS):
            mixer_slot(e, i, **kw)
        u32(e, MIXER_COUNT, len(SLOTS))
        return tuple(struct.unpack("<I", struct.pack("<f", v))[0] for v in vols)
    check(emu, 0x495E70, "Music_SetVolume", setup=setup, check_eax=False)


# A fake mixer setup: devices -> destinations -> sources -> volume controls
WAVE, AUX, CDROM, OTHER = 0x1008, 0x1009, 0x1005, 0x1003
VOLUME, MUTE = 0x50030001, 0x20010002


def src(comp=OTHER, name=b"Line In", controls=(VOLUME,), chan=2, ok=True, ctrl_ok=True, details_ok=True):
    return dict(comp=comp, name=name, controls=controls, chan=chan, ok=ok, ctrl_ok=ctrl_ok, details_ok=details_ok)


def dst(sources, target=1, ok=True):
    return dict(sources=sources, target=target, ok=ok)


def dev(dests, open_ok=True, caps_ok=True):
    return dict(dests=dests, open_ok=open_ok, caps_ok=caps_ok)


MIXER_CASES = {
    "none": [],
    "wave_cd": [dev([dst([src(WAVE), src(CDROM), src()])])],
    "names": [dev([dst([src(name=b"Wave"), src(name=b"CD Audio"), src(AUX, chan=1)])])],
    "fails": [dev([], open_ok=False), dev([], caps_ok=False),
              dev([dst([src(WAVE, ok=False), src(CDROM, ctrl_ok=False), src(WAVE, details_ok=False)]),
                   dst([src(WAVE)], ok=False), dst([src(WAVE)], target=3)])],
    "multi": [dev([dst([src(WAVE, controls=(MUTE, VOLUME, VOLUME)), src(CDROM, controls=(MUTE,))])]),
              dev([dst([src(CDROM, chan=1), src(WAVE, chan=3)]), dst([src(WAVE, controls=())], target=5)]),
              dev([dst([src(WAVE), src(CDROM)])])],
}


def mixer_env(e, devices):
    api(e, "mixerGetNumDevs", 0, len(devices))

    def find(h):
        return devices[h - 0x500]

    def mopen(e):
        i = e.arg(1)
        u32(e, e.arg(0), 0x500 + i)
        return 0 if devices[i]["open_ok"] else 1
    api(e, "mixerOpen", 5, before=mopen)

    def caps(e):
        d = find(e.arg(0))
        e.write(e.arg(1), noise(0x30, 5))
        u32(e, e.arg(1) + 0x2C, len(d["dests"]))
        return (0 if d["caps_ok"] else 1, e.arg(2))
    api(e, "mixerGetDevCapsA", 3, before=caps)

    def line_info(e):
        d, line, flags = find(e.arg(0)), e.arg(1), e.arg(2)
        cb, di, si = struct.unpack("<3I", e.read(line, 12))
        dd = d["dests"][di]
        info = (cb, di, si, flags)
        if flags & 0xF == 0:
            if not dd["ok"]:
                return (1, info)
            e.write(line + 0xC, noise(0xA8 - 0xC, di))
            u32(e, line + 0x20, len(dd["sources"]))
            u32(e, line + 0x78, dd["target"])
            return (0, info)
        s = dd["sources"][si]
        if not s["ok"]:
            return (1, info)
        e.write(line + 0xC, noise(0xA8 - 0xC, si + 7))
        u32(e, line + 0xC, 0x100 * di + si)
        u32(e, line + 0x18, s["comp"])
        u32(e, line + 0x1C, s["chan"])
        u32(e, line + 0x24, len(s["controls"]))
        e.write(line + 0x38, s["name"] + b"\0")
        return (0, info)
    api(e, "mixerGetLineInfoA", 3, before=line_info)

    def line_controls(e):
        d, mlc = find(e.arg(0)), e.arg(1)
        cb, line_id, _, count, size, pa = struct.unpack("<6I", e.read(mlc, 24))
        s = d["dests"][line_id >> 8]["sources"][line_id & 0xFF]
        for i, t in enumerate(s["controls"]):
            c = pa + 0x94 * i
            e.write(c, noise(0x94, i))
            e.write(c, struct.pack("<3I", 0x94, line_id * 16 + i, t))
            u32(e, c + 0x10, i)
        return (0 if s["ctrl_ok"] else 1, (cb, line_id, count, size, e.arg(2)))
    api(e, "mixerGetLineControlsA", 3, before=line_controls)

    def details(e):
        d, md = find(e.arg(0)), e.arg(1)
        cb, cid, ch, multi, size, pa = struct.unpack("<6I", e.read(md, 24))
        line_id = cid >> 4
        s = d["dests"][line_id >> 8]["sources"][line_id & 0xFF]
        for i in range(ch):
            u32(e, pa + 4 * i, 1000 * cid + i)
        return (0 if s["details_ok"] else 1, (cb, cid, ch, multi, size, e.arg(2)))
    api(e, "mixerGetControlDetailsA", 3, before=details)


@pytest.mark.parametrize("case", list(MIXER_CASES))
@pytest.mark.parametrize("cd", [0, 1])
def test_mixer_init(case, cd):
    def setup(e):
        e.call(0x4B0BA0)
        u8(e, CD_OPEN, cd)
        mixer_env(e, MIXER_CASES[case])
        e.write(MIXERS, noise(MIXER_SIZE * 4, 9))
    check(emu, 0x495640, "Mixer_Init", setup=setup, check_eax=False)


# --- DirectSound --------------------------------------------------------------

def ds_env(e, fail=(), flags=0x2 | 0x8):
    f = lambda name: FAIL if name in fail else 0
    u32(e, 0x5C8BC4, 0x1234)
    prim = com(e, "prim", DSBUFFER, {3: (f("bcaps"), lambda e: (None, e.read(e.arg(1), 20))),
                                     14: (f("format"), lambda e: (None, e.read(e.arg(1), 18))),
                                     12: f("play")})
    ds = com(e, "ds", DSOUND, {6: f("coop"),
                               4: (f("caps"), lambda e: (e.write(e.arg(1) + 4, struct.pack("<I", flags)), None)[1]),
                               3: (f("buffer"), out(2, prim, lambda e: e.read(e.arg(1), 0x24)))})
    api(e, "#1", 3, f("create"), before=out(1, ds))
    return ds


@pytest.mark.parametrize("fail", [None, "create", "coop", "caps", "buffer", "bcaps", "format", "play"])
def test_init_directsound(fail):
    def setup(e):
        e.call(0x4B0BA0)
        ds_env(e, (fail,))
        e.write(0x67A4A0, noise(40004, 3))
    check(emu, 0x4952F0, "Sound_InitDirectSound", setup=setup, check_eax=False)


@pytest.mark.parametrize("flags", [0, 2, 8])
def test_init_directsound_formats(flags):
    def setup(e):
        e.call(0x4B0BA0)
        ds_env(e, flags=flags)
    check(emu, 0x4952F0, "Sound_InitDirectSound", setup=setup, check_eax=False)


def test_sound_init():
    def setup(e):
        e.call(0x4B0BA0)
        api(e, "mciSendCommandA", 4, before=mci())
        mixer_env(e, MIXER_CASES["wave_cd"])
        ds_env(e)
    check(emu, 0x495280, "Sound_Init", setup=setup, check_eax=False)


@pytest.mark.parametrize("voices,ds,three_d", [(1, 1, 0), (1, 1, 1), (0, 0, 0), (0, 1, 1)])
def test_shutdown_directsound(voices, ds, three_d):
    def setup(e):
        e.call(0x4B0BA0)
        u32(e, 0x6840F0, e.call(0x4B0C60, 64, 4)[0] if voices else 0)
        u32(e, 0x6840F8, 0x3333)
        u32(e, 0x6840F4, 0x4444)
        u32(e, DSOUND_P, com(e, "ds", DSOUND) if ds else 0)
        u32(e, LISTENER, com(e, "listener", DSOUND))
        u32(e, PRIMARY, com(e, "prim", DSBUFFER))
        u8(e, SOUND_3D, three_d)
    check(emu, 0x495AE0, "Sound_ShutdownDirectSound", setup=setup, check_eax=False)


def test_sound_shutdown():
    def setup(e):
        e.call(0x4B0BA0)
        pyfake(e, 0x4B1690, "File_Close", 1)
        api(e, "mciSendCommandA", 4, before=mci())
        api(e, "mixerSetControlDetails", 3, before=details_stub)
        api(e, "mixerClose", 1)
        for i, kw in enumerate(SLOTS[:3]):
            mixer_slot(e, i, **kw)
        u32(e, MIXER_COUNT, 3)
        u32(e, STREAM_FILE, 0x9999)
        u32(e, STREAM_BUF, com(e, "stream", DSBUFFER))
        u32(e, DSOUND_P, com(e, "ds", DSOUND))
        u32(e, PRIMARY, com(e, "prim", DSBUFFER))
        u8(e, CD_OPEN, 1)
    check(emu, 0x495A90, "Sound_Shutdown", setup=setup, check_eax=False)


# --- music streaming ----------------------------------------------------------

def stream_env(e, end=1000, pos=0, lock=0, play=0, half=0, halfsize=256, loop=0, ready=1, has_file=1):
    pyfake(e, 0x4B1690, "File_Close", 1)
    pyfake(e, 0x4B16D0, "File_Read", 3, before=lambda e: e.arg(2))
    pyfake(e, 0x4B1730, "File_Seek", 3)
    mem = e.alloc(0x1000)

    def do_lock(e):
        u32(e, e.arg(3), mem + e.arg(1))
        u32(e, e.arg(4), e.arg(2) - 1)
    buf = com(e, "stream", DSBUFFER, {11: (lock, do_lock), 4: (0, out(1, play))})
    u32(e, STREAM_BUF, buf)
    u32(e, STREAM_FILE, 0x9999 if has_file else 0)
    u32(e, STREAM_END, end)
    u32(e, STREAM_POS, pos)
    u32(e, STREAM_HALF, half)
    u32(e, STREAM_HALFSIZE, halfsize)
    u32(e, STREAM_LOOP, loop)
    u8(e, STREAM_READY, ready)
    return buf


@pytest.mark.parametrize("end,pos,size,lock", [(1000, 0, 256, 0), (1000, 744, 256, 0), (1000, 900, 256, 0),
                                               (1000, 0, 256, FAIL)])
def test_stream_fill(end, pos, size, lock):
    def setup(e):
        buf = stream_env(e, end, pos, lock)
        return buf, 512, size
    check(emu, 0x495CD0, "Stream_Fill", setup=setup)


@pytest.mark.parametrize("has_file", [0, 1])
def test_stream_close(has_file):
    def setup(e):
        stream_env(e, has_file=has_file)
    check(emu, 0x495C80, "Stream_Close", setup=setup, check_eax=False)


@pytest.mark.parametrize("ready,has_file", [(0, 1), (1, 0), (1, 1)])
def test_stream_stop(ready, has_file):
    def setup(e):
        stream_env(e, ready=ready, has_file=has_file)
    check(emu, 0x495DA0, "Stream_Stop", setup=setup, check_eax=False)


@pytest.mark.parametrize("play,half", [(10, 0), (10, 256), (300, 0), (300, 256)])
@pytest.mark.parametrize("pos,loop", [(0, 0), (900, 0), (900, 1)])
def test_stream_update(play, half, pos, loop):
    def setup(e):
        stream_env(e, pos=pos, play=play, half=half, loop=loop)
    check(emu, 0x495DF0, "Stream_Update", setup=setup, check_eax=False)


@pytest.mark.parametrize("ready,has_file", [(0, 1), (1, 0)])
def test_stream_update_idle(ready, has_file):
    def setup(e):
        stream_env(e, ready=ready, has_file=has_file)
    check(emu, 0x495DF0, "Stream_Update", setup=setup, check_eax=False)


# --- sample loading -----------------------------------------------------------

def ilsf(fmt, bits=16, channels=2, rate=22050, samples=40, magic=b"ILSF"):
    return magic + struct.pack("<hhhhII", fmt, bits, channels, 0, rate, samples) + noise(200, 4)


LOAD_CASES = {
    "pcm": dict(res=ilsf(1)), "stream0": dict(res=ilsf(0)), "stream2": dict(res=ilsf(2)),
    "adpcm": dict(res=ilsf(3, 16, 1, 11025, 30)), "other": dict(res=ilsf(5)), "neg": dict(res=ilsf(-1)),
    "mono8": dict(res=ilsf(1, 8, 1, 8000, 25)), "odd": dict(res=ilsf(1, 4, 3, 8000, 25)),
    "badmagic": dict(res=ilsf(1, magic=b"RIFF")), "loaded": dict(res=ilsf(1), found=1),
    "createfail": dict(res=ilsf(1), create=FAIL), "adpcmfail": dict(res=ilsf(3), create=FAIL),
    "attachfail": dict(res=ilsf(1), attach=0), "adpcmattach": dict(res=ilsf(3), attach=0),
    "notready": dict(res=ilsf(1), ready=0),
}


@pytest.mark.parametrize("case", list(LOAD_CASES))
def test_sound_load(case):
    c = LOAD_CASES[case]

    def setup(e):
        e.call(0x4B0BA0)
        u8(e, SOUND_READY, c.get("ready", 1))
        res = e.alloc(len(c["res"]))
        e.write(res, c["res"])
        pyfake(e, 0x4B5F00, "Resource_Load", 2, res, before=lambda e: e.write(e.arg(1), struct.pack("<I", 216)))
        pyfake(e, 0x4B6380, "Resource_Unload", 1)
        pyfake(e, 0x4AF810, "Warning_Show", 2)
        pyfake(e, 0x497390, "sub_497390", 2)
        pyfake(e, 0x497450, "sub_497450", 1, 0x1111)
        pyfake(e, 0x497410, "sub_497410", 1, 0x2222)
        pyfake(e, 0x496450, "sub_496450", 2, c.get("found", 0))
        pyfake(e, 0x496420, "sub_496420", 2)
        pyfake(e, 0x496280, "sub_496280", 4, before=lambda e: (None, e.read(e.arg(0), 8)))
        pyfake(e, 0x4964A0, "sub_4964A0", 3, c.get("attach", 1),
               before=lambda e: (None, e.read(e.arg(1), 0x30)))
        pyfake(e, 0x496250, "sub_496250", 4, before=lambda e: (None, e.read(e.arg(3), 0x30)))
        buf = com(e, "buf", DSBUFFER)

        def create(e):
            d = e.read(e.arg(1), 0x24)
            wf = e.read(struct.unpack_from("<I", d, 0x10)[0], 18)
            return (None, (d[:0x10] + d[0x14:], wf))
        u32(e, DSOUND_P, com(e, "ds", DSOUND, {3: (c.get("create", 0),
                                                    lambda e: (e.write(e.arg(2), struct.pack("<I", buf)),
                                                               create(e))[1])}))
        return (cstr(e, b"sounds\\gun.wav"),)
    check(emu, 0x495F70, "Sound_Load", setup=setup, check_eax=False)
