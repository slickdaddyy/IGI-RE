"""Helpers for the transctx tests: x87 control word entry points and sound/voice records."""
import struct

from test_d3d_util import DSBUFFER, com, u32

SOUND_READY = 0x6840E9
ACTIVE, FREE = 0x6840F4, 0x6840F8
LISTS = 0x5CA1B8
DSOUND_P = 0x67A498
TIME = 0xC28C5C
UPDATE_RATE = 0x54391C


def code(e, data):
    """Code or data that survives emu.reset()."""
    p = e.fake_top
    e.fake_top += (len(data) + 15) & ~15
    e.write(p, data)
    return p


def with_cw(e, target, cw):
    """Entry point that loads an x87 control word (0x7F is what the game runs at) and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    tramps = e.__dict__.setdefault("cw_tramps", {})
    if (addr, cw) not in tramps:
        cell = code(e, struct.pack("<H", cw))
        p = code(e, b"\0" * 16)
        e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        tramps[(addr, cw)] = p
    return tramps[(addr, cw)]


def f32(v):
    return struct.pack("<f", v)


def u8(e, p, v):
    e.write(p, bytes([v & 0xFF]))


def noise(n, seed=0):
    return bytes((i * 151 + seed * 13 + 7) & 0xFF for i in range(n))


def sound(e, fmt=1, bits=16, channels=2, rate=22050, samples=40, buffer=0, name=0, data=0, key1=0, key2=0):
    s = e.alloc(0x30)
    e.write(s, struct.pack("<IIiiIhhhhiiIII", 0, 0, key1, key2, 0x46534C49, fmt, bits, channels, 0, rate, samples,
                           buffer, name, data))
    return s


def voice(e, snd, flags=0, buffer=None, buffer3d=0, file=0, state=0, play=0, read=0, write=0, end=0, half=0,
          adpcm=(1, 0, 0, 0, 0, 0), methods=None):
    """A Voice; buffer None makes a fake DirectSound buffer."""
    if buffer is None:
        buffer = com(e, "buf", DSBUFFER, methods)
    v = e.alloc(0x4C)
    e.write(v, struct.pack("<IIiIIIIiiiiiiiBxxxi", 0, 0, flags, snd, buffer, buffer3d, file, 0x1234, play, read, write,
                           end, 0x55, half, 0x66, state) + struct.pack("<hhhhhBx", *adpcm))
    return v


def link(e, head, nodes):
    """Doubly linked voice list at head."""
    prev = 0
    for i, n in enumerate(nodes):
        u32(e, n, nodes[i + 1] if i + 1 < len(nodes) else 0)
        u32(e, n + 4, prev)
        prev = n
    u32(e, head, nodes[0] if nodes else 0)
