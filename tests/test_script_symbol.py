import random
import struct

import pytest

from emu import Emu, compare

emu = Emu()
rng = random.Random(1)


def random_name():
    n = rng.randint(1, 24)
    return bytes(rng.choice(b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") for _ in range(n))


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def new_table(e, count):
    buckets = e.alloc(12 * count)
    for i in range(count):
        b = buckets + 12 * i
        e.write(b, struct.pack("<III", b + 4, 0, b))
    t = e.alloc(8)
    e.write(t, struct.pack("<II", buckets, count))
    return t


@pytest.mark.parametrize("count", [1, 7, 64, 1021])
def test_symtab_hash(count):
    for _ in range(20):
        name = random_name()
        compare(emu, 0x4C0530, "SymbolTable_Hash", setup=lambda e: (cstr(e, name), count))


def table_with_symbols(e, count, names):
    e.call(0x4B0BA0)
    t = new_table(e, count)
    for i, n in enumerate(names):
        e.call(0x4C0560, t, cstr(e, n), 0x1000 + i)
    return t


NAMES = [b"Config_IsGermany", b"GOStart", b"MoveLeft", b"a", b"ab", b"ba", b"KEY_ANGLE", b"Flow_RequestEvent"]


@pytest.mark.parametrize("name", NAMES + [b"missing", b"Config_IsGerman"])
def test_symtab_find(name):
    def setup(e):
        t = table_with_symbols(e, 5, NAMES)
        return t, e.alloc(4), cstr(e, name)
    compare(emu, 0x4C04B0, "SymbolTable_Find", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("name", NAMES[:3] + [b"missing"])
@pytest.mark.parametrize("want_value", [True, False])
def test_symtab_lookup(name, want_value):
    def setup(e):
        t = table_with_symbols(e, 3, NAMES)
        return t, e.alloc(4) if want_value else 0, cstr(e, name)
    compare(emu, 0x4C0620, "SymbolTable_Lookup", setup=setup)


def test_symtab_add():
    def setup(e):
        t = table_with_symbols(e, 3, NAMES)
        return t, cstr(e, b"NewSymbol"), 0x1234
    compare(emu, 0x4C0560, "SymbolTable_Add", setup=setup)


def script_setup(e):
    e.call(0x4B0BA0)
    t = new_table(e, 11)
    e.write(0xA43E88, struct.pack("<I", t))


# 0x4B88F0 and the other wrappers all go through Script_RegisterSymbol
@pytest.mark.parametrize("orig,ours,extra", [
    (0x4BAAC0, "Script_RegisterSymbol", (0x401000, 0x2000, 3, 1, 2, 0)),
    (0x4B7E80, "Script_RegisterInt", (0x567C90,)),
    (0x4B7FA0, "Script_RegisterInt2", (0x567C90,)),
    (0x4B7EE0, "Script_RegisterBool8", (0x567C90,)),
    (0x4B7FC0, "Script_RegisterReal", (0x567C90,)),
    (0x4B8810, "Script_RegisterConstant", (42,)),
    (0x4B8890, "Script_RegisterFunction", (0x403660, 0, 1)),
    (0x4B88F0, "Script_RegisterFunctionEx", (0x4048B0, 0, 1)),
])
def test_script_register(orig, ours, extra):
    def setup(e):
        script_setup(e)
        return (cstr(e, b"TestSymbol"),) + extra
    compare(emu, orig, ours, setup=setup, check_eax=False)


@pytest.mark.parametrize("name", [b"TestSymbol", b"Nope"])
def test_script_find_symbol(name):
    def setup(e):
        script_setup(e)
        e.call(0x4B8890, cstr(e, b"TestSymbol"), 0x403660, 0, 1)
        return cstr(e, name), e.alloc(4)
    compare(emu, 0x4BAB80, "Script_FindSymbol", setup=setup)


def test_script_set_symbol_context():
    def setup(e):
        script_setup(e)
        e.call(0x4B8890, cstr(e, b"TestSymbol"), 0x403660, 0, 1)
        return cstr(e, b"TestSymbol"), 0xABCD
    compare(emu, 0x4B8930, "Script_SetSymbolContext", setup=setup, check_eax=False)
