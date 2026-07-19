"""Hard-color teachers for TinySR labels (EPX / Scale2xSFX / nearest)."""

from __future__ import annotations

import numpy as np

# Neighbor class order (matches HLSL): row/col offsets for A B C / D E F / G H I
# 0:A 1:B 2:C 3:D 4:E 5:F 6:G 7:H 8:I
NEIGH_OFFSETS = (
    (-1, -1),
    (-1, 0),
    (-1, 1),
    (0, -1),
    (0, 0),
    (0, 1),
    (1, -1),
    (1, 0),
    (1, 1),
)


def nearest_x(lq: np.ndarray, scale: int = 4) -> np.ndarray:
    rgb = lq[..., :3].astype(np.float32)
    return np.repeat(np.repeat(rgb, scale, axis=0), scale, axis=1)


def _eq(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    return np.all(a == b, axis=-1)


def _neq(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    return np.any(a != b, axis=-1)


def _scale2x_epx(img: np.ndarray) -> np.ndarray:
    """Eric Johnston EPX / Scale2x — hard colors, good diagonals, fast."""
    h, w = img.shape[:2]
    pad = np.pad(img, ((1, 1), (1, 1), (0, 0)), mode="edge")
    B = pad[0:h, 1 : w + 1]
    D = pad[1 : h + 1, 0:w]
    E = pad[1 : h + 1, 1 : w + 1]
    F = pad[1 : h + 1, 2 : w + 2]
    H = pad[2 : h + 2, 1 : w + 1]

    out = np.empty((h * 2, w * 2, 3), dtype=np.float32)
    eqBD = _eq(B, D)
    eqBF = _eq(B, F)
    eqDH = _eq(D, H)
    eqFH = _eq(F, H)
    neBF = _neq(B, F)
    neDH = _neq(D, H)
    neBD = _neq(B, D)
    neFH = _neq(F, H)
    neBH = _neq(B, H)
    neDF = _neq(D, F)

    E0 = np.where((eqBD & neBF & neDH)[..., None], B, E)
    E1 = np.where((eqBF & neBD & neFH)[..., None], B, E)
    E2 = np.where((eqDH & neBD & neFH)[..., None], D, E)
    E3 = np.where((eqFH & neBF & neDH)[..., None], F, E)

    both = eqBD & eqFH & neBH & neDF
    E0 = np.where(both[..., None], E, E0)
    E1 = np.where(both[..., None], E, E1)
    E2 = np.where(both[..., None], E, E2)
    E3 = np.where(both[..., None], E, E3)

    out[0::2, 0::2] = E0
    out[0::2, 1::2] = E1
    out[1::2, 0::2] = E2
    out[1::2, 1::2] = E3
    return out


def _scale2x_sfx(img: np.ndarray) -> np.ndarray:
    """Scale2xSFX — improved diagonals vs EPX (libretro / ScaleNx rules)."""
    h, w = img.shape[:2]
    # Pad 2 so J/K/L/M (2-ring) are available
    pad = np.pad(img, ((2, 2), (2, 2), (0, 0)), mode="edge")
    # Center E at pad[y+2, x+2]
    A = pad[1 : h + 1, 1 : w + 1]
    B = pad[1 : h + 1, 2 : w + 2]
    C = pad[1 : h + 1, 3 : w + 3]
    D = pad[2 : h + 2, 1 : w + 1]
    E = pad[2 : h + 2, 2 : w + 2]
    F = pad[2 : h + 2, 3 : w + 3]
    G = pad[3 : h + 3, 1 : w + 1]
    H = pad[3 : h + 3, 2 : w + 2]
    I = pad[3 : h + 3, 3 : w + 3]
    J = pad[0:h, 2 : w + 2]
    K = pad[2 : h + 2, 0:w]
    L = pad[2 : h + 2, 4 : w + 4]
    M = pad[4 : h + 4, 2 : w + 2]

    r1 = E.copy()
    r2 = E.copy()
    r3 = E.copy()
    r4 = E.copy()

    neBF = _neq(B, F)
    neDH = _neq(D, H)
    cond1 = neBF & neDH
    # r1 = B if B==D and (A!=E or C==E or E==G or A==J or A==K)
    take_r1 = cond1 & _eq(B, D) & (
        _neq(A, E) | _eq(C, E) | _eq(E, G) | _eq(A, J) | _eq(A, K)
    )
    r1 = np.where(take_r1[..., None], B, r1)
    # r4 = H if H==F and (E!=I or C==E or E==G or I==L or I==M)
    take_r4 = cond1 & _eq(H, F) & (
        _neq(E, I) | _eq(C, E) | _eq(E, G) | _eq(I, L) | _eq(I, M)
    )
    r4 = np.where(take_r4[..., None], H, r4)

    neBD = _neq(B, D)
    neFH = _neq(F, H)
    cond2 = neBD & neFH
    # r2 = B if B==F and (C!=E or A==E or E==I or C==J or C==L)
    take_r2 = cond2 & _eq(B, F) & (
        _neq(C, E) | _eq(A, E) | _eq(E, I) | _eq(C, J) | _eq(C, L)
    )
    r2 = np.where(take_r2[..., None], B, r2)
    # r3 = H if H==D and (E!=G or A==E or E==I or G==K or G==M)
    take_r3 = cond2 & _eq(H, D) & (
        _neq(E, G) | _eq(A, E) | _eq(E, I) | _eq(G, K) | _eq(G, M)
    )
    r3 = np.where(take_r3[..., None], H, r3)

    out = np.empty((h * 2, w * 2, 3), dtype=np.float32)
    out[0::2, 0::2] = r1
    out[0::2, 1::2] = r2
    out[1::2, 0::2] = r3
    out[1::2, 1::2] = r4
    return out


def upscale_teacher(lq: np.ndarray, scale: int = 4, kind: str = "sfx") -> np.ndarray:
    """lq HxWx3|4 float/uint → HQ float01."""
    rgb = lq[..., :3]
    if rgb.dtype != np.float32:
        rgb = rgb.astype(np.float32)
        if rgb.max() > 1.5:
            rgb = rgb / 255.0

    if kind == "nearest":
        return nearest_x(rgb, scale)

    step = _scale2x_sfx if kind == "sfx" else _scale2x_epx
    out = rgb
    s = 1
    while s < scale:
        out = step(out)
        s *= 2
    if s > scale:
        factor = s // scale
        out = out.reshape(out.shape[0] // factor, factor, out.shape[1] // factor, factor, 3).mean(
            axis=(1, 3)
        )
    elif s < scale:
        out = nearest_x(out, scale // s)
    return out.astype(np.float32)


def _yuv(c: np.ndarray) -> np.ndarray:
    y = 0.299 * c[..., 0] + 0.587 * c[..., 1] + 0.114 * c[..., 2]
    u = (c[..., 2] - y) * 0.565
    v = (c[..., 0] - y) * 0.713
    return np.stack([y, u, v], axis=-1)


def neighbor_labels(lq: np.ndarray, hq: np.ndarray, scale: int = 4) -> np.ndarray:
    """Map each HQ pixel to nearest of 9 LQ neighbors (exact match preferred)."""
    h, w = lq.shape[:2]
    hs, ws = h * scale, w * scale
    assert hq.shape[0] == hs and hq.shape[1] == ws

    pad = np.pad(lq, ((1, 1), (1, 1), (0, 0)), mode="edge")
    # neigh: 9, H, W, 3
    neigh = np.stack(
        [pad[1 + dy : 1 + dy + h, 1 + dx : 1 + dx + w] for dy, dx in NEIGH_OFFSETS],
        axis=0,
    )
    # Expand to HQ: 9, Hs, Ws, 3
    neigh_hq = np.repeat(np.repeat(neigh, scale, axis=1), scale, axis=2)

    # Exact RGB match
    exact = np.all(np.abs(neigh_hq - hq[None, ...]) < 1e-5, axis=-1)  # 9,Hs,Ws
    any_exact = exact.any(axis=0)
    # Prefer center E (class 4) when multiple exact matches
    labels = np.full((hs, ws), 4, dtype=np.int64)
    # First exact index (0..8); for ties prefer E if exact
    first = np.argmax(exact, axis=0)
    labels = np.where(any_exact, first, labels)
    labels = np.where(any_exact & exact[4], 4, labels)

    # Fallback: YUV distance for non-exact (should be rare with hard teachers)
    if not bool(np.all(any_exact)):
        ny = _yuv(neigh_hq)
        hy = _yuv(hq)[None, ...]
        dist = np.abs(ny - hy).sum(axis=-1)
        best = np.argmin(dist, axis=0)
        labels = np.where(any_exact, labels, best)

    return labels


def colors_from_labels(lq: np.ndarray, labels: np.ndarray, scale: int = 4) -> np.ndarray:
    """Decode neighbor class map → RGB (hard palette)."""
    h, w = lq.shape[:2]
    pad = np.pad(lq, ((1, 1), (1, 1), (0, 0)), mode="edge")
    neigh = np.stack(
        [pad[1 + dy : 1 + dy + h, 1 + dx : 1 + dx + w] for dy, dx in NEIGH_OFFSETS],
        axis=0,
    )
    neigh_hq = np.repeat(np.repeat(neigh, scale, axis=1), scale, axis=2)
    # gather
    yy, xx = np.indices(labels.shape)
    return neigh_hq[labels, yy, xx].astype(np.float32)
