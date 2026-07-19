# DE10-Nano FPGA workspace

**Board:** Terasic DE10-Nano (Cyclone V SoC) — purchase later. **Lab only; replaceable** if Clarity/Regen outgrows it ([docs/VISION.md](../../docs/VISION.md)).  
**Algorithm on this board:** Clarity TinySR — not heavy Regen ([docs/AI_FPGA_PATH.md](../../docs/AI_FPGA_PATH.md), [docs/DE10_NANO.md](../../docs/DE10_NANO.md)).

## Status

Scaffold only. No Quartus project yet.

| Dir | Purpose |
|---|---|
| `notes/` | Clocks, HDMI, HPS↔FPGA bridge notes |
| `rtl/` | Future SystemVerilog (TinySR + video out) |
| `hps/` | Future Linux/ARM frame feeder |

## When the board arrives

1. Install Quartus Prime Lite + Terasic GHRD  
2. Bring up HDMI with Terasic demo  
3. Replace video path with our 256×240 → TinySR → HDMI pipeline  
4. Compare frames to PC `4kscaler --scale tinysr`

## Golden reference (PC)

```bat
build\4kscaler.exe --rom roms\colorbars.nes --scale tinysr
```
