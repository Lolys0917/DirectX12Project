#include "TextureDisplay.h"
#include "DirectX12.h"
#include "Camera.h"
#include "PlaybackSettings.h"
#include "MessageLog.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <climits>

namespace Engine
{
    namespace
    {
        const char* Shader = R"(
cbuffer Settings : register(b0) {
    float4x4 InverseViewProjection;
    float4 CameraAndMode;
    float4 Options;
    float4 ImageScale;
};
Texture2D Image : register(t0);
SamplerState ImageSampler : register(s0);
struct Output { float4 Position : SV_POSITION; float2 UV : TEXCOORD0; };
Output VSMain(uint id : SV_VertexID) {
    Output o;
    o.UV = float2((id << 1) & 2, id & 2);
    o.Position = float4(o.UV * float2(2,-2) + float2(-1,1), 1, 1);
    return o;
}
float4 PSMain(Output input) : SV_TARGET {
    float2 uv = input.UV;
    if (CameraAndMode.w > 0.5) {
        float4 farPoint = mul(float4(uv * float2(2,-2) + float2(-1,1), 1, 1), InverseViewProjection);
        float3 direction = normalize(farPoint.xyz / farPoint.w - CameraAndMode.xyz);
        uv = float2(atan2(direction.x, direction.z) / 6.283185307 + 0.5 + Options.x,
                    acos(clamp(direction.y, -1.0, 1.0)) / 3.141592654);
        return float4(Image.Sample(ImageSampler, uv).rgb * Options.y, 1);
    }
    uv = (uv - 0.5) * ImageScale.xy + 0.5;
    if (any(uv < 0) || any(uv > 1)) return float4(0.09,0.1,0.12,1);
    float checker = fmod(floor(input.Position.x / 12) + floor(input.Position.y / 12), 2);
    float3 background = lerp(float3(0.20,0.21,0.23), float3(0.32,0.33,0.35), checker);
    float4 color = Image.Sample(ImageSampler, uv);
    return float4(lerp(background, color.rgb, color.a), 1);
})";
    }

    bool TextureDisplay::Load(DirectX12& graphics, const std::wstring& path)
    {
        if (!Texture.LoadFromFile(graphics, path)) return false;
        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        D3D12_ROOT_PARAMETER params[2]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants.Num32BitValues = 28;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &range;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 2; desc.pParameters = params;
        desc.NumStaticSamplers = 1; desc.pStaticSamplers = &sampler;
        Microsoft::WRL::ComPtr<ID3DBlob> signature, errors, vs, ps;
        if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors)) ||
            FAILED(graphics.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
                signature->GetBufferSize(), IID_PPV_ARGS(&Root)))) return false;
        for (int stage = 0; stage < 2; ++stage)
        {
            if (FAILED(D3DCompile(Shader, std::strlen(Shader), nullptr, nullptr, nullptr,
                stage ? "PSMain" : "VSMain", stage ? "ps_5_0" : "vs_5_0", 0, 0,
                stage ? &ps : &vs, &errors)))
            {
                if (errors) MessageLog::GetInstance().AddLog(std::string("[Error] TextureDisplay | ") +
                    static_cast<const char*>(errors->GetBufferPointer()));
                return false;
            }
        }
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = Root.Get();
        pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.DepthStencilState.DepthEnable = TRUE;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pso.SampleMask = UINT_MAX;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = graphics.GetBackBufferFormat();
        pso.DSVFormat = graphics.GetDepthStencilFormat();
        pso.SampleDesc.Count = 1;
        return SUCCEEDED(graphics.GetDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&Pipeline)));
    }

    void TextureDisplay::Draw(DirectX12& graphics, const Camera* camera)
    {
        if (!Pipeline || !Texture.IsValid() || !graphics.IsFrameOpen()) return;
        struct Constants {
            DirectX::XMFLOAT4X4 Inverse;
            DirectX::XMFLOAT4 CameraMode;
            DirectX::XMFLOAT4 Options;
            DirectX::XMFLOAT4 ImageScale;
        } constants{};
        static_assert(sizeof(constants) == 28 * sizeof(float));
        if (camera)
        {
            DirectX::XMStoreFloat4x4(&constants.Inverse, DirectX::XMMatrixTranspose(
                DirectX::XMMatrixInverse(nullptr, camera->GetViewProjectionMatrix())));
            auto p = camera->GetPosition();
            constants.CameraMode = { p.x, p.y, p.z, 1 };
            constants.Options = { ActivePlaybackSettings.SkyYaw / 360.0f, ActivePlaybackSettings.SkyExposure, 0, 0 };
        }
        const float imageAspect = float(Texture.GetWidth()) / Texture.GetHeight();
        const float viewAspect = float(graphics.GetWidth()) / std::max(1u, graphics.GetHeight());
        constants.ImageScale = { std::max(1.0f, viewAspect / imageAspect),
            std::max(1.0f, imageAspect / viewAspect), 0, 0 };
        auto* commands = graphics.GetCommandList();
        auto* heap = Texture.GetSRVHeap();
        commands->SetDescriptorHeaps(1, &heap);
        commands->SetGraphicsRootSignature(Root.Get());
        commands->SetPipelineState(Pipeline.Get());
        commands->SetGraphicsRoot32BitConstants(0, 28, &constants, 0);
        commands->SetGraphicsRootDescriptorTable(1, Texture.GetSRVGPUHandle());
        commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commands->DrawInstanced(3, 1, 0, 0);
    }

    bool DirectX12::SetSkyTexture(const std::wstring& path)
    {
        if (IsFrameOpen() || !WaitGPU()) return false;
        if (path.empty()) { SkyTexture.reset(); return true; }
        auto candidate = std::make_unique<TextureDisplay>();
        if (!candidate->Load(*this, path)) return false;
        SkyTexture = std::move(candidate);
        return true;
    }

    void DirectX12::DrawSky(const Camera& camera)
    {
        if (SkyTexture) SkyTexture->Draw(*this, &camera);
    }
}
