#!/usr/bin/env python3
"""Generate a tiny public-domain NROM that fills the screen with a color bars pattern."""

from pathlib import Path

# 6502 program placed at $8000 (16KiB PRG, mapper 0).
# Waits for PPU warm-up, enables rendering, fills nametable with tile 1, sets palette, loops.
prg = bytearray([0xEA] * 0x4000)  # NOP fill

code = bytes(
    [
        0x78,  # SEI
        0xD8,  # CLD
        0xA2,
        0x40,  # LDX #$40
        # wait_vblank1
        0xAD,
        0x02,
        0x20,  # LDA $2002
        0x10,
        0xFB,  # BPL -5
        # wait_vblank2
        0xAD,
        0x02,
        0x20,
        0x10,
        0xFB,
        # set palette
        0xA9,
        0x3F,  # LDA #$3F
        0x8D,
        0x06,
        0x20,  # STA $2006
        0xA9,
        0x00,
        0x8D,
        0x06,
        0x20,  # STA $2006
        0xA9,
        0x0F,  # universal bg
        0x8D,
        0x07,
        0x20,
        0xA9,
        0x16,
        0x8D,
        0x07,
        0x20,
        0xA9,
        0x27,
        0x8D,
        0x07,
        0x20,
        0xA9,
        0x30,
        0x8D,
        0x07,
        0x20,
        # fill nametable $2000 with tile 1
        0xA9,
        0x20,
        0x8D,
        0x06,
        0x20,
        0xA9,
        0x00,
        0x8D,
        0x06,
        0x20,
        0xA2,
        0x00,  # X=0
        0xA0,
        0x04,  # Y=4 pages (1024 bytes)
        # fill
        0xA9,
        0x01,
        0x8D,
        0x07,
        0x20,
        0xE8,
        0xD0,
        0xF8,  # BNE fill
        0x88,
        0xD0,
        0xF5,  # DEY / BNE
        # enable NMI + BG
        0xA9,
        0x80,
        0x8D,
        0x00,
        0x20,  # PPUMASK? actually PPUCTRL
        0xA9,
        0x0A,
        0x8D,
        0x01,
        0x20,  # PPUMASK show bg
        # forever
        0x4C,
        0x00,
        0x80,  # JMP $8000 warm restart of loop end — patch below
    ]
)

# Place code at start of PRG ($8000)
prg[0 : len(code)] = code
# Fix forever jump to spin in place at end of setup
forever_off = len(code) - 3
prg[forever_off] = 0x4C
prg[forever_off + 1] = forever_off & 0xFF
prg[forever_off + 2] = 0x80

# Vectors at end of 16KB bank ($FFFA)
prg[0x3FFA] = 0x00  # NMI -> $8000
prg[0x3FFB] = 0x80
prg[0x3FFC] = 0x00  # RESET -> $8000
prg[0x3FFD] = 0x80
prg[0x3FFE] = 0x00  # IRQ
prg[0x3FFF] = 0x80

# CHR: tile 0 blank, tile 1 solid
chr_rom = bytearray(0x2000)
# tile 1 at offset 16: 8 bytes plane0 = 0xFF, plane1 = 0
for row in range(8):
    chr_rom[16 + row] = 0xFF

header = bytearray(16)
header[0:4] = b"NES\x1a"
header[4] = 1  # 1 x 16KB PRG
header[5] = 1  # 1 x 8KB CHR
header[6] = 0  # mapper 0, horizontal
header[7] = 0

out = Path(__file__).resolve().parents[1] / "roms" / "colorbars.nes"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(header + prg + chr_rom)
print(f"Wrote {out} ({out.stat().st_size} bytes)")
