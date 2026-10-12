#!/usr/bin/env python3
# Regenerates the divide-by-zero guards in battletanx.us.rev0.toml: the block
# between the "BEGIN GENERATED DIVISION GUARDS" and "END GENERATED DIVISION
# GUARDS" lines.
#
# N64Recomp turns div/divu into a plain C division, which on x86 traps on a
# zero divisor (and IDO code checks the divisor only after dividing). So each
# division is replaced: an instruction patch turns it into a nop, and a hook
# at the same address does the division only when the divisor is non-zero,
# giving the R4300's results otherwise (LO = -1 or 1 by the dividend's sign
# for div, all ones for divu, HI = the dividend).
#
# Run from the repo root after any change to the symbol file:
#   python3 tools/gen_div_guards.py
import re, struct

ROM = 'BattleTanx (USA).z64'
SYMS = 'BattleTanxSyms/battletanx.us.rev0.syms.toml'
TOML = 'battletanx.us.rev0.toml'
SYMBOL_LISTS = 'lib/N64ModernRuntime/N64Recomp/src/symbol_lists.cpp'
BEGIN = '# --- BEGIN GENERATED DIVISION GUARDS (tools/gen_div_guards.py) ---\n'
END = '# --- END GENERATED DIVISION GUARDS ---\n'

rom = open(ROM, 'rb').read()
toml = open(TOML).read()

def word(v):
    o = v - 0x80070000
    return struct.unpack('>I', rom[o:o + 4])[0]

# Functions N64Recomp won't recompile: its own lists, and our stubs/ignored.
skip = set()
lists = open(SYMBOL_LISTS).read()
for name in ('reimplemented_funcs', 'ignored_funcs'):
    m = re.search(name + r'[^{]*\{(.*?)\};', lists, re.S)
    skip |= set(re.findall(r'"(\w+)"', m.group(1)))
# Renamed functions are recompiled under name + "_recomp".
m = re.search(r'renamed_funcs[^{]*\{(.*?)\};', lists, re.S)
renamed = set(re.findall(r'"(\w+)"', m.group(1)))
for key in ('stubs', 'ignored'):
    m = re.search(r'^' + key + r' = \[(.*?)\]', toml, re.M | re.S)
    if m:
        skip |= set(re.findall(r'"(\w+)"', m.group(1)))

R = lambda n: f'ctx->r{n}'
out = []
count = 0
for m in re.finditer(r'name = "(\w+)", vram = (0x[0-9A-Fa-f]+), size = (0x[0-9A-Fa-f]+)', open(SYMS).read()):
    name, vram, size = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
    if name in skip:
        continue
    if name in renamed:
        name += '_recomp'
    for a in range(vram, vram + size, 4):
        w = word(a)
        if w >> 26 != 0 or (w >> 6) & 0x3FF != 0:
            continue
        funct = w & 0x3F
        rs, rt = (w >> 21) & 31, (w >> 16) & 31
        if funct == 0x1A:    # div
            text = (f'if (S32({R(rt)}) != 0) {{ lo = S32(S64(S32({R(rs)})) / S64(S32({R(rt)}))); '
                    f'hi = S32(S64(S32({R(rs)})) % S64(S32({R(rt)}))); }} '
                    f'else {{ lo = S32({R(rs)}) < 0 ? 1 : -1; hi = S32({R(rs)}); }}')
        elif funct == 0x1B:  # divu
            text = (f'if (U32({R(rt)}) != 0) {{ lo = S32(U32({R(rs)}) / U32({R(rt)})); '
                    f'hi = S32(U32({R(rs)}) % U32({R(rt)})); }} '
                    f'else {{ lo = -1; hi = S32({R(rs)}); }}')
        elif funct == 0x1E:  # ddiv
            text = (f'if (S64({R(rt)}) != 0) {{ DDIV(S64({R(rs)}), S64({R(rt)}), (int64_t*)&lo, (int64_t*)&hi); }} '
                    f'else {{ lo = S64({R(rs)}) < 0 ? 1 : -1; hi = {R(rs)}; }}')
        elif funct == 0x1F:  # ddivu
            text = (f'if (U64({R(rt)}) != 0) {{ DDIVU(U64({R(rs)}), U64({R(rt)}), (uint64_t*)&lo, (uint64_t*)&hi); }} '
                    f'else {{ lo = -1; hi = {R(rs)}; }}')
        else:
            continue
        count += 1
        out.append(f'[[patches.instruction]]\nfunc = "{name}"\nvram = 0x{a:08X}\nvalue = 0x00000000\n\n'
                   f'[[patches.hook]]\nfunc = "{name}"\nbefore_vram = 0x{a:08X}\ntext = """\n    {text}\n"""\n\n')

block = BEGIN + '\n' + ''.join(out) + END
if BEGIN in toml:
    toml = toml[:toml.index(BEGIN)] + block + toml[toml.index(END) + len(END):]
else:
    toml = toml.rstrip('\n') + '\n\n' + block
open(TOML, 'w').write(toml)
print(count, 'divisions guarded')
