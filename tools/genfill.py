# Writes src/exe/fill.c for igidecomp.exe: IGI.exe's data at its original addresses, for every
# byte no C definition covers. Definitions sit in sections .igd$XXXXXX (initialised data,
# 0x533000-0x550000) and .igb$XXXXXX (zeroed, 0x550000-0xC35000) named after the address they
# start at; the linker sorts them by name, and fill.c fills the gaps between them.
# usage: genfill.py IGI.exe objdir > src/exe/fill.c   (objdir: the exe's object files)
import glob
import os
import re
import subprocess
import sys

import pefile

pe = pefile.PE(sys.argv[1])
img = pe.get_memory_mapped_image()
B = pe.OPTIONAL_HEADER.ImageBase
assert B == 0x400000 and pe.FILE_HEADER.TimeDateStamp == 0x3A1D74C6, 'needs the EU IGI.exe'

SECTIONS = (('igd', 0x533000, 0x550000), ('igb', 0x550000, 0xC35000))

# the definitions' sections, from every object but fill.c's own
runs = {name: [] for name, _, _ in SECTIONS}
objs = [o for o in glob.glob(os.path.join(sys.argv[2], '**', '*.obj'), recursive=True)
        if not o.endswith('fill.c.obj')]
out = subprocess.run(['i686-w64-mingw32-objdump', '-h'] + objs, capture_output=True, text=True).stdout
for m in re.finditer(r'^\s*\d+ \.(ig[db])\$([0-9A-F]{6})\s+([0-9a-f]+)', out, re.M):
    runs[m[1]].append((int(m[2], 16), int(m[3], 16)))

print('/* IGI.exe\'s data that no definition covers yet, at its original address (see')
print('   tools/genfill.py, which writes this file from the EU IGI.exe; do not edit). */')
print('#include <stdint.h>\n')
# data that dinput.lib left in .text: the object formats of the mouse, keyboard and joystick
# data formats, which point here instead
for name, addr, n in (('dimouse', 0x4AE680, 0xB0), ('dikeyboard', 0x4AE740, 0x1000), ('dijoystick', 0x516FA0, 0x2C0)):
    b = img[addr - B:addr - B + n]
    print(f'const uint8_t igi_{name}[0x{n:X}] = {{')
    for i in range(0, n, 16):
        print('    ' + ', '.join(f'0x{x:02X}' for x in b[i:i + 16]) + ',')
    print('};\n')

for sec, lo, hi in SECTIONS:
    at = lo
    for a, n in sorted(runs[sec]) + [(hi, 0)]:
        if a < at:
            sys.exit(f'.{sec}${a:06X} overlaps the data before it (which ends at 0x{at:X})')
        if a > at:
            if sec == 'igb' or not img[at - B:a - B].strip(b'\0'):
                print(f'static uint8_t fill_{at:06X}[0x{a - at:X}] __attribute__((section(".{sec}${at:06X}"), used));')
            else:
                b = img[at - B:a - B]
                print(f'static uint8_t fill_{at:06X}[0x{a - at:X}] __attribute__((section(".{sec}${at:06X}"), used)) = {{')
                for i in range(0, len(b), 16):
                    print('    ' + ', '.join(f'0x{x:02X}' for x in b[i:i + 16]) + ',')
                print('};')
        at = a + n
