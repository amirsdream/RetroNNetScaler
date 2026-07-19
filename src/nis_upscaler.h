#pragma once

#include <cstdint>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11SamplerState;

namespace scaler
{
class NisUpscaler
{
public:
    NisUpscaler() = default;
    ~NisUpscaler();

    bool init(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& shaderDir);
    void update(float sharpness, std::uint32_t inW, std::uint32_t inH, std::uint32_t outW, std::uint32_t outH);
    void dispatch(ID3D11ShaderResourceView* input, ID3D11UnorderedAccessView* output);

    bool ready() const { return m_cs != nullptr; }

private:
    ID3D11Device*              m_device  = nullptr;
    ID3D11DeviceContext*       m_context = nullptr;
    ID3D11ComputeShader*       m_cs      = nullptr;
    ID3D11Buffer*              m_cbuffer = nullptr;
    ID3D11Texture2D*           m_coefScale = nullptr;
    ID3D11Texture2D*           m_coefUsm   = nullptr;
    ID3D11ShaderResourceView*  m_coefScaleSRV = nullptr;
    ID3D11ShaderResourceView*  m_coefUsmSRV   = nullptr;
    ID3D11SamplerState*        m_sampler = nullptr;

    std::uint32_t m_blockW = 32;
    std::uint32_t m_blockH = 24;
    std::uint32_t m_outW   = 1;
    std::uint32_t m_outH   = 1;
};
}
