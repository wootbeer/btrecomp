#!/usr/bin/env python3
# Lists loops that may wait on another thread without ever giving the
# scheduler a chance: a backward branch whose body makes no call and stores
# nothing, but loads from memory (or loads nothing at all, spinning on a
# register). On the N64 an interrupt would let another thread run; on the
# cooperative runtime such a loop hangs unless something in it yields.
# Usage: python3 tools/scan_spin_loops.py
import re, struct
import rabbitizer as rz
rom = open('BattleTanx (USA).z64', 'rb').read()
def word(v):
    o = v - 0x80070000
    return struct.unpack('>I', rom[o:o + 4])[0]
syms = open('BattleTanxSyms/battletanx.us.rev0.syms.toml').read()
for m in re.finditer(r'name = "(\w+)", vram = (0x[0-9A-Fa-f]+), size = (0x[0-9A-Fa-f]+)', syms):
    name, v, size = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
    if not name.startswith('func_'):
        continue
    for a in range(v, v + size, 4):
        i = rz.Instruction(word(a), a)
        if not i.isBranch():
            continue
        t = i.getBranchVramGeneric()
        if not (v <= t <= a):
            continue
        body = [rz.Instruction(word(b), b) for b in range(t, a + 8, 4)]
        names = [x.getOpcodeName() for x in body]
        if any(x.isFunctionCall() or x.getOpcodeName() in ('jal', 'jalr') for x in body):
            continue
        if any(x.doesStore() for x in body):
            continue
        loads = [x for x in body if x.doesLoad()]
        kind = 'register spin' if not loads else 'memory poll'
        print(f'{name} loop {t:08X}-{a:08X} ({len(body)} instr): {kind}')
