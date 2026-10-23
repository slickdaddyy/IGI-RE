"""ConditionalContainer handlers and GuardGenerator."""
import pytest

from test_levelflow_util import (LOG, RET, add_child, check_init, handler, kids, logfake, make_emu,
                                 make_init_emu, pyfake, run, set_handler, task, trap, u16, u32, vfake)

SIZE_OP, COPY_OP = 0x51, 0x52

emu = make_emu()
logfake(emu, 0x4B0D60, 1)  # Mem_PushMode
logfake(emu, 0x4B0D90, 0)  # Mem_PopMode
logfake(emu, 0x4B0D10, 1)  # Mem_Free
logfake(emu, 0x4F1090, 2)  # Level_SetTaskId
logfake(emu, 0x463170, 1, effect=lambda e, a: RET.get("dead", 0))  # Human_IsDead
logfake(emu, 0x463190, 1, effect=lambda e, a: RET.get("dying", 0))
logfake(emu, 0x401E30, 3, masks={1: 0xFF}, derefs={2: 1})  # Task_BroadcastChildren
logfake(emu, 0x4F1B90, 0, ret=0x1200 + SIZE_OP)  # ParamOp_GetSize, junk above the byte
logfake(emu, 0x4F1BA0, 0, ret=0x1200 + COPY_OP)  # ParamOp_Copy
vfake(emu, 0x4B0C60, 2, effect=lambda e, a: e.alloc(a[0]))  # Mem_Alloc
vfake(emu, 0x4012A0, 3, show=lambda e, a: (e.normalize(a[0]), a[1] & 0xFFFF, a[2]),
      effect=lambda e, a: task(e, 0x40, a[1] & 0xFFFF))  # Task_New


def param_call(e, a):
    field, type_, op, arg = a[0], a[1], a[2] & 0xFF, a[3]
    if op == SIZE_OP:
        u32(e, arg, RET["sizes"][type_])
    if LOG:
        LOG[-1].append(("param", e.normalize(field), type_, op, e.u32(arg)))
    e.ret(0)


pyfake(emu, 0x4F1AF0, lambda e: param_call(e, [e.arg(i) for i in range(4)]))  # ParamType_Call


def get_params(e):
    t, out = e.arg(0), e.arg(1)
    LOG[-1].append(("get_params", e.normalize(t)))
    u32(e, out, RET["params"].get(t, 0))
    e.ret(0)


GET_PARAMS = trap(emu, "get_params", get_params)
FWD = handler(emu, "fwd", 2)
RUN = handler(emu, "run", 1)
DAC = handler(emu, "dac", 1)
emu.baseline = emu.snapshot()
init = make_init_emu()


# ConditionalContainer

def cc(e, run_=0, registered=0, in_world=0):
    t = task(e, 0x114, 0x50)
    e.write(t + 0x110, bytes([run_, registered, in_world]))
    return t


@pytest.mark.parametrize("registered", [0, 1])
def test_cc_activate(registered):
    run(emu, 0x487920, "ConditionalContainer_Activate", lambda e: (cc(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_cc_deactivate(registered):
    run(emu, 0x487980, "ConditionalContainer_Deactivate", lambda e: (cc(e, registered=registered),))


@pytest.mark.parametrize("n", [0, 1, 3])
def test_cc_forward_event4(n):
    def setup(e):
        t = cc(e)
        kids(e, t, n, 0x3F, FWD)
        return t, 0x1234
    run(emu, 0x4879E0, "ConditionalContainer_ForwardEvent4", setup)


@pytest.mark.parametrize("run_,in_world", [(0, 0), (1, 0), (1, 1), (0, 1)])
@pytest.mark.parametrize("n", [0, 3])
def test_cc_world_add(run_, in_world, n):
    def setup(e):
        t = cc(e, run_=run_, in_world=in_world)
        kids(e, t, n, 0x39, FWD)
        return t, 0x5678
    run(emu, 0x487A70, "ConditionalContainer_WorldAdd", setup)


@pytest.mark.parametrize("in_world", [0, 1])
@pytest.mark.parametrize("n", [0, 3])
def test_cc_world_remove(in_world, n):
    def setup(e):
        t = cc(e, in_world=in_world)
        kids(e, t, n, 0x3A, FWD)
        return t, 0x5678
    run(emu, 0x487B20, "ConditionalContainer_WorldRemove", setup)


@pytest.mark.parametrize("addr,name,globs", [(0x487BC0, "ConditionalContainer_Close", (0x5C8A80, 0x5C8A7C)),
                                             (0x4885F0, "GuardGenerator_Close", (0x541094, 0x5C8A88))])
def test_close(addr, name, globs):
    def setup(e):
        for i, g in enumerate(globs):
            u32(e, g, 0xABCD0010 + i)
    run(emu, addr, name, setup)


# GuardGenerator

def test_init():
    check_init(init, 0x487BE0, "GuardGenerator_Init")


def guard(e, on=0, registered=0, spawned=0, n=0, max_=3, tree=0):
    g = task(e, 0x80, 0x52)
    e.write(g + 0x70, bytes([on, registered, spawned]))
    u32(e, g + 0x74, n, max_, tree)
    return g


def template(e, parent=0, type_=0x70, id_=0, nparams=2):
    """A GuardTemplate with nparams parameter values."""
    t = e.alloc(0x2C)
    u32(e, t + 8, t + 0xC, 0, t + 8, parent)
    u16(e, t + 0x1C, type_)
    u32(e, t + 0x20, id_)
    params = e.alloc(12)
    entries = e.alloc(16 * max(nparams, 1))
    u32(e, params, nparams, nparams, entries)
    for i in range(nparams):
        u32(e, entries + 16 * i, 0x20 + 8 * i, 3 + i, 0, 7)
    values = e.alloc(4 * max(nparams, 1))
    for i in range(nparams):
        u32(e, values + 4 * i, e.alloc(8) if i != 1 else 0)
    u32(e, t + 0x24, params, values)
    if parent:
        last = e.u32(parent + 0x10)
        u32(e, t, parent + 0xC, last)
        u32(e, last, t)
        u32(e, parent + 0x10, t)
    return t


def tree(e, kids_=2):
    root = template(e, id_=5)
    for i in range(kids_):
        template(e, root, type_=0x71 + i, id_=i, nparams=i + 1)
    return root


@pytest.mark.parametrize("n", [0, 3])
@pytest.mark.parametrize("addr,name,slot", [(0x487D90, "GuardGenerator_WorldAdd", 0x39),
                                            (0x487E20, "GuardGenerator_WorldRemove", 0x3A)])
def test_world(n, addr, name, slot):
    def setup(e):
        g = guard(e)
        kids(e, g, n, slot, FWD)
        return g, 0x4321
    run(emu, addr, name, setup)


def test_create():
    run(emu, 0x487EB0, "GuardGenerator_Create", lambda e: (guard(e, 1, 1, 1, 5, 6, 0x7000),))


@pytest.mark.parametrize("has,value", [(0, 0.0), (1, 0.0), (1, 1.0), (1, 0.6), (1, 258.0)])
@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("n", [0, 3])
@pytest.mark.parametrize("child,dead,dying", [(0, 0, 0), (1, 0, 0), (1, 1, 0), (1, 1, 1)])
@pytest.mark.parametrize("has_tree", [0, 1])
def test_run(has, value, on, n, child, dead, dying, has_tree):
    def setup(e):
        RET.update(has=[has], evals=[value], dead=dead, dying=dying)
        g = guard(e, on=on, n=n, tree=tree(e) if has_tree else 0)
        if child:
            kids(e, g, 2, 1, RUN)
        return (g,)
    run(emu, 0x487EE0, "GuardGenerator_Run", setup)


@pytest.mark.parametrize("has_tree", [0, 1])
def test_spawn(has_tree):
    run(emu, 0x488000, "GuardGenerator_Spawn", lambda e: (guard(e, tree=tree(e, 3) if has_tree else 0),))


@pytest.mark.parametrize("id_", [-1, 0, 4])
@pytest.mark.parametrize("nparams", [0, 1, 3])
@pytest.mark.parametrize("nkids", [0, 1, 2])
def test_template_spawn(id_, nparams, nkids):
    def setup(e):
        t = template(e, id_=id_, nparams=nparams)
        for i in range(nkids):
            c = template(e, t, type_=0x71 + i, nparams=i)
            if i == 1:
                template(e, c, type_=0x75)
        return t, task(e, 0x40, 0x52)
    run(emu, 0x488040, "GuardTemplate_Spawn", setup)


@pytest.mark.parametrize("id_", [-1, 0, 2])
@pytest.mark.parametrize("handled", [0, 1])
@pytest.mark.parametrize("has_tree", [0, 1])
def test_delete(id_, handled, has_tree):
    def setup(e):
        RET["id"] = id_
        set_handler(e, 0x37, 0x52, DAC if handled else 0)
        return (guard(e, tree=tree(e, 3) if has_tree else 0),)
    run(emu, 0x4880F0, "GuardGenerator_Delete", setup)


@pytest.mark.parametrize("nkids", [0, 2])
@pytest.mark.parametrize("no_params", [0, 1])
def test_template_free(nkids, no_params):
    def setup(e):
        t = tree(e, nkids)
        if no_params:
            u32(e, t + 0x24, 0)
        return (t,)
    run(emu, 0x488160, "GuardTemplate_Free", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x5C8A88, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x4881F0, "GuardGenerator_GetParams", setup)


def params(e, n):
    p = e.alloc(12)
    entries = e.alloc(16 * max(n, 1))
    u32(e, p, n, n, entries)
    for i in range(n):
        u32(e, entries + 16 * i, 0x20 + 4 * i, i, 0, 7)
    return p


SIZES = [8, 0, 4, 12]


def guard_tree(e, shape):
    """A GuardGenerator with a first child (the guard) and children below it.
    shape: list of (has_params, grandchildren) for the guard's children."""
    g = guard(e)
    RET.update(params={}, sizes=SIZES)
    guard_ = task(e, 0x60, 0x70)
    add_child(e, g, guard_)
    RET["params"][guard_] = params(e, 3)
    for i, (p, gk) in enumerate(shape):
        c = task(e, 0x60, 0x71 + i)
        add_child(e, guard_, c)
        if p:
            RET["params"][c] = params(e, p)
        for j in range(gk):
            add_child(e, c, task(e, 0x40, 0x78))
    for t in range(0x70, 0x79):
        set_handler(e, 0x34, t, GET_PARAMS if t != 0x73 else 0)
    return g


SHAPES = [[(1, 0)], [(2, 0), (0, 2), (4, 0)], [(0, 0), (0, 0)], [(3, 0), (0, 0), (0, 1)]]


@pytest.mark.parametrize("shape", SHAPES)
@pytest.mark.parametrize("ids", [0, 1])
def test_snapshot(shape, ids):
    def setup(e):
        RET["id"] = 3 if ids else -1
        return (guard_tree(e, shape),)
    run(emu, 0x488220, "GuardGenerator_Snapshot", setup)


def test_snapshot_empty():
    run(emu, 0x488220, "GuardGenerator_Snapshot", lambda e: (guard(e),))


@pytest.mark.parametrize("shape", SHAPES[:3])
def test_changed(shape):
    run(emu, 0x488200, "GuardGenerator_Changed", lambda e: (guard_tree(e, shape),))


@pytest.mark.parametrize("has_parent", [0, 1])
def test_template_new(has_parent):
    def setup(e):
        RET["id"] = 9
        parent = template(e) if has_parent else 0
        if has_parent:
            template(e, parent)
        return parent, task(e, 0x60, 0x73)
    run(emu, 0x4883C0, "GuardTemplate_New", setup, check_eax=True)


@pytest.mark.parametrize("registered", [0, 1])
def test_activate(registered):
    run(emu, 0x488430, "GuardGenerator_Activate", lambda e: (guard(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_deactivate(registered):
    run(emu, 0x4884D0, "GuardGenerator_Deactivate", lambda e: (guard(e, registered=registered),))


@pytest.mark.parametrize("spawned", [0, 1])
@pytest.mark.parametrize("n", [0, 3])
def test_computer_event(spawned, n):
    def setup(e):
        g = guard(e, spawned=spawned)
        kids(e, g, n, 0x38, FWD)
        return g, 0x2468
    run(emu, 0x488560, "GuardGenerator_ComputerEvent", setup)


def test_get_type():
    def setup(e):
        u32(e, 0x541094, 0xABCD1234)
    run(emu, 0x488610, "GuardGenerator_GetType", setup, check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("has_tree", [0, 1])
def test_free_tree(has_tree):
    run(emu, 0x488140, "GuardGenerator_FreeTree", lambda e: (guard(e, tree=tree(e, 2) if has_tree else 0),))
