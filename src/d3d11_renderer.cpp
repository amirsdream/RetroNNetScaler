#include "d3d11_renderer.h"
#include "nis_upscaler.h"
#include "tiny_sr_upscaler.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_5.h>

#ifndef DXGI_PRESENT_ALLOW_TEARING
#define DXGI_PRESENT_ALLOW_TEARING 0x00000200UL
#endif
#ifndef DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING 2048
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace scaler
{
namespace
{
template <typename T>
void releaseT(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

const char* kFullscreenVS = R"(
struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};
VSOut main(float2 pos : POSITION, float2 uv : TEXCOORD0) {
    VSOut o;
    o.pos = float4(pos, 0, 1);
    o.uv = uv;
    return o;
}
)";

const char* kFullscreenPS = R"(
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    return tex0.Sample(samp0, uv);
}
)";

struct Vertex
{
    float x, y;
    float u, v;
};
}

D3D11Renderer::D3D11Renderer() = default;

D3D11Renderer::~D3D11Renderer()
{
    releaseT(m_layout);
    releaseT(m_vb);
    releaseT(m_ps);
    releaseT(m_vs);
    releaseT(m_sampLinear);
    releaseT(m_sampPoint);
    releaseT(m_outUAV);
    releaseT(m_outSRV);
    releaseT(m_outTex);
    releaseT(m_srcSRV);
    releaseT(m_srcTex);
    releaseT(m_rtv);
    releaseT(m_swap);
    releaseT(m_context);
    releaseT(m_device);
}

bool D3D11Renderer::create(HWND hwnd, int width, int height, bool vsync)
{
    m_vsync = vsync;
    if (!createDevice(hwnd, width, height))
        return false;
    if (!createFullscreenQuad())
        return false;
    return true;
}

bool D3D11Renderer::createDevice(HWND hwnd, int width, int height)
{
    DXGI_SWAP_CHAIN_DESC scd {};
    scd.BufferCount                        = 2;
    scd.BufferDesc.Width                   = width;
    scd.BufferDesc.Height                  = height;
    scd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                       = hwnd;
    scd.SampleDesc.Count                   = 1;
    scd.Windowed                           = TRUE;
    scd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr,
                                               D3D_DRIVER_TYPE_HARDWARE,
                                               nullptr,
                                               flags,
                                               levels,
                                               1,
                                               D3D11_SDK_VERSION,
                                               &scd,
                                               &m_swap,
                                               &m_device,
                                               &level,
                                               &m_context);
    if (FAILED(hr))
    {
        // Retry without tearing flag / with older swap effect.
        scd.Flags       = 0;
        hr              = D3D11CreateDeviceAndSwapChain(nullptr,
                                           D3D_DRIVER_TYPE_HARDWARE,
                                           nullptr,
                                           flags,
                                           levels,
                                           1,
                                           D3D11_SDK_VERSION,
                                           &scd,
                                           &m_swap,
                                           &m_device,
                                           &level,
                                           &m_context);
    }
    if (FAILED(hr))
    {
        scd.SwapEffect  = DXGI_SWAP_EFFECT_DISCARD;
        scd.BufferCount = 1;
        scd.Flags       = 0;
        hr              = D3D11CreateDeviceAndSwapChain(nullptr,
                                           D3D_DRIVER_TYPE_HARDWARE,
                                           nullptr,
                                           0,
                                           levels,
                                           1,
                                           D3D11_SDK_VERSION,
                                           &scd,
                                           &m_swap,
                                           &m_device,
                                           &level,
                                           &m_context);
        if (FAILED(hr))
        {
            std::cerr << "D3D11CreateDeviceAndSwapChain failed: 0x" << std::hex << hr << std::endl;
            return false;
        }
    }

    m_bbWidth  = width;
    m_bbHeight = height;

    ID3D11Texture2D* backbuffer = nullptr;
    m_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer));
    m_device->CreateRenderTargetView(backbuffer, nullptr, &m_rtv);
    releaseT(backbuffer);

    D3D11_SAMPLER_DESC sd {};
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD                               = D3D11_FLOAT32_MAX;
    sd.Filter                               = D3D11_FILTER_MIN_MAG_MIP_POINT;
    m_device->CreateSamplerState(&sd, &m_sampPoint);
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    m_device->CreateSamplerState(&sd, &m_sampLinear);
    return true;
}

void D3D11Renderer::resize(int width, int height)
{
    if (!m_swap || width <= 0 || height <= 0)
        return;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    releaseT(m_rtv);
    HRESULT hr = m_swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    if (FAILED(hr))
        hr = m_swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
        return;

    ID3D11Texture2D* backbuffer = nullptr;
    m_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer));
    m_device->CreateRenderTargetView(backbuffer, nullptr, &m_rtv);
    releaseT(backbuffer);
    m_bbWidth  = width;
    m_bbHeight = height;
}

bool D3D11Renderer::createSourceTexture(int width, int height)
{
    releaseT(m_srcSRV);
    releaseT(m_srcTex);
    m_srcW = width;
    m_srcH = height;

    D3D11_TEXTURE2D_DESC td {};
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_device->CreateTexture2D(&td, nullptr, &m_srcTex)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv {};
    srv.Format              = td.Format;
    srv.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    return SUCCEEDED(m_device->CreateShaderResourceView(m_srcTex, &srv, &m_srcSRV));
}

bool D3D11Renderer::ensureOutputTexture(int width, int height)
{
    if (m_outTex && m_outW == width && m_outH == height)
        return true;

    releaseT(m_outUAV);
    releaseT(m_outSRV);
    releaseT(m_outTex);
    m_outW = width;
    m_outH = height;

    D3D11_TEXTURE2D_DESC td {};
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    if (FAILED(m_device->CreateTexture2D(&td, nullptr, &m_outTex)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv {};
    srv.Format              = td.Format;
    srv.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    if (FAILED(m_device->CreateShaderResourceView(m_outTex, &srv, &m_outSRV)))
        return false;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav {};
    uav.Format        = td.Format;
    uav.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    return SUCCEEDED(m_device->CreateUnorderedAccessView(m_outTex, &uav, &m_outUAV));
}

bool D3D11Renderer::createFullscreenQuad()
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* err    = nullptr;
    if (FAILED(D3DCompile(kFullscreenVS, std::strlen(kFullscreenVS), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0,
                          &vsBlob, &err)))
    {
        if (err)
        {
            std::cerr << (char*)err->GetBufferPointer() << std::endl;
            err->Release();
        }
        return false;
    }
    if (FAILED(D3DCompile(kFullscreenPS, std::strlen(kFullscreenPS), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0,
                          &psBlob, &err)))
    {
        if (err)
        {
            std::cerr << (char*)err->GetBufferPointer() << std::endl;
            err->Release();
        }
        vsBlob->Release();
        return false;
    }

    m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout);
    vsBlob->Release();
    psBlob->Release();

    D3D11_BUFFER_DESC bd {};
    bd.ByteWidth = sizeof(Vertex) * 6;
    bd.Usage     = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(m_device->CreateBuffer(&bd, nullptr, &m_vb));
}

void D3D11Renderer::integerFit(int srcW, int srcH, int& outW, int& outH, int& scale) const
{
    if (srcW <= 0 || srcH <= 0 || m_bbWidth <= 0 || m_bbHeight <= 0)
    {
        outW = outH = scale = 1;
        return;
    }
    scale = (std::max)(1, (std::min)(m_bbWidth / srcW, m_bbHeight / srcH));
    outW  = srcW * scale;
    outH  = srcH * scale;
}

void D3D11Renderer::letterboxPixels(int srcW, int srcH, int& outW, int& outH) const
{
    int scale = 1;
    integerFit(srcW, srcH, outW, outH, scale);
}

void D3D11Renderer::uploadFrame(const std::uint8_t* rgba, int width, int height)
{
    if (!m_srcTex || m_srcW != width || m_srcH != height)
    {
        if (!createSourceTexture(width, height))
            return;
    }

    m_context->UpdateSubresource(m_srcTex, 0, nullptr, rgba, width * 4, 0);
}

void D3D11Renderer::drawPixelPerfect(ID3D11ShaderResourceView* srv,
                                     bool nearest,
                                     int texW,
                                     int texH,
                                     int destW,
                                     int destH,
                                     int phaseX,
                                     int phaseY)
{
    (void)texW;
    (void)texH;

    // Integer-centered placement (OSSC-style). Phase nudges sampling grid by whole pixels.
    int ox = (m_bbWidth - destW) / 2 + phaseX;
    int oy = (m_bbHeight - destH) / 2 + phaseY;
    ox     = (std::max)(0, (std::min)(ox, (std::max)(0, m_bbWidth - destW)));
    oy     = (std::max)(0, (std::min)(oy, (std::max)(0, m_bbHeight - destH)));

    // Full NDC quad inside a pixel-aligned viewport → 1:1 or exact integer resampling phase.
    const Vertex verts[6] = {
        { -1.f, 1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 0.f }, { -1.f, -1.f, 0.f, 1.f },
        { 1.f, 1.f, 1.f, 0.f },  { 1.f, -1.f, 1.f, 1.f }, { -1.f, -1.f, 0.f, 1.f },
    };

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (SUCCEEDED(m_context->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, verts, sizeof(verts));
        m_context->Unmap(m_vb, 0);
    }

    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    const float clear[4] = { 0.f, 0.f, 0.f, 1.f };
    m_context->ClearRenderTargetView(m_rtv, clear);

    D3D11_VIEWPORT vp {};
    vp.TopLeftX = float(ox);
    vp.TopLeftY = float(oy);
    vp.Width    = float(destW);
    vp.Height   = float(destH);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    m_context->RSSetViewports(1, &vp);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetInputLayout(m_layout);
    m_context->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vs, nullptr, 0);
    m_context->PSSetShader(m_ps, nullptr, 0);
    m_context->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* samp = nearest ? m_sampPoint : m_sampLinear;
    m_context->PSSetSamplers(0, 1, &samp);
    m_context->Draw(6, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}

void D3D11Renderer::present(ScaleMode mode, float sharpness, NisUpscaler* nis, TinySrUpscaler* tinySr, int phaseX, int phaseY)
{
    if (!m_srcSRV)
        return;

    ID3D11ShaderResourceView* displaySRV = m_srcSRV;
    bool                      nearest    = true;
    int                       texW       = m_srcW;
    int                       texH       = m_srcH;
    int                       destW = 0, destH = 0, scale = 1;
    integerFit(m_srcW, m_srcH, destW, destH, scale);
    m_lastScale = scale;

    if (mode == ScaleMode::TinySR || mode == ScaleMode::Smooth || mode == ScaleMode::Neural)
    {
        std::uint32_t srMode = 0u;
        if (mode == ScaleMode::Smooth)
            srMode = 1u;
        else if (mode == ScaleMode::Neural)
            srMode = 2u;
        if (tinySr && tinySr->ready() && ensureOutputTexture(destW, destH))
        {
            tinySr->dispatch(m_srcSRV,
                             m_outUAV,
                             m_srcW,
                             m_srcH,
                             destW,
                             destH,
                             (std::uint32_t)scale,
                             sharpness,
                             srMode);
            displaySRV = m_outSRV;
            texW       = destW;
            texH       = destH;
            nearest    = true;
        }
    }
    else if (mode == ScaleMode::NIS || mode == ScaleMode::DLSS)
    {
        // NIS may soft-scale; still present in integer rect for stable framing.
        if (nis && nis->ready() && ensureOutputTexture(destW, destH))
        {
            nis->update(sharpness, m_srcW, m_srcH, destW, destH);
            nis->dispatch(m_srcSRV, m_outUAV);
            displaySRV = m_outSRV;
            texW       = destW;
            texH       = destH;
            nearest    = false;
        }
        else
        {
            nearest = false;
        }
    }
    else if (mode == ScaleMode::Bilinear)
    {
        nearest = false;
    }
    else
    {
        // Nearest: point-sample source into integer viewport (exact s×s blocks).
        nearest = true;
        texW    = m_srcW;
        texH    = m_srcH;
    }

    drawPixelPerfect(displaySRV, nearest, texW, texH, destW, destH, phaseX, phaseY);
    // Always Present(0): on 144/165Hz panels Present(1) returns every refresh and the
    // game loop can run ~165 NES frames/sec. Emulation rate is paced in main().
    // Optional vsync: still Present(0); tearing is acceptable vs wrong game speed.
    UINT flags = 0;
    if (!m_vsync)
        flags |= DXGI_PRESENT_ALLOW_TEARING;
    // ALLOW_TEARING requires flip model + not fullscreen exclusive; ignore failure.
    if (FAILED(m_swap->Present(0, flags)))
        m_swap->Present(0, 0);
}
}
