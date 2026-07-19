# RetroNNetScaler

Realtime retro scaler lab: classic **8-bit / 16-bit** frames → high-clarity (and later optional **AI regeneration**) output.

**Vision:** [docs/VISION.md](docs/VISION.md) — **4K** quality target with [best-practice integer Clarity](docs/BEST_UPSCALE.md); transformers for Regen research; FPGA replaceable.

## Current prototype

- **Core:** NES via embedded [SimpleNES](https://github.com/amhndu/SimpleNES)  
- **Present:** Direct3D 11 on Windows  
- **Next systems:** SNES / Genesis-class cores after NES Clarity is strong  

### Scaling modes

| Mode | Kind | Notes |
|---|---|---|
| Nearest / Bilinear | Classic | Baselines / Pure-leaning |
| NIS | Spatial (NVIDIA) | Non-ML upscaler |
| **Clarity (tinysr)** | Max integer + luma unsharp | Best-practice 4K path — [docs/BEST_UPSCALE.md](docs/BEST_UPSCALE.md) |
| **Smooth** | Sharp xBR | Hard-palette diagonals |
| **Neural** | TinySR 9-class CNN | Scale2xSFX teacher; discrete neighbors — no cream ([tools/train](tools/train)) |
| DLSS | Proprietary AI ref | PC only; falls back to TinySR — [docs/DLSS_EVAL.md](docs/DLSS_EVAL.md) |

### Hardware

| Stage | Board |
|---|---|
| Now | PC |
| Lab FPGA (buy later) | [DE10-Nano](docs/DE10_NANO.md) — Clarity TinySR |
| Later | Larger FPGA or hybrid if Regen / 4K60 needs it |

Scaffold: [`fpga/de10_nano/`](fpga/de10_nano/).

## Requirements

- Windows 10/11  
- CMake 3.16+  
- C/C++ toolchain (LLVM-MinGW or MSVC)  
- Direct3D 11 GPU  

## Build

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build -j
```

## Run

Provide your own legally obtained `.nes` ROM:

```bat
python tools\gen_test_rom.py
build\4kscaler.exe --rom roms\smb.nes --scale neural --res 4k
```

### Controls

| Input | Action |
|---|---|
| WASD | D-pad |
| J / K | A / B |
| Enter / RShift | Start / Select |
| F1 or 1 | Cycle scale modes |
| `[` / `]` | Sharpness |
| V | Vsync |
| F11 | Fullscreen |
| Esc | Quit |

Prefs: `%APPDATA%\4kscaler\prefs.ini`.

## Docs

- [docs/VISION.md](docs/VISION.md) — product north star (8/16-bit, clarity, regen)  
- [docs/AI_FPGA_PATH.md](docs/AI_FPGA_PATH.md) — AI → hardware  
- [docs/DE10_NANO.md](docs/DE10_NANO.md) — lab board (replaceable)  
- [docs/DLSS_EVAL.md](docs/DLSS_EVAL.md) — DLSS as PC reference only  
- [tools/train/README.md](tools/train/README.md) — Neural TinySR training (CUDA)
