"""Helpers shared by the test_res tests."""
import struct

from emu import TRAP_BASE


def fake(e, name, fn):
    """A fake function in the import trap area, fn(e) does the work and returns with e.ret()."""
    if name not in e.stubs:
        addr = TRAP_BASE + len(e.imports) * 4
        e.imports.append(("test", name))
        e.write(addr, b"\xc3")
        e.stub(name, fn)
        e.fakes = getattr(e, "fakes", {})
        e.fakes[name] = addr
    e.stubs[name] = fn
    return e.fakes[name]


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def u32(e, p, v):
    e.write(p, struct.pack("<I", v))


LOG = []


def log_call(nargs, ret=0):
    def fn(e):
        LOG[-1].append(tuple(e.arg(i) for i in range(nargs)))
        e.ret(ret)
    return fn


# device 1, named "X:", with fake handlers
FILES = {}


def dev_open(e):
    name = e.read_cstr(e.arg(1))
    LOG[-1].append(("open", name))
    if name not in FILES:
        e.ret(0)
        return
    h = e.alloc(0x200)
    e.write(h + 4, struct.pack("<I", len(FILES[name])))
    e.write(h + 0x100, name + b"\0")
    e.ret(h)


def dev_unpack(e):
    h, size, rec = e.arg(1), e.arg(2), e.arg(3)
    name = e.read_cstr(h + 0x100)
    data = e.alloc(len(FILES[name]))
    e.write(data, FILES[name])
    u32(e, size, len(FILES[name]))
    u32(e, rec + 0x18, data)
    e.ret(data)


def dev_foreach(e):
    LOG[-1].append((e.arg(0), e.read_cstr(e.arg(1)), e.arg(2), e.arg(3)))
    e.ret(0)


def dev_time(e):
    u32(e, e.arg(1), 0x11223344)
    u32(e, e.arg(1) + 4, 0x55667788)
    e.ret(0)


def init(e, device=True):
    LOG.append([])
    e.call(0x4B0BA0)
    e.call(0x4B5E70)
    files = 0x943970
    e.write(files, struct.pack("<III", files + 4, 0, files))
    if device:
        d = 0x9436B8 + 0x8C
        e.write(0x9437B8, b"X:\0")
        u32(e, 0x943794, 1)
        u32(e, d, fake(e, "dev_open", dev_open))
        u32(e, d + 4, fake(e, "dev_close", lambda e: e.ret(0)))
        u32(e, d + 0x30, fake(e, "dev_unpack", dev_unpack))
        u32(e, d + 0x34, fake(e, "dev_foreach", dev_foreach))
        u32(e, d + 0x38, fake(e, "dev_time", dev_time))


def resource(e, name, data=0, size=0, type=3, time=0, info=0):
    return e.call(0x4B59F0, cstr(e, name), info, data, size, type, time)[0]


def logs_equal():
    assert LOG[-2] == LOG[-1]


def ilff(form, children, align=4):
    """An ILFF form chunk holding (id, data) or nested (id, form, children) chunks."""
    body = b""
    out = []
    for c in children:
        if len(c) == 3:
            blob = ilff(c[1], c[2], align)
        else:
            data = c[1]
            pad = (-(0x10 + len(data))) % align if align > 1 else 0
            blob = struct.pack("<4sIII", c[0], len(data), align, 0) + data + b"\0" * pad
            blob = blob[:0xC] + struct.pack("<I", len(blob)) + blob[0x10:]
        out.append(blob)
    for i, blob in enumerate(out):
        if i == len(out) - 1:
            blob = blob[:0xC] + b"\0\0\0\0" + blob[0x10:]
        body += blob
    head = (0x14 + align - 1) & ~(align - 1)
    total = head + len(body)
    return struct.pack("<4sIIII", b"ILFF", total, align, total, int.from_bytes(form, "little")) + b"\0" * (head - 0x14) + body


GOOD = ilff(b"IRES", [
    (b"NAME", b"Fonts\\Small.fnt\0"),
    (b"BODY", b"font data here!"),
    (b"ILFF", b"DBUG", [(b"CSTR", b"made by hand\0")]),
    (b"NAME", b"text\0"),
    (b"CSTR", b"hello\0"),
    (b"NAME", b"sub\0"),
    (b"ILFF", b"IRES", [(b"NAME", b"inner\0"), (b"BODY", b"xyz")]),
    (b"NAME", b"search\0"),
    (b"PATH", b"text;inner\0"),
    (b"ILFF", b"DBUG", [(b"CSTR", b"dbg\0")]),
])


def blob(e, data):
    p = e.alloc(len(data) + 32)
    p = (p + 31) & ~31
    e.write(p, data)
    return p


def respack(members):
    out = b"RESOURCE" + struct.pack("<I", len(members)) + b"\0" * 20
    for name, data in members:
        e = struct.pack("<III", 0, len(data), len(name)) + name
        e += b"\0" * (((len(name) + 0x2B) & ~0x1F) - len(e))
        out += e + data + b"\0" * (((len(data) + 0x1F) & ~0x1F) - len(data))
    return out


# Render driver: fixed ids for the functions the res code registers, each
# a fake that logs what it is given.
QVERTEX, ALLOC_PAL, TEX_A, TEX_B, TEX_FREE, DEALLOC_PAL = 1, 2, 3, 4, 5, 6


def render_log(slot, size):
    def fn(e):
        p = e.arg(0)
        if slot == QVERTEX:
            hdr = struct.unpack("<5I", e.read(p, 20))
            entry = (slot, hdr[:4], e.read(hdr[4], hdr[3] * 0x24), e.read(0xA84A50, 12))
        else:
            entry = (slot, e.read(p, size))
        LOG[-1].append(entry)
        e.ret(0)
    return fn


def render_setup(e):
    for slot, size in ((QVERTEX, 0), (ALLOC_PAL, 12), (TEX_A, 0x30), (TEX_B, 0x30)):
        u32(e, 0xA94E84 + 4 * slot, fake(e, "render%d" % slot, render_log(slot, size)))
    for slot in (TEX_FREE, DEALLOC_PAL):
        u32(e, 0xA950A4 + 4 * slot, fake(e, "render%d" % slot, log_call(1)))
    u32(e, 0xA94A64, 0)
    u32(e, 0xA84A54, QVERTEX)
    u32(e, 0xA84A48, ALLOC_PAL)
    u32(e, 0xA84A40, DEALLOC_PAL)
    e.write(0x546FA8, struct.pack("<III", TEX_A, TEX_FREE, TEX_B))


def f32(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def font(proportional=1, nframes=4, texh=1):
    """An ILFF "FONT" form."""
    fnth = struct.pack("<6i", proportional, nframes, 0, 9, 8, 12)
    tran = b"".join(struct.pack("<H", (c - 0x40) if 0x41 <= c < 0x41 + nframes else 0) for c in range(256))
    anmf = b""
    for i in range(nframes):
        anmf += struct.pack("<i4f4h3h2hh", 0x100 * i, 0.25 * i, 0.5, 0.75, 1.0, i % texh, 5 + i, 10, 6 + i, 0, 0, 0, i, -i, 0)
    kids = [(b"FNTH", fnth), (b"TRAN", tran), (b"ANMF", anmf)]
    for t in range(texh):
        texhd = struct.pack("<3i8h", 3, 77, 0, 2, 16, 16, 8, 0, 4, 0, 0)
        kids += [(b"TEXH", texhd), (b"BODY", bytes(range(32)) * 4)]
    return ilff(b"FONT", kids)


def loop1(palette=True):
    pal = struct.pack("<4i", 0, 0, 0, 3) + bytes(range(12))
    head = struct.pack("<4si i i 3h 3H", b"LOOP", 1, 0, 0, 0, 0, 0, 4, 16, 8)
    body = head + bytes(range(64))
    if palette:
        body = body[:12] + struct.pack("<i", len(body)) + body[16:] + pal
    return body


def loop(version, nframes, palette=True, charmap=False):
    head = bytearray(struct.pack("<4si i i hh i i i i i i i i", b"LOOP", version, 0, 0, 0, -3, 0, 0, 0, nframes, 0, 32, 24, 5))
    frames = b""
    pix = 0x34 + nframes * (0x28 if version == 7 else 0x20)
    for i in range(nframes):
        if version == 7:
            frames += struct.pack("<10i", pix + 16 * i, 1, 4 + i, 4, 6 + i, 0, 0, 0, i, 2 * i)
        else:
            frames += struct.pack("<iiHHHh3hhhhi", pix + 16 * i, 2, 1, 4 + i, 4, 6 + i, 0, 0, 0, i, 2 * i, 0, 0x10000 * (i & 1))
    body = bytes(head) + frames + bytes(range(16)) * nframes
    if palette:
        off = len(body)
        body += struct.pack("<4i", 0, 0, 0, 7) + bytes(range(21))
        body = body[:0x18] + struct.pack("<i", off) + body[0x1C:]
    if charmap:
        off = len(body)
        cols, rows = 3, 2
        cm = struct.pack("<9i", 0, 0, 0, 0, cols, rows, 0, 0, 0)
        for k in range(cols * rows):
            cm += bytes([k % (nframes + 1)]) + b"\xee" * 15
        body += cm
        body = body[:0x1C] + struct.pack("<i", off) + body[0x20:]
    return body


def errors_equal(emu, orig, ours, setup):
    """Both versions stop in Error_Show with the same format string."""
    from emu import ErrorShown
    msgs = []
    for target in (orig, ours):
        emu.reset()
        args = setup(emu) or ()
        try:
            emu.call(target, *args)
        except ErrorShown as ex:
            msgs.append(str(ex))
        else:
            raise AssertionError("no error from 0x%x" % orig if target == orig else "no error from %s" % ours)
    assert msgs[0] == msgs[1]
    return msgs[0]
