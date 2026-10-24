import random
import struct

import pytest

from emu import compare
from test_d3d_util import DSBUFFER, DSOUND, D3DEmu, check, com, cstr, pyfake, u32
from test_transctx_util import (ACTIVE, DSOUND_P, FREE, LISTS, SOUND_READY, TIME, UPDATE_RATE, f32, link, noise,
                                sound, u8, voice, with_cw)

emu = D3DEmu()
FAIL = 0x80004005


def ready(e, on=1):
    u8(e, SOUND_READY, on)


def init_lists(e):
    e.call(0x4955F0, LISTS)


# --- sound lists ----------------------------------------------------------------

def test_list_add():
    def setup(e):
        init_lists(e)
        e.call(0x496250, LISTS, 7, 0x99, sound(e))
        return LISTS, 7, 0x1234, sound(e)
    compare(emu, 0x496250, "SoundList_Add", setup=setup, check_eax=False)


def filled(e):
    init_lists(e)
    for k1, k2 in ((3, 10), (3, 11), (3, 12), (5, 10)):
        e.call(0x496250, LISTS, k1, k2, sound(e, key1=0x77))


@pytest.mark.parametrize("k1,k2", [(3, 10), (3, 12), (3, 13), (5, 10), (5, 11), (0, 10)])
def test_find(k1, k2):
    def setup(e):
        filled(e)
    compare(emu, 0x496450, "Sound_Find", args=(k1, k2), setup=setup)
    compare(emu, 0x496470, "SoundList_Find", args=(LISTS, k1, k2), setup=setup)


def test_list_remove():
    def setup(e):
        filled(e)
        return LISTS, e.u32(e.u32(LISTS + 3 * 12))
    compare(emu, 0x496640, "SoundList_Remove", setup=setup, check_eax=False)


# --- ADPCM ----------------------------------------------------------------------

@pytest.mark.parametrize("seed", range(12))
def test_adpcm_decode(seed):
    rng = random.Random(seed)
    channels = rng.choice([1, 2])
    frames = rng.choice([0, 1, 2, 3, 7, 40])
    st = struct.pack("<hhhhhBB", channels, rng.randint(-32768, 32767), rng.randint(-32768, 32767),
                     rng.randint(0, 88), rng.randint(0, 88), rng.randint(0, 1), 0xAA)
    if seed == 0:
        st = struct.pack("<hhhhhBB", 2, 32000, -32000, 88, 88, 0, 0)
    src = bytes(rng.randrange(256) for _ in range(64))

    def setup(e):
        s, d, p = e.alloc(64), e.alloc(256), e.alloc(12)
        e.write(s, src)
        e.write(d, noise(256))
        e.write(p, st)
        return s, d, frames, p
    compare(emu, 0x496280, "Adpcm_Decode", setup=setup, check_eax=False)


def test_adpcm_init():
    def setup(e):
        p = e.alloc(12)
        e.write(p, noise(12))
        return p, 0x55550002
    compare(emu, 0x496420, "Adpcm_Init", setup=setup, check_eax=False)


# --- upload ---------------------------------------------------------------------

def lock(n1, n2, fail=0):
    def before(e):
        p1, p2 = e.alloc(max(n1, 4)), e.alloc(max(n2, 4))
        e.write(p1, noise(max(n1, 4), 1))
        e.write(p2, noise(max(n2, 4), 2))
        for i, v in ((3, p1), (4, n1), (5, p2), (6, n2)):
            u32(e, e.arg(i), v)
        return fail
    return before


@pytest.mark.parametrize("n1,n2,fail,unlock", [(160, 0, 0, 0), (200, 0, 0, 0), (100, 60, 0, 0), (100, 50, 0, 0),
                                               (100, 300, 0, 0), (0, 160, 0, 0), (0, 100, 0, 0), (160, 0, FAIL, 0),
                                               (160, 0, 0, FAIL)])
@pytest.mark.parametrize("use_voice", [0, 1])
def test_upload(n1, n2, fail, unlock, use_voice):
    def setup(e):
        data = e.alloc(400)
        e.write(data, noise(400, 3))
        buf = com(e, "sbuf", DSBUFFER, {11: (fail, lock(n1, n2)), 19: unlock})
        s = sound(e, bits=16, channels=2, samples=40, buffer=buf, data=data)
        v = voice(e, s, methods={11: (fail, lock(n1, n2)), 19: unlock}) if use_voice else 0
        return v, s, 0x100
    check(emu, 0x4964A0, "Sound_Upload", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("ret", [0, FAIL])
def test_upload_restore(ret):
    def setup(e):
        s = sound(e, buffer=com(e, "sbuf", DSBUFFER, {20: ret}))
        return 0, s, 1
    check(emu, 0x4964A0, "Sound_Upload", setup=setup, eax_mask=0xFF)


# --- unload ---------------------------------------------------------------------

@pytest.mark.parametrize("case", ["found", "nobuf", "noname", "notres", "missing", "noext"])
def test_unload(case):
    def setup(e):
        init_lists(e)
        pyfake(e, 0x4B0D10, "Mem_Free", 1)
        pyfake(e, 0x4B64E0, "Resource_Find", 1, 0 if case == "notres" else 1,
               before=lambda e: (None, e.read_cstr(e.arg(0))))
        pyfake(e, 0x4B6380, "Resource_Unload", 1, before=lambda e: (None, e.read_cstr(e.arg(0))))
        pyfake(e, 0x4AF810, "Warning_Show", 2)
        name = cstr(e, b"sound\\gun.wav")
        key = cstr(e, b"GUN")
        k1 = e.call(0x497450, key)[0]
        k2 = e.call(0x497410, key)[0]
        if case != "missing":
            s = sound(e, buffer=0 if case == "nobuf" else com(e, "sbuf", DSBUFFER),
                      name=0 if case == "noname" else cstr(e, b"x"))
            e.call(0x496250, LISTS, k1, k2, s)
        return (cstr(e, b"gun.wav") if case == "noext" else name,)
    check(emu, 0x4965A0, "Sound_Unload", setup=setup, check_eax=False)


# --- voices ---------------------------------------------------------------------

@pytest.mark.parametrize("flags,file,on", [(0, 0, 1), (4, 0x4444, 1), (0, 0x4444, 1), (4, 0, 0)])
def test_voice_free(flags, file, on):
    def setup(e):
        ready(e, on)
        pyfake(e, 0x4B1690, "File_Close", 1)
        s = sound(e)
        vs = [voice(e, s) for _ in range(3)]
        v = voice(e, s, flags=flags, buffer3d=com(e, "b3d", {}), file=file)
        link(e, ACTIVE, [vs[0], v, vs[1]])
        link(e, FREE, [vs[2]])
        return (v,)
    check(emu, 0x496650, "Voice_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("pos", [0, 1, 2, 3])
def test_voice_unlink(pos):
    def setup(e):
        s = sound(e)
        vs = [voice(e, s, buffer=0) for _ in range(3)]
        head = e.alloc(4)
        link(e, head, vs)
        return (vs[pos] if pos < 3 else voice(e, s, buffer=0)), head
    compare(emu, 0x4966B0, "Voice_Unlink", setup=setup, check_eax=False)


def status(v):
    return lambda e: u32(e, e.arg(1), v)


@pytest.mark.parametrize("fmt,state,st,ret,restore,play,on", [
    (0, 2, 0, 0, 0, 0, 1), (2, 1, 0, 0, 0, 0, 1), (1, 0, 1, 0, 0, 0, 1), (1, 0, 0, 0, 0, 0, 1), (3, 0, 1, FAIL, 0, 0, 1),
    (1, 0, 3, 0, 0, 0, 1), (1, 0, 7, 0, 0, 0, 1), (1, 0, 6, 0, FAIL, 0, 1), (1, 0, 6, 0, 0, FAIL, 1),
    (1, 0, 1, 0, 0, 0, 0)])
def test_voice_is_playing(fmt, state, st, ret, restore, play, on):
    def setup(e):
        ready(e, on)
        s = sound(e, fmt=fmt)
        return (voice(e, s, state=state, methods={9: (ret, status(st)), 20: restore, 12: play}),)
    check(emu, 0x4966E0, "Voice_IsPlaying", setup=setup, eax_mask=0xFF)


def test_voice_stop():
    for on in (0, 1):
        check(emu, 0x496E40, "Voice_Stop", setup=lambda e: (ready(e, on), (voice(e, sound(e)),))[1],
              check_eax=False)


# --- file streaming ----------------------------------------------------------------

def file_fakes(e, open_ret=0x4444):
    def read(e):
        n = e.arg(2)
        if 0 < n < 0x10000:
            e.write(e.arg(1), noise(n, n & 7))
        return (None, n)
    pyfake(e, 0x4B1510, "File_Open", 0, open_ret, before=lambda e: (None, (e.read_cstr(e.arg(0)),
                                                                            e.read_cstr(e.arg(1)))))
    pyfake(e, 0x4B16D0, "File_Read", 3, before=read)
    pyfake(e, 0x4B1730, "File_Seek", 3)
    pyfake(e, 0x4B17C0, "File_Flush", 1)
    pyfake(e, 0x4B1690, "File_Close", 1)


@pytest.mark.parametrize("frames,ch,read", [(10, 1, 0), (10, 1, 3), (9, 1, 0), (9, 1, 1), (9, 2, 0), (0, 1, 0),
                                            (1, 1, 0), (-3, 1, 0)])
def test_read_adpcm(frames, ch, read):
    def setup(e):
        file_fakes(e)
        v = voice(e, sound(e, fmt=2, bits=16, channels=ch), file=0x4444, read=read, adpcm=(ch, 5, -7, 20, 30, 0))
        d = e.alloc(256)
        return d, v, frames, 2 * ch
    check(emu, 0x496CE0, "Voice_ReadAdpcm", setup=setup, check_eax=False)


@pytest.mark.parametrize("frames", [0, -1, 7])
def test_read_pcm(frames):
    def setup(e):
        file_fakes(e)
        return e.alloc(64), voice(e, sound(e), file=0x4444), frames, 4
    check(emu, 0x496D80, "Voice_ReadPcm", setup=setup, check_eax=False)


def ring(size, fail=0):
    def before(e):
        p = e.alloc(size)
        e.write(p, noise(size, 5))
        u32(e, e.arg(3), p)
        u32(e, e.arg(4), size)
        return (fail, e.arg(1))
    return before


FILL_CASES = {
    # fmt, flags, samples, state, read, write, frames, lockfail, unlockfail
    "pcm": (0, 0, 100000, 0, 0, 0, 400, 0, 0),
    "wrap": (0, 0, 100000, 0, 0, 1000, 200, 0, 0),
    "end": (0, 0, 300, 0, 100, 0, 400, 0, 0),
    "endlate": (0, 0, 300, 0, 100, 0, 400, 0, 0),
    "loop": (0, 0x10, 300, 0, 100, 0, 400, 0, 0),
    "adpcm": (2, 0, 100000, 0, 0, 0, 101, 0, 0),
    "ending": (0, 0, 300, 1, 300, 50, 120, 0, 0),
    "stopped": (0, 0, 300, 2, 0, 0, 100, 0, 0),
    "lockfail": (0, 0, 300, 0, 0, 0, 100, FAIL, 0),
    "unlockfail": (0, 0, 300, 0, 0, 0, 100, 0, FAIL),
    "zero": (0, 0, 300, 0, 0, 0, 0, 0, 0),
}


@pytest.mark.parametrize("case", list(FILL_CASES))
def test_voice_fill(case):
    fmt, flags, samples, state, read, write, frames, lf, uf = FILL_CASES[case]

    def setup(e):
        file_fakes(e)
        u32(e, TIME, 5000)
        u32(e, UPDATE_RATE, 30)
        s = sound(e, fmt=fmt, bits=16 if fmt == 0 else 16, channels=1, rate=300, samples=samples)
        v = voice(e, s, flags=flags, file=0x4444, state=state, read=read, write=write,
                  play=9000 if case == "endlate" else 10, methods={11: (lf, ring(300 * 4 * 2)), 19: uf},
                  adpcm=(1, 3, 0, 10, 0, 0))
        return v, frames
    check(emu, 0x496AB0, "Voice_Fill", setup=setup, check_eax=False)


UPDATE_CASES = {
    # state, play bytes, write, half, endtime, time
    "behind": (0, 100, 300, 600, 0, 0),
    "wrapped": (0, 1000, 100, 600, 0, 0),
    "full": (0, 100, 900, 300, 0, 0),
    "ending": (1, 100, 300, 600, 9000, 5000),
    "over": (1, 100, 300, 600, 4000, 5000),
    "stopped": (2, 100, 300, 600, 0, 0),
    "error": (-1, 100, 300, 600, 0, 0),
}


@pytest.mark.parametrize("case", list(UPDATE_CASES))
@pytest.mark.parametrize("lockfail", [0, FAIL])
def test_voice_update(case, lockfail):
    state, play, write, half, end, time = UPDATE_CASES[case]

    def setup(e):
        file_fakes(e)
        u32(e, TIME, time)
        u32(e, UPDATE_RATE, 30)
        s = sound(e, fmt=0, bits=16, channels=1, rate=300, samples=100000)
        v = voice(e, s, file=0x4444, state=state, write=write, half=half, end=end,
                  methods={4: (0, status(play)), 11: (lockfail, ring(300 * 4 * 2))})
        return (v,)
    check(emu, 0x4972B0, "Voice_Update", setup=setup, eax_mask=0xFF)


# --- create / play ----------------------------------------------------------------

def create_env(e, fmt, create=0, dup=0, open_ret=0x4444, free=True, rate=300, bits=16, channels=1):
    file_fakes(e, open_ret)
    ready(e)
    u32(e, TIME, 5000)
    u32(e, UPDATE_RATE, 30)
    init_lists(e)
    newbuf = com(e, "nbuf", DSBUFFER, {4: (0, status(40)), 11: (0, ring(rate * 4 * 4))})

    def make(e):
        u32(e, e.arg(2), newbuf)
        d = e.read(e.arg(1), 0x24)
        wf = e.read(struct.unpack_from("<I", d, 0x10)[0], 18)
        return (None, (d[:0x10] + d[0x14:], wf))

    def dupl(e):
        u32(e, e.arg(2), newbuf)
        return None
    u32(e, DSOUND_P, com(e, "ds", DSOUND, {3: (create, make), 5: (dup, dupl)}))
    s = sound(e, fmt=fmt, bits=bits, channels=channels, rate=rate, samples=5000, buffer=0x5151,
              name=cstr(e, b"music\\x.wav"))
    e.call(0x496250, LISTS, 4, 0x321, s)
    vs = [voice(e, 0, buffer=0) for _ in range(2)] if free else []
    if vs:
        link(e, FREE, vs)
    link(e, ACTIVE, [voice(e, 0, buffer=0)])


CREATE_CASES = {
    "stream": dict(fmt=0), "adpcm": dict(fmt=2, channels=2), "stereo8": dict(fmt=0, bits=8, channels=2, rate=11025),
    "pcm": dict(fmt=1), "pcm3": dict(fmt=3), "neg": dict(fmt=-1), "big": dict(fmt=7),
    "createfail": dict(fmt=0, create=FAIL), "openfail": dict(fmt=0, open_ret=0), "dupfail": dict(fmt=1, dup=FAIL),
    "nofree": dict(fmt=1, free=False),
}


@pytest.mark.parametrize("case", list(CREATE_CASES))
def test_voice_create(case):
    def setup(e):
        create_env(e, **CREATE_CASES[case])
        return 4, 0x321, 0x13
    check(emu, 0x496860, "Voice_Create", setup=setup)


def test_voice_create_missing():
    def setup(e):
        create_env(e, 1)
        return 4, 0x322, 0
    check(emu, 0x496860, "Voice_Create", setup=setup)
    check(emu, 0x496860, "Voice_Create", setup=lambda e: (create_env(e, 1), ready(e, 0), (4, 0x321, 0))[2])


def params(e, vol=0.5, pitch=1.25, pan=0.25, mn=1.0, mx=4.0, pos=(0, 0, 0), freq=0.75):
    p = e.alloc(0x58)
    e.write(p, f32(vol) + f32(pitch) + f32(pan) + struct.pack("<I", 0x77) + struct.pack("<5d", mn, mx, *pos)
            + noise(0x18, 9) + f32(freq) + b"\x11\x22\x33\x44")
    return p


@pytest.mark.parametrize("case", ["pcm", "loop", "stream", "fail", "default", "notready"])
def test_voice_play(case):
    def setup(e):
        create_env(e, 0 if case == "stream" else 1, dup=FAIL if case == "fail" else 0)
        if case == "notready":
            ready(e, 0)
        return 4, 0x321, 0x10 if case == "loop" else 0, 0 if case == "default" else params(e)
    check(emu, 0x496770, "Voice_Play", setup=setup)


# --- parameters ----------------------------------------------------------------------

@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(24))
def test_voice_set_params(seed, cw):
    rng = random.Random(seed)
    flags = rng.choice([0, 1, 2, 3, 2 | 0x20, 3 | 0x20, 6, 0x20, 2 | 1])
    if seed < 4:
        flags = [2, 3, 0x22, 0x23][seed]

    vols = f32(rng.uniform(0, 1)) + f32(rng.uniform(0, 1)) + f32(rng.uniform(0, 1))
    swap = rng.choice([0, 1])
    m = struct.pack("<10f", *(rng.uniform(-1, 1) for _ in range(10)))
    lp = [rng.uniform(-50000, 50000) for _ in range(3)]
    mn, mx = rng.uniform(0.1, 5), rng.uniform(5, 20)
    dist = rng.choice([0, rng.uniform(0, mn), rng.uniform(mn, mx), rng.uniform(mx, 2 * mx)]) * 4096
    off = [dist, rng.uniform(-1, 1) * dist, rng.uniform(-1, 1) * dist] if seed % 5 else [0, 0, 0]
    rng.shuffle(off)
    rate = rng.choice([11025, 22050])
    vol, pitch, pan = rng.uniform(-0.5, 1.5), rng.uniform(0.2, 3), rng.uniform(-1.5, 1.5)

    def setup(e):
        ready(e)
        e.write(0x543904, vols)
        u32(e, 0x543914, swap)
        e.write(0x5CA160, m)
        e.write(0x5CA138, struct.pack("<3d", *lp))
        v = voice(e, sound(e, fmt=1, rate=rate), flags=flags)
        p = params(e, vol=vol, pitch=pitch, pan=pan, mn=mn, mx=mx, pos=[a + b for a, b in zip(lp, off)])
        return v, p
    check(emu, with_cw(emu, 0x496E60, cw), with_cw(emu, "Voice_SetParams", cw), setup=setup, check_eax=False)


@pytest.mark.parametrize("case", ["stopped", "streaming", "notready"])
def test_voice_set_params_stream(case):
    def setup(e):
        file_fakes(e)
        ready(e, case != "notready")
        u32(e, TIME, 5000)
        u32(e, UPDATE_RATE, 30)
        s = sound(e, fmt=0, channels=1, rate=300, samples=100000)
        v = voice(e, s, file=0x4444, state=2 if case == "stopped" else 0, write=300, half=600,
                  methods={4: (0, status(100)), 11: (0, ring(300 * 4 * 2))})
        return v, params(e)
    check(emu, 0x496E60, "Voice_SetParams", setup=setup, check_eax=False)


@pytest.mark.parametrize("on,found", [(0, 1), (1, 0), (1, 1)])
def test_duration(on, found):
    def setup(e):
        filled(e)
        ready(e, on)
        s = e.call(0x496450, 3, 11)[0]
        u32(e, s + 0x1C, 22050)
        u32(e, s + 0x20, 0x90000001)
        return 3, 11 if found else 99
    compare(emu, 0x496DB0, "Sound_GetDuration", setup=setup, check_eax=False, check_st0=True)


def test_set_listener():
    def setup(e):
        a, b, c = e.alloc(40), e.alloc(24), e.alloc(24)
        e.write(a, noise(40, 1))
        e.write(b, noise(24, 2))
        e.write(c, noise(24, 3))
        return a, b, c
    compare(emu, 0x496E00, "Sound_SetListener", setup=setup, check_eax=False)


# --- names ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", [b"sounds\\gun.wav", b"a/b/Shot.Wav", b"c:x.y.z", b"noext", b"", b"gun.wav",
                                  b"dir\\.wav", b"dir\\name"])
def test_get_name(name):
    def setup(e):
        pyfake(e, 0x4AF810, "Warning_Show", 2, before=lambda e: (None, e.read_cstr(e.arg(1))))
        out = e.alloc(64)
        e.write(out, noise(64))
        return out, cstr(e, name)
    check(emu, 0x497390, "Sound_GetName", setup=setup, check_eax=False)


@pytest.mark.parametrize("key", [b"", b"GUN", b"music_Loop_01", bytes(range(0x20, 0x7F)), b"\xe9\xff\x80abc" * 9])
def test_hashes(key):
    def setup(e):
        return (cstr(e, key),)
    compare(emu, 0x497410, "Sound_HashName", setup=setup)
    compare(emu, 0x497450, "Sound_HashBucket", setup=setup)


def test_params_init():
    def setup(e):
        p = e.alloc(0x58)
        e.write(p, noise(0x58))
        return (p,)
    compare(emu, 0x4974A0, "SoundParams_Init", setup=setup, check_eax=False)
    compare(emu, 0x4974E0, "SoundParams_InitBasic", setup=setup, check_eax=False)


def test_voice_count():
    compare(emu, 0x497510, "Sound_GetVoiceCount", setup=lambda e: u32(e, 0x5CA134, 0x1F))
