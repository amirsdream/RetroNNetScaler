#!/usr/bin/env python3
"""Placeholder: export trained TinySR weights to a C/HLSL-friendly header for PC + FPGA.

Replace the stub weight functions in shaders/tiny_sr.hlsl with tables generated here
after offline training (PyTorch/etc). Keep ops to 3x3 conv + ReLU + pixel-shuffle
so the same tensors can be quantized to int8 for FPGA BRAM.
"""

from pathlib import Path


def main() -> None:
    out = Path(__file__).resolve().parents[1] / "src" / "tiny_sr_weights.inc"
    out.write_text(
        "// Auto-generated placeholder. Train TinySR offline, then emit float/int8 arrays here.\n"
        "// Expected tensors: W1[8][3][3][3], W2[8][8][3][3], W3[48][8]\n",
        encoding="utf-8",
    )
    print(f"Wrote stub {out}")
    print("Next: train ESPCN-like 4x model on NES frames and fill arrays.")


if __name__ == "__main__":
    main()
