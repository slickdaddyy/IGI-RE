"""Graph files: GraphStream values, GraphOpen and the old format (0x4F9CD0 - 0x4FAA10)."""
import random
import struct

import pytest

from test_graphnode_util import (LOG, RET, Files, alloc_fake, cstr, d64, logfake, new_emu, run, set_handler, stream,
                                 t_blob, t_double, t_float, t_int, t_str, t_u8, t_u16, t_vec, trap, u16, u32)

GTYPE = 0x60
NODE_SIZE = 0x240
HANDLERS, FREE = 0xA774A0, 0xA76D18
SIZES = 0xA77228
EVENTS = {0x54E369: 0x41, 0x54E36B: 0x42, 0x54E36D: 0x43, 0x54E36F: 0x44}

e = new_emu()
files = Files(e)
alloc_fake(e)
logfake(e, 0x4B0D10, 1)  # Mem_Free
logfake(e, 0x4B0D60, 1)  # Mem_PushMode
logfake(e, 0x4B0D90, 0)  # Mem_PopMode
logfake(e, 0x4AF810, 2, strings=(0, 1))  # Warning_Show
logfake(e, 0x4B8770, 1, strings=(0,), ret=0x5A5A)  # Script_EvalExpr
logfake(e, 0x4CE400, 2, effect=lambda e, a: 0x100 + a[1])  # World_GetMaterial


def event_handler(slot):
    """Logs the graph and what the argument points to; streams may live on the stack, so only
    their file is logged."""
    def fn(e):
        g, arg = e.arg(0), e.arg(1)
        if slot == 0x42:
            shown = e.read(e.u32(arg) + 4, 4)
        elif slot == 0x43:
            shown = (e.normalize(e.u32(arg)), e.read(e.u32(arg + 4) + 4, 4))
        else:
            shown = e.normalize(arg)
        LOG[-1].append(("event", slot, g, shown))
        e.ret(0)
    return trap(e, "event_%x" % slot, fn)


for ev, slot in EVENTS.items():
    e.write(ev, bytes([slot]))
    set_handler(e, slot, GTYPE, event_handler(slot))
SIZE_OF = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 8, 8: 24}


def value_handler(e):
    """A GraphStream handler: logs the value record and skips the value in the file."""
    v = e.arg(0)
    s, g, a, b, type_ = struct.unpack("<5I", e.read(v, 20))
    LOG[-1].append(("value", e.read(s + 4, 4), g, a, b, type_))
    cur = files.cur
    if type_ in SIZE_OF:
        files.pos += 1 + SIZE_OF[type_]
    elif type_ == 9:
        files.pos += 2 + cur[files.pos + 1]
    elif type_ == 10:
        files.pos += 2 + 5 + struct.unpack("<I", cur[files.pos + 3:files.pos + 7])[0]
    e.ret(0)


VALUE_FN = trap(e, "value_handler", value_handler)
e.baseline = e.snapshot()


def list_init(e, lst):
    u32(e, lst, lst + 4, 0, lst)


def list_add(e, lst, node):
    last = e.u32(lst + 8)
    u32(e, node, lst + 4, last)
    u32(e, last, node)
    u32(e, lst + 8, node)


def py_hash(name):
    h, m = 0, 1
    for c in name:
        if 0x61 <= c <= 0x7A:
            c -= 0x20
        c = c - 256 if c >= 128 else c
        h = (h + c * m) & 0xFFFFFFFF
        m = (m + 0x13D) & 0xFFFF
    return h


def handlers(e, names, free=4):
    list_init(e, HANDLERS)
    list_init(e, FREE)
    for i, name in enumerate(names):
        h = e.alloc(0x14)
        u32(e, h + 8, py_hash(name), 10, VALUE_FN)
        list_add(e, HANDLERS, h)
    for i in range(free):
        list_add(e, FREE, e.alloc(0x14))


def graph(e, cap=0):
    g = e.alloc(0xC0)
    e.write(g, bytes(random.Random(5).randrange(256) for _ in range(0xC0)))
    u16(e, g + 0x1C, GTYPE)
    list_init(e, g + 0x70)
    u32(e, g + 0x58, cap, 0, 0)
    u32(e, g + 0x6C, 0)
    if cap:
        u32(e, g + 0x6C, e.alloc(NODE_SIZE * cap))
    for i in range(4):
        u16(e, SIZES + 8 * i, 0x181)
    u16(e, SIZES, GTYPE)
    u32(e, SIZES + 4, NODE_SIZE)
    return g


def test_hash():
    for name in [b"", b"RouteTable", b"routetable", b"Max Nodes", b"\xe9t\xff{`az", b"x" * 300]:
        run(e, 0x4F9E70, "GraphStream_Hash", lambda e, n=name: (cstr(e, n),), check_eax=True)


@pytest.mark.parametrize("names,free", [([], 2), ([b"A", b"B"], 2), ([b"A", b"ROUTETABLE"], 2), ([b"A"], 0)])
def test_add_handler(names, free):
    def setup(e):
        handlers(e, names, free)
        return cstr(e, b"RouteTable"), 10, 0x509A00
    run(e, 0x4F9DF0, "GraphStream_AddHandler", setup)


@pytest.mark.parametrize("want", [b"A", b"C", b"Q"])
def test_find(want):
    def setup(e):
        handlers(e, [b"A", b"B", b"C"])
        return (py_hash(want),)
    run(e, 0x4FA140, "GraphStream_Find", setup, check_eax=True)


DATA = (t_u8(0x81) + t_u8(0x7F, 1) + t_u16(0xBEEF) + t_u16(0x8001, 3) + t_int(0xDEADBEEF, 4) + t_int(-5) +
        t_float(1.5) + t_double(-2.75) + t_vec(1.0, 2.0, 3.0) + t_str(b"hello") + t_blob(b"0123456789"))


@pytest.mark.parametrize("type_,offset,size", [
    (0, 0, 0), (1, 2, 0), (2, 4, 0), (3, 7, 0), (4, 10, 0), (5, 15, 0), (6, 20, 0), (7, 25, 0), (8, 34, 0),
    (9, 59, 64), (10, 67, 10), (10, 67, 4), (11, 0, 0), (0xFF, 0, 0)])
def test_read_value(type_, offset, size):
    def setup(e):
        files.set(DATA[offset:])
        e.call(0x4B1510, cstr(e, b"graph.dat"), cstr(e, b"r"))
        v = e.alloc(20)
        u32(e, v, stream(e), 0x1111, 0, 0, type_)
        out = e.alloc(0x50)
        return v, out, size
    run(e, 0x4F9CD0, "GraphStream_ReadValue", setup)


VALUES = [(0, b"\x81"), (1, b"\x7F"), (2, b"\xEF\xBE"), (3, b"\x01\x80"), (4, b"\xEF\xBE\xAD\xDE"), (5, b"\xFB\xFF\xFF\xFF"),
          (6, struct.pack("<f", -1.5)), (7, struct.pack("<d", 3.25)), (8, struct.pack("<3d", 1, 2, 3)),
          (9, b"some text\0"), (10, b"BLOBDATA12"), (11, b"x"), (0x80, b"y")]


@pytest.mark.parametrize("type_,data", VALUES)
def test_write_value(type_, data):
    def setup(e):
        p = e.alloc(0x40)
        e.write(p, data)
        return stream(e), cstr(e, b"Node Position"), type_, p, len(data)
    run(e, 0x4F9EB0, "GraphStream_WriteValue", setup)


WRITERS = [(0x4FA4E0, "GraphStream_WriteUByte", 0xAB), (0x4FA520, "GraphStream_WriteByte", 0x12345680),
           (0x4FA560, "GraphStream_WriteUShort", 0x1234BEEF), (0x4FA5A0, "GraphStream_WriteShort", 0x8001),
           (0x4FA5E0, "GraphStream_WriteUInt", 0xDEADBEEF), (0x4FA620, "GraphStream_WriteInt", 0xFFFFFFFE),
           (0x4FA660, "GraphStream_WriteFloat", 0x3FC00000)]


@pytest.mark.parametrize("addr,name,v", WRITERS)
def test_write_simple(addr, name, v):
    run(e, addr, name, lambda e: (stream(e), v), check_eax=True, eax_mask=0xFF)


def test_write_double():
    run(e, 0x4FA6A0, "GraphStream_WriteDouble", lambda e: (stream(e), 0x89ABCDEF, 0x40091EB8), check_eax=True,
        eax_mask=0xFF)


def test_write_vec3d():
    def setup(e):
        p = e.alloc(24)
        d64(e, p, 1.0, -2.0, 1e300)
        return stream(e), p
    run(e, 0x4FA6E0, "GraphStream_WriteVec3d", setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("text", [b"", b"NODECRITERIA_DOOR", b"x" * 254])
def test_write_string(text):
    run(e, 0x4FA730, "GraphStream_WriteString", lambda e: (stream(e), cstr(e, text)), check_eax=True, eax_mask=0xFF)


def test_write_blob():
    run(e, 0x4FA7A0, "GraphStream_WriteBlob", lambda e: (stream(e), cstr(e, b"abcdefgh"), 7), check_eax=True,
        eax_mask=0xFF)


READERS = [(0x4FA7E0, "GraphStream_ReadByte", t_u8(0x9C)), (0x4FA810, "GraphStream_ReadShort", t_u16(0xBEEF)),
           (0x4FA850, "GraphStream_ReadInt", t_int(-7)), (0x4FA890, "GraphStream_ReadFloat", t_float(-0.1)),
           (0x4FA890, "GraphStream_ReadFloat", b"\x06\x01\x00\xa0\x7f"),
           (0x4FA8D0, "GraphStream_ReadDouble", t_double(1e-300)),
           (0x4FA8D0, "GraphStream_ReadDouble", b"\x07\x01\x00\x00\x00\x00\x00\xf4\x7f"),
           (0x4FA910, "GraphStream_ReadVec3d", t_vec(4.0, 5.0, -6.0))]


def open_file(e, data):
    files.set(data)
    e.call(0x4B1510, cstr(e, b"graph.dat"), cstr(e, b"r"))
    return stream(e)


@pytest.mark.parametrize("addr,name,data", READERS)
def test_read_simple(addr, name, data):
    def setup(e):
        out = e.alloc(0x20)
        e.write(out, b"\xCC" * 0x20)
        return open_file(e, data), out
    run(e, addr, name, setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("text", [b"", b"abc", b"y" * 250])
def test_read_string(text):
    def setup(e):
        out = e.alloc(0x110)
        e.write(out, b"\xCC" * 0x110)
        return open_file(e, t_str(text) + b"junk"), out, 0x100
    run(e, 0x4FA950, "GraphStream_ReadString", setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("size", [0, 5, 12])
def test_read_blob(size):
    def setup(e):
        out = e.alloc(0x20)
        e.write(out, b"\xCC" * 0x20)
        return open_file(e, t_blob(b"0123456789") + b"ab"), out, size
    run(e, 0x4FA9D0, "GraphStream_ReadBlob", setup, check_eax=True, eax_mask=0xFF)


SKIP_DATA = (t_u8(1) + t_u8(2, 1) + t_u16(3) + t_u16(4, 3) + t_int(5, 4) + t_int(6) + t_float(7) + t_double(8) +
             t_vec(9, 9, 9) + t_str(b"ten") + b"\x09\x00" + b"\x0a" + struct.pack("<I", 3) + b"abc" +
             b"\x0a" + struct.pack("<I", 0) + b"\x0b\x20" + t_u8(0))


@pytest.mark.parametrize("count", [0, 1, 2, 5, 11, 14, 17, 40])
def test_skip(count):
    run(e, 0x4FA3B0, "GraphStream_Skip", lambda e: (open_file(e, SKIP_DATA), count))


def old_graph(nodes, links, cap, extra=b""):
    out = t_int(len(nodes)) + t_int(cap) + t_int(len(links))
    for nid, pos, gamma, radius, unk, mat, crit in nodes:
        out += t_int(nid) + t_vec(*pos) + t_float(gamma) + t_float(radius)
        if unk is not None:
            out += t_float(unk)
        if mat is not None:
            out += t_int(mat)
        out += t_str(crit)
    for a, b, c in links:
        out += t_int(a) + t_int(b) + t_int(c)
    return out + extra


NODES = [(0, (1.0, 2.0, 3.0), 0.5, 1.0, None, 3, b"0"), (5, (4.0, 5.0, 6.0), -0.5, 2.0, 9.0, None, b"NODECRITERIA_DOOR"),
         (130, (7.0, 8.0, 9.0), 1.5, 0.25, None, -1, b""), (5000, (0.0, 0.0, 0.0), 0, 0, None, 7, b"far")]
LINKS = [(0, 5, 100), (5, 130, 7)]


@pytest.mark.parametrize("nodes,links,cap,old", [
    ([], [], 0, 0), (NODES[:1], [], 10, 1), (NODES, LINKS, 10, 0), (NODES[:3], LINKS, 200, 1)])
def test_graph_read(nodes, links, cap, old):
    def setup(e):
        g = graph(e, 10 if old else 0)
        return g, open_file(e, old_graph(nodes, links, cap))
    run(e, 0x4FA160, "Graph_Read", setup)


@pytest.mark.parametrize("case", ["missing", "old", "tagged", "empty"])
def test_graph_open(case):
    tagged = (struct.pack("<I", 0xFFEEDDCC) + t_int(py_hash(b"Max Nodes"), 4) + t_u8(5) + t_int(64) +
              t_int(py_hash(b"Unknown"), 4) + t_u8(9) + t_str(b"skipped") +
              t_int(py_hash(b"RouteTable"), 4) + t_u8(10) + t_blob(b"ROUTES") +
              t_int(py_hash(b"Node Position"), 4) + t_u8(8) + t_vec(1, 2, 3))

    def setup(e):
        handlers(e, [b"Max Nodes", b"RouteTable", b"Node Position"])
        if case == "old":
            files.set(old_graph(NODES[:3], LINKS, 150))
        elif case == "tagged":
            files.set(tagged)
        elif case == "empty":
            files.set(struct.pack("<I", 0xFFEEDDCC))
        else:
            files.set(b"", name=b"other")
        return graph(e), cstr(e, b"graph.dat")
    run(e, 0x4F9FF0, "GraphOpen", setup)
    RET.clear()
