#pragma once

#include <cstdint>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Buffer;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

namespace scaler
{
class TinySrUpscaler
{
public:
    TinySrUpscaler() = default;
    ~TinySrUpscaler();

    bool init(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& shaderPath);
    // mode: 0 = Clarity, 1 = Smooth xBR-sharp, 2 = Neural TinySR
    void dispatch(ID3D11ShaderResourceView* input,
                  ID3D11UnorderedAccessView* output,
                  std::uint32_t srcW,
                  std::uint32_t srcH,
                  std::uint32_t dstW,
                  std::uint32_t dstH,
                  std::uint32_t scale,
                  float sharpen,
                  std::uint32_t mode);

    bool ready() const { return m_cs != nullptr; }
    bool neuralReady() const { return m_weightSRV != nullptr; }

private:
    bool loadWeights(const std::string& binPath);

    ID3D11Device*             m_device     = nullptr;
    ID3D11DeviceContext*      m_context    = nullptr;
    ID3D11ComputeShader*      m_cs         = nullptr;
    ID3D11Buffer*             m_cbuffer    = nullptr;
    ID3D11SamplerState*       m_sampler    = nullptr;
    ID3D11Buffer*             m_weightBuf  = nullptr;
    ID3D11ShaderResourceView* m_weightSRV  = nullptr;
    std::uint32_t             m_channels   = 16;
    std::uint32_t             m_netScale   = 4;
};
}
