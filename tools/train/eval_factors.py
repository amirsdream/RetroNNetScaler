#!/usr/bin/env python3
"""Compare nearest / teacher / TinySR classifier — accuracy, L1, stroke, palette."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F
from PIL import Image

from model import TinySR
from teacher_xbr import colors_from_labels, neighbor_labels, upscale_teacher


def load_rgb(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255.0


def sobel(img: np.ndarray) -> np.ndarray:
    t = torch.from_numpy(img).permute(2, 0, 1).unsqueeze(0)
    gray = t.mean(dim=1, keepdim=True)
    kx = torch.tensor([[1, 0, -1], [2, 0, -2], [1, 0, -1]], dtype=torch.float32).view(1, 1, 3, 3)
    ky = torch.tensor([[1, 2, 1], [0, 0, 0], [-1, -2, -1]], dtype=torch.float32).view(1, 1, 3, 3)
    gx = F.conv2d(gray, kx, padding=1)
    gy = F.conv2d(gray, ky, padding=1)
    return torch.sqrt(gx * gx + gy * gy + 1e-6).squeeze().numpy()


def palette_drift(pred: np.ndarray, lq: np.ndarray, scale: int) -> float:
    """Fraction of HQ pixels not equal to any of 9 LQ neighbors (should be 0)."""
    h, w = lq.shape[:2]
    pad = np.pad(lq, ((1, 1), (1, 1), (0, 0)), mode="edge")
    bad = 0
    total = h * w * scale * scale
    for y in range(h):
        for x in range(w):
            n = []
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    n.append(pad[1 + y + dy, 1 + x + dx])
            n = np.stack(n, 0)
            block = pred[y * scale : (y + 1) * scale, x * scale : (x + 1) * scale].reshape(-1, 3)
            for px in block:
                if not np.any(np.all(np.abs(n - px) < 1e-5, axis=1)):
                    bad += 1
    return bad / max(1, total)


def stroke_var(edge: np.ndarray) -> float:
    mask = edge > np.percentile(edge, 85)
    if mask.sum() < 10:
        return 0.0
    return float(np.var(edge[mask]))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, default=Path("data/nes/val"))
    ap.add_argument("--ckpt", type=Path, default=Path("tools/train/runs/tinysr_smb_cls/best.pt"))
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--out", type=Path, default=Path("tools/train/runs/tinysr_smb_cls/eval.json"))
    ap.add_argument("--preview", type=Path, default=Path("tools/train/runs/tinysr_smb_cls/preview"))
    args = ap.parse_args()

    meta = json.loads((args.data / "meta.json").read_text(encoding="utf-8"))
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    ckpt = torch.load(args.ckpt, map_location=device, weights_only=False)
    model = TinySR(
        channels=int(ckpt["channels"]),
        scale=int(ckpt["scale"]),
        num_classes=int(ckpt.get("num_classes", 9)),
    ).to(device)
    model.load_state_dict(ckpt["model"], strict=False)
    model.eval()

    args.preview.mkdir(parents=True, exist_ok=True)
    rows = []
    for i, m in enumerate(meta[:12]):
        lq = load_rgb(args.data / m["lq"])
        hq_teacher = load_rgb(args.data / m["hq"])
        nearest = upscale_teacher(lq, args.scale, kind="nearest")
        gt_labels = neighbor_labels(lq, hq_teacher, args.scale)

        with torch.no_grad():
            t = torch.from_numpy(lq).permute(2, 0, 1).unsqueeze(0).to(device)
            logits = model.forward_logits(t)
            raw = logits.argmax(dim=1).squeeze(0)
            best = logits.max(dim=1).values.squeeze(0)
            vE = logits[:, 4].squeeze(0)
            pred_labels = torch.where(best < vE + 0.35, torch.full_like(raw, 4), raw).cpu().numpy()
            raw_np = raw.cpu().numpy()
            pred = model(t).squeeze(0).permute(1, 2, 0).cpu().numpy()

        edge = gt_labels != 4
        row = {
            "id": m["lq"],
            "acc_all": float(np.mean(pred_labels == gt_labels)),
            "acc_edge": float(np.mean(pred_labels[edge] == gt_labels[edge])) if edge.any() else 1.0,
            "acc_edge_raw": float(np.mean(raw_np[edge] == gt_labels[edge])) if edge.any() else 1.0,
            "nearest_l1_vs_teacher": float(np.mean(np.abs(nearest - hq_teacher))),
            "neural_l1_vs_teacher": float(np.mean(np.abs(pred - hq_teacher))),
            "nearest_edge_l1": float(np.mean(np.abs(sobel(nearest) - sobel(hq_teacher)))),
            "neural_edge_l1": float(np.mean(np.abs(sobel(pred) - sobel(hq_teacher)))),
            "neural_palette_drift": palette_drift(pred, lq, args.scale),
            "teacher_stroke_var": stroke_var(sobel(hq_teacher)),
            "neural_stroke_var": stroke_var(sobel(pred)),
            "nearest_stroke_var": stroke_var(sobel(nearest)),
        }
        rows.append(row)

        # preview strip: nearest | neural | teacher
        strip = np.concatenate([nearest, pred, hq_teacher], axis=1)
        Image.fromarray(np.clip(strip * 255, 0, 255).astype(np.uint8)).save(
            args.preview / f"{i:02d}.png"
        )

    means = {k: float(np.mean([r[k] for r in rows])) for k in rows[0] if k != "id"}
    out = {"means": means, "rows": rows}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(json.dumps(means, indent=2))
    print(f"wrote {args.out}")
    print(f"previews {args.preview}")


if __name__ == "__main__":
    main()
