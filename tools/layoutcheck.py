# Checks igidecomp.exe's data layout against IGI.exe: every global named in src/addr.c sits at
# its address, and the initialised data (.igd) holds IGI.exe's bytes, except words that point at
# code, which must point at the rewrite of the function at that address (src HOOK lines).
# usage: layoutcheck.py IGI.exe igidecomp.exe   (run from the repo root; silent when it matches)
import glob
import re
import subprocess
import sys

import pefile

orig, exe = pefile.PE(sys.argv[1]), pefile.PE(sys.argv[2])
B = 0x400000
oimg, ximg = orig.get_memory_mapped_image(), exe.get_memory_mapped_image()
bad = 0

at = {n: int(a, 16) for n, a in re.findall(r'^AT\((\w+), (0x[0-9A-F]+)\)', open('src/addr.c').read(), re.M)}
sym = {}
for line in subprocess.run(['i686-w64-mingw32-nm', sys.argv[2]], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[2].startswith('_') and p[1].isupper():  # globals only
        sym[p[2][1:]] = int(p[0], 16)
for n, a in sorted(at.items(), key=lambda kv: kv[1]):
    if n in sym and sym[n] != a:
        print(f'{n} is at 0x{sym[n]:X}, not 0x{a:X}')
        bad += 1

# code addresses: original function -> our function's address in the exe
ours = {}
for f in glob.glob('src/**/*.c', recursive=True):
    for a, n in re.findall(r'^HOOK\((0x[0-9A-Fa-f]+), (\w+)\);', open(f).read(), re.M):
        if n in sym:
            ours[sym[n]] = int(a, 16)

# dinput.lib's object formats, which the exe carries as data instead of in its code range
for n, a, size in (('igi_dimouse', 0x4AE680, 0xB0), ('igi_dikeyboard', 0x4AE740, 0x1000), ('igi_dijoystick', 0x516FA0, 0x2C0)):
    if n in sym:
        for k in range(size):
            ours[sym[n] + k] = a + k

lo, hi = 0x533000, 0x550000
for a in range(lo, hi, 4):
    o, x = oimg[a - B:a - B + 4], ximg[a - B:a - B + 4]
    if o == x:
        continue
    xv, ov = int.from_bytes(x, 'little'), int.from_bytes(o, 'little')
    if ours.get(xv) == ov:
        continue
    print(f'0x{a:06X}: 0x{xv:08X}, IGI.exe has 0x{ov:08X}')
    bad += 1
    if bad > 50:
        break
sys.exit(1 if bad else 0)
