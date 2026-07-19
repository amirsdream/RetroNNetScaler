#pragma once

#include "prefs.h"

#include <cstdint>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11SamplerState;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11Buffer;
struct ID3D11InputLayout;

namespace scaler
{
class NisUpscaler;
class TinySrUpscaler;

class D3D11Renderer
{
public:
    D3D11Renderer();
    ~D3D11Renderer();

    bool create(HWND hwnd, int width, int height, bool vsync);
    void resize(int width, int height);
    void setVsync(bool vsync) { m_vsync = vsync; }

    void uploadFrame(const std::uint8_t* rgba, int width, int height);
    void present(ScaleMode mode, float sharpness, NisUpscaler* nis, TinySrUpscaler* tinySr, int phaseX, int phaseY);

    ID3D11Device*        device() const { return m_device; }
    ID3D11DeviceContext* context() const { return m_context; }

    int backbufferWidth() const { return m_bbWidth; }
    int backbufferHeight() const { return m_bbHeight; }
    int lastIntegerScale() const { return m_lastScale; }

    ID3D11ShaderResourceView* sourceSRV() const { return m_srcSRV; }
    ID3D11UnorderedAccessView* outputUAV() const { return m_outUAV; }
    ID3D11ShaderResourceView* outputSRV() const { return m_outSRV; }

    bool ensureOutputTexture(int width, int height);

private:
    bool createDevice(HWND hwnd, int width, int height);
    bool createSourceTexture(int width, int height);
    bool createFullscreenQuad();
    // Pixel-perfect: integer scale fit + integer-centered viewport (OSSC-style phase lock).
    void drawPixelPerfect(ID3D11ShaderResourceView* srv,
                          bool nearest,
                          int texW,
                          int texH,
                          int destW,
                          int destH,
                          int phaseX,
                          int phaseY);
    void integerFit(int srcW, int srcH, int& outW, int& outH, int& scale) const;
    void letterboxPixels(int srcW, int srcH, int& outW, int& outH) const;

    ID3D11Device*              m_device   = nullptr;
    ID3D11DeviceContext*       m_context  = nullptr;
    IDXGISwapChain*            m_swap     = nullptr;
    ID3D11RenderTargetView*    m_rtv      = nullptr;
    ID3D11Texture2D*           m_srcTex   = nullptr;
    ID3D11ShaderResourceView*  m_srcSRV   = nullptr;
    ID3D11Texture2D*           m_outTex   = nullptr;
    ID3D11ShaderResourceView*  m_outSRV   = nullptr;
    ID3D11UnorderedAccessView* m_outUAV   = nullptr;
    ID3D11SamplerState*        m_sampPoint = nullptr;
    ID3D11SamplerState*        m_sampLinear = nullptr;
    ID3D11VertexShader*        m_vs       = nullptr;
    ID3D11PixelShader*         m_ps       = nullptr;
    ID3D11Buffer*              m_vb       = nullptr;
    ID3D11InputLayout*         m_layout   = nullptr;

    int  m_bbWidth  = 0;
    int  m_bbHeight = 0;
    int  m_srcW     = 0;
    int  m_srcH     = 0;
    int  m_outW      = 0;
    int  m_outH      = 0;
    int  m_lastScale = 1;
    bool m_vsync     = true;
};
}
