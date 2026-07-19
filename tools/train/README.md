# TinySR training (NES → 4× neighbor classifier)

Neural mode learns which of 9 LQ neighbors to write per HQ pixel (Scale2xSFX teacher). Hard palette — no silky/cream colors. Train on CUDA GPU.


## Pipeline

```bat
:: 1) Dump frames from current ROM
build\4kscaler.exe --rom roms\smb.nes --dump-frames 400 --dump-dir data\nes\raw --res 1080 --mute

:: 2) Python env
cd tools\train
python -m pip install -r requirements.txt

:: 3) Build LQ/HQ + neighbor-index labels (Scale2xSFX teacher)
python build_dataset.py --raw ..\..\data\nes\raw --out ..\..\data\nes --scale 4 --teacher sfx --stride 1

:: 4) Train 9-class classifier on NVIDIA CUDA GPU
python train_tinysr.py --data ..\..\data\nes --out runs\tinysr_smb_cls --epochs 80 --batch 48 --channels 32

:: 5) Measure factors
python eval_factors.py --ckpt runs\tinysr_smb_cls\best.pt --data ..\..\data\nes\val

:: 6) Export weights for the app
python export_weights.py --ckpt runs\tinysr_smb_cls\best.pt --bin ..\..\build\tinysr_weights.bin
```

Then relaunch with `--scale neural` (shader compiles at runtime; needs matching `tinysr_weights.bin`).

## What we measure (other factors)

| Factor | Why it matters |
|---|---|
| **L1 vs teacher** | Overall match to smooth diagonals |
| **Edge L1** | Contour / diagonal quality |
| **Palette drift** | Cream / dirty colors (should stay low) |
| **Stroke variance** | Uneven letter/sprite thickness |

Ablate in `train_tinysr.py` with `--w-l1 --w-edge --w-palette`.

## Notes

- Teacher is **EPX 4×** (hard colors). Swap `--teacher nearest` for ablation.
- Model is residual on nearest-up — designed to stay sharp and FPGA-small (8 ch).
- Neural runtime = shader `mode=2` + `tiny_sr_weights.inc`.
