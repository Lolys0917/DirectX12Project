#include "Grid.h"

#include "DirectX12.h"
#include "Camera.h"

#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine
{
    namespace
    {
        UINT Align256(UINT size)
        {
            return (size + 255) & ~255;
        }

        const char* GridShaderCode = R"(

cbuffer GridCB : register(b0)
{
    float4x4 gWorldViewProjection;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position =
        mul(float4(input.position, 1.0f), gWorldViewProjection);

    output.color = input.color;

    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return input.color;
}

)";
    }

    Grid::Grid()
        : m_Camera(nullptr)
        , m_VertexBufferView{}
        , m_MappedConstantBuffer(nullptr)
    {
    }

    Grid::~Grid()
    {
        if (m_ConstantBuffer && m_MappedConstantBuffer)
        {
            m_ConstantBuffer->Unmap(0, nullptr);
            m_MappedConstantBuffer = nullptr;
        }
    }

    void Grid::SetCamera(Camera* camera)
    {
        m_Camera = camera;
    }

    bool Grid::Initialize(DirectX12& dx12)
    {
        BuildGrid();

        if (!CreateRootSignature(dx12)) return false;
        if (!CreatePipelineState(dx12)) return false;
        if (!CreateVertexBuffer(dx12)) return false;
        if (!CreateConstantBuffer(dx12)) return false;

        return true;
    }

    void Grid::Update(float deltaTime)
    {
        (void)deltaTime;
    }

    void Grid::Draw(DirectX12& dx12)
    {
        if (!m_Camera)
            return;

        if (!m_PipelineState)
            return;

        if (!m_RootSignature)
            return;

        ID3D12GraphicsCommandList* commandList =
            dx12.GetCommandList();

        using namespace DirectX;

        XMMATRIX world =
            XMMatrixIdentity();

        XMMATRIX viewProjection =
            m_Camera->GetViewProjectionMatrix();

        XMMATRIX wvp =
            world * viewProjection;

        XMMATRIX transposed =
            XMMatrixTranspose(wvp);

        XMStoreFloat4x4(
            &m_MappedConstantBuffer->worldViewProjection,
            transposed
        );

        commandList->SetGraphicsRootSignature(
            m_RootSignature.Get()
        );

        commandList->SetPipelineState(
            m_PipelineState.Get()
        );

        commandList->SetGraphicsRootConstantBufferView(
            0,
            m_ConstantBuffer->GetGPUVirtualAddress()
        );

        commandList->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_LINELIST
        );

        commandList->IASetVertexBuffers(
            0,
            1,
            &m_VertexBufferView
        );

        commandList->DrawInstanced(
            static_cast<UINT>(m_Vertices.size()),
            1,
            0,
            0
        );
    }

    void Grid::BuildGrid()
    {
        m_Vertices.clear();

        const float halfSize = 5.0f;

        const DirectX::XMFLOAT4 black =
        {
            0.0f, 0.0f, 0.0f, 1.0f
        };

        const DirectX::XMFLOAT4 red =
        {
            1.0f, 0.0f, 0.0f, 1.0f
        };

        const DirectX::XMFLOAT4 green =
        {
            0.0f, 1.0f, 0.0f, 1.0f
        };

        const DirectX::XMFLOAT4 blue =
        {
            0.0f, 0.0f, 1.0f, 1.0f
        };

        auto AddLine =
            [this](
                const DirectX::XMFLOAT3& start,
                const DirectX::XMFLOAT3& end,
                const DirectX::XMFLOAT4& color)
            {
                GridVertex v0{};
                v0.position = start;
                v0.color = color;

                GridVertex v1{};
                v1.position = end;
                v1.color = color;

                m_Vertices.push_back(v0);
                m_Vertices.push_back(v1);
            };

        /*
            X-Z平面の10×10グリッド

            範囲：
                X = -5 ～ +5
                Z = -5 ～ +5

            10マス作るため、線は11本ずつ。
        */
        for (int i = -5; i <= 5; ++i)
        {
            const float p = static_cast<float>(i);

            /*
                X方向の線

                z = 0 の線をX軸として赤にする。
            */
            AddLine(
                DirectX::XMFLOAT3(-halfSize, 0.0f, p),
                DirectX::XMFLOAT3(halfSize, 0.0f, p),
                i == 0 ? red : black
            );

            /*
                Z方向の線

                x = 0 の線をZ軸として青にする。
            */
            AddLine(
                DirectX::XMFLOAT3(p, 0.0f, -halfSize),
                DirectX::XMFLOAT3(p, 0.0f, halfSize),
                i == 0 ? blue : black
            );
        }

        /*
            Y軸

            地面から上方向に伸びる緑線。
        */
        AddLine(
            DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 5.0f, 0.0f),
            green
        );
    }

    bool Grid::CreateRootSignature(DirectX12& dx12)
    {
        D3D12_ROOT_PARAMETER rootParameter{};
        rootParameter.ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameter.Descriptor.ShaderRegister = 0;
        rootParameter.Descriptor.RegisterSpace = 0;
        rootParameter.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 1;
        desc.pParameters = &rootParameter;
        desc.NumStaticSamplers = 0;
        desc.pStaticSamplers = nullptr;
        desc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        HRESULT hr = D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            &error
        );

        if (FAILED(hr))
        {
            return false;
        }

        hr = dx12.GetDevice()->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_RootSignature)
        );

        return SUCCEEDED(hr);
    }

    bool Grid::CreatePipelineState(DirectX12& dx12)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

        UINT compileFlags = 0;

#if defined(_DEBUG)
        compileFlags =
            D3DCOMPILE_DEBUG |
            D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT hr = D3DCompile(
            GridShaderCode,
            std::strlen(GridShaderCode),
            nullptr,
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_0",
            compileFlags,
            0,
            &vsBlob,
            &errorBlob
        );

        if (FAILED(hr))
        {
            return false;
        }

        hr = D3DCompile(
            GridShaderCode,
            std::strlen(GridShaderCode),
            nullptr,
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_0",
            compileFlags,
            0,
            &psBlob,
            &errorBlob
        );

        if (FAILED(hr))
        {
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC inputLayout[2]{};

        inputLayout[0].SemanticName = "POSITION";
        inputLayout[0].SemanticIndex = 0;
        inputLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        inputLayout[0].InputSlot = 0;
        inputLayout[0].AlignedByteOffset = 0;
        inputLayout[0].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        inputLayout[0].InstanceDataStepRate = 0;

        inputLayout[1].SemanticName = "COLOR";
        inputLayout[1].SemanticIndex = 0;
        inputLayout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        inputLayout[1].InputSlot = 0;
        inputLayout[1].AlignedByteOffset = 12;
        inputLayout[1].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        inputLayout[1].InstanceDataStepRate = 0;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_RootSignature.Get();

        psoDesc.VS.pShaderBytecode =
            vsBlob->GetBufferPointer();
        psoDesc.VS.BytecodeLength =
            vsBlob->GetBufferSize();

        psoDesc.PS.pShaderBytecode =
            psBlob->GetBufferPointer();
        psoDesc.PS.BytecodeLength =
            psBlob->GetBufferSize();

        psoDesc.InputLayout.pInputElementDescs =
            inputLayout;
        psoDesc.InputLayout.NumElements = 2;

        psoDesc.PrimitiveTopologyType =
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] =
            dx12.GetBackBufferFormat();

        psoDesc.DSVFormat =
            dx12.GetDepthStencilFormat();

        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.SampleMask = UINT_MAX;

        psoDesc.RasterizerState.FillMode =
            D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode =
            D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias =
            D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp =
            D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias =
            D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster =
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;

        D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
        blendDesc.BlendEnable = FALSE;
        blendDesc.LogicOpEnable = FALSE;
        blendDesc.SrcBlend = D3D12_BLEND_ONE;
        blendDesc.DestBlend = D3D12_BLEND_ZERO;
        blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;

        psoDesc.BlendState.RenderTarget[0] = blendDesc;

        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc =
            D3D12_COMPARISON_FUNC_LESS_EQUAL;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        psoDesc.IBStripCutValue =
            D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        hr = dx12.GetDevice()->CreateGraphicsPipelineState(
            &psoDesc,
            IID_PPV_ARGS(&m_PipelineState)
        );

        return SUCCEEDED(hr);
    }

    bool Grid::CreateVertexBuffer(DirectX12& dx12)
    {
        const UINT bufferSize =
            static_cast<UINT>(
                sizeof(GridVertex) * m_Vertices.size()
                );

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = bufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags =
            D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = dx12.GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_VertexBuffer)
        );

        if (FAILED(hr))
        {
            return false;
        }

        void* mapped = nullptr;

        hr = m_VertexBuffer->Map(
            0,
            nullptr,
            &mapped
        );

        if (FAILED(hr))
        {
            return false;
        }

        std::memcpy(
            mapped,
            m_Vertices.data(),
            bufferSize
        );

        m_VertexBuffer->Unmap(
            0,
            nullptr
        );

        m_VertexBufferView.BufferLocation =
            m_VertexBuffer->GetGPUVirtualAddress();

        m_VertexBufferView.SizeInBytes =
            bufferSize;

        m_VertexBufferView.StrideInBytes =
            sizeof(GridVertex);

        return true;
    }

    bool Grid::CreateConstantBuffer(DirectX12& dx12)
    {
        const UINT bufferSize =
            Align256(sizeof(GridConstantBuffer));

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = bufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags =
            D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = dx12.GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_ConstantBuffer)
        );

        if (FAILED(hr))
        {
            return false;
        }

        hr = m_ConstantBuffer->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&m_MappedConstantBuffer)
        );

        return SUCCEEDED(hr);
    }
}