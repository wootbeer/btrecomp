# BattleTanxSyms

`battletanx.us.rev0.syms.toml` is the symbol file N64Recomp reads: every
function's start and size in the game's one code section.

It is generated, not hand-edited:

```bash
python3 -m venv .venv && . .venv/bin/activate && pip install rabbitizer
python3 tools/find_functions.py "BattleTanx (USA).z64" syms/functions.json report.txt
python3 tools/make_syms.py syms/functions.json
python3 tools/gen_div_guards.py     # the config's division guards follow the symbols
```

## How the functions are found

`tools/find_functions.py` follows control flow from each function's entry:
branches, jumps, jump tables (`jr` through a table found by constant
propagation, including through a stack slot) until every path returns,
tail-calls or stops. A function's size is what it actually reaches, so it
can't swallow the function after it. That was the sequel's main source of
crashes (btgarecomp STATUS.md, "Merged functions"): its symbol sizes came
from the gap to the next known symbol, so functions called only through
pointers were merged into the one before and failed at runtime with "Failed
to find function at 0x...".

Entries, strongest evidence first:
1. the entry point and every `jal` target, transitively;
2. pointers into the code stored in the data segment, unless they are
   jump-table entries or land inside a function already found;
3. `lui`/`addiu` constants that point at code (function pointers passed
   around), if the target looks like a function entry;
4. clean code in what's left, right after a `jr $ra` and its delay slot:
   functions nothing visibly calls (dead code, or calls through pointers
   computed at runtime).

Steps 3-4 are speculative: such a function is dropped if it overlaps one
found otherwise, and the gap scan rejects code with opcodes this compiler
doesn't emit (64-bit, COP0, traps). After every new entry, all functions are
re-traversed, since a `j` to a known entry is a tail call but a `j` to
anywhere else is a jump inside the function.

The report lists anything left unexplained. Currently: the exception
preamble at `0x80111690` (`jr $k0`; N64Recomp skips it by name).

## Names

`syms/library_names.txt` names the library functions (libultra, libaudio):
n64sym's signature matches, restricted to the library region and corrected
where it contradicted itself. N64Recomp replaces or skips libultra functions
by name, so these must be right; the previous BattleTanx recompilation
mislabelled several (for instance `osInvalDCache` as `osWritebackDCache` and
`__osEnqueueAndYield` as `__osDispatchThread`), which swaps in the wrong
runtime code. Everything else is `func_<address>`.
