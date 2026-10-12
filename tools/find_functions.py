#!/usr/bin/env python3
# Function discovery for BattleTanx (USA) by control-flow traversal.
#
# Each function's extent is everything reachable from its entry, following
# branches, intra-function jumps and jump tables until every path returns
# (jr $ra), tail-calls, or stops. A function therefore can't swallow the one
# after it (the "merged function" crashes of the sequel's port, which came from
# extents guessed from the gap to the next known symbol).
#
# Entries come from: the boot entry point, jal targets, function pointers
# (words in the data segment and lui/addiu pairs in code that point into
# .text, minus jump-table entries), and, last, clean code in the gaps that
# nothing references (dead code or calls we can't see statically).
#
# Writes a JSON list of {vram, size, end, kind} plus a report of anything
# odd: overlaps, pointers into the middle of a function, unresolved jr.
#
# Usage: python3 tools/find_functions.py <rom.z64> <out.json> [report.txt]
import json, struct, sys
import rabbitizer as rz

ROM_BASE = 0x80070000           # rom offset = vram - ROM_BASE
TEXT = (0x80071000, 0x80125550)  # code (with embedded RSP microcode and data)
DATA = (0x80125550, 0x80147040)  # data/rodata, then BSS

rom = open(sys.argv[1], 'rb').read()

def word(v):
    o = v - ROM_BASE
    return struct.unpack('>I', rom[o:o + 4])[0]

def in_text(v):
    return TEXT[0] <= v < TEXT[1] and v % 4 == 0

_dec = {}
def dec(v):
    i = _dec.get(v)
    if i is None:
        i = rz.Instruction(word(v), v)
        _dec[v] = i
    return i

def se16(x):
    return x - 0x10000 if x & 0x8000 else x

GPR_RA = 31

class Fail(Exception):
    pass

def find_jump_table(func_start, jr_addr, reg):
    """IDO switch: sltiu at,t,N; beqz at,default; sll t,t,2; then the table
    address (lui/addiu, possibly set up long before) is added to the index and
    loaded: lw t,%lo(T)(at); jr t. Returns (table_vram, count) or None."""
    a = jr_addr - 4
    lw = None
    while a >= max(func_start, jr_addr - 0x40):
        i = dec(a)
        if i.getOpcodeName() == 'lw' and i.rt.value == reg:
            lw = i
            break
        a -= 4
    if lw is None:
        return None
    # Constant propagation, in address order, over the code before the lw.
    # addu of a constant and a non-constant (the scaled index) keeps the
    # constant, which is all we need to recover the table base.
    const = {}
    stack = {}  # constants spilled to the stack frame
    b = max(func_start, lw.vram - 0x800)
    while b <= lw.vram:
        i = dec(b)
        n = i.getOpcodeName()
        w = word(b)
        if b == lw.vram:
            if lw.rs.value == 29 and se16(w & 0xFFFF) in stack:
                const[29] = stack[se16(w & 0xFFFF)] - se16(w & 0xFFFF)
            break
        if n == 'sw' and i.rs.value == 29:
            if i.rt.value in const:
                stack[se16(w & 0xFFFF)] = const[i.rt.value]
            else:
                stack.pop(se16(w & 0xFFFF), None)
        elif n == 'lw' and i.rs.value == 29 and se16(w & 0xFFFF) in stack:
            const[i.rt.value] = stack[se16(w & 0xFFFF)]
        elif n == 'lui':
            const[i.rt.value] = (w & 0xFFFF) << 16
        elif n == 'addiu' and i.rs.value in const:
            const[i.rt.value] = (const[i.rs.value] + se16(w & 0xFFFF)) & 0xFFFFFFFF
        elif n == 'ori' and i.rs.value in const:
            const[i.rt.value] = const[i.rs.value] | (w & 0xFFFF)
        elif n == 'addu' and (i.rs.value in const) != (i.rt.value in const):
            const[i.rd.value] = const[i.rs.value if i.rs.value in const else i.rt.value]
        else:
            for f in ('modifiesRd', 'modifiesRt'):
                if getattr(i, f)():
                    r = i.rd.value if f == 'modifiesRd' else i.rt.value
                    const.pop(r, None)
            if i.doesLink():
                for r in list(const):
                    if r not in (16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30):
                        const.pop(r)
        b += 4
    if lw.rs.value not in const:
        return None
    table = (const[lw.rs.value] + se16(word(lw.vram) & 0xFFFF)) & 0xFFFFFFFF
    if not (TEXT[0] <= table < DATA[1]):
        return None
    # bound from the sltiu check
    count = None
    b = lw.vram - 4
    while b >= max(func_start, lw.vram - 0x80):
        i = dec(b)
        if i.getOpcodeName() == 'sltiu':
            count = word(b) & 0xFFFF
            break
        b -= 4
    if count is None:
        # fallback: read entries while they look like code addresses near here
        count = 0
        while count < 512:
            t = word(table + 4 * count)
            if not (in_text(t) and abs(t - jr_addr) < 0x4000):
                break
            count += 1
    return table, count

def traverse(start, starts, jtables):
    """Returns (set of instruction addresses, list of call targets, list of
    tail-call targets, list of issues). Raises Fail if it hits an invalid
    instruction (so it isn't code)."""
    seen = set()
    calls, tails, issues = [], [], []
    work = [start]
    def visit_delay(a):
        if not in_text(a):
            raise Fail(f'delay slot out of text at {a:08X}')
        i = dec(a)
        if not i.isValid():
            raise Fail(f'invalid delay slot at {a:08X}')
        seen.add(a)
    while work:
        a = work.pop()
        while True:
            if a in seen:
                break
            if not in_text(a):
                raise Fail(f'ran out of text at {a:08X}')
            i = dec(a)
            if not i.isValid():
                raise Fail(f'invalid instruction {word(a):08X} at {a:08X}')
            name = i.getOpcodeName()
            if name == 'break':
                # a trap (IDO's divide-by-zero check, or the one after the
                # entry point's call): never part of a function's normal flow
                break
            seen.add(a)
            if name in ('eret',):
                break
            if i.isJumpWithAddress():  # j / jal
                t = i.getInstrIndexAsVram()
                visit_delay(a + 4)
                if i.doesLink():          # jal
                    calls.append(t)
                    a += 8
                    continue
                # j: tail call if it targets a known start other than us, or
                # leaves backwards past our entry
                if t in starts and t != start or t < start:
                    tails.append(t)
                    break
                work.append(t)
                break
            if i.isBranch():
                t = i.getBranchVramGeneric()
                visit_delay(a + 4)
                if i.doesLink():          # bal / bgezal: a call
                    calls.append(t)
                    a += 8
                    continue
                if t < start or (t in starts and t != start):
                    # branch-as-tail-call (rare, but IDO does emit `b func`)
                    tails.append(t)
                else:
                    work.append(t)
                if i.isUnconditionalBranch():
                    break
                a += 8
                continue
            if name == 'jr':
                visit_delay(a + 4)
                if i.rs.value == GPR_RA:
                    break
                jt = find_jump_table(start, a, i.rs.value)
                if jt is None:
                    issues.append(f'unresolved jr ${i.rs.value} at {a:08X}')
                    break
                table, count = jt
                jtables[table] = (a, count)
                for k in range(count):
                    t = word(table + 4 * k)
                    if not in_text(t):
                        issues.append(f'jump table {table:08X}[{k}] -> {t:08X} outside text')
                        continue
                    work.append(t)
                break
            if name == 'jalr':
                visit_delay(a + 4)
                a += 8
                continue
            a += 4
    return seen, calls, tails, issues

def is_padding(v):
    return word(v) == 0

# Opcodes this game's compiler doesn't emit in ordinary code; a "function"
# starting with one is a data word that happens to decode.
UNLIKELY = {'lld', 'ld', 'sd', 'ldl', 'ldr', 'sdl', 'sdr', 'ldc2', 'sdc2', 'lwc2',
            'swc2', 'cache', 'll', 'sc', 'scd', 'lwu', 'syscall', 'eret',
            'mtc0', 'mfc0', 'dmtc0', 'dmfc0', 'teq', 'tne', 'tlt', 'tltu', 'tge',
            'tgeu', 'teqi', 'tnei', 'tlti', 'tltiu', 'tgei', 'tgeiu', 'daddi',
            'daddiu', 'dsll', 'dsrl', 'dsra', 'dsllv', 'dsrlv', 'dsrav', 'dsll32',
            'dsrl32', 'dsra32', 'dadd', 'daddu', 'dsub', 'dsubu', 'dmult',
            'dmultu', 'ddiv', 'ddivu'}

def looks_like_entry(a, limit):
    """addiu $sp,$sp,-N, or a small leaf of ordinary instructions reaching
    jr $ra soon."""
    w = word(a)
    if (w >> 16) == 0x27BD and (w & 0x8000):
        return True
    for k in range(24):
        b = a + 4 * k
        if b >= limit:
            return False
        i = dec(b)
        if not i.isValid() or i.getOpcodeName() in UNLIKELY:
            return False
        if word(b) == 0x03E00008:
            return True
    return False

def main():
    funcs = {}        # start -> dict(seen, calls, tails, issues, kind)
    failed = {}
    jtables = {}
    report = []
    kinds = {}

    data_ptrs = {}
    for v in range(DATA[0], DATA[1], 4):
        t = word(v)
        if in_text(t):
            data_ptrs.setdefault(t, []).append(v)

    def jt_entries():
        e = set()
        for table, (_, count) in jtables.items():
            for k in range(count):
                e.add(word(table + 4 * k))
        return e

    def coverage():
        c = {}
        for s, f in funcs.items():
            for a in f['seen']:
                c.setdefault(a, []).append(s)
        return c

    def accept(s, kind, speculative=False):
        if s in funcs or s in failed:
            return False
        try:
            seen, calls, tails, issues = traverse(s, set(funcs) | {s}, jtables)
        except Fail as e:
            failed[s] = (kind, str(e))
            return False
        if kind == 'gap' and any(dec(a).getOpcodeName() in UNLIKELY for a in seen):
            failed[s] = (kind, 'unlikely opcode')
            return False
        if speculative:
            cov = coverage()
            if any(a in cov for a in seen):
                failed[s] = (kind, 'overlaps an existing function')
                return False
        funcs[s] = dict(seen=seen, calls=calls, tails=tails, issues=issues)
        kinds[s] = kind
        return True

    def close_calls():
        # every call/tail target of an accepted function is a function
        changed = True
        while changed:
            changed = False
            for f in list(funcs.values()):
                for t in f['calls'] + f['tails']:
                    if in_text(t) and t not in funcs and t not in failed:
                        if accept(t, 'call'):
                            changed = True

    def retraverse():
        # tail calls are only recognised against known starts, so redo every
        # function once the set of starts has grown
        while True:
            starts = set(funcs)
            changed = False
            for s in sorted(funcs):
                seen, calls, tails, issues = traverse(s, starts, jtables)
                if seen != funcs[s]['seen']:
                    changed = True
                funcs[s].update(seen=seen, calls=calls, tails=tails, issues=issues)
            close_calls()
            if not changed and set(funcs) == starts:
                return

    accept(TEXT[0], 'entry')
    close_calls()
    # definite: pointers stored in the data segment
    for rnd in range(5):
        n = len(funcs)
        cov = coverage()
        jte = jt_entries()
        for t in sorted(data_ptrs):
            if t not in funcs and t not in jte and t not in cov:
                accept(t, 'data-ptr')
        close_calls()
        retraverse()
        if len(funcs) == n:
            break

    # speculative: lui/addiu constants into .text (run to a fixed point), then
    # clean code in the gaps. Gaps go last, so they never claim a fragment of
    # a function that stronger evidence would find later.
    def code_ptr_pass():
        cov = coverage()
        jte = jt_entries()
        found = False
        for f in list(funcs.values()):
            lui = {}
            for a in sorted(f['seen']):
                i = dec(a); nm = i.getOpcodeName()
                if nm == 'lui':
                    lui[i.rt.value] = (word(a) & 0xFFFF) << 16
                elif nm in ('addiu', 'ori') and i.rs.value in lui:
                    imm = word(a) & 0xFFFF
                    val = (lui[i.rs.value] + (se16(imm) if nm == 'addiu' else imm)) & 0xFFFFFFFF
                    if in_text(val) and val not in jte and val not in cov and val not in funcs \
                            and looks_like_entry(val, TEXT[1]):
                        if accept(val, 'code-ptr', speculative=True):
                            found = True
                            cov = coverage()
        return found

    def gap_pass():
        ends = sorted((min(f['seen']), max(f['seen']) + 4) for f in funcs.values())
        gaps, cur = [], TEXT[0]
        for s, e in ends:
            if s > cur:
                gaps.append((cur, s))
            cur = max(cur, e)
        if cur < TEXT[1]:
            gaps.append((cur, TEXT[1]))
        for g0, g1 in gaps:
            a = g0
            while a < g1:
                if is_padding(a) or a in failed or a in funcs:
                    a += 4
                    continue
                # an entry follows a return: jr $ra, its delay slot, then
                # any zero padding
                b = a - 4
                while b > g0 - 0x40 and word(b) == 0:
                    b -= 4
                boundary = word(b) == 0x03E00008 or word(b - 4) == 0x03E00008
                if boundary and dec(a).isValid() and dec(a).getOpcodeName() not in UNLIKELY \
                        and accept(a, 'gap', speculative=True):
                    return True
                a += 4
        return False

    while True:
        while code_ptr_pass():
            close_calls()
            retraverse()
        if not gap_pass():
            break
        close_calls()
        retraverse()

    # a speculative entry that ended up sharing code with a real function
    # was a misread (a data word just before a function, for instance)
    cov = coverage()
    for s in sorted(funcs):
        if kinds[s] in ('gap', 'code-ptr') and any(len(cov[a]) > 1 for a in funcs[s]['seen']):
            del funcs[s]
            cov = coverage()

    # extents and consistency
    jte = jt_entries()
    cov = coverage()
    order = sorted(funcs)
    for s in order:
        f = funcs[s]
        lo, hi = min(f['seen']), max(f['seen']) + 4
        if lo != s:
            report.append(f'{s:08X}: reaches below its entry, to {lo:08X}')
        f['end'] = hi
    for s in order:
        f = funcs[s]
        shared = sorted({o for a in f['seen'] for o in cov[a] if o != s})
        if shared:
            report.append(f'{s:08X}-{f["end"]:08X} shares instructions with ' + ', '.join(f'{o:08X}' for o in shared))
        for msg in f['issues']:
            report.append(f'{s:08X}: {msg}')
    prev = None
    for s in order:
        if prev is not None and s < funcs[prev]['end']:
            report.append(f'extent overlap: {prev:08X}-{funcs[prev]["end"]:08X} and {s:08X}')
        if prev is None or funcs[s]['end'] > funcs[prev]['end']:
            prev = s
    for t, refs in sorted(data_ptrs.items()):
        if t in funcs or t in jte:
            continue
        if t in cov:
            report.append(f'data pointer to {t:08X} (from {refs[0]:08X}) lands inside {cov[t][0]:08X}')
    for s, (kind, why) in sorted(failed.items()):
        if kind == 'call':
            report.append(f'call target {s:08X} rejected: {why}')
    out = [dict(vram=s, size=funcs[s]['end'] - s, kind=kinds[s]) for s in order]
    json.dump(out, open(sys.argv[2], 'w'), indent=0)
    if len(sys.argv) > 3:
        open(sys.argv[3], 'w').write('\n'.join(report) + '\n')
    from collections import Counter
    print(len(out), 'functions', dict(Counter(o['kind'] for o in out)), len(jtables), 'jump tables,', len(report), 'report lines')

main()
