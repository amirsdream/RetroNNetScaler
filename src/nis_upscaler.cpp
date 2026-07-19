#include "nis_upscaler.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>

#include "NIS_Config.h"

namespace scaler
{
namespace
{
void release(IUnknown*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

template <typename T>
void releaseT(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

bool compileCS(ID3D11Device* device,
               const std::wstring& path,
               const std::string& includeDir,
               const D3D_SHADER_MACRO* macros,
               ID3D11ComputeShader** out)
{
    class Include : public ID3DInclude
    {
    public:
        explicit Include(std::string dir)
          : m_dir(std::move(dir))
        {
        }

        HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR file, LPCVOID, LPCVOID* data, UINT* bytes) override
        {
            std::ifstream in(m_dir + "/" + file, std::ios::binary);
            if (!in)
                return E_FAIL;
            m_buf.assign(std::istreambuf_iterator<char>(in), {});
            *data  = m_buf.data();
            *bytes = static_cast<UINT>(m_buf.size());
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Close(LPCVOID) override { return S_OK; }

        std::string m_dir;
        std::string m_buf;
    };

    Include include(includeDir);
    ID3DBlob* blob  = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hr = D3DCompileFromFile(path.c_str(), macros, &include, "main", "cs_5_0", 0, 0, &blob, &error);
    if (FAILED(hr))
    {
        if (error)
        {
            std::cerr << (char*)error->GetBufferPointer() << std::endl;
            error->Release();
        }
        return false;
    }
    hr = device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, out);
    blob->Release();
    return SUCCEEDED(hr);
}

std::wstring widen(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}
}

NisUpscaler::~NisUpscaler()
{
    releaseT(m_sampler);
    releaseT(m_coefUsmSRV);
    releaseT(m_coefScaleSRV);
    releaseT(m_coefUsm);
    releaseT(m_coefScale);
    releaseT(m_cbuffer);
    releaseT(m_cs);
}

bool NisUpscaler::init(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& shaderDir)
{
    m_device  = device;
    m_context = context;

    NISOptimizer opt(true, NISGPUArchitecture::NVIDIA_Generic);
    m_blockW = opt.GetOptimalBlockWidth();
    m_blockH = opt.GetOptimalBlockHeight();
    const uint32_t threadGroupSize = opt.GetOptimalThreadGroupSize();

    char bw[16], bh[16], tg[16];
    std::snprintf(bw, sizeof(bw), "%u", m_blockW);
    std::snprintf(bh, sizeof(bh), "%u", m_blockH);
    std::snprintf(tg, sizeof(tg), "%u", threadGroupSize);

    const D3D_SHADER_MACRO macros[] = {
        { "NIS_SCALER", "1" },
        { "NIS_HDR_MODE", "0" },
        { "NIS_BLOCK_WIDTH", bw },
        { "NIS_BLOCK_HEIGHT", bh },
        { "NIS_THREAD_GROUP_SIZE", tg },
        { nullptr, nullptr },
    };

    const std::string hlsl = shaderDir + "/NIS_Main.hlsl";
    if (!compileCS(device, widen(hlsl), shaderDir, macros, &m_cs))
    {
        std::cerr << "Failed to compile NIS shader from " << hlsl << std::endl;
        return false;
    }

    const int rowPitch  = static_cast<int>(kFilterSize * 4);
    const int imageSize = rowPitch * static_cast<int>(kPhaseCount);

    D3D11_TEXTURE2D_DESC td {};
    td.Width            = static_cast<UINT>(kFilterSize / 4);
    td.Height           = static_cast<UINT>(kPhaseCount);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sdScale {};
    sdScale.pSysMem     = coef_scale;
    sdScale.SysMemPitch = rowPitch;
    D3D11_SUBRESOURCE_DATA sdUsm {};
    sdUsm.pSysMem     = coef_usm;
    sdUsm.SysMemPitch = rowPitch;

    if (FAILED(device->CreateTexture2D(&td, &sdScale, &m_coefScale)) ||
        FAILED(device->CreateTexture2D(&td, &sdUsm, &m_coefUsm)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv {};
    srv.Format              = td.Format;
    srv.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(m_coefScale, &srv, &m_coefScaleSRV)) ||
        FAILED(device->CreateShaderResourceView(m_coefUsm, &srv, &m_coefUsmSRV)))
        return false;

    D3D11_SAMPLER_DESC samp {};
    samp.Filter   = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(device->CreateSamplerState(&samp, &m_sampler)))
        return false;

    D3D11_BUFFER_DESC cbd {};
    cbd.ByteWidth      = sizeof(NISConfig);
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&cbd, nullptr, &m_cbuffer)))
        return false;

    return true;
}

void NisUpscaler::update(float sharpness, std::uint32_t inW, std::uint32_t inH, std::uint32_t outW, std::uint32_t outH)
{
    NISConfig config {};
    NVScalerUpdateConfig(config,
                         sharpness,
                         0,
                         0,
                         inW,
                         inH,
                         inW,
                         inH,
                         0,
                         0,
                         outW,
                         outH,
                         outW,
                         outH,
                         NISHDRMode::None);

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (SUCCEEDED(m_context->Map(m_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &config, sizeof(config));
        m_context->Unmap(m_cbuffer, 0);
    }
    m_outW = outW;
    m_outH = outH;
}

void NisUpscaler::dispatch(ID3D11ShaderResourceView* input, ID3D11UnorderedAccessView* output)
{
    m_context->CSSetShaderResources(0, 1, &input);
    m_context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
    m_context->CSSetShaderResources(1, 1, &m_coefScaleSRV);
    m_context->CSSetShaderResources(2, 1, &m_coefUsmSRV);
    m_context->CSSetSamplers(0, 1, &m_sampler);
    m_context->CSSetConstantBuffers(0, 1, &m_cbuffer);
    m_context->CSSetShader(m_cs, nullptr, 0);
    const UINT gx = static_cast<UINT>(std::ceil(m_outW / float(m_blockW)));
    const UINT gy = static_cast<UINT>(std::ceil(m_outH / float(m_blockH)));
    m_context->Dispatch(gx, gy, 1);

    ID3D11ShaderResourceView* nullSRV[3] = {};
    ID3D11UnorderedAccessView* nullUAV   = nullptr;
    m_context->CSSetShaderResources(0, 3, nullSRV);
    m_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    m_context->CSSetShader(nullptr, nullptr, 0);
}
}
