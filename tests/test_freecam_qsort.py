"""The C runtime qsort, its insertion sort and swap (0x5321F0 - 0x5323C0)."""
import random
import struct

import pytest

from test_freecam_util import new_emu, run, u32

e = new_emu()

# comparator: logs (a, b) at *LOGP, then returns sign(*(int *)a - *(int *)b)
LOGP = e.alloc(4)
CODE = e.fake_top
e.fake_top += 0x40
e.write(CODE, b"\x8b\x44\x24\x04" b"\x8b\x4c\x24\x08" b"\x8b\x15" + struct.pack("<I", LOGP) +
        b"\x89\x02" b"\x89\x4a\x04" b"\x83\xc2\x08" b"\x89\x15" + struct.pack("<I", LOGP) +
        b"\x8b\x00" b"\x8b\x09" b"\x31\xd2" b"\x39\xc8" b"\x0f\x9f\xc2" b"\x0f\x9c\xc0" b"\x0f\xb6\xc0"
        b"\x29\xc2" b"\x89\xd0" b"\xc3")
e.baseline = e.snapshot()


def array(em, rng, num, width, kind):
    if kind == "random":
        keys = [rng.randrange(-50, 50) for _ in range(num)]
    elif kind == "sorted":
        keys = sorted(rng.randrange(-1000, 1000) for _ in range(num))
    elif kind == "reversed":
        keys = sorted((rng.randrange(-1000, 1000) for _ in range(num)), reverse=True)
    elif kind == "equal":
        keys = [7] * num
    else:
        keys = [rng.choice([0, 1]) for _ in range(num)]
    data = b"".join(struct.pack("<i", k) + bytes(rng.randrange(256) for _ in range(width - 4)) for k in keys)
    base = em.alloc(max(len(data), 4))
    em.write(base, data)
    log = em.alloc(0x40000)
    u32(em, LOGP, log)
    return base


@pytest.mark.parametrize("width", [4, 8, 13])
@pytest.mark.parametrize("num", [0, 1, 2, 3, 7, 8, 9, 10, 17, 64, 300])
@pytest.mark.parametrize("kind", ["random", "sorted", "reversed", "equal", "two"])
def test_qsort(kind, num, width):
    def setup(em):
        rng = random.Random(num * 31 + width)
        return array(em, rng, num, width, kind), num, width, CODE
    run(e, 0x5321F0, "crt_qsort", setup)


def test_qsort_zero_width():
    def setup(em):
        return array(em, random.Random(1), 5, 4, "random"), 5, 0, CODE
    run(e, 0x5321F0, "crt_qsort", setup)


@pytest.mark.parametrize("width", [4, 6])
@pytest.mark.parametrize("num", [1, 2, 5, 9])
def test_shortsort(num, width):
    def setup(em):
        base = array(em, random.Random(num + width), num, width, "random")
        return base, base + width * (num - 1), width, CODE
    run(e, 0x532344, "crt_shortsort", setup)


@pytest.mark.parametrize("width", [0, 1, 4, 7])
@pytest.mark.parametrize("same", [0, 1])
def test_swap(width, same):
    def setup(em):
        a = em.alloc(16)
        em.write(a, bytes(range(16)))
        b = a if same else em.alloc(16)
        if not same:
            em.write(b, bytes(range(100, 116)))
        return a, b, width
    run(e, 0x532392, "crt_swap", setup)
