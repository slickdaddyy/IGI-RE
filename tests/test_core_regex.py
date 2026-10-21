import struct

import pytest

from emu import Emu, compare
from test_core_error import cstr

emu = Emu()

STACK = 0x93DAE0

PATTERNS = [
    b"", b"abc", b"a*b", b"a.c", b"^ab", b"ab$", b"^$", b"[a-c]+x", b"[~0-9]*", b"[~]", b"(ab)*c",
    b"(a(bc)*)*d", b"a?b", b"(ab)?c", b"{.*}:(.*//)*", b"{.*:(.*//)*}.*", b"a+", b"[abc]+", b"(ab)+",
    b"/.x", b"[/]a]", b"[\\n\\t\\r\\x]", b"{a}{b*}", b"[a-c][a-c][d-f][a-c]", b".*/.qsc$", b"a(b*)*c",
    b"(a*)*b", b"x(y)+z", b"a\\b", b"[a/-z]",
    # invalid
    b"{{a}}", b"(a", b"a)", b"*a", b"a**", b"[a", b"[z-a]", b"({a})", b"{a", b"a}", b"+", b"?", b"(*)",
    b"[\\", b"a{b(c)}", b"x(y)+?z",
]

STRINGS = [b"abc", b"aabbcabd", b"xyz", b"mission.qsc", b"LOCAL:missions/location0/level1/", b"",
           b"ABC", b"abababc", b"aaab", b"bcbcd", b"\t\n", b"xyyz", b"a]b-", b"012z"]


def classes_prog(e):
    return e.alloc(512 * 20), e.alloc(32 * 256)


@pytest.mark.parametrize("pattern", PATTERNS + [b"a++", b"([a])+", b"[a]+*", b"x/", b"x//", b"(a)+b+"])
def test_expand(pattern):
    compare(emu, 0x4AFFB0, "Regex_Expand", setup=lambda e: (e.alloc(1024), cstr(e, pattern)))


@pytest.mark.parametrize("pattern", PATTERNS)
def test_compile(pattern):
    def setup(e):
        prog, classes = classes_prog(e)
        return prog, classes, cstr(e, pattern)
    compare(emu, 0x4AF9D0, "Regex_Compile", setup=setup)


def test_compile_class_limit():
    # 32 different classes are too many
    pattern = b"".join(b"[%c]" % c for c in range(0x41, 0x41 + 32))
    for n in (30, 31, 32):
        def setup(e):
            prog, classes = classes_prog(e)
            return prog, classes, cstr(e, pattern[:3 * n])
        compare(emu, 0x4AF9D0, "Regex_Compile", setup=setup)


def test_init_classes():
    compare(emu, 0x4AFF90, "Regex_InitClasses", setup=lambda e: (e.alloc(256), e.alloc(4)), check_eax=False)


VALID = [p for p in PATTERNS[:30] if p]


RUN_STRINGS = [b"abc", b"aabbcabd", b"mission.qsc", b"LOCAL:missions/location0/level1/", b"",
               b"abababc", b"bcbcd", b"\t\n", b"xyyz", b"a]b-"]


def run(pattern, strings, nocase):
    for s in strings:
        def setup(e):
            prog, classes = classes_prog(e)
            assert e.call(0x4AF9D0, prog, classes, cstr(e, pattern))[0]
            return e.alloc(4), e.alloc(4), cstr(e, s), prog, classes, nocase
        compare(emu, 0x4B0270, "Regex_Run", setup=setup)


@pytest.mark.parametrize("pattern", VALID)
def test_run(pattern):
    run(pattern, RUN_STRINGS, 0)


@pytest.mark.parametrize("pattern", [b"abc", b"[a-c]+x", b"a.c", b"{.*:(.*//)*}.*"])
def test_run_nocase(pattern):
    run(pattern, [b"ABC", b"aBc", b"abx", b"Local:A/b/c", b"012z"], 1)


def test_run_null_outputs():
    def setup(e):
        prog, classes = classes_prog(e)
        e.call(0x4AF9D0, prog, classes, cstr(e, b"{a*}b"))
        return 0, 0, cstr(e, b"aab"), prog, classes, 0
    compare(emu, 0x4B0270, "Regex_Run", setup=setup)


def test_run_too_many_groups():
    def setup(e):
        prog, classes = classes_prog(e)
        e.call(0x4AF9D0, prog, classes, cstr(e, b"{a}" * 11))
        return e.alloc(4), e.alloc(4), cstr(e, b"a" * 11), prog, classes, 0
    compare(emu, 0x4B0270, "Regex_Run", setup=setup)


def test_run_stack_overflow():
    def setup(e):
        prog, classes = classes_prog(e)
        e.call(0x4AF9D0, prog, classes, cstr(e, b"(ab?)*c"))
        return e.alloc(4), e.alloc(4), cstr(e, b"a" * 600), prog, classes, 0
    compare(emu, 0x4B0270, "Regex_Run", setup=setup)


@pytest.mark.parametrize("c,index", [(0x41, 0), (0x61, 1), (0x7F, 1), (-3, 1)])
def test_class_has(c, index):
    def setup(e):
        classes = e.alloc(512)
        e.write(classes, bytes(range(256)) * 2)
        return c, classes, index
    compare(emu, 0x4B0670, "Regex_ClassHas", setup=setup)


def frames(e, rows):
    stack = e.alloc(20 * 16)
    for i, r in enumerate(rows):
        e.write(stack + 20 * i, struct.pack("<5i", *r))
    top = e.alloc(4)
    e.write(top, struct.pack("<i", len(rows)))
    return stack, top


@pytest.mark.parametrize("rows,args", [
    ([], (3, 4, 1, 0)),
    ([(0, 1, 3, 3, 0)], (3, 4, 1, 0)),
    ([(0, 1, 3, 3, 2)], (3, 4, 1, 0)),
    ([(0, 1, 3, 3, 0)], (3, 5, 1, 0)),
    ([(0, 1, 3, 3, 0)], (2, 4, 1, 0)),
    ([(1, 1, 3, 3, 0)], (3, 4, 1, 0)),
    ([(0, 0, 3, 3, 0)], (3, 4, 9, 0)),
])
@pytest.mark.parametrize("maxdepth", [0, 5])
def test_push(rows, args, maxdepth):
    def setup(e):
        e.write(0x93AAD8, struct.pack("<i", maxdepth))
        stack, top = frames(e, rows)
        return (stack, top) + args
    compare(emu, 0x4B0690, "Regex_Push", setup=setup, check_eax=False)


@pytest.mark.parametrize("rows", [[], [(1, 2, 3, 4, 0)], [(9, 9, 9, 9, 9), (1, 2, 3, 4, 2)]])
@pytest.mark.parametrize("outs", [True, False])
def test_pop(rows, outs):
    def setup(e):
        stack, top = frames(e, rows)
        o = [e.alloc(4) if outs else 0 for _ in range(4)]
        return (stack, top) + tuple(o)
    compare(emu, 0x4B0720, "Regex_Pop", setup=setup)


@pytest.mark.parametrize("state,matched", [(-1, 1), (0, 1), (1, 0), (1, 7)])
def test_next(state, matched):
    def setup(e):
        prog = e.alloc(40)
        e.write(prog, struct.pack("<10i", 0, 0, 0, 11, 12, 0, 0, 0, 21, 22))
        return prog, state, matched, 0, 0
    compare(emu, 0x4B07A0, "Regex_Next", setup=setup)


SEARCHES = [(b"b*c", b"aabbcd"), (b"{.*}:(.*//)*", b"LOCAL:a/b/"), (b"xyz", b"abc"), (b"B", b"abc"),
            (b"b", b"ABCb"), (b"a", b""), (b"(a", b"a"), (b"{[a-z]+}.qvm$", b"LOCAL:qvm/x.qvm"), (b"^b", b"ab")]


@pytest.mark.parametrize("pattern,s", SEARCHES)
@pytest.mark.parametrize("nocase", [0, 1])
def test_search(pattern, s, nocase):
    def setup(e):
        prog, classes = classes_prog(e)
        e.call(0x4AF9D0, prog, classes, cstr(e, pattern))
        return e.alloc(4), e.alloc(4), e.alloc(4), cstr(e, s), prog, classes, nocase
    compare(emu, 0x4B07D0, "Regex_Search", setup=setup)


@pytest.mark.parametrize("pattern,s", SEARCHES)
def test_find(pattern, s):
    setup = lambda e: (e.alloc(4), e.alloc(4), e.alloc(4), cstr(e, pattern), cstr(e, s), 0)
    compare(emu, 0x4B0850, "Regex_Find", setup=setup)


@pytest.mark.parametrize("pattern,s", SEARCHES + [(b"{.*:(.*//)*}.*", b"LOCAL:qvm/x.qvm")])
@pytest.mark.parametrize("nocase", [0, 1])
def test_match(pattern, s, nocase):
    setup = lambda e: (e.alloc(4), e.alloc(4), cstr(e, pattern), cstr(e, s), nocase)
    compare(emu, 0x4B08A0, "Regex_Match", setup=setup)


WILDCARDS = [b"", b"*.qsc", b"mission.qsc", b"a?c", b"a$bc", b"x(y)z^w", b"a/*b", b"a\\?", b"[ab]", b"$", b"a$"]


@pytest.mark.parametrize("wildcard", WILDCARDS)
def test_wildcard_to_regex(wildcard):
    def setup(e):
        src = e.alloc(64)
        e.write(src, wildcard + b"\0Z\0")
        return e.alloc(128), src
    compare(emu, 0x4B08F0, "Wildcard_ToRegex", setup=setup, check_eax=False)


@pytest.mark.parametrize("wildcard,s", [(b"*.qsc", b"mission.qsc"), (b"*.qsc", b"mission.qvm"), (b"M?SS*", b"mission"),
                                        (b"a?c", b"abc"), (b"x*", b"")])
@pytest.mark.parametrize("nocase", [0, 1])
def test_wildcard_find(wildcard, s, nocase):
    setup = lambda e: (e.alloc(4), e.alloc(4), e.alloc(4), cstr(e, wildcard), cstr(e, s), nocase)
    compare(emu, 0x4B09C0, "Wildcard_Find", setup=setup)


def matched(e, pattern, s):
    src = cstr(e, s)
    e.call(0x4B08A0, 0, 0, cstr(e, pattern), src, 0)
    return src


@pytest.mark.parametrize("size,group", [(64, 0), (64, 1), (64, 2), (3, 0), (1, 0), (0, 0), (0, 5), (64, -1)])
def test_get_group(size, group):
    def setup(e):
        src = matched(e, b"{[a-z]*}:{.*}", b"local:missions/a")
        out = e.alloc(64)
        e.write(out, b"\x55" * 64)
        return out, size, src, group
    compare(emu, 0x4B0A10, "Regex_GetGroup", setup=setup)


@pytest.mark.parametrize("tmpl,size", [(b"/0qvmbin.tmp", 0x104), (b"/1-/0-/5//x", 64), (b"abc/", 64),
                                       (b"/0/0/0", 10), (b"xyz", 1), (b"xyz", 0), (b"", 8)])
def test_substitute(tmpl, size):
    def setup(e):
        src = matched(e, b"{.*:(.*//)*}{.*}", b"LOCAL:qvm/x.qvm")
        t = e.alloc(64)
        e.write(t, tmpl + b"\0\0Q\0")
        out = e.alloc(0x110)
        e.write(out, b"\x55" * 0x110)
        return out, size, t, src
    compare(emu, 0x4B0A80, "Regex_Substitute", setup=setup, check_eax=False)
