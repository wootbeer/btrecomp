#!/usr/bin/env python3
# Disassemble a range of BattleTanx (USA) by VRAM.
# Usage: python3 tools/disasm.py <vram_hex> [count_or_end_hex] [rom]
import struct, sys
import rabbitizer as rz
rom = open(sys.argv[3] if len(sys.argv) > 3 else 'BattleTanx (USA).z64', 'rb').read()
start = int(sys.argv[1], 16)
arg = sys.argv[2] if len(sys.argv) > 2 else '32'
end = int(arg, 16) if arg.lower().startswith('0x8') else start + 4 * int(arg, 0)
for v in range(start, end, 4):
    w = struct.unpack('>I', rom[v - 0x80070000:v - 0x80070000 + 4])[0]
    print(f'{v:08X} {w:08X}  {rz.Instruction(w, v).disassemble()}')
