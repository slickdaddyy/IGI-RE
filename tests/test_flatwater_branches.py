"""FlatWater_Render (0x51C5B0): degenerate views and depths."""
import pytest

from test_flatwater_util import CWS, add_child, d64, f32, fake_emu, project, pyfake, run, task, u16, u32, view, with_cw
from test_menu_util import strip_logger

LAYER = 0x35

emu = fake_emu({})
pyfake(emu, 0x498000, project)
strip_logger(emu)
emu.baseline = emu.snapshot()

SCREEN = (0.0, 0.0, 640.0, 480.0, 320.0, 240.0)


def water(e, layers, height=-1000.0, reach=30000.0, cam=(1234.5, -987.25, 0.0)):
    d64(e, 0xBCAB08, *cam)
    f32(e, 0xA7DA18, 0.004)
    w = task(e, 0x90)
    d64(e, w + 0x70, height)
    f32(e, w + 0x78, reach)
    f32(e, w + 0x80, 0.1, 0.3, 0.5)
    for i, (loaded, scale) in enumerate(layers):
        t = task(e, 0x26C, LAYER)
        u32(e, t + 0x44, loaded)
        f32(e, t + 0x248, scale, 0.01, -0.02, 0.25 + 0.1 * i, 0.75, 0.6, 0.9, 0.8, 0.7 - 0.1 * i)
        add_child(e, w, task(e, 0x40, 0x34))
        add_child(e, w, t)
    return w


def setup_for(m, screen=SCREEN, scale=(300.0, 300.0), layers=((0, 0.0003), (1, 0.00007)), **kw):
    """m: the nine view matrix floats (right, up, forward rows)."""
    def setup(e):
        u16(e, 0xBA2018, LAYER)
        f32(e, 0xBCAAE0, *m)
        u32(e, 0xBCAB04, 7)
        f32(e, 0xBCAB28, *scale)
        f32(e, 0xBCABB8, *screen)
        return (water(e, layers, **kw),)
    return setup


# the far edge is in front by less than the smallest float: the depth passes
# in extended precision and fails once stored
TINY = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1e-20, 1e-20, 0.0]
# rolled a quarter turn, forward level: the first row is a vertical line
ROLLED = [0.0, 0.0, 1.0, 0.0, 1.0, 0.0, -1.0, 0.0, 0.0]
ROLLED2 = [0.0, 0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0]
# no forward row: the ray through the screen centre is zero
NOFWD = [0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0]

CASES = [
    (TINY, dict(reach=1e-30)),
    (ROLLED, dict(reach=1.0)),
    (ROLLED, dict(reach=1.0, height=5000.0)),
    (ROLLED2, dict(reach=2.0)),
    (ROLLED2, dict(reach=0.5, height=800.0)),
    (NOFWD, dict(screen=(0.0, 0.0, 640.0, 480.0, 0.0, 0.0))),
    (NOFWD, dict(screen=(20.0, 10.0, 600.0, 400.0, 0.0, 0.0), height=3000.0)),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(CASES)))
def test_render_degenerate(cw, case):
    m, kw = CASES[case]
    run(emu, with_cw(emu, 0x51C5B0, cw), with_cw(emu, "FlatWater_Render", cw), setup_for(m, **kw))


# a point that lies exactly on the first row's line, and two first row points
# at the same height, found by searching views at the default control word
EDGE_CASES = [
    ([-0.33849617871959886, -0.940967766181302, 0.0, 0.46164197337122537, -0.1660673718472799,
      -0.8713829906703815, 0.8199433062194912, -0.29495981254318, 0.49060338723896096],
     dict(screen=(88.16263781548847, 0.0, 640.0, 479.9, -12.824398004704122, -38.22779959260346),
          scale=(107.62916786140167, 663.1565321810182), height=-1000.0, reach=30000.0)),
    ([0.4717884598213077, -0.8817117721678884, 0.0, 0.36207923954844473, 0.19374223204462784,
      -0.9117908596878922, 0.803936734741894, 0.4301724053712967, 0.41065487722613797],
     dict(screen=(0.1, 0.0, 479.9, 586.6971629363055, -50.64932786049301, 0.8296722488942123),
          scale=(773.2000634342601, 415.3107727100075), height=-1000.0, reach=32.104601115389)),
    ([-0.9999193648597376, -0.012698967615469837, 0.0, 0.0026627129543219247, -0.20966257468409707,
      -0.9777701748041604, 0.012416671785210314, -0.9776913321689708, 0.2096794822185559],
     dict(screen=(117.84044057658852, 0.0, 254.476469360176, 480.0, 689.9403056962607, 156.86929316362802),
          scale=(845.0207233495152, 198.08572005468554), height=5000.0, reach=52.000791059247014)),
    ([0.9970120037944608, 0.07724677527090855, 0.0, -0.019247692808879124, 0.2484269499729901,
      -0.9684593831692956, -0.07481036433068129, 0.9655656302071669, 0.24917147339000809],
     dict(screen=(0.1, 0.0, 479.9, 413.93337276321796, 413.40700614453976, 48.714819396142815),
          scale=(820.5782752219301, 868.649156604852), height=5000.0, reach=30000.0)),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(EDGE_CASES)))
def test_render_on_edge(cw, case):
    m, kw = EDGE_CASES[case]
    run(emu, with_cw(emu, 0x51C5B0, cw), with_cw(emu, "FlatWater_Render", cw), setup_for(m, **kw))


# views (yaw, pitch, roll) whose outline needs the side points and the
# interpolated points, found by searching
VIEW_CASES = [
    ((-2.3400688376806325, 1.1117879581991446, 0.8440787807251651),
     dict(height=-15283.251853157559, reach=23612.36772975712, cam=(2887.2335113551317, -4061.4041322576513, 0.0)),
     (10.0, 20.0, 300.0, 200.0, 160.0, 100.0), (566.3222968424712, 702.0839847757677)),
    ((-1.495484413073539, 0.9658443743887477, 0.0),
     dict(height=-1000.0, reach=30000.0, cam=(4391.491627785106, -1187.9576231178758, 0.0)),
     SCREEN, (498.48500220803845, 699.145845861766)),
    ((0.04939040327242328, 1.3125921886040106, -0.9924808668287156),
     dict(height=3503.224245742378, reach=42309.90168193195, cam=(890.0225798255169, -4654.741698486584, 0.0)),
     (10.0, 20.0, 300.0, 200.0, 160.0, 100.0), (615.0319933671973, 362.05525671302485)),
    ((-2.3486911215928012, -1.0670318386319213, 0.12226796657963246),
     dict(height=-500.0, reach=18405.526348682575, cam=(-3860.8708760726986, -2648.1048861578247, 0.0)),
     (0.0, 0.0, 640.0, 480.0, 604.1567354392172, 374.1791258608369), (300.0, 300.0)),
    ((-0.2702767421360299, -0.7130151484553654, 0.9184469233930521),
     dict(height=-1000.0, reach=30000.0, cam=(3261.1998830035373, 2898.782122136916, 0.0)),
     (0.0, 0.0, 640.0, 480.0, 120.2701389295261, 377.04582448539196), (300.0, 300.0)),
    ((2.641023171643412, 0.9545515706067089, -0.28435221018929324),
     dict(height=-500.0, reach=24227.257036957602, cam=(103.26452891439658, -2291.7952753394156, 0.0)),
     SCREEN, (300.0, 300.0)),
    ((-2.6173586754746, -1.5116067528805246, 0.4400416405275278),
     dict(height=5000.0, reach=33150.87682169697, cam=(-1701.9501603347799, -2320.2683054212625, 0.0)),
     SCREEN, (300.0, 300.0)),
    ((-0.7272676175291073, 1.2972460914227542, -0.956159802266993),
     dict(height=-500.0, reach=30000.0, cam=(2887.116652701967, 1235.5673564291956, 0.0)),
     (0.0, 0.0, 640.0, 480.0, 551.0707258509367, 49.37252286158285), (608.6900081101567, 836.9409199981834)),
]


def view_setup(angles, kw, screen, scale):
    def setup(e):
        u16(e, 0xBA2018, LAYER)
        view(e, *angles)
        f32(e, 0xBCABB8, *screen)
        f32(e, 0xBCAB28, *scale)
        return (water(e, ((0, 0.0003), (1, 0.00007)), **kw),)
    return setup


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(VIEW_CASES)))
def test_render_outline(cw, case):
    run(emu, with_cw(emu, 0x51C5B0, cw), with_cw(emu, "FlatWater_Render", cw), view_setup(*VIEW_CASES[case]))
