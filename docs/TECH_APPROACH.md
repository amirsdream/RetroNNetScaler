# Technical approach (for collaboration)

You have an image-processing background — here is what we are actually doing, what was wrong, and what to build next.

## Product constraint (reminder)

| Item | Choice |
|---|---|
| Content | 8-bit → 16-bit pixel art (NES first) |
| Output | **4K60** quality target (best-practice integer path); 1080p for lighter / FPGA lab |
| Modes | Pure / Clarity / Regen (optional) |
| FPGA later | Own ops + weights; DE10-Nano lab; board replaceable |
| Not product core | DLSS (no MVs; closed stack) |

## What exists on PC today

```
NES core → 256×240 RGBA → D3D11 upload → scaler mode → letterbox present
```

| Mode | Algorithm class | Status |
|---|---|---|
| Nearest / Bilinear | Classical resampling | Working baselines |
| NIS | NVIDIA spatial (fixed kernel + adaptive sharpen) | Working |
| TinySR / Clarity | Max **integer** scale into canvas + luma-only unsharp | See [BEST_UPSCALE.md](BEST_UPSCALE.md) |
| DLSS | Label only → falls back to **NIS** | Not linked |

### Honest status of “TinySR”

It is **not** a trained ESPCN/CNN right now. Recent iterations were:

1. Random-weight pseudo-CNN → too dark  
2. Bilinear + residual → soft  
3. Nearest + Scale2x-like corners + strong RGB unsharp → **ghosting + oversaturation** (what you see)

So for Clarity we need a real, specified algorithm — not more shader thrash.

## Failure modes you observed (latest)

| Symptom | Likely cause in last shader |
|---|---|
| Ghosting | Diagonal “Scale2x” color copy + border `c+(c−n)·gain` bleeding into neighbors |
| Oversaturation | Sharpen / contrast in **RGB** (chroma inflated with luma) |

**Corrective principle:** resample crisp (nearest or edge-directed), sharpen **luma only**, keep chroma from the base sample; clamp residual.

## Proposed Clarity stack (what we should implement)

Ordered so each step is measurable (PSNR/SSIM optional; visual A/B mandatory).

### Stage A — Spec + golden (your call on kernels)

1. **Base upsample to 4× (1024×960)**  
   - Default: nearest (Pure-leaning Clarity)  
   - Optional: edge-directed (EPX / Scale2x / xBR-lite) with **strict** rules — no soft bilinear  
2. **Luma-only adaptive unsharp**  
   - `Y' = Y + k·(Y − Gaussian_σ(Y))` with `k` from local contrast / gradient  
   - Chroma `CbCr` or simply `RGB ← Y_new * (RGB_base / Y_base)`  
3. **Compose into 1080p** with **point sampling** (never bilinear the Clarity result again)  
4. Metrics folder: dump `nearest`, `clarity`, `nis` frames for the same ROM frame index  

### Stage B — Learnable Clarity (FPGA-shaped)

Train a **small residual network** that predicts **ΔY only** (or ΔRGB with chroma penalty):

- Topology: ESPCN-like or FSRCNN-tiny, **r=4**, width 8–16  
- Loss: e.g. `L1(ΔY) + λ_edge·edge + λ_chroma·‖ΔC‖` + optional adversarial later for Regen  
- Data: NES (then SNES) frames; downsample GT from integer-scaled captures or synthetic  
- Export: float → int8 tables → same HLSL / future RTL  

This is the path that can land on DE10-Nano.

### Stage C — Regen / transformers (PC research)

- Windowed transformer SR (SwinIR-class) trained on retro domain → **1080p**  
- Optional; quality ceiling; distill or hybrid for device  
- Not on DE10 day one  

## What I need from you (so we move forward cleanly)

Pick defaults (reply with letters is enough):

1. **Clarity base:** (a) nearest+luma-unsharp  (b) EPX/Scale2x then luma-unsharp  (c) you specify kernel  
2. **Training GT:** (a) integer-scale “oracle”  (b) external hi-res packs  (c) later  
3. **First metric:** (a) visual only  (b) SSIM/LPIPS on held-out frames  

I will implement Stage A to your choice, then Stage B scaffolding (PyTorch export → `tiny_sr` weights).

## Non-goals near-term

- Shipping real DLSS on NES  
- Stuffing a full transformer into DE10-Nano  
- RGB oversharpen “wow” filters that break palette feel  
