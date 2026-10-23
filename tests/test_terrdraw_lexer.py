"""Token reader and string helpers (0x517260 - 0x5175A0)."""
import pytest

from emu import Emu, compare
from test_d3d_util import cstr, u32

e = Emu()

TOKENS = [b"", b"   ", b"name rest", b"\t\r\n _a.b9+", b"123 x", b"12.5e+3;", b"1.2.3e4e5", b"7e", b"7e-", b"7E+x",
          b"0x1fG", b"0X", b"0xZ", b"$ff00 ", b"$", b"$g", b"\"quoted\" tail", b"\"a\\nb\\tc\\rd\\be\\\"f\\\\g\"",
          b"\"\\x41\\X7a\\xg1\\x9:\"", b"\"unterminated", b"\"\"", b"\"esc at end\\", b"-5", b"+", b"\x80abc",
          b"\"\\\x80\\Y\\y\"", b"9abc", b"0.5.", b"..e", b"1e5e", b"0x", b"\"a\\x", b"abcdefghijklmnopqrstuvwxyz"]


@pytest.mark.parametrize("size", [0, 1, 2, 3, 4, 6, 10, 0x7F])
@pytest.mark.parametrize("text", TOKENS)
@pytest.mark.parametrize("want_end", [0, 1])
def test_read_token(text, size, want_end):
    def setup(em):
        out = em.alloc(0x100)
        em.write(out, b"\xcc" * 0x100)
        end = 0
        if want_end:
            end = em.alloc(4)
            u32(em, end, 0xDEADBEEF)
        return cstr(em, text + b"\0\0\0\0"), end, out, size
    compare(e, 0x517260, "Lex_ReadToken", setup=setup)


@pytest.mark.parametrize("text", [b"0", b"123", b"-45", b"$ff", b"$FF", b"$", b"0x10", b"0Xabc", b"0x", b"$7fffffff",
                                  b"$123456789", b"0xg", b"$:;@", b"  12", b"$\x80", b"0", b"0y5"])
def test_str_to_int(text):
    compare(e, 0x517510, "Str_ToInt", setup=lambda em: (cstr(em, text),))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("text", [b"1.25", b"  -3e2", b"x", b"0.1", b"1e-320"])
def test_str_to_float(text, cw):
    from test_dxinit_util import with_cw
    compare(e, with_cw(e, 0x517570, cw), with_cw(e, "Str_ToFloat", cw), setup=lambda em: (cstr(em, text),),
            check_eax=False, check_st0=True)


@pytest.mark.parametrize("text", [b"", b"abc", b"Hello, World! {`az}", b"\xe0\xff\x7a"])
def test_str_to_upper(text):
    compare(e, 0x517580, "Str_ToUpper", setup=lambda em: (cstr(em, text),), check_eax=False)
