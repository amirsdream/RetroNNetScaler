#!/usr/bin/env python3
"""Train TinySR 9-class neighbor classifier on CUDA (hard diagonals, no cream)."""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F
from PIL import Image
from torch.utils.data import DataLoader, Dataset

from model import TinySR, count_params, decode_labels
from teacher_xbr import neighbor_labels


class PairDataset(Dataset):
    def __init__(
        self,
        root: Path,
        crop: int = 0,
        scale: int = 4,
        augment: bool = False,
        repeats: int = 1,
    ):
        self.root = root
        self.crop = crop
        self.scale = scale
        self.augment = augment
        self.repeats = max(1, repeats)
        meta = json.loads((root / "meta.json").read_text(encoding="utf-8"))
        self.items = meta

    def __len__(self) -> int:
        return len(self.items) * self.repeats

    def __getitem__(self, idx: int):
        m = self.items[idx % len(self.items)]
        lq = np.asarray(Image.open(self.root / m["lq"]).convert("RGB"), dtype=np.float32) / 255.0
        if "idx" in m and (self.root / m["idx"]).exists():
            labels = np.load(self.root / m["idx"]).astype(np.int64)
        else:
            hq = np.asarray(Image.open(self.root / m["hq"]).convert("RGB"), dtype=np.float32) / 255.0
            labels = neighbor_labels(lq, hq, scale=self.scale)

        if self.augment:
            k = int(np.random.randint(0, 4))
            if k:
                lq = np.rot90(lq, k).copy()
                labels = np.rot90(labels, k).copy()
                # Rotate neighbor class ids with the image
                labels = _remap_labels_rot90(labels, k)
            if np.random.rand() < 0.5:
                lq = np.ascontiguousarray(lq[:, ::-1])
                labels = np.ascontiguousarray(labels[:, ::-1])
                labels = _remap_labels_flip_h(labels)
            if np.random.rand() < 0.5:
                lq = np.ascontiguousarray(lq[::-1, :])
                labels = np.ascontiguousarray(labels[::-1, :])
                labels = _remap_labels_flip_v(labels)

        if self.crop > 0:
            h, w = lq.shape[:2]
            ch = min(self.crop, h)
            cw = min(self.crop, w)
            y0, x0 = 0, 0
            # Prefer crops that contain diagonal/edge labels (~1% of pixels)
            for _ in range(8):
                y0 = np.random.randint(0, h - ch + 1) if h > ch else 0
                x0 = np.random.randint(0, w - cw + 1) if w > cw else 0
                lab = labels[
                    y0 * self.scale : (y0 + ch) * self.scale,
                    x0 * self.scale : (x0 + cw) * self.scale,
                ]
                if float((lab != 4).mean()) > 0.002 or np.random.rand() < 0.25:
                    break
            lq = lq[y0 : y0 + ch, x0 : x0 + cw]
            labels = labels[
                y0 * self.scale : (y0 + ch) * self.scale,
                x0 * self.scale : (x0 + cw) * self.scale,
            ]

        return (
            torch.from_numpy(np.ascontiguousarray(lq)).permute(2, 0, 1),
            torch.from_numpy(np.ascontiguousarray(labels)).long(),
        )


# Class layout: 0A 1B 2C / 3D 4E 5F / 6G 7H 8I
# numpy.rot90 k=1 = 90° CCW: (dr,dc)->(dc,-dr) on offsets → A->C->I->G->A, B->F->H->D->B
_ROT90 = np.array([2, 5, 8, 1, 4, 7, 0, 3, 6], dtype=np.int64)
_FLIP_H = np.array([2, 1, 0, 5, 4, 3, 8, 7, 6], dtype=np.int64)  # mirror cols
_FLIP_V = np.array([6, 7, 8, 3, 4, 5, 0, 1, 2], dtype=np.int64)  # mirror rows


def _remap_labels_rot90(labels: np.ndarray, k: int) -> np.ndarray:
    out = labels
    for _ in range(k % 4):
        out = _ROT90[out]
    return out


def _remap_labels_flip_h(labels: np.ndarray) -> np.ndarray:
    return _FLIP_H[labels]


def _remap_labels_flip_v(labels: np.ndarray) -> np.ndarray:
    return _FLIP_V[labels]


def resolve_device(force_cpu: bool) -> torch.device:
    if force_cpu:
        print("WARNING: --cpu set; training on CPU (slow)", flush=True)
        return torch.device("cpu")
    if not torch.cuda.is_available():
        raise RuntimeError(
            "CUDA GPU required for training, but torch.cuda.is_available() is False. "
            "Install a CUDA PyTorch build, or pass --cpu to override."
        )
    torch.backends.cudnn.benchmark = True
    torch.backends.cuda.matmul.allow_tf32 = True
    name = torch.cuda.get_device_name(0)
    props = torch.cuda.get_device_properties(0)
    print(
        f"GPU: {name}  VRAM={props.total_memory / (1024**3):.1f} GiB  "
        f"torch={torch.__version__} cuda={torch.version.cuda}",
        flush=True,
    )
    return torch.device("cuda")


def ce_masked(logits: torch.Tensor, labels: torch.Tensor, edge_weight: float) -> torch.Tensor:
    """CE with heavy weight on non-center (diagonal) labels — they are ~1% of pixels."""
    # Per-class weights: boost every non-E class
    cw = torch.ones(9, device=logits.device, dtype=logits.dtype)
    cw[torch.arange(9, device=logits.device) != 4] = edge_weight
    ce = F.cross_entropy(logits, labels, weight=cw, reduction="none")
    # Extra pixel mask so flat E doesn't dominate the mean
    w = torch.where(labels != 4, torch.full_like(ce, edge_weight), torch.ones_like(ce))
    return (ce * w).sum() / w.sum().clamp_min(1.0)


def train_one(args) -> Path:
    device = resolve_device(args.cpu)
    use_cuda = device.type == "cuda"
    train_ds = PairDataset(
        Path(args.data) / "train",
        crop=args.crop,
        scale=args.scale,
        augment=True,
        repeats=args.repeats,
    )
    val_ds = PairDataset(Path(args.data) / "val", crop=0, scale=args.scale, augment=False)
    loader_kw = {
        "num_workers": args.workers,
        "pin_memory": use_cuda,
        "persistent_workers": use_cuda and args.workers > 0,
    }
    if args.workers > 0:
        loader_kw["prefetch_factor"] = 2
    train_loader = DataLoader(train_ds, batch_size=args.batch, shuffle=True, **loader_kw)
    val_loader = DataLoader(val_ds, batch_size=1, shuffle=False, **loader_kw)

    model = TinySR(
        channels=args.channels,
        scale=args.scale,
        dropout=args.dropout,
        num_classes=9,
    ).to(device)
    p0 = next(model.parameters())
    if use_cuda and p0.device.type != "cuda":
        raise RuntimeError(f"model params on {p0.device}, expected cuda")

    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.wd)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=args.epochs, eta_min=args.lr * 0.05)

    print(
        f"device={device} params={count_params(model)} train={len(train_ds)} val={len(val_ds)} "
        f"batch={args.batch} workers={args.workers} mode=classifier9",
        flush=True,
    )
    print(
        f"reg: dropout={args.dropout} wd={args.wd} edge_w={args.edge_weight} early_stop={args.patience}",
        flush=True,
    )

    run_dir = Path(args.out)
    run_dir.mkdir(parents=True, exist_ok=True)
    best_val = 1e9
    bad = 0
    history = []

    for epoch in range(1, args.epochs + 1):
        model.train()
        tr = 0.0
        n = 0
        for lq, labels in train_loader:
            lq = lq.to(device, non_blocking=use_cuda)
            labels = labels.to(device, non_blocking=use_cuda)
            logits = model.forward_logits(lq)
            loss = ce_masked(logits, labels, args.edge_weight)
            opt.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            tr += float(loss.item()) * lq.size(0)
            n += lq.size(0)
        tr /= max(1, n)
        sched.step()

        model.eval()
        va_ce = 0.0
        va_acc = 0.0
        va_edge_acc = 0.0
        edge_n = 0
        with torch.no_grad():
            for lq, labels in val_loader:
                lq = lq.to(device, non_blocking=use_cuda)
                labels = labels.to(device, non_blocking=use_cuda)
                logits = model.forward_logits(lq)
                va_ce += float(F.cross_entropy(logits, labels).item())
                pred = logits.argmax(dim=1)
                va_acc += float((pred == labels).float().mean().item())
                edge = labels != 4
                if edge.any():
                    va_edge_acc += float((pred[edge] == labels[edge]).float().mean().item())
                    edge_n += 1
        va_ce /= max(1, len(val_ds))
        va_acc /= max(1, len(val_ds))
        va_edge_acc = va_edge_acc / max(1, edge_n)
        # Minimize: prioritize diagonal/edge class accuracy
        score = (1.0 - va_edge_acc) + 0.05 * (1.0 - va_acc) + 0.02 * va_ce
        history.append(
            {
                "epoch": epoch,
                "train": tr,
                "val_ce": va_ce,
                "val_acc": va_acc,
                "val_edge_acc": va_edge_acc,
                "score": score,
            }
        )
        print(
            f"epoch {epoch:03d}  train={tr:.4f}  val_ce={va_ce:.4f}  "
            f"acc={va_acc:.4f}  edge_acc={va_edge_acc:.4f}  lr={sched.get_last_lr()[0]:.2e}",
            flush=True,
        )
        (run_dir / "history.json").write_text(json.dumps(history, indent=2), encoding="utf-8")

        ckpt = {
            "model": model.state_dict(),
            "channels": args.channels,
            "scale": args.scale,
            "num_classes": 9,
            "mode": "classifier9",
            "dropout": 0.0,
            "epoch": epoch,
            "val_ce": va_ce,
            "val_acc": va_acc,
            "val_edge_acc": va_edge_acc,
            "args": {k: (str(v) if isinstance(v, Path) else v) for k, v in vars(args).items()},
        }
        torch.save(ckpt, run_dir / "last.pt")
        if score < best_val - 1e-6:
            best_val = score
            bad = 0
            clean = TinySR(channels=args.channels, scale=args.scale, dropout=0.0, num_classes=9)
            clean.load_state_dict(model.state_dict(), strict=False)
            ckpt["model"] = clean.state_dict()
            torch.save(ckpt, run_dir / "best.pt")
        else:
            bad += 1
            if args.patience > 0 and bad >= args.patience:
                print(f"early stop at epoch {epoch} (best score={best_val:.5f})", flush=True)
                break

    print(f"best score={best_val:.5f} -> {run_dir / 'best.pt'}", flush=True)
    return run_dir / "best.pt"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, default=Path("data/nes"))
    ap.add_argument("--out", type=Path, default=Path("tools/train/runs/tinysr_smb_cls"))
    ap.add_argument("--epochs", type=int, default=80)
    ap.add_argument("--batch", type=int, default=64)
    ap.add_argument("--lr", type=float, default=1.5e-3)
    ap.add_argument("--wd", type=float, default=2e-3)
    ap.add_argument("--dropout", type=float, default=0.1)
    ap.add_argument("--patience", type=int, default=15)
    ap.add_argument("--channels", type=int, default=32)
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument(
        "--edge-weight",
        type=float,
        default=24.0,
        help="CE weight multiplier for non-center (diagonal) labels",
    )
    ap.add_argument("--cpu", action="store_true")
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--crop", type=int, default=96)
    ap.add_argument("--repeats", type=int, default=3)
    args = ap.parse_args()
    t0 = time.time()
    train_one(args)
    if torch.cuda.is_available() and not args.cpu:
        torch.cuda.synchronize()
        print(
            f"GPU peak allocated={torch.cuda.max_memory_allocated() / (1024**2):.0f} MiB",
            flush=True,
        )
    print(f"done in {time.time() - t0:.1f}s", flush=True)


if __name__ == "__main__":
    main()
