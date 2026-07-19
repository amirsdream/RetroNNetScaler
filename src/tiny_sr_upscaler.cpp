#include "tiny_sr_upscaler.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>

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

struct Params
{
    UINT  srcWidth;
    UINT  srcHeight;
    UINT  dstWidth;
    UINT  dstHeight;
    UINT  scale;
    float sharpen;
    UINT  mode;
    UINT  netScale;
};

bool loadFile(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

std::string sibling(const std::string& shaderPath, const char* name)
{
    auto slash = shaderPath.find_last_of("\\/");
    if (slash == std::string::npos)
        return name;
    return shaderPath.substr(0, slash + 1) + name;
}
}

TinySrUpscaler::~TinySrUpscaler()
{
    releaseT(m_weightSRV);
    releaseT(m_weightBuf);
    releaseT(m_sampler);
    releaseT(m_cbuffer);
    releaseT(m_cs);
}

bool TinySrUpscaler::loadWeights(const std::string& binPath)
{
    std::ifstream in(binPath, std::ios::binary);
    if (!in)
        return false;
    char magic[4] = {};
    in.read(magic, 4);
    if (std::memcmp(magic, "TNSR", 4) != 0)
        return false;
    std::uint32_t ch = 0, sc = 0;
    in.read(reinterpret_cast<char*>(&ch), 4);
    in.read(reinterpret_cast<char*>(&sc), 4);
    m_channels = ch;
    m_netScale = sc;

    // Classifier head: 9 neighbor classes × scale² (hard palette, no RGB residual)
    constexpr size_t kClasses = 9;
    // W1 + B1 + W2 + B2 + W3 + B3
    const size_t nFloats = size_t(ch) * 3 * 3 * 3 + ch + size_t(ch) * ch * 3 * 3 + ch +
                           kClasses * size_t(sc) * sc * ch * 3 * 3 + kClasses * size_t(sc) * sc;
    std::vector<float> data(nFloats);
    in.read(reinterpret_cast<char*>(data.data()), std::streamsize(nFloats * sizeof(float)));
    if (!in)
        return false;

    D3D11_BUFFER_DESC bd {};
    bd.ByteWidth      = UINT(nFloats * sizeof(float));
    bd.Usage          = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
    bd.MiscFlags      = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(float);
    D3D11_SUBRESOURCE_DATA sd {};
    sd.pSysMem = data.data();
    if (FAILED(m_device->CreateBuffer(&bd, &sd, &m_weightBuf)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd {};
    svd.Format             = DXGI_FORMAT_UNKNOWN;
    svd.ViewDimension      = D3D11_SRV_DIMENSION_BUFFER;
    svd.Buffer.FirstElement = 0;
    svd.Buffer.NumElements  = UINT(nFloats);
    return SUCCEEDED(m_device->CreateShaderResourceView(m_weightBuf, &svd, &m_weightSRV));
}

bool TinySrUpscaler::init(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& shaderPath)
{
    m_device  = device;
    m_context = context;

    std::string src;
    if (!loadFile(shaderPath, src))
    {
        std::cerr << "TinySR: cannot read shader " << shaderPath << std::endl;
        return false;
    }

    ID3DBlob* blob  = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hr = D3DCompile(src.data(),
                            src.size(),
                            shaderPath.c_str(),
                            nullptr,
                            D3D_COMPILE_STANDARD_FILE_INCLUDE,
                            "main",
                            "cs_5_0",
                            0,
                            0,
                            &blob,
                            &error);
    if (FAILED(hr))
    {
        if (error)
        {
            std::cerr << (char*)error->GetBufferPointer() << std::endl;
            error->Release();
        }
        return false;
    }

    hr = device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_cs);
    blob->Release();
    if (FAILED(hr))
        return false;

    D3D11_BUFFER_DESC cbd {};
    cbd.ByteWidth      = sizeof(Params);
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&cbd, nullptr, &m_cbuffer)))
        return false;

    D3D11_SAMPLER_DESC samp {};
    samp.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(device->CreateSamplerState(&samp, &m_sampler)))
        return false;

    // Weights next to shader or exe
    const std::string candidates[] = {
        sibling(shaderPath, "tinysr_weights.bin"),
        "tinysr_weights.bin",
        "shaders\\tinysr_weights.bin",
        "..\\tinysr_weights.bin",
    };
    bool got = false;
    for (const auto& c : candidates)
    {
        if (loadWeights(c))
        {
            std::cout << "TinySR neural weights: " << c << " (C=" << m_channels << " scale=" << m_netScale << ")\n";
            got = true;
            break;
        }
    }
    if (!got)
        std::cerr << "TinySR: no tinysr_weights.bin — Neural mode falls back to nearest\n";

    return true;
}

void TinySrUpscaler::dispatch(ID3D11ShaderResourceView* input,
                              ID3D11UnorderedAccessView* output,
                              std::uint32_t srcW,
                              std::uint32_t srcH,
                              std::uint32_t dstW,
                              std::uint32_t dstH,
                              std::uint32_t scale,
                              float sharpen,
                              std::uint32_t mode)
{
    // Without weights, Neural → Clarity
    if (mode == 2u && !m_weightSRV)
        mode = 0u;

    Params p {};
    p.srcWidth  = srcW;
    p.srcHeight = srcH;
    p.dstWidth  = dstW;
    p.dstHeight = dstH;
    p.scale     = scale < 1 ? 1 : scale;
    p.sharpen   = sharpen;
    p.mode      = mode;
    p.netScale  = m_netScale;

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (SUCCEEDED(m_context->Map(m_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &p, sizeof(p));
        m_context->Unmap(m_cbuffer, 0);
    }

    ID3D11ShaderResourceView* srvs[2] = { input, m_weightSRV };
    m_context->CSSetShaderResources(0, m_weightSRV ? 2u : 1u, srvs);
    m_context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
    m_context->CSSetSamplers(0, 1, &m_sampler);
    m_context->CSSetConstantBuffers(0, 1, &m_cbuffer);
    m_context->CSSetShader(m_cs, nullptr, 0);

    const UINT gw = (mode == 2u) ? srcW : dstW;
    const UINT gh = (mode == 2u) ? srcH : dstH;
    m_context->Dispatch((gw + 7) / 8, (gh + 7) / 8, 1);

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* nullUAV    = nullptr;
    m_context->CSSetShaderResources(0, 2, nullSRVs);
    m_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    m_context->CSSetShader(nullptr, nullptr, 0);
}
}
