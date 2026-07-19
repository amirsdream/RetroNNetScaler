# DLSS evaluation (PC AI reference)

## Role in this project

We want **AI** sharpening/scaling experiments. DLSS is the **proprietary PC reference** for neural temporal upscaling quality.

It is **not** the FPGA target. See [AI_FPGA_PATH.md](AI_FPGA_PATH.md) for the portable TinySR track.

## Constraints on NES

DLSS Super Resolution expects:

- low-resolution color
- motion vectors
- depth (plus jitter / exposure constants in full integrations)

A NES PPU gives **color only**. Without MVs/depth, DLSS quality is usually weak (ghosting, unstable edges) on pixel art.

## Verdict

| Question | Answer |
|---|---|
| Default ship scaler? | **NIS** (spatial) or **TinySR** (open AI) |
| Worth experimenting on RTX PC? | **Yes** — quality ceiling / temporal behavior reference |
| Implement on FPGA later? | **No** for DLSS itself; use TinySR topology instead |

## Streamline spike (optional)

When NVIDIA Streamline + NGX DLSS libs are installed:

1. CMake: `-D4KSCALER_ENABLE_STREAMLINE=ON` (hook reserved; not required for TinySR)
2. Tag color + zero (or scroll-estimated) MVs + clear depth each frame
3. Screenshot A/B: Nearest / NIS / TinySR / DLSS on the same ROM frame
4. Record whether temporal AI beats TinySR enough to justify MV heuristics

Until Streamline is linked, the UI **DLSS** mode shows a notice and falls back to **TinySR** (AI path) rather than abandoning neural scaling.
