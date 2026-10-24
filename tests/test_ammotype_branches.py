"""GunShot_MoveBullets (0x47B750): axis aligned normals and directions."""
import pytest

from test_ammotype_bullets import f, run, simple

CASES = {
    "ricochet_y_normal": dict(hits=[{"t": 0.3, "normal": (0.0, f(0.7), 0.0)}], frand=[0.0], ricochet=100.0),
    "ricochet_z_normal": dict(hits=[{"t": 0.3, "normal": (0.0, 0.0, f(1.2))}], frand=[0.0], ricochet=100.0),
    "ricochet_yz_path": dict(hits=[{"t": 0.3, "normal": (0.0, 0.0, 1.0)}], frand=[0.0], ricochet=100.0,
                             d=(0.0, 0.8, 0.6)),
    "ricochet_z_path": dict(hits=[{"t": 0.3, "normal": (0.0, 0.0, 1.0)}], frand=[0.0], ricochet=100.0,
                            d=(0.0, 0.0, -1.0)),
    "ricochet_no_path": dict(hits=[{"t": 0.3, "normal": (0.0, 1.0, 0.0)}], frand=[0.0], ricochet=100.0,
                             d=(0.0, 0.0, 0.0)),
    "ricochet_nothing": dict(hits=[{"t": 0.3, "normal": (0.0, 0.0, 0.0)}], frand=[0.0], ricochet=100.0,
                             d=(0.0, 0.0, 0.0)),
    "impact_y_normal": dict(hits=[{"t": 0.3, "normal": (0.0, f(0.7), 0.0)}], frand=[0.99]),
    "impact_z_normal": dict(hits=[{"t": 0.3, "normal": (0.0, 0.0, f(1.2))}], frand=[0.99]),
}


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", sorted(CASES))
def test_case(case, cw):
    run(simple(**CASES[case]), cw)
