import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, ErrorShown, compare

emu = Emu()

MEM_INIT = 0x4B0BA0
MODEL_INIT = 0x4CEEB0
VIRMODELS, VIRMODEL_COUNT, VIRMODEL_SAVED = 0xA443A0, 0xA4EDEC, 0xA4EDF0
MODEL_FILES, MODEL_DIR = 0xA4EC60, 0xA4ED6C
LOAD_METHOD, RENDER_FUNCS = 0x548208, 0xA94E84
RESOURCE_LOAD, RESOURCE_UNLOAD, TEXTURE_LOAD = 0x4B5F00, 0x4B6380, 0x4B24C0

# --- python fakes: log calls by value, so stack buffers compare by content --

LOG = []


def pyfake(e, addr, nargs=0, ret=0, strs=(), deref=None, name=None):
    deref = deref or {}

    def hook(uc, a, size, user):
        args = [e.arg(i) for i in range(nargs)]
        rec = [name or hex(addr)]
        for i, v in enumerate(args):
            if i in strs:
                rec.append(e.read_cstr(v))
            elif i in deref:
                rec.append(e.read(v, deref[i]))
            else:
                rec.append(v)
        LOG[-1].append(tuple(rec))
        e.ret(ret(e, args) if callable(ret) else ret)
    e.uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr)


def cmp(e, orig, ours, setup=None, **kw):
    LOG.clear()

    def s(em):
        LOG.append([])
        return setup(em) if setup else None
    r = compare(e, orig, ours, setup=s, **kw)
    assert LOG[0] == LOG[1], "calls differ:\n orig %s\n ours %s" % (LOG[0], LOG[1])
    return r


def both_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        LOG.append([])
        args = setup(e) or ()
        with pytest.raises(ErrorShown) as ex:
            e.call(target, *args)
        msgs.append(str(ex.value))
    assert msgs[0] == msgs[1]


# resources by path, filled by each setup
RES = {}
pyfake(emu, RESOURCE_LOAD, 2, ret=lambda e, a: RES.get(e.read_cstr(a[0]), 0), strs=(0,), name="load")
pyfake(emu, RESOURCE_UNLOAD, 1, strs=(0,), name="unload")
pyfake(emu, TEXTURE_LOAD, 3, strs=(2,), name="texture")
pyfake(emu, 0x4CF910, 1, name="4cf910")
pyfake(emu, 0x4CF870, 1, name="4cf870")
pyfake(emu, 0x4CF9D0, 1, name="free")
RENDER_HANDLER = emu.fake_top
emu.fake_top += 16
emu.write(RENDER_HANDLER, b"\xc3")
pyfake(emu, RENDER_HANDLER, 1, deref={0: 8}, name="render")


def put(e, data):
    p = e.alloc(len(data))
    e.write(p, data)
    return p


def cstr(e, s):
    return put(e, s + b"\0")


def u32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


# --- file builders ----------------------------------------------------------

def be_chunk(cid, data):
    return struct.pack(">II", cid, len(data)) + data + b"\0" * (-len(data) % 4)


def mtp(chunks):
    body = b"MTP " + b"".join(be_chunk(c, d) for c, d in chunks)
    return b"FORM" + struct.pack(">I", len(body)) + body


def ilff(chunks):
    body = b""
    for i, (cid, data) in enumerate(chunks):
        data = data + b"\0" * (-len(data) % 4)
        nxt = 0 if i == len(chunks) - 1 else 0x10 + len(data)
        body += struct.pack("<4I", cid, len(data), 4, nxt) + data
    return struct.pack("<5I", 0x46464C49, 0x14 + len(body), 4, 0, 0x48534D49) + body


def names(*ss):
    return struct.pack("<i", len(ss)) + b"".join(s + b"\0" for s in ss)


def mef(version=0.1, naxt=0):
    head = struct.pack("<f", version) + bytes(range(1, 0x9C))
    chunks = [(0x4D455348, head), (0x41545441, b"atta"), (0x4D565458, b"mvtx" * 3), (0x504F5254, b"port"),
              (0x50544643, b"ptfc"), (0x50545658, b"ptvx")]
    for i in range(naxt):
        chunks.insert(2 + i * 2, (0x4E415854, struct.pack("<ii", 10 + i, 20 + i) + b"frames%d" % i))
    return ilff(chunks)


def coll_mef(parts):
    smes = b"".join(struct.pack("<7i", i, i + 1, i * 2, 5, 6, 7, 8) for i in range(parts))
    return mtp([(0x534D4553, smes), (0x53464143, b"F" * 0x1C * (parts + 1)), (0x53565458, b"V" * 0xC * (parts + 2)),
                (0x45444745, b"E" * 8 * (parts * 2 + 1))])


# --- model file list --------------------------------------------------------

def new_list(e, p):
    e.write(p, struct.pack("<III", p + 4, 0, p))


def add_tail(e, lst, node):
    last = e.u32(lst + 8)
    e.write(node, struct.pack("<II", lst + 4, last))
    u32(e, last, node)
    u32(e, lst + 8, node)


def model_init(e):
    e.call(MEM_INIT)
    e.call(MODEL_INIT)


def add_file(e, lod_names, textures=0):
    f = e.alloc(0x4C)
    for off in (0x10, 0x1C, 0x28):
        new_list(e, f + off)
    if textures:
        u32(e, f + 0x34, e.alloc(textures * 0x24))
        u32(e, f + 0x38, e.alloc(textures * 0xC))
        u32(e, f + 0x3C, e.alloc(textures * 0xC))
    add_tail(e, MODEL_FILES, f)
    lods = []
    for n in lod_names:
        lod = e.alloc(0x1C)
        u32(e, lod + 8, cstr(e, n))
        add_tail(e, f + 0x28, lod)
        lods.append(lod)
    return f, lods


def add_mesh(e, f, tag):
    m = e.alloc(0x18)
    u32(e, m + 8, tag)
    add_tail(e, f + 0x10, m)
    return m


FILES = [[b"box", b"boy", b"gun_a", b"gun_b", b"gun_c"],
         [b"tree1", b"tree2", b"tree3", b"tree4", b"tree5", b"tree6", b"rock", b"missing"]]


def world(e, missing=True):
    model_init(e)
    for lods in FILES:
        add_file(e, [n for n in lods if missing or n != b"missing"])


# --- tests ------------------------------------------------------------------

@pytest.mark.parametrize("addr,name,var", [
    (0x4CEA60, "Model_GetInitMethod", 0x548200), (0x4CEA70, "Model_GetDeinitMethod", 0x548204),
    (0x4CEA80, "Model_GetLoadMethod", 0x548208), (0x4CEA90, "Model_GetUnloadMethod", 0x54820C)])
def test_getters(addr, name, var):
    cmp(emu, addr, name, setup=lambda e: u32(e, var, 0x1234567))


@pytest.mark.parametrize("addr,name", [(0x4CEEA0, "VirModel_RestoreCount"), (0x4CEF30, "VirModel_SaveCount")])
def test_save_restore(addr, name):
    def setup(e):
        u32(e, VIRMODEL_COUNT, 17)
        u32(e, VIRMODEL_SAVED, 9)
    cmp(emu, addr, name, setup=setup, check_eax=False)


MTP = mtp([(0x564E414D, b"abc"), (0x494E5354, b"12345678"), (0x47545420, b""), (0x4D4F4453, b"x" * 13)])


@pytest.mark.parametrize("cid", [0x564E414D, 0x494E5354, 0x47545420, 0x4D4F4453, 0x53564F4C, 0x4D414E56])
@pytest.mark.parametrize("want_size", [0, 1])
def test_mtp_find_chunk(cid, want_size):
    cmp(emu, 0x4CEAA0, "Mtp_FindChunk", setup=lambda e: (put(e, MTP), cid, e.alloc(4) if want_size else 0))


@pytest.mark.parametrize("name", [None, b"tank", b"a" * 15])
@pytest.mark.parametrize("count", [0, 5])
def test_virmodel_add(name, count):
    def setup(e):
        model_init(e)
        u32(e, VIRMODEL_COUNT, count)
        return (cstr(e, name) if name else 0,)
    cmp(emu, 0x4CEBA0, "VirModel_Add", setup=setup)


@pytest.mark.parametrize("addr,name", [(0x4CEBA0, "VirModel_Add"), (0x4CEC40, "VirModel_Get")])
def test_virmodel_full(addr, name):
    def setup(e):
        model_init(e)
        u32(e, VIRMODEL_COUNT, 600)
        return (cstr(e, b"tank"),)
    both_error(emu, addr, name, setup)


def table(e, missing=True):
    world(e, missing)
    for n in (b"box", b"gun_a", b"nothing"):
        e.call(0x4CEC40, cstr(e, n))
    e.call(0x4CEBA0, cstr(e, b"empty"))


@pytest.mark.parametrize("name", [b"box", b"gun_a", b"empty", b"tree1", b"rock", b"gun_b", b"nope", b""])
def test_virmodel_get(name):
    cmp(emu, 0x4CEC40, "VirModel_Get", setup=lambda e: (table(e), (cstr(e, name),))[1])


@pytest.mark.parametrize("name", [b"box", b"tree1", b"nope", b"", b"empty"])
@pytest.mark.parametrize("with_missing", [True, False])
def test_virmodel_find(name, with_missing):
    def setup(e):
        table(e, with_missing)
        return (cstr(e, name),)
    cmp(emu, 0x4CEC10, "VirModel_Find", setup=setup)


@pytest.mark.parametrize("name", [b"box", b"gun_a", b"tree1", b"rock", b"zzz", b"tree"])
def test_virmodel_init_lods(name):
    def setup(e):
        world(e)
        m = e.alloc(0x48)
        e.write(m, b"\x77" * 0x48)
        e.write(m + 0x14, name + b"\0")
        return (m,)
    cmp(emu, 0x4CED50, "VirModel_InitLods", setup=setup, check_eax=False)


@pytest.mark.parametrize("name", [b"box", b"gun_c", b"tree6", b"missing", b"tree", b""])
@pytest.mark.parametrize("empty", [False, True])
def test_modellod_find(name, empty):
    def setup(e):
        if empty:
            model_init(e)
        else:
            world(e)
        return (cstr(e, name),)
    cmp(emu, 0x4CEE30, "ModelLod_Find", setup=setup)


def test_virmodel_init_all_lods():
    cmp(emu, 0x4CF050, "VirModel_InitAllLods", setup=table, check_eax=False)


def test_model_init():
    def setup(e):
        table(e)
        e.write(MODEL_DIR, b"LOCAL:\0")
    cmp(emu, MODEL_INIT, "Model_Init", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("files", [0, 1, 3])
def test_model_shutdown(files):
    def setup(e):
        model_init(e)
        for i in range(files):
            add_file(e, [b"x%d" % i])
    cmp(emu, 0x4CEF40, "Model_Shutdown", setup=setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 2, 3, 4, -1])
@pytest.mark.parametrize("size", [0, 1, 3])
def test_list_get_nth(n, size):
    def setup(e):
        lst = e.alloc(12)
        new_list(e, lst)
        for _ in range(size):
            add_tail(e, lst, e.alloc(8))
        return lst, n
    cmp(emu, 0x4CF300, "List_GetNth", setup=setup)


def mesh_setup(e, data, method=None, handler=True):
    model_init(e)
    RES.clear()
    RES[b"LOCAL:models/crate.mef"] = put(e, data)
    if method is not None:
        u32(e, LOAD_METHOD, method)
        if handler:
            u32(e, RENDER_FUNCS + 4 * method, RENDER_HANDLER)
    return (cstr(e, b"crate"),)


@pytest.mark.parametrize("naxt", [0, 1, 3])
@pytest.mark.parametrize("method,handler", [(None, False), (5, True), (6, False)])
def test_mesh_load(naxt, method, handler):
    cmp(emu, 0x4CF390, "Mesh_Load", setup=lambda e: mesh_setup(e, mef(naxt=naxt), method, handler))


def test_mesh_load_version():
    both_error(emu, 0x4CF390, "Mesh_Load", lambda e: mesh_setup(e, mef(version=0.2)))


@pytest.mark.parametrize("parts", [0, 1, 4])
def test_coll_mesh_load(parts):
    def setup(e):
        model_init(e)
        e.write(MODEL_DIR, b"LOCAL:\0")
        RES.clear()
        RES[b"LOCAL:models/wall.mef"] = put(e, coll_mef(parts))
        return (cstr(e, b"wall"),)
    cmp(emu, 0x4CF690, "CollMesh_Load", setup=setup)


def file_setup(e, chunks):
    f, _ = add_file(e, [])
    u32(e, f + 8, put(e, mtp(chunks)))
    return f


@pytest.mark.parametrize("count", [0, 1, 3])
def test_load_textures(count):
    def setup(e):
        model_init(e)
        return (file_setup(e, [(0x54455846, names(*[b"tex%d" % i for i in range(count)]))]),)
    cmp(emu, 0x4CF790, "ModelFile_LoadTextures", setup=setup, check_eax=False)


@pytest.mark.parametrize("count", [0, 1, 3])
def test_load_meshes(count):
    def setup(e):
        model_init(e)
        RES.clear()
        for i in range(count):
            RES[b"LOCAL:models/m%d.mef" % i] = put(e, mef(naxt=i))
        return (file_setup(e, [(0x4D4F4453, names(*[b"m%d" % i for i in range(count)]))]),)
    cmp(emu, 0x4CF330, "ModelFile_LoadMeshes", setup=setup, check_eax=False)


@pytest.mark.parametrize("count", [None, 0, 2])
def test_load_coll_meshes(count):
    def setup(e):
        model_init(e)
        RES.clear()
        chunks = [(0x4D4F4453, names())]
        if count is not None:
            chunks.append((0x53564F4C, names(*[b"c%d" % i for i in range(count)])))
            for i in range(count):
                RES[b"models/c%d.mef" % i] = put(e, coll_mef(i + 1))
        return (file_setup(e, chunks),)
    cmp(emu, 0x4CF630, "ModelFile_LoadCollMeshes", setup=setup, check_eax=False)


def level_mtp(first):
    """VNAM, INST and GTT chunks of a file with three instances."""
    lods = [b"lod_a", b"lod_b", b"rock"]
    inst = [0, 2, 0, 1, 1, 1, 3, 0x2000, 3, 2, 3, 0, 0]
    if first:
        inst = [0, 2, 0, 1, 1, 1, 3, 1, 3, 2, 3, 0, 0]
    gtt = [(0, 0), (1, -1), (0x2001, 0x2000), (2, 1), (0x2000, -1)]
    return [(0x564E414D, names(*lods)),
            (0x494E5354, struct.pack("<%di" % len(inst), *inst)),
            (0x47545420, struct.pack("<i", len(gtt)) + b"".join(struct.pack("<ii", *g) for g in gtt)),
            (0x4D4F4453, names(b"m0", b"m1")),
            (0x54455846, names(b"t0", b"t1", b"t2"))]


def setup_files(e, second, textures, unk3c=True):
    model_init(e)
    RES.clear()
    e.write(MODEL_DIR, b"LOCAL:\0")
    for i in range(2):
        RES[b"LOCAL:models/m%d.mef" % i] = put(e, mef(naxt=i))
    RES[b"LOCAL:models/lvl.res"] = 0x5000
    RES[b"LOCAL:textures/lvl.res"] = 0x6000
    first = file_setup(e, level_mtp(True))
    for i in range(2):
        add_mesh(e, first, 0x100 + i)
    if textures:
        u32(e, first + 0x34, e.alloc(0x24 * 3))
        u32(e, first + 0x38, e.alloc(0xC * 3))
    f = first
    if second:
        f = file_setup(e, level_mtp(False))
        if unk3c:
            u32(e, f + 0x3C, e.alloc(0xC * 3))
    return f


@pytest.mark.parametrize("second", [False, True])
@pytest.mark.parametrize("textures", [0, 1])
@pytest.mark.parametrize("first_textures", [False, True])
def test_modelfile_setup(second, textures, first_textures):
    def setup(e):
        f = setup_files(e, second, first_textures, unk3c=textures == 0)
        return f, cstr(e, b"lvl"), textures
    cmp(emu, 0x4CF080, "ModelFile_Setup", setup=setup, check_eax=False)


def test_modelfile_load():
    def setup(e):
        setup_files(e, False, True)
        e.call(0x4CEC40, cstr(e, b"lod_a"))
        e.call(0x4CEBA0, cstr(e, b"rock"))
        RES[b"LOCAL:lvl.mtp"] = put(e, mtp(level_mtp(False)))
        return (cstr(e, b"lvl"),)
    cmp(emu, 0x4CEFA0, "ModelFile_Load", setup=setup)
