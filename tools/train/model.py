"""TinySR: 9-class neighbor classifier for hard 4x pixel-art upscaling."""

from __future__ import annotations

import torch
import torch.nn as nn
import torch.nn.functional as F

# Must match teacher_xbr.NEIGH_OFFSETS / HLSL
NUM_CLASSES = 9


class TinySR(nn.Module):
    """LQ RGB -> 9 logits per HQ pixel (which 3x3 neighbor color to use).

    Weight layout (export): W1 B1 W2 B2 W3 B3  with C channels, scale S,
    W3 out = NUM_CLASSES * S * S.
    """

    def __init__(
        self,
        channels: int = 16,
        scale: int = 4,
        dropout: float = 0.0,
        num_classes: int = NUM_CLASSES,
        residual_gain: float = 1.0,  # ignored; kept for ckpt compat
    ):
        super().__init__()
        self.scale = scale
        self.channels = channels
        self.num_classes = int(num_classes)
        self.residual_gain = float(residual_gain)
        self.conv1 = nn.Conv2d(3, channels, 3, padding=1)
        self.conv2 = nn.Conv2d(channels, channels, 3, padding=1)
        self.conv3 = nn.Conv2d(channels, self.num_classes * scale * scale, 3, padding=1)
        self.drop = nn.Dropout2d(p=dropout) if dropout > 0 else nn.Identity()

    def forward_logits(self, x: torch.Tensor) -> torch.Tensor:
        """Return (B, 9, H*s, W*s) logits."""
        h = F.relu(self.conv1(x), inplace=True)
        h = self.drop(h)
        h = F.relu(self.conv2(h), inplace=True)
        h = self.drop(h)
        h = self.conv3(h)
        return F.pixel_shuffle(h, self.scale)

    def forward(self, x: torch.Tensor, margin: float = 0.35) -> torch.Tensor:
        """Hard RGB decode via argmax over neighborhood (eval path)."""
        logits = self.forward_logits(x)
        labels = logits.argmax(dim=1)  # B, Hs, Ws
        if margin > 0:
            best = logits.max(dim=1).values
            vE = logits[:, 4]
            labels = torch.where(best < vE + margin, torch.full_like(labels, 4), labels)
        return decode_labels(x, labels, self.scale)


def decode_labels(lq: torch.Tensor, labels: torch.Tensor, scale: int) -> torch.Tensor:
    """lq (B,3,H,W), labels (B,Hs,Ws) → RGB (B,3,Hs,Ws)."""
    b, _, h, w = lq.shape
    pad = F.pad(lq, (1, 1, 1, 1), mode="replicate")
    # Stack 9 neighbors: B,9,3,H,W
    chunks = []
    for dy, dx in (
        (-1, -1),
        (-1, 0),
        (-1, 1),
        (0, -1),
        (0, 0),
        (0, 1),
        (1, -1),
        (1, 0),
        (1, 1),
    ):
        chunks.append(pad[:, :, 1 + dy : 1 + dy + h, 1 + dx : 1 + dx + w])
    neigh = torch.stack(chunks, dim=1)  # B,9,3,H,W
    neigh = neigh.repeat_interleave(scale, dim=3).repeat_interleave(scale, dim=4)
    # gather
    idx = labels.unsqueeze(1).unsqueeze(2).expand(-1, 1, 3, -1, -1)  # B,1,3,Hs,Ws
    # neigh is B,9,3,Hs,Ws — gather on dim 1
    out = torch.gather(neigh, 1, idx.expand(-1, 1, 3, labels.shape[1], labels.shape[2]))
    return out.squeeze(1)


def count_params(m: nn.Module) -> int:
    return sum(p.numel() for p in m.parameters())
