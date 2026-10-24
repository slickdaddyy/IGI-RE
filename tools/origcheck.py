# Every ORIG and FN must name the function HOOKed at its address (or an alias in src/exe/link.c
# to it): the standalone exe links them by name, so a mismatch calls the wrong function.
import pathlib, re, sys

src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'src')
hook = {}
for p in src.rglob('*.c'):
    if p.match('terrdraw/crt.c'):  # the exe takes the c runtime from msvcrt.dll instead
        continue
    for a, n in re.findall(r'^HOOK\((0x[0-9A-Fa-f]+),\s*(\w+)\)', p.read_text(), re.M):
        hook[int(a, 16)] = n
alias = dict(re.findall(r'^ALIAS\((\w+), (\w+)\);', (src / 'exe/link.c').read_text(), re.M))
bad = 0
for p in sorted(src.rglob('*.[ch]')):
    for a, n in re.findall(r'ORIG(?:_STD)?\((0x[0-9A-Fa-f]+),[^;]*?,\s*(\w+)\s*,\s*\(', p.read_text(), re.S):
        a = int(a, 16)
        if a in hook and alias.get(n, n) != hook[a]:
            print(f'{p.relative_to(src)}: ORIG 0x{a:X} {n} but the rewrite there is {hook[a]}')
            bad += 1
    for a, n in re.findall(r'\bFN\((0x[0-9A-Fa-f]+), (\w+)\)', p.read_text()):
        a = int(a, 16)
        if a in hook and hook[a] != n:
            print(f'{p.relative_to(src)}: FN 0x{a:X} {n} but the rewrite there is {hook[a]}')
            bad += 1
sys.exit(1 if bad else 0)
