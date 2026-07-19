#!/usr/bin/env python3
"""Build LQ/HQ pairs from dumped NES RGBA frames + teacher upscale."""

from __future__ import annotations

import argparse
import json
import random
from pathlib import Path

import numpy as np
from PIL import Image
from tqdm import tqdm

from teacher_xbr import neighbor_labels, upscale_teacher

W, H = 256, 240


def load_rgba(path: Path) -> np.ndarray:
    raw = np.frombuffer(path.read_bytes(), dtype=np.uint8)
    need = W * H * 4
    if raw.size < need:
        raise ValueError(f"short frame {path} ({raw.size} < {need})")
    return raw[:need].reshape(H, W, 4)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw", type=Path, default=Path("data/nes/raw"))
    ap.add_argument("--out", type=Path, default=Path("data/nes"))
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--teacher", choices=("sfx", "epx", "nearest"), default="sfx")
    ap.add_argument("--val-ratio", type=float, default=0.15)
    ap.add_argument("--stride", type=int, default=2, help="keep every Nth frame")
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()

    frames = sorted(args.raw.glob("frame_*.rgba"))
    frames = frames[:: max(1, args.stride)]
    if not frames:
        raise SystemExit(f"No frames in {args.raw}. Run: 4kscaler --rom roms/smb.nes --dump-frames 400")

    random.seed(args.seed)
    random.shuffle(frames)
    n_val = max(1, int(len(frames) * args.val_ratio))
    splits = {"val": frames[:n_val], "train": frames[n_val:]}

    for split, paths in splits.items():
        lq_dir = args.out / split / "lq"
        hq_dir = args.out / split / "hq"
        idx_dir = args.out / split / "idx"
        lq_dir.mkdir(parents=True, exist_ok=True)
        hq_dir.mkdir(parents=True, exist_ok=True)
        idx_dir.mkdir(parents=True, exist_ok=True)
        meta = []
        for i, p in enumerate(tqdm(paths, desc=f"teacher-{split}")):
            rgba = load_rgba(p)
            lq = rgba[..., :3].astype(np.float32) / 255.0
            hq = upscale_teacher(lq, scale=args.scale, kind=args.teacher)
            labels = neighbor_labels(lq, hq, scale=args.scale)
            hq_u8 = np.clip(np.rint(hq * 255.0), 0, 255).astype(np.uint8)
            lq_u8 = np.clip(np.rint(lq * 255.0), 0, 255).astype(np.uint8)
            stem = f"{i:05d}"
            Image.fromarray(lq_u8, mode="RGB").save(lq_dir / f"{stem}.png")
            Image.fromarray(hq_u8, mode="RGB").save(hq_dir / f"{stem}.png")
            np.save(idx_dir / f"{stem}.npy", labels.astype(np.uint8))
            meta.append(
                {
                    "lq": f"lq/{stem}.png",
                    "hq": f"hq/{stem}.png",
                    "idx": f"idx/{stem}.npy",
                    "src": p.name,
                }
            )
        (args.out / split / "meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
        print(f"{split}: {len(meta)} pairs @ {args.scale}x teacher={args.teacher}")


if __name__ == "__main__":
    main()
