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
            fontSize=17,
            leading=21,
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
    canvas.drawString(2.2 * cm, A4[1] - 1.2 * cm, "RetroNNetScaler — Technical Report")
    canvas.drawRightString(A4[0] - 2.2 * cm, A4[1] - 1.2 * cm, "Hard-Palette Contour Upscaling")
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
        title="RetroNNetScaler: Contour Upscaling for Hard-Palette NES Pixel Art",
        author="RetroNNetScaler Project",
    )

    story = []

    def P(style, text):
        return Paragraph(text, s[style])

    story.append(Spacer(1, 6 * mm))
    story.append(
        P(
            "title",
            "RetroNNetScaler: Contour Upscaling for<br/>Hard-Palette NES Pixel Art",
        )
    )
    story.append(
        P(
            "subtitle",
            "From silky CNN residuals to shared half-plane diagonals —<br/>"
            "a systems report on Clarity, Smooth, Contour, and failed neural paths",
        )
    )
    story.append(P("author", "RetroNNetScaler Project"))
    story.append(
        P(
            "meta",
            "Technical Report | August 2026 | "
            "https://github.com/amirsdream/RetroNNetScaler",
        )
    )
    story.append(HRFlowable(width="100%", thickness=0.8, color=s["rule"], spaceAfter=12))

    # ----- Abstract -----
    story.append(P("h1", "Abstract"))
    story.append(
        P(
            "abstract",
            "<b>Abstract.</b> Upscaling palette-limited NES frames for 4K displays is "
            "not natural-image super-resolution. Continuous filters and photo-SR networks "
            "invent mid-tone <i>cream</i> colours and silky outlines that erase the aliased "
            "8-bit aesthetic. We present <b>RetroNNetScaler</b>, a Windows realtime pipeline "
            "(SimpleNES + Direct3D 11) with three hard-palette production modes: "
            "<i>Clarity</i> (maximum integer nearest), <i>Smooth</i> (xBR-sharp tip wedges "
            "with neighbourhood snap), and <i>Contour</i> (Kopf-style planarized diagonals "
            "with <b>shared global half-planes</b> so long 45° stairs become one continuous "
            "edge). We also document a TinySR neighbour-classification CNN (~52k weights) "
            "trained against Scale2xSFX: palette drift can be forced to zero, yet "
            "unconstrained decoding produces multicolour edge fringes. The central result "
            "is methodological — <b>discretise the output domain first</b>; for continuous "
            "diagonals without cream, agree on one geometric edge across adjacent cells "
            "rather than regressing RGB. The Contour pass is FPGA-shaped (local 3×3 "
            "equality tests + plane tests, no weights).",
        )
    )
    story.append(
        P(
            "body0",
            "<b>Keywords:</b> pixel-art upscaling, NES, Contour, Kopf–Lischinski, xBR, "
            "hard palette, realtime D3D11, discrete super-resolution, FPGA",
        )
    )

    # ----- 1 -----
    story.append(P("h1", "1. Introduction"))
    story.append(
        P(
            "body",
            "Classic console graphics were authored on a square pixel lattice with a "
            "small discrete palette. Magnified onto a 3840×2160 panel, bilinear and "
            "photo-SR methods blur edges and invent colours that never existed in the "
            "ROM. Players and preservationists reject that <i>silky</i> look: soft "
            "outlines, chroma bleed, and palette drift.",
        )
    )
    story.append(
        P(
            "body",
            "Rule-based scalers (EPX/Scale2x, Scale2xSFX, HQX, xBR/xBRZ) copy existing "
            "neighbour colours. Kopf and Lischinski depixelization goes further toward "
            "resolution-independent vectors — often too smooth for classic NES draw. "
            "Our product goal is narrower: <b>hard colours, continuous 45° diagonals, "
            "realtime 4K presentation</b>, with a path toward FPGA implementation.",
        )
    )
    story.append(
        P(
            "body",
            "This report covers system architecture; Clarity / Smooth / Contour "
            "algorithms; why EPX-local cuts still look pixelated; the shared "
            "half-plane fix; the TinySR CNN experiment and its failure modes; and "
            "an FPGA-oriented roadmap.",
        )
    )

    # ----- 2 -----
    story.append(P("h1", "2. Related Work"))
    story.append(P("h2", "2.1 Classical pixel-art scalers"))
    story.append(
        P(
            "body",
            "EPX/Scale2x expands each pixel with equality tests on a 3×3 neighbourhood "
            "and never invents colours. Scale2xSFX widens support to reduce hollow "
            "diamonds. xBR/xBRZ detect edges with weighted YUV distances and write tip "
            "wedges; Hyllian-style sharp variants avoid blending. These remain the "
            "reference for hard-palette look.",
        )
    )
    story.append(P("h2", "2.2 Depixelization"))
    story.append(
        P(
            "body",
            "Kopf and Lischinski (SIGGRAPH 2011) build a similarity graph, planarize "
            "crossing diagonals (curves / sparse / islands heuristics), reshape cells "
            "via a generalized Voronoi on nodes and half-edges, then fit contour "
            "splines. Kreuzer et al. (I3D 2015) target realtime GPU paths. Full spline "
            "fitting yields the classic “inked” vector look; we adopt the "
            "<b>planarization + hard ownership</b> stage and deliberately omit soft "
            "B-spline shading so fills stay flat NES colours.",
        )
    )
    story.append(P("h2", "2.3 Learned upscalers"))
    story.append(
        P(
            "body",
            "ESPCN-style networks are attractive for FPGA (small parameter counts). "
            "Photo and anime SR (Real-ESRGAN, SwinIR) optimise continuous or perceptual "
            "losses and routinely violate discrete palettes. We tried residual RGB and "
            "9-class neighbour classification; both are documented in Section 6 as "
            "negative results for our product constraint.",
        )
    )

    # ----- 3 -----
    story.append(P("h1", "3. System Architecture"))
    story.append(P("body0", "The PC prototype is a single process:"))
    story.append(
        Preformatted(
            "  NES core (SimpleNES)\n"
            "       |  256×240 RGBA @ ~60.098 Hz\n"
            "       v\n"
            "  D3D11 upload → scaler compute shader → integer letterbox present\n"
            "       |\n"
            "  Modes: Clarity | Smooth | Contour | NIS | Nearest | Bilinear",
            s["code"],
        )
    )
    story.append(
        P(
            "body",
            "A custom Win32 launcher provides ROM selection (browse / drag-drop), "
            "mode chips (Clarity / Smooth / Contour), and output resolution "
            "(1080p / 1440p / 4K). Preferences persist under AppData. Presentation "
            "uses maximum integer scale into the window with phase-locked letterboxing "
            "(OSSC-style), so output pixels stay uniform width.",
        )
    )
    story.append(P("caption", "Table 1. Scaling modes in the current build."))
    story.append(
        make_table(
            [
                ["Mode", "Algorithm class", "Palette behaviour"],
                ["Clarity", "Max integer nearest", "Exact LQ colours"],
                ["Smooth", "xBR-sharp tips + snapExact9", "Hard neighbourhood only"],
                ["Contour", "Planar diagonals + shared half-planes", "Exact neighbour colours"],
                ["NIS", "NVIDIA Image Scaling", "Continuous (reference)"],
                ["Nearest / Bilinear", "Baselines", "Exact / blended"],
            ],
            [2.6 * cm, 7.4 * cm, 5.5 * cm],
        )
    )

    # ----- 4 -----
    story.append(P("h1", "4. Methods"))
    story.append(P("h2", "4.1 Clarity — integer oracle"))
    story.append(
        P(
            "body",
            "Clarity selects the largest integer scale factor that fits the backbuffer, "
            "point-samples NES texels into that canvas, and letterboxes. At 1080p, "
            "NES 256×240 typically yields s=4 (1024×960); at 4K, s=9 (2304×2160). "
            "This is the colour-perfect reference: blocky, but any better mode must "
            "improve diagonals without inventing cream.",
        )
    )
    story.append(P("h2", "4.2 Smooth — sharp xBR with hard snap"))
    story.append(
        P(
            "body",
            "Smooth implements a Hyllian-inspired xBR-sharp pass in HLSL. Edge "
            "dominance uses YUV colour distance; writes are restricted to tip wedges; "
            "an outline guard protects high-contrast corners; finally "
            "<font face='Courier'>snapExact9</font> replaces the result by the nearest "
            "of the nine LQ neighbourhood colours. Mid-tones cannot survive the snap.",
        )
    )
    story.append(P("h2", "4.3 Contour — shared half-plane diagonals"))
    story.append(
        P(
            "body",
            "Contour is the production answer to “sharp fills, continuous edges.” "
            "For each output sample inside LQ cell E with fractional offsets "
            "(f<sub>x</sub>, f<sub>y</sub>), we:",
        )
    )
    for b in [
        "Fetch the 3×3 neighbourhood A…I and test similarity with Kopf/hqx YUV "
        "thresholds (ΔY≤48/255, ΔU≤7/255, ΔV≤6/255).",
        "Planarize each of the four 2×2 blocks around E: keep at most one diagonal "
        "(A–E or B–D, etc.) using curves / sparse / islands scoring when both cross.",
        "Assign colour by a <b>global</b> half-plane on U=x+y (slash /) or V=x−y "
        "(backslash \\), with bisectors at *.5 so the dark cell and the light cell "
        "of a stair share the <b>same</b> edge equation.",
        "If no 45° rule fires, apply 2:1 / 1:2 hard wedges (still exact neighbour "
        "colours).",
    ]:
        story.append(P("bullet", f"• {b}"))
    story.append(
        P(
            "body",
            "Output colour is always one of A…I — never a blend. No network weights "
            "are required at runtime.",
        )
    )
    story.append(P("h2", "4.4 Why local EPX cuts still look pixelated"))
    story.append(
        P(
            "body",
            "Classic EPX paints a triangle in each cell (e.g. f<sub>x</sub>+f<sub>y</sub> "
            "&lt; 0.5). On a long slash stair, the dark cell’s cut and the light cell’s "
            "cut often sit on <b>parallel but offset</b> lines (different constants of "
            "U=x+y). The visible border becomes a thick zigzag at the scale of source "
            "pixels — still “pixelated,” even though each cut is continuous inside its "
            "cell. Contour’s contribution is to force both sides onto one shared "
            "bisector (e.g. U = s<sub>x</sub>+s<sub>y</sub>+0.5 and "
            "U = s<sub>x</sub>+s<sub>y</sub>+1.5 on complementary corners), recovering "
            "a single straight 45° interface.",
        )
    )
    story.append(
        Preformatted(
            "  / stair (keep B–D in TL of E):   if U < Us + 0.5  → colour B\n"
            "  / stair (keep F–H in BR of E):   if U > Us + 1.5  → colour F\n"
            "  \\ stair (keep B–F / D–H):        compare V against Vs ± 0.5\n"
            "  Us = sx+sy,  Vs = sx−sy,  U = x+y,  V = x−y   (continuous)",
            s["code"],
        )
    )

    # ----- 5 -----
    story.append(P("h1", "5. Implementation Notes"))
    story.append(
        P(
            "body",
            "Clarity, Smooth, and Contour share one compute shader "
            "(<font face='Courier'>shaders/tiny_sr.hlsl</font>), selected by a mode "
            "constant. Contour dispatches at destination resolution with integer "
            "scale s from the present path. The shader is compiled at runtime with "
            "D3DCompile (cs_5_0). The Win32 launcher maps the Contour chip to the "
            "internal <font face='Courier'>ScaleMode::Neural</font> enum for prefs "
            "compatibility; CLI accepts <font face='Courier'>--scale neural</font>.",
        )
    )

    # ----- 6 -----
    story.append(P("h1", "6. The Neural Experiment (Negative Result)"))
    story.append(
        P(
            "body",
            "Before Contour, we trained a TinySR network to predict, for each LQ "
            "pixel, a class in {A…I} for every subpixel of a 4×4 block (pixel-shuffle "
            "head). Decoded colour is exactly neighbour[argmax] — zero palette drift "
            "by construction.",
        )
    )
    story.append(
        Preformatted(
            "  Conv3×3 C=32 ReLU → Conv3×3 C=32 ReLU → Conv3×3 9·4·4\n"
            "  → pixel_shuffle(r=4) → logits (9, H·4, W·4)   ≈ 51.8k params",
            s["code"],
        )
    )
    story.append(
        P(
            "body",
            "Labels came from a Scale2xSFX teacher chained to 4×. Non-centre classes "
            "are rare (~0.3–1% of pixels), so training used class-weighted CE with a "
            "large edge multiplier and edge-biased crops (CUDA, RTX 4080).",
        )
    )
    story.append(P("caption", "Table 2. Classifier validation (SMB hold-out, illustrative)."))
    story.append(
        make_table(
            [
                ["Metric", "Nearest", "Neural CNN", "Teacher (SFX)"],
                ["Neighbour acc (all)", "—", "~96%", "100%"],
                ["Neighbour acc (edge)", "—", "~72%", "100%"],
                ["Palette drift", "0", "0", "0"],
            ],
            [5.5 * cm, 3.5 * cm, 3.5 * cm, 3.5 * cm],
        )
    )
    story.append(P("h2", "6.1 Failure modes"))
    story.append(
        P(
            "body",
            "<b>Cream.</b> Residual RGB regression underfits sparse EPX/SFX deltas; "
            "raising gain without discrete decoding invents mid-tones.<br/>"
            "<b>Fringes.</b> At ~72% edge accuracy, false non-E classes copy a wrong "
            "palette entry (e.g. HUD red into a black silhouette) — multicolour static "
            "at 4×. Tip gates and runtime fallback to Smooth mitigated symptoms but "
            "did not beat Contour’s geometric consistency.<br/>"
            "<b>Lesson.</b> For this medium, learn geometry (or hard-code it) rather "
            "than continuous colour.",
        )
    )

    # ----- 7 -----
    story.append(P("h1", "7. Discussion"))
    story.append(
        P(
            "body",
            "Contour targets 45° stairs and shallow 2:1 wedges. Very irregular "
            "silhouettes and text still benefit from Smooth’s tip logic or Clarity’s "
            "honest blocks. At modest integer scales (s=4 at 1080p), even a "
            "mathematically straight hard edge is a one-pixel staircase of "
            "<i>output</i> samples — that is rasterization, not the source-pixel "
            "zigzag Contour removes. Higher s (4K) makes continuous diagonals more "
            "visually obvious.",
        )
    )
    story.append(
        P(
            "body",
            "We intentionally omit Kopf spline optimisation: curved inked contours "
            "read as silky to many NES players. Hard half-planes keep the look "
            "closer to “drawn with exact colours” than “vectorized illustration.”",
        )
    )

    # ----- 8 -----
    story.append(P("h1", "8. FPGA and Future Work"))
    story.append(
        P(
            "body",
            "Contour maps to a streaming pipeline: line buffers for a 3×3 window, "
            "YUV similarity comparators, four planarDiag units, and a few plane "
            "tests per output sample — no DRAM weight traffic. Clarity is already "
            "trivial on FPGA. Intended evaluation board: DE10-Nano for Clarity/"
            "Contour; larger devices only if a learned tip classifier returns.",
        )
    )
    for b in [
        "Tighten Contour on shallow non-45° hills and sprite silhouettes.",
        "Optional NES PAR (8:7) toggle for CRT-accurate width.",
        "Tip-only learned gate distilled from Smooth, if Contour gaps remain.",
        "Int8 / cycle-accurate RTL for the Contour datapath.",
        "Multi-game visual QA sets beyond SMB.",
    ]:
        story.append(P("bullet", f"• {b}"))

    # ----- 9 -----
    story.append(P("h1", "9. Conclusion"))
    story.append(
        P(
            "body",
            "RetroNNetScaler is a complete path from NES emulation to 4K presentation "
            "with explicit respect for discrete palettes. Clarity is the honest "
            "oracle; Smooth improves tips without cream; Contour makes long diagonals "
            "share one geometric edge instead of EPX’s offset zigzags. A FPGA-sized "
            "neighbour CNN achieved zero palette drift but fringed until abandoned "
            "for Contour at runtime. The project’s central lesson: "
            "<b>discretise first</b>; for continuous edges without silk, make both "
            "sides of the stair agree on the same half-plane.",
        )
    )

    # ----- Refs -----
    story.append(P("h1", "References"))
    refs = [
        "[1] A. Mazzoleni, Scale2x / Scale3x — AdvMAME pixel-art scalers.",
        "[2] Scale2xSFX / Scale3xSFX (libretro / ScaleNx community implementations).",
        "[3] H. Hyllian et al., xBR / xBRZ — edge-directed pixel-art upscaling.",
        "[4] J. Kopf and D. Lischinski, Depixelizing Pixel Art, ACM TOG (SIGGRAPH), 2011.",
        "[5] F. Kreuzer, J. Kopf, M. Wimmer, Depixelizing Pixel Art in Real-Time, I3D, 2015.",
        "[6] W. Shi et al., ESPCN — Real-Time Single Image and Video Super-Resolution, CVPR, 2016.",
        "[7] SimpleNES — github.com/amhndu/SimpleNES.",
        "[8] NVIDIA Image Scaling (NIS) SDK — spatial upscaler reference.",
        "[9] RetroNNetScaler — github.com/amirsdream/RetroNNetScaler, 2026.",
    ]
    for r in refs:
        story.append(P("ref", r))

    story.append(Spacer(1, 8 * mm))
    story.append(HRFlowable(width="100%", thickness=0.5, color=s["rule"], spaceAfter=8))
    story.append(
        P(
            "meta",
            "Correspondence via the project repository. Training dumps must use "
            "legally obtained ROMs; ROMs are not redistributed with the project.",
        )
    )

    doc.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
    print("Wrote", OUT)


if __name__ == "__main__":
    build()
