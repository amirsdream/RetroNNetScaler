# AI sharpening / scaling → hardware path

Aligned with [VISION.md](VISION.md): **1080p primary**, clarity first, optional regen, **transformers on PC**.  
FPGA lab: [DE10-Nano](DE10_NANO.md) (replaceable).

## Output

- **Ship target:** 1920×1080 @ 60 Hz, letterboxed native aspect  
- Native (e.g. 256×240) → AI/integer scale → compose into 1080p frame  
- 4K is not required for v1  

## Model tracks

| Track | Intent | Where it runs |
|---|---|---|
| **Clarity CNN (TinySR)** | Honest sharp upscale | PC now + **FPGA** |
| **Transformer SR** | Max clarity / Regen research | **PC first** (PyTorch / ONNX / D3D later) |
| **DLSS** | Closed PC reference | PC only — [DLSS_EVAL.md](DLSS_EVAL.md) |
| **NIS** | Non-ML baseline | PC |

### Why transformers are in the plan

- Windowed / Swin-style SR often beats tiny CNNs on structure and “regen” detail  
- Good fit for **pixel-art domain training** (NES/SNES crops → 1080p targets)  
- Lets us measure the real quality ceiling before freezing FPGA topology  

### Why transformers are not the DE10 default

- Attention needs large working sets vs 3×3 line buffers  
- Harder int8 realtime mapping on ~112 DSP Cyclone V  
- Path: **train transformer on PC** → either (a) distill to TinySR-sized CNN for FPGA, or (b) hybrid NPU for Regen  

## Clarity CNN (TinySR — FPGA-friendly)

PC mode `ScaleMode::TinySR` today:

1. Conv 3×3 RGB→8, ReLU  
2. Conv 3×3 8→8, ReLU  
3. Expand + pixel-shuffle **4×**  
4. Present into **1080p** letterbox (bilinear/point for remaining scale if needed)  

Weights: placeholders now; export via `tools/export_tinysr_weights.py`.

## Transformer track (PC research)

Suggested shape (not implemented yet):

1. Offline PyTorch: SwinIR-class or tiny ViT-SR, trained on retro frames  
2. Inference: ONNX Runtime / DirectML / CUDA → 1080p  
3. Wire into `4kscaler` as future `--scale transformer` (optional)  
4. Compare A/B vs TinySR / NIS on same ROM  
5. If quality wins enough: distill student CNN for FPGA Clarity; keep teacher for Regen on PC/hybrid  

Keep patches/windows aligned with scanline/tile thinking so distillation stays FPGA-realistic.

## Experiment ladder

1. PC: Nearest / Bilinear / NIS / TinySR → **1080p**  
2. **Train TinySR on dumped ROM frames** — see `tools/train/README.md`  
   - Dump: `--dump-frames N`  
   - Teacher: EPX 4× (hard colors)  
   - Factors: L1, edge-L1, palette drift, stroke variance (`eval_factors.py`)  
   - Runtime: `--scale neural` (shader mode 2 + `tiny_sr_weights.inc`)  
3. Add transformer SR prototype on PC (Regen/Clarity ceiling)  
4. int8 TinySR golden → DE10 HDMI ~1080p  
5. 16-bit cores; reuse 1080p pipeline  
6. Distill or hybrid — only then buy bigger silicon for on-device transformers  

## Success

- 1080p Clarity clearly better than mushy scales  
- Transformer path exists on PC for regen/max quality thinking  
- FPGA ships CNN Clarity without waiting on full attention engines  
