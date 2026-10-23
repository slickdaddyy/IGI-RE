from emu import Emu, compare

emu = Emu()


def test_get_weapon_select():
    compare(emu, 0x45E3C0, "HumanPlayerInput_GetWeaponSelect")
