// Clarity / Smooth / Contour upscaler
// mode 0 = Clarity: integer nearest
// mode 1 = Smooth: sharp xBR (hard palette, tip wedges)
// mode 2 = Contour: Kopf hard Voronoi — sharp fills, straight diagonal edges (no cream)
//
// Weight layout (tinysr_weights.bin): W1[864] B1[32] W2[9216] B2[32] W3[41472] B3[144]
// (C=32, s=4, classes=9)  — W3 out = 9*s*s

cbuffer TinySRParams : register(b0)
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
    uint scale;
    float sharpen;
    uint mode;
    uint netScale; // neural pixel-shuffle scale (usually 4)
};

Texture2D<float4> g_input : register(t0);
StructuredBuffer<float> g_weights : register(t1); // optional / unused by Contour
RWTexture2D<float4> g_output : register(u0);
SamplerState g_linear : register(s0);

float3 fetch(int2 p)
{
    p = clamp(p, int2(0, 0), int2(srcWidth, srcHeight) - 1);
    return g_input.Load(int3(p, 0)).rgb;
}

float3 rgb2yuv(float3 c)
{
    float y = dot(c, float3(0.299f, 0.587f, 0.114f));
    float u = (c.b - y) * 0.565f;
    float v = (c.r - y) * 0.713f;
    return float3(y, u, v);
}

float cdist(float3 a, float3 b)
{
    float3 d = rgb2yuv(a) - rgb2yuv(b);
    return abs(d.x) + abs(d.y) + abs(d.z);
}

bool ceq(float3 a, float3 b)
{
    // Tight: NES colors are discrete — stops “almost same” dirty joins
    return cdist(a, b) < (10.0f / 255.0f);
}

float wd(float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h)
{
    return cdist(a, b) + cdist(a, c) + cdist(d, e) + cdist(d, f) + 4.0f * cdist(g, h);
}

// Keep only exact neighborhood texels (no mid-tones / cream)
float3 snapExact9(float3 c, float3 A, float3 B, float3 C, float3 D, float3 E, float3 F, float3 G, float3 H, float3 I)
{
    float3 best = E;
    float bd = cdist(c, E);
#define TRY(P)                     \
    {                              \
        float d = cdist(c, P);     \
        if (d < bd)                \
        {                          \
            bd = d;                \
            best = P;              \
        }                          \
    }
    TRY(A) TRY(B) TRY(C) TRY(D) TRY(F) TRY(G) TRY(H) TRY(I)
#undef TRY
    return best;
}

float3 xbrSharp(int2 src, float fx, float fy)
{
    float3 A1 = fetch(src + int2(-1, -2));
    float3 B1 = fetch(src + int2(0, -2));
    float3 C1 = fetch(src + int2(1, -2));

    float3 A = fetch(src + int2(-1, -1));
    float3 B = fetch(src + int2(0, -1));
    float3 C = fetch(src + int2(1, -1));
    float3 D = fetch(src + int2(-1, 0));
    float3 E = fetch(src);
    float3 F = fetch(src + int2(1, 0));
    float3 G = fetch(src + int2(-1, 1));
    float3 H = fetch(src + int2(0, 1));
    float3 I = fetch(src + int2(1, 1));

    float3 G5 = fetch(src + int2(-1, 2));
    float3 H5 = fetch(src + int2(0, 2));
    float3 I5 = fetch(src + int2(1, 2));
    float3 A0 = fetch(src + int2(-2, -1));
    float3 D0 = fetch(src + int2(-2, 0));
    float3 G0 = fetch(src + int2(-2, 1));
    float3 C4 = fetch(src + int2(2, -1));
    float3 F4 = fetch(src + int2(2, 0));
    float3 I4 = fetch(src + int2(2, 1));

    float3 res = E;

    // Tip threshold: only outer corner of each quadrant (keeps fills/borders sharp)
    const float tip = 1.0f; // q.x+q.y >= tip  → 45° wedge only

    // ---------- BR ----------
    if (fx >= 0.5f && fy >= 0.5f)
    {
        float2 q = float2(fx - 0.5f, fy - 0.5f) * 2.0f;
        bool ir = !ceq(E, F) || !ceq(E, H);
        bool edr = (wd(E, C, G, I, H5, F4, H, F) < wd(H, D, I5, F, I4, B, E, I)) && ir;

        // Outline / dirty-border guard: high contrast corner needs diagonal twin
        bool strongBorder = (cdist(E, F) > 0.12f && cdist(E, H) > 0.12f);
        if (strongBorder && !ceq(E, I) && !(ceq(F, H)))
            edr = false;

        if (edr && (q.x + q.y) >= tip)
        {
            // Prefer pure diagonal twin when present (clean stroke)
            if (ceq(E, I) && !ceq(F, H))
                res = I;
            else
                res = (cdist(E, F) <= cdist(E, H)) ? F : H;

            // 2:1 / 1:2 tips (only if clearly dominated)
            bool steep = (cdist(F, G) >= 2.0f * cdist(H, C)) && !ceq(E, C);
            bool shallow = (2.0f * cdist(F, G) <= cdist(H, C)) && !ceq(E, G);
            if (steep && q.y > q.x && q.x < 0.55f)
                res = H;
            else if (shallow && q.x > q.y && q.y < 0.55f)
                res = F;
        }
    }
    // ---------- BL ----------
    else if (fx < 0.5f && fy >= 0.5f)
    {
        float2 q = float2(0.5f - fx, fy - 0.5f) * 2.0f;
        bool ir = !ceq(E, D) || !ceq(E, H);
        bool edr = (wd(E, A, I, G, H5, D0, H, D) < wd(H, F, G5, D, G0, B, E, G)) && ir;
        bool strongBorder = (cdist(E, D) > 0.12f && cdist(E, H) > 0.12f);
        if (strongBorder && !ceq(E, G) && !(ceq(D, H)))
            edr = false;

        if (edr && (q.x + q.y) >= tip)
        {
            if (ceq(E, G) && !ceq(D, H))
                res = G;
            else
                res = (cdist(E, D) <= cdist(E, H)) ? D : H;

            bool steep = (cdist(D, I) >= 2.0f * cdist(H, A)) && !ceq(E, A);
            bool shallow = (2.0f * cdist(D, I) <= cdist(H, A)) && !ceq(E, I);
            if (steep && q.y > q.x && q.x < 0.55f)
                res = H;
            else if (shallow && q.x > q.y && q.y < 0.55f)
                res = D;
        }
    }
    // ---------- TR ----------
    else if (fx >= 0.5f && fy < 0.5f)
    {
        float2 q = float2(fx - 0.5f, 0.5f - fy) * 2.0f;
        bool ir = !ceq(E, B) || !ceq(E, F);
        bool edr = (wd(E, G, C, I, F4, B1, F, B) < wd(F, D, I4, B, C4, H, E, C)) && ir;
        bool strongBorder = (cdist(E, B) > 0.12f && cdist(E, F) > 0.12f);
        if (strongBorder && !ceq(E, C) && !(ceq(B, F)))
            edr = false;

        if (edr && (q.x + q.y) >= tip)
        {
            if (ceq(E, C) && !ceq(B, F))
                res = C;
            else
                res = (cdist(E, B) <= cdist(E, F)) ? B : F;

            bool steep = (cdist(B, I) >= 2.0f * cdist(F, G)) && !ceq(E, G);
            bool shallow = (2.0f * cdist(B, I) <= cdist(F, G)) && !ceq(E, I);
            if (steep && q.y > q.x && q.x < 0.55f)
                res = F;
            else if (shallow && q.x > q.y && q.y < 0.55f)
                res = B;
        }
    }
    // ---------- TL ----------
    else
    {
        float2 q = float2(0.5f - fx, 0.5f - fy) * 2.0f;
        bool ir = !ceq(E, B) || !ceq(E, D);
        bool edr = (wd(E, I, A, G, D0, B1, D, B) < wd(D, F, G0, B, A0, H, E, A)) && ir;
        bool strongBorder = (cdist(E, B) > 0.12f && cdist(E, D) > 0.12f);
        if (strongBorder && !ceq(E, A) && !(ceq(B, D)))
            edr = false;

        if (edr && (q.x + q.y) >= tip)
        {
            if (ceq(E, A) && !ceq(B, D))
                res = A;
            else
                res = (cdist(E, B) <= cdist(E, D)) ? B : D;

            bool steep = (cdist(B, G) >= 2.0f * cdist(D, I)) && !ceq(E, I);
            bool shallow = (2.0f * cdist(B, G) <= cdist(D, I)) && !ceq(E, G);
            if (steep && q.y > q.x && q.x < 0.55f)
                res = D;
            else if (shallow && q.x > q.y && q.y < 0.55f)
                res = B;
        }
    }

    return snapExact9(res, A, B, C, D, E, F, G, H, I);
}

// Contour: Kopf–Lischinski hard reshape (no cream).
// Similarity graph → planarize crossing diagonals → generalized Voronoi on
// node + half-edges. Long 45° stairs share one straight edge (not EPX zigzags).

bool similarYUV(float3 a, float3 b)
{
    // Kopf / hqx channel thresholds
    float3 d = abs(rgb2yuv(a) - rgb2yuv(b));
    return d.x <= (48.0f / 255.0f) && d.y <= (7.0f / 255.0f) && d.z <= (6.0f / 255.0f);
}

// 2x2 layout: a b
//             d e
// returns: 0 = no diagonal, 1 = keep a–e, 2 = keep b–d
uint planarDiag(float3 a, float3 b, float3 d, float3 e)
{
    const bool ae = similarYUV(a, e);
    const bool bd = similarYUV(b, d);
    if (!ae && !bd)
        return 0u;

    const bool ab = similarYUV(a, b);
    const bool be = similarYUV(b, e);
    const bool ad = similarYUV(a, d);
    const bool de = similarYUV(d, e);

    // Fully connected shaded block — diagonals unused
    if (ab && be && ad && de)
        return 0u;

    if (ae && !bd)
        return 1u;
    if (bd && !ae)
        return 2u;

    // Both diagonals cross — choose (curves / sparse / islands, local approx)
    const bool onlyDiag = !(ab || be || ad || de);
    if (!onlyDiag)
        return 0u;

    uint sa = (ab ? 1u : 0u) + (ad ? 1u : 0u);
    uint se = (be ? 1u : 0u) + (de ? 1u : 0u);
    uint sb = (ab ? 1u : 0u) + (be ? 1u : 0u);
    uint sd = (ad ? 1u : 0u) + (de ? 1u : 0u);
    int islandAE = ((sa == 0u) || (se == 0u)) ? 5 : 0;
    int islandBD = ((sb == 0u) || (sd == 0u)) ? 5 : 0;
    float curveAE = 4.0f / (1.0f + 32.0f * cdist(a, e));
    float curveBD = 4.0f / (1.0f + 32.0f * cdist(b, d));
    float scoreAE = curveAE + (float)islandAE - (float)(sa + se);
    float scoreBD = curveBD + (float)islandBD - (float)(sb + sd);
    if (scoreAE > scoreBD + 1e-4f)
        return 1u;
    if (scoreBD > scoreAE + 1e-4f)
        return 2u;
    return 0u;
}

float3 contourHard(int2 src, float fx, float fy)
{
    const float3 A = fetch(src + int2(-1, -1));
    const float3 B = fetch(src + int2(0, -1));
    const float3 C = fetch(src + int2(1, -1));
    const float3 D = fetch(src + int2(-1, 0));
    const float3 E = fetch(src);
    const float3 F = fetch(src + int2(1, 0));
    const float3 G = fetch(src + int2(-1, 1));
    const float3 H = fetch(src + int2(0, 1));
    const float3 I = fetch(src + int2(1, 1));

    // Four 2×2 blocks around E
    const uint kTL = planarDiag(A, B, D, E);
    const uint kTR = planarDiag(B, C, E, F);
    const uint kBL = planarDiag(D, E, G, H);
    const uint kBR = planarDiag(E, F, H, I);

    // Global axes so both sides of a stair share ONE edge (fixes EPX zigzag)
    const float2 p = float2(src) + float2(fx, fy);
    const float U = p.x + p.y;
    const float V = p.x - p.y;
    const float Us = float(src.x + src.y);
    const float Vs = float(src.x - src.y);

    float3 res = E;
    bool hit = false;

    // / stairs — constant U, bisector at *.5 so adjacent cells agree
    if (kTL == 2u && U < Us + 0.5f) { res = B; hit = true; } // keep B–D
    if (kBR == 2u && U > Us + 1.5f) { res = F; hit = true; } // keep F–H
    if (kTL == 1u && U < Us + 0.5f) { res = A; hit = true; } // keep A–E stroke
    if (kBR == 1u && U > Us + 1.5f) { res = I; hit = true; } // keep E–I stroke

    // \ stairs — constant V
    if (kTR == 1u && V > Vs + 0.5f) { res = B; hit = true; } // keep B–F
    if (kBL == 1u && V < Vs - 0.5f) { res = D; hit = true; } // keep D–H
    if (kTR == 2u && V > Vs + 0.5f) { res = C; hit = true; } // keep C–E stroke
    if (kBL == 2u && V < Vs - 0.5f) { res = G; hit = true; } // keep E–G stroke

    // 2:1 / 1:2 hard wedges when no 45° edge claimed this sample
    if (!hit)
    {
        if (fx >= 0.5f && fy >= 0.5f && !ceq(E, F) && !ceq(E, H) && !ceq(F, H))
        {
            float qx = (fx - 0.5f) * 2.0f;
            float qy = (fy - 0.5f) * 2.0f;
            if (similarYUV(E, I) && !similarYUV(F, H))
            {
                if (qx + qy >= 1.0f)
                    res = I;
            }
            else
            {
                bool steep = cdist(F, G) >= 2.0f * cdist(H, C);
                bool shallow = 2.0f * cdist(F, G) <= cdist(H, C);
                if (steep && qy > 2.0f * qx)
                    res = H;
                else if (shallow && qx > 2.0f * qy)
                    res = F;
                else if (qx + qy >= 1.0f)
                    res = (cdist(E, F) <= cdist(E, H)) ? F : H;
            }
        }
        else if (fx < 0.5f && fy >= 0.5f && !ceq(E, D) && !ceq(E, H) && !ceq(D, H))
        {
            float qx = (0.5f - fx) * 2.0f;
            float qy = (fy - 0.5f) * 2.0f;
            if (similarYUV(E, G) && !similarYUV(D, H))
            {
                if (qx + qy >= 1.0f)
                    res = G;
            }
            else
            {
                bool steep = cdist(D, I) >= 2.0f * cdist(H, A);
                bool shallow = 2.0f * cdist(D, I) <= cdist(H, A);
                if (steep && qy > 2.0f * qx)
                    res = H;
                else if (shallow && qx > 2.0f * qy)
                    res = D;
                else if (qx + qy >= 1.0f)
                    res = (cdist(E, D) <= cdist(E, H)) ? D : H;
            }
        }
        else if (fx >= 0.5f && fy < 0.5f && !ceq(E, B) && !ceq(E, F) && !ceq(B, F))
        {
            float qx = (fx - 0.5f) * 2.0f;
            float qy = (0.5f - fy) * 2.0f;
            if (similarYUV(E, C) && !similarYUV(B, F))
            {
                if (qx + qy >= 1.0f)
                    res = C;
            }
            else
            {
                bool steep = cdist(B, I) >= 2.0f * cdist(F, G);
                bool shallow = 2.0f * cdist(B, I) <= cdist(F, G);
                if (steep && qy > 2.0f * qx)
                    res = F;
                else if (shallow && qx > 2.0f * qy)
                    res = B;
                else if (qx + qy >= 1.0f)
                    res = (cdist(E, B) <= cdist(E, F)) ? B : F;
            }
        }
        else if (fx < 0.5f && fy < 0.5f && !ceq(E, B) && !ceq(E, D) && !ceq(B, D))
        {
            float qx = (0.5f - fx) * 2.0f;
            float qy = (0.5f - fy) * 2.0f;
            if (similarYUV(E, A) && !similarYUV(B, D))
            {
                if (qx + qy >= 1.0f)
                    res = A;
            }
            else
            {
                bool steep = cdist(B, G) >= 2.0f * cdist(D, I);
                bool shallow = 2.0f * cdist(B, G) <= cdist(D, I);
                if (steep && qy > 2.0f * qx)
                    res = D;
                else if (shallow && qx > 2.0f * qy)
                    res = B;
                else if (qx + qy >= 1.0f)
                    res = (cdist(E, B) <= cdist(E, D)) ? B : D;
            }
        }
    }

    return res;
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    const uint s = max(scale, 1u);

    if (tid.x >= dstWidth || tid.y >= dstHeight)
        return;

    const int2 src = int2(tid.x / s, tid.y / s);
    float3 outRgb = fetch(src);

    if (mode == 1u || mode == 2u)
    {
        float fx = (s == 1) ? 0.5f : (float(tid.x % s) + 0.5f) / float(s);
        float fy = (s == 1) ? 0.5f : (float(tid.y % s) + 0.5f) / float(s);
        if (mode == 1u)
            outRgb = xbrSharp(src, fx, fy);
        else
            outRgb = contourHard(src, fx, fy);
    }

    g_output[tid.xy] = float4(outRgb, 1.0f);
}
