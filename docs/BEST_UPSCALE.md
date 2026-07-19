# Best practices: retro pixel art → 4K

This is the Clarity path we implement in `TinySR` / `--scale tinysr` when the window is 4K.

## What “best” means for NES/SNES (not photos)

Photoreal SR (Lanczos, bicubic, many CNN photo models) **softens** pixel art. For 8/16-bit:

| Do | Don't |
|---|---|
| Uniform **integer** scale | Fractional bilinear/bicubic as primary |
| Preserve aspect with **letterbox** | Stretch to fill 16:9 |
| Sharpen **luma only**, clamp halos | RGB contrast blowouts |
| Point-sample after Clarity | Bilinear the Clarity result again |
| Optional edge-directed (xBRZ) later | Soft “AI glow” without a GT |

## Phase lock (OSSC-style)

Soft float letterboxing causes **uneven pixel widths** (sampling phase error) — the same class of artifact OSSC users tune with sampler phase.

We lock phase by:

1. `s = min(⌊bbW/srcW⌋, ⌊bbH/srcH⌋)` — uniform integer scale only  
2. Content size exactly `srcW·s × srcH·s`  
3. **Integer-centered viewport** on the backbuffer (no fractional NDC stretch)  
4. Point sampling inside that viewport  

Arrow keys nudge `phaseX/phaseY` by 1 output pixel (like OSSC H/V phase). `0` resets.

## Pipeline (implemented)

```
256×240
  → s = min(⌊W/256⌋, ⌊H/240⌋)     e.g. 4K: s = min(15,9) = 9
  → nearest to (256s)×(240s)        e.g. 2304×2160
  → optional bilateral luma unsharp only if sharpness > 0
  → integer-centered viewport in 3840×2160   (+ optional phase nudge)
```

### Anti-blockiness (“perfect draw”)

| Mode | Look |
|---|---|
| **Clarity** | Pure integer nearest — authentic blocks |
| **Smooth** | Paper stack for **geometric diagonals**, hard colors only |

**Smooth** = **Hyllian xBR-lv3** (same family RetroArch uses): YUV edge weights,
multi-slope wedges (1:1 / 2:1 / 1:2), default **noblend** (exact neighbor colors).
A 4080 is far more than enough — quality is algorithmic, not a GPU limit.

`F1` → Smooth(xBR). `[`/`]` optional thin edge AA (0 = hard palette).


At **1080p**: `s = min(7,4) = 4` → 1024×960 letterboxed.  
At **4K**: `s = 9` → much denser integer grid than fixed 4× then stretch.

## Why this beats “4× then stretch to 4K”

Fixed 4× (1024×960) then scaling to ~2304×2160 **reintroduces** a non-integer (or soft) step.  
Max-integer-into-canvas does **one** crisp enlarge, then only translation into the 4K frame.

## Next upgrades (research order)

1. **xBRZ / ScaleFx** pre-pass before integer (edge-directed, still discrete)  
2. **Learned ΔY residual** on top of integer oracle (FPGA-shaped TinySR v2)  
3. **Transformer Regen** on PC for optional hallucinated detail  
4. Correct NES **PAR** (8:7) as a toggle if you want CRT-accurate width  

## CLI

```bat
build\4kscaler.exe --rom roms\smb.nes --scale tinysr --res 4k
```

`F3` cycles 1080 ↔ 1440 ↔ 4K.  
`F1` cycles scaler mode. `[`/`]` unsharp amount.
