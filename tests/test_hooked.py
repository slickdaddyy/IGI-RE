import struct

from emu import Emu, compare_hooked

plain = Emu()
hooked = Emu()
hooked.install_hooks()


def script_table(e):
    e.call(0x4B0BA0)
    buckets = e.alloc(12 * 997)
    for i in range(997):
        b = buckets + 12 * i
        e.write(b, struct.pack("<III", b + 4, 0, b))
    t = e.alloc(8)
    e.write(t, struct.pack("<II", buckets, 997))
    e.write(0xA43E88, struct.pack("<I", t))


def test_config_register_script_symbols():
    compare_hooked(plain, hooked, 0x402FF0, setup=script_table)
