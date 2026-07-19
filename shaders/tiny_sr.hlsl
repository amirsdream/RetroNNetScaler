// Clarity / Smooth / Neural upscaler
// mode 0 = Clarity: integer nearest
// mode 1 = Smooth: sharp xBR (hard palette)
// mode 2 = Neural CNN (experimental): tip-gated classifier — prefer Smooth/xBR until stable
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
StructuredBuffer<float> g_weights : register(t1);
RWTexture2D<float4> g_output : register(u0);
SamplerState g_linear : register(s0);

static const uint kC = 32;
static const uint kClasses = 9;
static const uint kOffW1 = 0;
static const uint kOffB1 = 864;
static const uint kOffW2 = 896;
static const uint kOffB2 = 10112;
static const uint kOffW3 = 10144;
static const uint kOffB3 = 51616; // 10144 + 32*144*9

float W1(uint i) { return g_weights[kOffW1 + i]; }
float B1(uint i) { return g_weights[kOffB1 + i]; }
float W2(uint i) { return g_weights[kOffW2 + i]; }
float B2(uint i) { return g_weights[kOffB2 + i]; }
float W3(uint i) { return g_weights[kOffW3 + i]; }
float B3(uint i) { return g_weights[kOffB3 + i]; }

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

// ---- TinySR CNN (matches tools/train/model.py, C=16, 3 convs) ----
float relu(float x) { return max(x, 0.0f); }

void conv1_at(int2 p, out float h[32])
{
    [loop] for (int oc = 0; oc < 32; ++oc)
    {
        float acc = B1(oc);
        [loop] for (int ky = -1; ky <= 1; ++ky)
            [loop] for (int kx = -1; kx <= 1; ++kx)
            {
                float3 c = fetch(p + int2(kx, ky));
                [loop] for (int ic = 0; ic < 3; ++ic)
                {
                    uint idx = uint((((oc * 3 + ic) * 3 + (ky + 1)) * 3 + (kx + 1)));
                    acc += W1(idx) * c[ic];
                }
            }
        h[oc] = relu(acc);
    }
}

// Returns argmax class id [0..8] per 4x4 subpixel (A..I neighborhood)
void tinysr_classes(int2 p, out uint cls[16])
{
    float f1[9][32];
    [loop] for (int dy = -1; dy <= 1; ++dy)
        [loop] for (int dx = -1; dx <= 1; ++dx)
            conv1_at(p + int2(dx, dy), f1[(dy + 1) * 3 + (dx + 1)]);

    float f2c[32];
    [loop] for (int oc = 0; oc < 32; ++oc)
    {
        float acc = B2(oc);
        [loop] for (int ky = -1; ky <= 1; ++ky)
            [loop] for (int kx = -1; kx <= 1; ++kx)
            {
                int id = (ky + 1) * 3 + (kx + 1);
                [loop] for (int ic = 0; ic < 32; ++ic)
                {
                    uint widx = uint((((oc * 32 + ic) * 3 + (ky + 1)) * 3 + (kx + 1)));
                    acc += W2(widx) * f1[id][ic];
                }
            }
        f2c[oc] = relu(acc);
    }

    // 9 classes × 16 subpixels (pixel_shuffle layout: class-major then oy,ox)
    const int nOut = 144; // 9 * 4 * 4
    float ch[144];
    [loop] for (int oc = 0; oc < nOut; ++oc)
    {
        float acc = B3(oc);
        [loop] for (int ky = -1; ky <= 1; ++ky)
            [loop] for (int kx = -1; kx <= 1; ++kx)
                [loop] for (int ic = 0; ic < 32; ++ic)
                {
                    uint widx = uint((((oc * 32 + ic) * 3 + (ky + 1)) * 3 + (kx + 1)));
                    acc += W3(widx) * f2c[ic];
                }
        ch[oc] = acc;
    }

    [loop] for (int oy = 0; oy < 4; ++oy)
        [loop] for (int ox = 0; ox < 4; ++ox)
        {
            uint best = 4u; // E
            float bestv = ch[(4 * 4 + oy) * 4 + ox];
            float vE = bestv;
            [loop] for (uint c = 0u; c < kClasses; ++c)
            {
                float v = ch[(c * 4 + oy) * 4 + ox];
                if (v > bestv)
                {
                    bestv = v;
                    best = c;
                }
            }
            // Strong confidence gate — weak scores caused rainbow edge fringes
            if (best != 4u && bestv < vE + 1.25f)
                best = 4u;
            cls[oy * 4 + ox] = best;
        }
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    const uint s = max(scale, 1u);

    if (mode == 2u)
    {
        // Safe Neural: tip-gated CNN. Wrong neighbor classes were painting
        // multicolor "explosions" on glyph/sprite edges — only accept a non-E
        // class on outer tip wedges, matching Scale2x/xBR geometry, with a
        // strong margin over center (E).
        if (tid.x >= srcWidth || tid.y >= srcHeight)
            return;
        int2 lp = int2(tid.xy);
        float3 neigh[9];
        neigh[0] = fetch(lp + int2(-1, -1));
        neigh[1] = fetch(lp + int2(0, -1));
        neigh[2] = fetch(lp + int2(1, -1));
        neigh[3] = fetch(lp + int2(-1, 0));
        neigh[4] = fetch(lp);
        neigh[5] = fetch(lp + int2(1, 0));
        neigh[6] = fetch(lp + int2(-1, 1));
        neigh[7] = fetch(lp + int2(0, 1));
        neigh[8] = fetch(lp + int2(1, 1));
        uint cls[16];
        tinysr_classes(lp, cls);

        [loop] for (uint oy = 0u; oy < s; ++oy)
            [loop] for (uint ox = 0u; ox < s; ++ox)
            {
                uint ns = max(netScale, 1u);
                uint nx = (ox * ns) / s;
                uint ny = (oy * ns) / s;
                nx = min(nx, ns - 1u);
                ny = min(ny, ns - 1u);

                // Position inside the LQ cell [0,1]
                float fx = (s == 1u) ? 0.5f : (float(ox) + 0.5f) / float(s);
                float fy = (s == 1u) ? 0.5f : (float(oy) + 0.5f) / float(s);

                uint c = 4u; // default: center color (no fringe)
                uint raw = min(cls[ny * 4u + nx], 8u);

                // Outer tip of each quadrant only (same idea as xBR tip wedges)
                const float tip = 0.85f;
                bool allow = false;
                if (fx >= 0.5f && fy >= 0.5f)
                {
                    float2 q = float2(fx - 0.5f, fy - 0.5f) * 2.0f;
                    allow = (q.x + q.y) >= tip && (raw == 5u || raw == 7u || raw == 8u); // F,H,I
                }
                else if (fx < 0.5f && fy >= 0.5f)
                {
                    float2 q = float2(0.5f - fx, fy - 0.5f) * 2.0f;
                    allow = (q.x + q.y) >= tip && (raw == 3u || raw == 7u || raw == 6u); // D,H,G
                }
                else if (fx < 0.5f && fy < 0.5f)
                {
                    float2 q = float2(0.5f - fx, 0.5f - fy) * 2.0f;
                    allow = (q.x + q.y) >= tip && (raw == 1u || raw == 3u || raw == 0u); // B,D,A
                }
                else
                {
                    float2 q = float2(fx - 0.5f, 0.5f - fy) * 2.0f;
                    allow = (q.x + q.y) >= tip && (raw == 1u || raw == 5u || raw == 2u); // B,F,C
                }

                if (allow)
                    c = raw;

                // Reject painting a totally different palette into fills:
                // neighbor must share an edge with E in the LQ grid sense —
                // already enforced by tip classes; also skip if equal to E.
                float3 outc = neigh[c];
                if (c != 4u && cdist(outc, neigh[4]) < (2.0f / 255.0f))
                    outc = neigh[4];

                int2 dp = int2(lp.x * int(s) + int(ox), lp.y * int(s) + int(oy));
                if (dp.x < int(dstWidth) && dp.y < int(dstHeight))
                    g_output[dp] = float4(outc, 1.0f);
            }
        return;
    }

    if (tid.x >= dstWidth || tid.y >= dstHeight)
        return;

    const int2 src = int2(tid.x / s, tid.y / s);
    float3 outRgb = fetch(src);

    if (mode == 1u)
    {
        float fx = (s == 1) ? 0.5f : (float(tid.x % s) + 0.5f) / float(s);
        float fy = (s == 1) ? 0.5f : (float(tid.y % s) + 0.5f) / float(s);
        outRgb = xbrSharp(src, fx, fy);
    }

    g_output[tid.xy] = float4(outRgb, 1.0f);
}
