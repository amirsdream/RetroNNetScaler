#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate RetroNNetScaler technical paper PDF."""

from __future__ import annotations

from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm, mm
from reportlab.platypus import (
    HRFlowable,
    Paragraph,
    Preformatted,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

OUT = Path(__file__).resolve().parent / "RetroNNetScaler_Technical_Paper.pdf"


def make_styles():
    base = getSampleStyleSheet()
    ink = colors.HexColor("#1a1a1a")
    muted = colors.HexColor("#444444")
    return {
        "title": ParagraphStyle(
            "TitleCustom",
            parent=base["Title"],
            fontName="Times-Bold",
            fontSize=18,
            leading=22,
            alignment=TA_CENTER,
            textColor=ink,
            spaceAfter=8,
        ),
        "subtitle": ParagraphStyle(
            "SubtitleCustom",
            parent=base["Normal"],
            fontName="Times-Italic",
            fontSize=11,
            leading=14,
            alignment=TA_CENTER,
            textColor=muted,
            spaceAfter=6,
        ),
        "author": ParagraphStyle(
            "AuthorCustom",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=11,
            leading=14,
            alignment=TA_CENTER,
            textColor=ink,
            spaceAfter=4,
        ),
        "meta": ParagraphStyle(
            "MetaCustom",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=9,
            leading=12,
            alignment=TA_CENTER,
            textColor=muted,
            spaceAfter=14,
        ),
        "h1": ParagraphStyle(
            "H1Custom",
            parent=base["Heading1"],
            fontName="Times-Bold",
            fontSize=13,
            leading=16,
            textColor=ink,
            spaceBefore=16,
            spaceAfter=8,
        ),
        "h2": ParagraphStyle(
            "H2Custom",
            parent=base["Heading2"],
            fontName="Times-Bold",
            fontSize=11.5,
            leading=14,
            textColor=ink,
            spaceBefore=12,
            spaceAfter=6,
        ),
        "body": ParagraphStyle(
            "BodyCustom",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=10.5,
            leading=14,
            alignment=TA_JUSTIFY,
            textColor=ink,
            spaceAfter=8,
            firstLineIndent=12,
        ),
        "body0": ParagraphStyle(
            "Body0Custom",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=10.5,
            leading=14,
            alignment=TA_JUSTIFY,
            textColor=ink,
            spaceAfter=8,
            firstLineIndent=0,
        ),
        "abstract": ParagraphStyle(
            "AbstractCustom",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=10,
            leading=13,
            alignment=TA_JUSTIFY,
            textColor=ink,
            leftIndent=12,
            rightIndent=12,
            spaceAfter=10,
        ),
        "caption": ParagraphStyle(
            "CaptionCustom",
            parent=base["Normal"],
            fontName="Times-Italic",
            fontSize=9,
            leading=11,
            alignment=TA_CENTER,
            textColor=muted,
            spaceBefore=4,
            spaceAfter=10,
        ),
        "ref": ParagraphStyle(
            "RefCustom",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=9,
            leading=12,
            alignment=TA_LEFT,
            textColor=ink,
            leftIndent=14,
            firstLineIndent=-14,
            spaceAfter=4,
        ),
        "code": ParagraphStyle(
            "CodeCustom",
            parent=base["Code"],
            fontName="Courier",
            fontSize=8.5,
            leading=11,
            textColor=ink,
            backColor=colors.HexColor("#f4f2ee"),
            leftIndent=6,
            rightIndent=6,
            spaceBefore=4,
            spaceAfter=8,
        ),
        "bullet": ParagraphStyle(
            "BulletCustom",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=10.5,
            leading=13,
            textColor=ink,
            leftIndent=8,
            spaceAfter=3,
        ),
        "rule": colors.HexColor("#2a2a2a"),
    }


def header_footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#888888"))
    canvas.setLineWidth(0.4)
    y_top = A4[1] - 1.4 * cm
    canvas.line(2.2 * cm, y_top, A4[0] - 2.2 * cm, y_top)
    canvas.setFont("Times-Italic", 8)
    canvas.setFillColor(colors.HexColor("#555555"))
    canvas.drawString(2.2 * cm, A4[1] - 1.2 * cm, "RetroNNetScaler - Technical Report")
    canvas.drawRightString(A4[0] - 2.2 * cm, A4[1] - 1.2 * cm, "Realtime NES Upscaling")
    canvas.line(2.2 * cm, 1.5 * cm, A4[0] - 2.2 * cm, 1.5 * cm)
    canvas.drawCentredString(A4[0] / 2, 1.1 * cm, str(doc.page))
    canvas.restoreState()


def make_table(data, col_widths):
    t = Table(data, colWidths=col_widths, hAlign="CENTER")
    t.setStyle(
        TableStyle(
            [
                ("FONTNAME", (0, 0), (-1, 0), "Times-Bold"),
                ("FONTNAME", (0, 1), (-1, -1), "Times-Roman"),
                ("FONTSIZE", (0, 0), (-1, -1), 9),
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#ebe6dc")),
                ("TEXTCOLOR", (0, 0), (-1, -1), colors.HexColor("#1a1a1a")),
                ("ALIGN", (0, 0), (-1, 0), "CENTER"),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#888888")),
                ("LEFTPADDING", (0, 0), (-1, -1), 5),
                ("RIGHTPADDING", (0, 0), (-1, -1), 5),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
                (
                    "ROWBACKGROUNDS",
                    (0, 1),
                    (-1, -1),
                    [colors.white, colors.HexColor("#f7f5f1")],
                ),
            ]
        )
    )
    return t


def build():
    s = make_styles()
    doc = SimpleDocTemplate(
        str(OUT),
        pagesize=A4,
        leftMargin=2.2 * cm,
        rightMargin=2.2 * cm,
        topMargin=2.0 * cm,
        bottomMargin=2.0 * cm,
        title="RetroNNetScaler: Hard-Palette Realtime Upscaling for NES Pixel Art",
        author="RetroNNetScaler Project",
    )

    story = []

    def P(style, text):
        return Paragraph(text, s[style])

    story.append(Spacer(1, 8 * mm))
    story.append(
        P(
            "title",
            "RetroNNetScaler: Hard-Palette Realtime Upscaling<br/>for Classic NES Pixel Art",
        )
    )
    story.append(
        P(
            "subtitle",
            "A systems and learning report on integer Clarity, sharp xBR geometry,<br/>"
            "and discrete neighbor-classification CNNs for 4K presentation",
        )
    )
    story.append(P("author", "RetroNNetScaler Project"))
    story.append(
        P(
            "meta",
            "Technical Report | July 2026 | "
            "https://github.com/amirsdream/RetroNNetScaler",
        )
    )
    story.append(HRFlowable(width="100%", thickness=0.8, color=s["rule"], spaceAfter=12))

    story.append(P("h1", "Abstract"))
    abs_text = (
        "<b>Abstract.</b> Upscaling palette-limited pixel art for modern 4K displays is "
        "fundamentally different from natural-image super-resolution. Methods that "
        "minimise continuous reconstruction error (bilinear, ESRGAN-class networks) "
        "introduce mid-tone cream colours and silky outlines that destroy the aliased "
        "aesthetic of 8-bit games. We present <b>RetroNNetScaler</b>, a Windows realtime "
        "pipeline that embeds a NES core (SimpleNES), presents frames through Direct3D 11, "
        "and offers three production paths: <i>Clarity</i> (maximum integer nearest), "
        "<i>Smooth</i> (Hyllian-style xBR-sharp with hard neighbourhood snapping), and "
        "<i>Neural</i> (a tiny 9-class neighbour classifier trained against Scale2xSFX "
        "teachers). We show that residual RGB regression underfits sparse diagonal "
        "structure and that unconstrained discrete classifiers create multicolour edge "
        "fringes when false-positive neighbour picks exceed about 1% of pixels. A "
        "tip-gated, hard-palette design - and falling back Neural presentation to sharp "
        "xBR when the classifier is unreliable - preserves discrete NES colours while "
        "improving perceived diagonals. The network (about 52k weights, C=32) is small "
        "enough for future FPGA mapping. We report training setup, validation metrics, "
        "failure modes, and an FPGA-oriented roadmap."
    )
    story.append(P("abstract", abs_text))
    story.append(
        P(
            "body0",
            "<b>Keywords:</b> pixel-art upscaling, NES, xBR, Scale2xSFX, discrete "
            "super-resolution, realtime D3D11, FPGA-oriented CNN",
        )
    )

    story.append(P("h1", "1. Introduction"))
    story.append(
        P(
            "body",
            "Classic console graphics were authored on a square pixel lattice with a "
            "small discrete palette. When those frames are shown on a 3840x2160 panel, "
            "naive bilinear or bicubic filters blur edges and invent colours that never "
            "existed in the ROM. Recent deep super-resolution models, trained for "
            "photographs or anime with continuous losses, tend to produce a "
            "<i>silky</i> look: soft outlines, chroma bleed, and palette drift. For "
            "retro enthusiasts and preservationists, that look is a regression.",
        )
    )
    story.append(
        P(
            "body",
            "Rule-based pixel-art scalers such as EPX/Scale2x, Scale2xSFX, HQX, and "
            "xBR/xBRZ address diagonals by copying existing neighbour colours rather "
            "than interpolating. Kopf and Lischinski depixelization goes further toward "
            "resolution-independent vectors, which many players perceive as too smooth "
            "for classic NES draw. Our product goal is different: <b>hard colours, "
            "cleaner 45-degree diagonals, realtime 4K presentation</b>, with a path "
            "toward FPGA implementation.",
        )
    )
    story.append(
        P(
            "body",
            "This report documents the RetroNNetScaler prototype: system architecture, "
            "shader algorithms, a CUDA training pipeline for a discrete TinySR "
            "classifier, quantitative validation on Super Mario Bros. dumps, and "
            "lessons from failure modes (cream residuals, edge colour explosions).",
        )
    )

    story.append(P("h1", "2. Related Work"))
    story.append(P("h2", "2.1 Classical pixel-art scalers"))
    story.append(
        P(
            "body",
            "EPX/Scale2x expands each pixel into a 2x2 block using equality tests on "
            "the 3x3 neighbourhood, never inventing colours. Scale2xSFX (and Scale3xSFX) "
            "refine diagonal decisions with a wider support and reduce hollow-diamond "
            "artifacts. xBR and xBRZ detect edges with weighted colour distances "
            "(often in YUV) and write tip wedges; Hyllian-style sharp variants avoid "
            "blending. These methods remain the gold standard for hard-palette look.",
        )
    )
    story.append(P("h2", "2.2 Depixelization and continuous SR"))
    story.append(
        P(
            "body",
            "Kopf-Lischinski depixelization extracts smooth contours suitable for "
            "vector magnification. Deep unsupervised pixelization and modern "
            "GAN/transformer SR (Real-ESRGAN, SwinIR, anime SR) optimise perceptual "
            "or continuous losses. Structure-aware pixel-art work (2026) emphasises "
            "that continuous models violate discrete palette integrity. Quantized "
            "generators (for example Gumbel-softmax palette models) align better with "
            "our constraint of no new colours.",
        )
    )
    story.append(P("h2", "2.3 Tiny learned upscalers"))
    story.append(
        P(
            "body",
            "ESPCN-style networks with pixel-shuffle heads are attractive for "
            "realtime and FPGA targets because of small parameter counts. We adopt "
            "that footprint but replace RGB residual regression with a "
            "<b>9-way neighbour classifier</b>, so every output texel is exactly one "
            "of the nine colours in the local LQ neighbourhood.",
        )
    )

    story.append(P("h1", "3. System Architecture"))
    story.append(P("body0", "The PC prototype is organised as a single process:"))
    story.append(
        Preformatted(
            "  NES core (SimpleNES)\n"
            "       |  256x240 RGBA @ ~60.098 Hz\n"
            "       v\n"
            "  D3D11 upload  ->  scaler compute shader  ->  letterbox present\n"
            "       |\n"
            "  Modes: Nearest | Bilinear | NIS | Clarity | Smooth | Neural",
            s["code"],
        )
    )
    story.append(
        P(
            "body",
            "A custom Win32 launcher provides ROM selection (browse / drag-drop), "
            "scale mode chips (Clarity / Smooth / Neural), and output resolution "
            "(1080p / 1440p / 4K). Preferences persist under AppData. Gameplay is "
            "paced to the NES NTSC rate independent of panel refresh (for example 165 Hz).",
        )
    )
    story.append(P("caption", "Table 1. Scaling modes in the current build."))
    story.append(
        make_table(
            [
                ["Mode", "Algorithm class", "Palette behaviour"],
                ["Clarity", "Max integer nearest", "Exact LQ colours"],
                ["Smooth", "xBR-sharp tips + snapExact9", "Hard neighbourhood only"],
                ["Neural*", "TinySR 9-class (safe: xBR)", "Hard; CNN gated when unsafe"],
                ["NIS", "NVIDIA Image Scaling", "Continuous (reference)"],
                ["Nearest / Bilinear", "Baselines", "Exact / blended"],
            ],
            [2.8 * cm, 7.2 * cm, 5.5 * cm],
        )
    )
    story.append(
        P(
            "caption",
            "*Neural presentation uses sharp xBR when the classifier would fringe; "
            "CNN weights remain in-tree for research.",
        )
    )

    story.append(P("h1", "4. Methods"))
    story.append(P("h2", "4.1 Clarity - integer oracle"))
    story.append(
        P(
            "body",
            "Clarity selects the largest integer scale factor that fits the window, "
            "point-samples NES texels into that canvas, and letterboxes. This is the "
            "reference Pure look: blocky but colour-perfect. Any subsequent filter "
            "must beat Clarity on diagonals without introducing cream.",
        )
    )
    story.append(P("h2", "4.2 Smooth - sharp xBR with hard snap"))
    story.append(
        P(
            "body",
            "Smooth implements a Hyllian-inspired xBR-sharp pass in HLSL. For each "
            "output sample we evaluate edge dominance with a YUV colour distance, "
            "restrict writes to outer tip wedges of each quadrant, apply an outline "
            "guard on high-contrast corners, and finally snapExact9: the result is "
            "replaced by the nearest of the nine LQ neighbourhood colours. Mid-tones "
            "cannot survive the snap, which eliminates cream while keeping intentional "
            "tip geometry.",
        )
    )
    story.append(P("h2", "4.3 Neural - discrete neighbour classification"))
    story.append(
        P(
            "body",
            "We train a tiny convolutional network that, for each LQ pixel, predicts "
            "a class in {A,B,C,D,E,F,G,H,I} for every subpixel of a 4x4 block "
            "(pixel-shuffle head). The decoded colour is exactly neighbour[argmax]. "
            "Topology:",
        )
    )
    story.append(
        Preformatted(
            "  Conv 3x3, C=32, ReLU  ->  Conv 3x3, C=32, ReLU  ->  Conv 3x3, 9*4*4\n"
            "  -> pixel_shuffle(r=4)  ->  logits (9, H*4, W*4)\n"
            "  Parameters ~51.8k  |  Dropout2d during train  |  AdamW + wd",
            s["code"],
        )
    )
    story.append(
        P(
            "body",
            "Labels come from a Scale2xSFX teacher chained to 4x; each HQ pixel is "
            "mapped to the nearest of the nine LQ neighbours (exact match preferred, "
            "YUV distance fallback). Non-centre classes are rare (about 0.3-1% of "
            "pixels), so training uses class-weighted cross-entropy with a large edge "
            "multiplier (for example 32x) and early stopping on edge-class accuracy.",
        )
    )
    story.append(P("h2", "4.4 Why residual RGB failed"))
    story.append(
        P(
            "body",
            "An earlier residual ESPCN (nearest + g*tanh(res)) matched teacher L1 "
            "closely after regularisation, but with g &lt;= 0.55 could not reach full "
            "neighbour swaps. Raising gain without discrete decoding produced soft "
            "fringes. Critically, EPX/SFX deltas occupy much less than 1% of HQ "
            "pixels; unmasked L1 is dominated by flat regions, so the net learns "
            "near-zero residual. Masked residual loss improved corner agreement "
            "(about 88% after hard snap) but stroke statistics still lagged a pure "
            "geometric teacher.",
        )
    )

    story.append(P("h1", "5. Training Pipeline"))
    story.append(
        P(
            "body",
            "Frames are dumped from the live emulator (--dump-frames N), converted by "
            "build_dataset.py into LQ/HQ PNG pairs plus uint8 neighbour-index maps, "
            "and trained with train_tinysr.py on NVIDIA CUDA (validated on GeForce "
            "RTX 4080, PyTorch 2.11 + cu128). Export writes tinysr_weights.bin "
            "(magic TNSR, channels, scale, then W1 B1 W2 B2 W3 B3 floats) for "
            "StructuredBuffer load in the compute shader.",
        )
    )
    story.append(P("caption", "Table 2. Classifier training configuration (final run)."))
    story.append(
        make_table(
            [
                ["Item", "Value"],
                ["Teacher", "Scale2xSFX x2 -> 4x"],
                ["Dataset", "~240 train / 60 val SMB frames (stride 1)"],
                ["Crop / aug", "96x96 LQ, rot90 + flips, edge-biased crops"],
                ["Optimiser", "AdamW, lr 1.5e-3 cosine, wd 2e-3"],
                ["Regularisation", "Dropout2d 0.1, grad clip 1.0, patience 20"],
                ["Batch / device", "48, CUDA GPU (no CPU offload)"],
                ["Export", "C=32, scale=4, 9 classes, 51760 floats"],
            ],
            [4.5 * cm, 11.0 * cm],
        )
    )

    story.append(P("h1", "6. Experiments and Results"))
    story.append(
        P(
            "body",
            "We evaluate on held-out SMB validation frames. Metrics: overall and "
            "edge-only neighbour accuracy (edge = label != E), L1 vs teacher, edge-map "
            "L1, palette drift (fraction of HQ pixels outside the 3x3 LQ set), and a "
            "stroke-variance proxy on strong edges.",
        )
    )
    story.append(
        P("caption", "Table 3. Validation means (12-frame subset, classifier checkpoint).")
    )
    story.append(
        make_table(
            [
                ["Metric", "Nearest", "Neural CNN", "Teacher (SFX)"],
                ["Neighbour acc (all)", "-", "96.4%", "100% (by def.)"],
                ["Neighbour acc (edge)", "-", "71.8%", "100%"],
                ["L1 vs teacher", "0.00279", "0.00810", "0"],
                ["Edge-map L1 vs teacher", "0.0206", "0.0559", "0"],
                ["Palette drift", "0", "0", "0"],
                ["Stroke variance proxy", "0.892", "0.939", "1.231"],
            ],
            [5.5 * cm, 3.5 * cm, 3.5 * cm, 3.5 * cm],
        )
    )
    story.append(
        P(
            "body",
            "Palette drift is identically zero for the discrete decoder - an essential "
            "product constraint. Edge accuracy of about 72% is meaningful but "
            "insufficient: the remaining errors concentrate on glyph and sprite "
            "silhouettes and are visually catastrophic (Section 7). Stroke variance of "
            "the CNN lies between nearest and teacher, indicating partial geometric "
            "learning.",
        )
    )

    story.append(P("h1", "7. Failure Analysis"))
    story.append(P("h2", "7.1 Silky / cream look"))
    story.append(
        P(
            "body",
            "Continuous residuals and any path that averages colours across an edge "
            "create mid-tones. Mitigation: hard snap to the LQ neighbourhood; forbid "
            "perceptual / adversarial losses aimed at natural images.",
        )
    )
    story.append(P("h2", "7.2 Exploded colours on edges"))
    story.append(
        P(
            "body",
            "When the classifier assigns a false non-E class, the shader copies a "
            "neighbour that may be a completely different palette entry (for example "
            "red HUD vs black background). At 4x these errors appear as multicolour "
            "static around letters and sprites. Weight layout verification against "
            "PyTorch confirmed correct indexing; the fault is statistical, not "
            "binding. Mitigations implemented: (i) strong logit margin vs centre E; "
            "(ii) tip-only acceptance masks per quadrant; (iii) runtime presentation "
            "fallback of Neural to sharp xBR until edge accuracy is reliable.",
        )
    )
    story.append(P("h2", "7.3 Sparse teacher signal"))
    story.append(
        P(
            "body",
            "Only a fraction of a percent of HQ pixels differ from nearest under "
            "EPX/SFX. Unweighted losses drown the diagonal signal. Class-balanced CE "
            "and edge-biased crops help but do not yet match Smooth geometric "
            "consistency on text.",
        )
    )

    story.append(P("h1", "8. FPGA and Future Work"))
    story.append(
        P(
            "body",
            "The classifier parameter count and 3x3 conv structure map naturally to "
            "streaming FPGA pipelines (line buffers + MAC arrays). Intended lab board: "
            "DE10-Nano for Clarity/TinySR; larger devices if transformer Regen is "
            "pursued on PC first. Next research steps:",
        )
    )
    for b in [
        "Tip-only label generation (supervise only xBR/SFX tip loci).",
        "Two-head model: edge vs flat binary gate + 8-way direction on tips.",
        "Multi-game dumps to reduce SMB-specific overfitting.",
        "Distill Smooth xBR decisions as an alternative teacher to SFX.",
        "On-device int8 export and cycle-accurate RTL for the stem convolutions.",
    ]:
        story.append(P("bullet", f"- {b}"))

    story.append(P("h1", "9. Conclusion"))
    story.append(
        P(
            "body",
            "RetroNNetScaler demonstrates a complete path from NES emulation to 4K "
            "presentation with explicit respect for discrete palettes. Sharp xBR with "
            "neighbourhood snap currently delivers the best realtime diagonals without "
            "silk. A FPGA-sized neighbour-classification CNN achieves zero palette "
            "drift and partial edge learning, but unconstrained argmax decoding "
            "fringes until tip gating and higher edge accuracy are achieved. The "
            "project central lesson is methodological: for pixel art, "
            "<b>discretise the output domain first</b>, then learn; continuous SR "
            "objectives fight the medium.",
        )
    )

    story.append(P("h1", "References"))
    refs = [
        "[1] A. Mazzoleni, Scale2x / Scale3x, scale2x.it - AdvMAME pixel-art scalers.",
        "[2] Scale2xSFX / Scale3xSFX proposals (libretro community; ScaleNx implementations).",
        "[3] H. Hyllian et al., xBR / xBRZ family - edge-directed pixel-art upscaling.",
        "[4] J. Kopf and D. Lischinski, Depixelizing Pixel Art, ACM TOG (SIGGRAPH), 2011.",
        "[5] W. Shi et al., Real-Time Single Image and Video Super-Resolution Using an Efficient Sub-Pixel Convolutional Neural Network (ESPCN), CVPR, 2016.",
        "[6] Structure-Aware Pixel Art Scaling via Block Size Detection, Appl. Sci., 2026.",
        "[7] SD-piXL: Generating Low-Resolution Quantized Imagery via Score Distillation, 2024.",
        "[8] SimpleNES - github.com/amhndu/SimpleNES (NES emulator core).",
        "[9] NVIDIA Image Scaling (NIS) SDK - spatial upscaler reference.",
        "[10] RetroNNetScaler source - github.com/amirsdream/RetroNNetScaler, 2026.",
    ]
    for r in refs:
        story.append(P("ref", r))

    story.append(Spacer(1, 10 * mm))
    story.append(HRFlowable(width="100%", thickness=0.5, color=s["rule"], spaceAfter=8))
    story.append(
        P(
            "meta",
            "Correspondence via the project repository. ROM dumps used for training "
            "must be legally obtained by the user; ROMs are not redistributed.",
        )
    )

    doc.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
    print("Wrote", OUT)


if __name__ == "__main__":
    build()
