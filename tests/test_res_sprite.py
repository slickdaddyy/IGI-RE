import struct

import pytest

from emu import Emu, compare
from test_res_util import LOG, blob, cstr, errors_equal, f32, font, ilff, init, loop, loop1, render_setup, u32


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


emu = Emu()


def setup_all(e):
    init(e)
    render_setup(e)
    files(e)


def register(e, name, data):
    p = blob(e, data)
    e.call(0x4B6420, cstr(e, name), len(data), p)
    return p


FILES = {
    b"font": lambda: font(),
    b"fixedfont": lambda: font(proportional=0, nframes=3, texh=2),
    b"loop1": lambda: loop1(),
    b"loop1np": lambda: loop1(palette=False),
    b"loop7": lambda: loop(7, 3, charmap=True),
    b"loop7one": lambda: loop(7, 1, palette=False),
    b"loop9": lambda: loop(9, 2, charmap=True),
    b"loop9np": lambda: loop(9, 3, palette=False),
}


def files(e):
    for name, make in FILES.items():
        register(e, name, make())


def sprite(e, name):
    s = e.call(0x4B0C60, 0x18, 4)[0]
    assert e.call(0x4B6720, s, cstr(e, name))[0] == s
    return s


def run(orig, ours, setup, **kw):
    compare(emu, orig, ours, setup=setup, **kw)
    assert LOG[-2] == LOG[-1]


@pytest.mark.parametrize("name", list(FILES))
def test_sprite_load(name):
    def setup(e):
        setup_all(e)
        return e.call(0x4B0C60, 0x18, 4)[0], cstr(e, name)
    run(0x4B6720, "Sprite_Load", setup)


@pytest.mark.parametrize("name", [b"font", b"loop7"])
def test_sprite_new_delete(name):
    def setup(e):
        setup_all(e)
        return (cstr(e, name),)
    run(0x4B7000, "Sprite_New", setup)

    def setup_del(e):
        setup_all(e)
        return (e.call(0x4B7000, cstr(e, name))[0],)
    run(0x4B7030, "Sprite_Delete", setup_del, check_eax=False)


@pytest.mark.parametrize("name", list(FILES))
def test_sprite_free(name):
    def setup(e):
        setup_all(e)
        return (sprite(e, name),)
    run(0x4B66A0, "Sprite_Free", setup, check_eax=False)


def test_sprite_init():
    def setup(e):
        setup_all(e)
        return e.alloc(0x18), 0x1FF, 1, 2, 300, 0x12345, 7, 0x10000, 0x5555, 3, 2
    run(0x4B65E0, "Sprite_Init", setup, check_eax=False)


def test_sprite_set_frame():
    def setup(e):
        setup_all(e)
        return (sprite(e, b"loop7"), 1, 2, 30, 0x10011, -5, 6, 9, 0xABC,
                f32(0.125), f32(-2.5), f32(3e9), f32(1.0))
    run(0x4B6540, "Sprite_SetFrame", setup, check_eax=False)


def test_sprite_load_texture():
    def setup(e):
        setup_all(e)
        return sprite(e, b"loop7"), 1, 0x1234, 4, 8, 16, 2, 3, 0x10000
    run(0x4B64F0, "Sprite_LoadTexture", setup, check_eax=False)


@pytest.mark.parametrize("name", [b"font", b"fixedfont", b"loop7"])
@pytest.mark.parametrize("orig,ours", [(0x4B6CE0, "Sprite_Width"), (0x4B6D00, "Sprite_Height"),
                                       (0x4B6D20, "Sprite_Advance"), (0x4B6D40, "Sprite_IsFixed"),
                                       (0x4B70F0, "Sprite_GetHeight")])
def test_sprite_metrics(name, orig, ours):
    def setup(e):
        setup_all(e)
        return (sprite(e, name),)
    run(orig, ours, setup)


@pytest.mark.parametrize("on", [0, 1, 2])
def test_sprite_set_fixed(on):
    def setup(e):
        setup_all(e)
        return sprite(e, b"font"), on
    run(0x4B6D60, "Sprite_SetFixed", setup, check_eax=False)


@pytest.mark.parametrize("name", [b"font", b"fixedfont"])
@pytest.mark.parametrize("text", [b"", b"A", b"ABCD hello", b"\xff\x80BB"])
def test_sprite_string_width(name, text):
    def setup(e):
        setup_all(e)
        return sprite(e, name), cstr(e, text)
    run(0x4B7050, "Sprite_StringWidth", setup)


# Grid


def grid(e, name=b"font", cols=6, rows=3, text=(b"ABC", b"", b"DA D")):
    s = sprite(e, name)
    g = e.call(0x4B7630, cols, rows, s)[0]
    for y, t in enumerate(text):
        e.call(0x4B77F0, g, cstr(e, t), 0, y)
    return g


def test_grid_load_char_map():
    def setup(e):
        setup_all(e)
        s = sprite(e, b"font")
        cm = struct.pack("<9i", 0, 0, 0, 0, 4, 3, 0, 0, 0) + b"".join(bytes([k]) + b"\x55" * 15 for k in range(12))
        return blob(e, cm), s
    run(0x4B75C0, "Grid_LoadCharMap", setup)


@pytest.mark.parametrize("cols,rows", [(256, 1), (5, 3), (0, 2), (3, 0)])
@pytest.mark.parametrize("name", [b"font", b"fixedfont"])
def test_grid_new(cols, rows, name):
    def setup(e):
        setup_all(e)
        return cols, rows, sprite(e, name)
    run(0x4B7630, "Grid_New", setup)


def test_grid_free_set_get():
    def setup(e):
        setup_all(e)
        return (grid(e),)
    run(0x4B76D0, "Grid_Free", setup, check_eax=False)

    def setup_set(e):
        setup_all(e)
        return grid(e), 4, 2, 0x12345678
    run(0x4B76F0, "Grid_Set", setup_set, check_eax=False)
    for x, y in ((0, 0), (1, 0), (3, 2), (5, 1)):
        def setup_get(e):
            setup_all(e)
            return grid(e), x, y
        run(0x4B7710, "Grid_Get", setup_get, eax_mask=0xFF)


@pytest.mark.parametrize("name", [b"font", b"fixedfont"])
@pytest.mark.parametrize("row", [0, 1, 2])
@pytest.mark.parametrize("prop", [None, 2])
def test_grid_update_row(name, row, prop):
    def setup(e):
        setup_all(e)
        g = grid(e, name)
        e.write(g + 0x10, b"\x01\x00\x03\x00\x00\x02" * 3)
        if prop is not None:
            u32(e, g, prop)
        return g, row
    run(0x4B7730, "Grid_UpdateRow", setup, check_eax=False)


@pytest.mark.parametrize("text,x,y", [(b"ABC", 0, 0), (b"", 1, 1), (b"DDDD", 2, 2), (b"\xc1A", 4, 0)])
def test_grid_print(text, x, y):
    def setup(e):
        setup_all(e)
        return grid(e), cstr(e, text), x, y
    run(0x4B77F0, "Grid_Print", setup, check_eax=False)


@pytest.mark.parametrize("n", [-1, 0, 1, 2, 3, 5])
@pytest.mark.parametrize("cols", [6, 0])
def test_grid_scroll(n, cols):
    def setup(e):
        setup_all(e)
        return grid(e, cols=cols, text=(b"", b"", b"") if cols == 0 else (b"ABC", b"", b"DA D")), n
    run(0x4B7850, "Grid_Scroll", setup, check_eax=False)


@pytest.mark.parametrize("name", [b"font", b"fixedfont"])
@pytest.mark.parametrize("flags", [0, 0x80, 0x100, 0x180, 0x40000001])
@pytest.mark.parametrize("x,y", [(10.0, 20.0), (0.3, -7.77), (1e9, 3.0)])
def test_grid_draw(name, flags, x, y):
    def setup(e):
        setup_all(e)
        g = grid(e, name, text=(b"ABC", b"", b"DA DB"))
        return g, f32(x), f32(y), f32(128.0), f32(64.5), f32(255.0), f32(0.75), f32(1.0), flags, 3
    run(0x4B7930, "Grid_Draw", setup, check_eax=False)


def test_grid_draw_other_type():
    def setup(e):
        setup_all(e)
        g = grid(e)
        u32(e, g, 2)
        return g, f32(1), f32(2), 0, 0, 0, 0, 0, 0, 0
    run(0x4B7930, "Grid_Draw", setup, check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x4B7BF0, "Grid_Width"), (0x4B7C20, "Grid_Height")])
@pytest.mark.parametrize("rows", [0, 1, 3])
def test_grid_size(orig, ours, rows):
    def setup(e):
        setup_all(e)
        return (grid(e, rows=rows, text=(b"AB", b"", b"DDDD")[:rows]),)
    run(orig, ours, setup)


def test_grid_clear():
    def setup(e):
        setup_all(e)
        return (grid(e),)
    run(0x4B7C40, "Grid_Clear", setup, check_eax=False)


# Text


def text(e, name=b"font", s=b"ABBA"):
    return e.call(0x4B6EC0, sprite(e, name), f32(12.5), f32(3.25), cstr(e, s))[0]


def test_text_load():
    def setup(e):
        setup_all(e)
        return (cstr(e, b"loop7"),)
    run(0x4B6DB0, "Text_Load", setup)

    def setup_del(e):
        setup_all(e)
        return (e.call(0x4B6DB0, cstr(e, b"loop9"))[0],)
    run(0x4B6E30, "Text_Delete", setup_del, check_eax=False)


@pytest.mark.parametrize("name", [b"font", b"fixedfont"])
def test_text_new(name):
    def setup(e):
        setup_all(e)
        return sprite(e, name), f32(-1.5), f32(100.0), cstr(e, b"DCBA A")
    run(0x4B6EC0, "Text_New", setup)


@pytest.mark.parametrize("orig,ours,eax", [
    (0x4B6E60, "Text_Draw", False), (0x4B6E70, "Text_Width", True), (0x4B6E80, "Text_Height", True),
    (0x4B6F30, "Text_Free", False), (0x4B6F50, "Text_DrawAlt", False), (0x4B7DB0, "Text_Render", False),
    (0x4B7DF0, "Text_GetWidth", True), (0x4B7E00, "Text_GetHeight", True)])
@pytest.mark.parametrize("name", [b"font", b"fixedfont"])
def test_text_calls(orig, ours, eax, name):
    def setup(e):
        setup_all(e)
        t = text(e, name)
        u32(e, t + 0x24, 0x80)
        return (t,)
    run(orig, ours, setup, check_eax=eax)


def test_text_set_string():
    def setup(e):
        setup_all(e)
        return text(e), cstr(e, b"CAB")
    run(0x4B6E90, "Text_SetString", setup, check_eax=False)


@pytest.mark.parametrize("x,y", [(12.5, 3.25), (13.5, -2.5), (-0.4, 1e10), (7.0, 0.5)])
def test_text_get_rect(x, y):
    def setup(e):
        setup_all(e)
        t = text(e)
        e.write(t + 4, struct.pack("<ff", x, y))
        return t, e.alloc(16)
    run(0x4B6F90, "Text_GetRect", setup, check_eax=False)


# Palette


def test_palette_register():
    run(0x4B7C80, "Palette_Register", lambda e: setup_all(e), check_eax=False, normalized=True)
    run(0x4B7CC0, "Palette_Unregister", lambda e: setup_all(e), check_eax=False)


def test_palette_create_free():
    def setup(e):
        setup_all(e)
        return e.alloc(12), 0x1234, 16
    run(0x4B7CF0, "Palette_Create", setup, check_eax=False)

    def setup_free(e):
        setup_all(e)
        p = e.alloc(12)
        e.call(0x4B7CF0, p, 0x1234, 16)
        return (p,)
    run(0x4B7D30, "Palette_Free", setup_free, check_eax=False)


def test_palette_load():
    def setup(e):
        setup_all(e)
        register(e, b"pal", b"LOOP" + struct.pack("<iii", 3, 0, 5) + bytes(15))
        return e.alloc(12), cstr(e, b"pal")
    run(0x4B7D50, "Palette_Load", setup, check_eax=False)


# QVertex


def frames_sprite(e):
    return sprite(e, b"loop9")


def floats(*v):
    return tuple(f32(x) for x in v)


def test_qvertex_draw_frame():
    for frame in (0, 1):
        for x, y in ((0.0, 0.0), (10.25, -3.5), (1e30, 16777217.0)):
            def setup(e):
                setup_all(e)
                return (frames_sprite(e),) + floats(x, y, 255.0, 1.0, 0.1, 0.5, 2.0) + (frame, 0x80, 7)
            run(0x4B50B0, "QVertex_DrawFrame", setup, check_eax=False)


def test_qvertex_draw_quad():
    def setup(e):
        setup_all(e)
        return (frames_sprite(e),) + floats(1, 2, 3, 4, 5, 6, 7, 8, 100, 200, 3.3) + (1, 0x40, 9)
    run(0x4B51F0, "QVertex_DrawQuad", setup, check_eax=False)

    def setup_ex(e):
        setup_all(e)
        return (frames_sprite(e),) + floats(1, 2, 3, 4, 5, 6, 7, 8, 100, 200, 3.3, 0.5, 0.25) + (0, 0x40, 9)
    run(0x4B53B0, "QVertex_DrawQuadEx", setup_ex, check_eax=False)

    def setup_uv(e):
        setup_all(e)
        return (frames_sprite(e),) + floats(*range(1, 17)) + floats(100, 200, 3.3, 0.5, 0.25) + (1, 0x40, 9)
    run(0x4B5560, "QVertex_DrawQuadUV", setup_uv, check_eax=False)


def test_qvertex_draw_line():
    def setup(e):
        setup_all(e)
        return floats(1.5, 2.25, 0.5, 100.75, -3, 1, 10, 20, 30, 40, 50, 60, 0.5) + (3, 4)
    run(0x4B5730, "QVertex_DrawLine", setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 2, 5])
@pytest.mark.parametrize("angle", [0.0, -0.0, 0.7, 3.14159, float("nan")])
def test_qvertex_draw_polyline(n, angle):
    def setup(e):
        setup_all(e)
        pts = blob(e, struct.pack("<10f", 0.1, 0.2, 10.3, -4.7, 1e7, 3.3, 5.5, 5.5, -1, -2))
        return (pts, n) + floats(100.3, 50.7, 0.5, 1, 2, 3, 0.9, angle, 1.7, 0.3) + (5, 6)
    run(0x4B5860, "QVertex_DrawPolyline", setup, check_eax=False)


@pytest.mark.parametrize("data,msg", [
    (ilff(b"FONT", []), "corrupt"),
    (ilff(b"FONT", [(b"FNTH", bytes(24)), (b"TRAN", bytes(512))]), "corrupt"),
    (ilff(b"FONT", [(b"FNTH", bytes(24)), (b"TRAN", bytes(512)), (b"ANMF", b"")]), "corrupt"),
    (ilff(b"FONT", [(b"FNTH", struct.pack("<6i", 1, 1, 0, 0, 0, 0)), (b"TRAN", bytes(512)), (b"ANMF", bytes(40))]), "corrupt"),
    (b"LOOP" + struct.pack("<i", 5) + bytes(64), "packed"),
    (b"LOOP" + struct.pack("<i", 10) + bytes(64), "packed"),
    (b"LOOP" + struct.pack("<i", 4) + bytes(64), "Invalid"),
    (b"LOOX" + struct.pack("<i", 1) + bytes(64), "Invalid"),
    (b"XOOP" + struct.pack("<i", 1) + bytes(64), "Invalid"),
])
def test_sprite_load_errors(data, msg):
    def setup(e):
        setup_all(e)
        register(e, b"bad", data)
        return e.alloc(0x18), cstr(e, b"bad")
    assert msg in errors_equal(emu, 0x4B6720, "Sprite_Load", setup)


def test_palette_load_error():
    def setup(e):
        setup_all(e)
        register(e, b"pal", b"LOOP" + struct.pack("<iii", 2, 0, 5) + bytes(15))
        return e.alloc(12), cstr(e, b"pal")
    assert "palette" in errors_equal(emu, 0x4B7D50, "Palette_Load", setup)

