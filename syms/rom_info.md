# ROM identification

| Field | Value |
|---|---|
| Title | BattleTanx (USA), revision 1.0 |
| Internal name | `BATTLETANX` |
| Game code | `NBXE`, version 0 |
| Size | 8,388,608 bytes (8 MiB) |
| SHA-1 (`.z64`) | `535860d941738ac1210c20a9b80114fea0e0ff17` |
| XXH3-64 (`.z64`, the launcher's check) | `0x8e485e76e7448bf3` |
| Entry point | `0x80071000` |

## Layout

- ROM `0x1000` is loaded to `0x80071000` (rom offset = vram - `0x80070000`),
  `0xD6040` bytes: code to `0x80125550`, then data to `0x80147040`. BSS runs
  from there to `0x803C8880` (cleared by the entry point, `func_80071000`,
  which then calls `func_800779CC`). There are no overlays; the rest of the
  ROM is assets.
- Inside the code range: data right after the entry point (`0x8007103C` to
  `0x80076000`: jump tables, strings), and the RSP microcode text at the end
  of the game's own code (`rspboot` at `0x8010E160`, F3DEX at `0x8010E230`,
  `aspMain` at `0x8010F660`).
- Library code (libultra, libaudio, libgcc helpers) starts at `0x80110480`.
- RSP tasks: audio (type 2) is set up in `func_8007A5A4` with `aspMain` and
  data at `0x80145AA0`; graphics around `0x8007D59C` with F3DEX and data at
  `0x801452A0`.
