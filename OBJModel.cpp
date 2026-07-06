#include "OBJModel.h"

#include "DirectX12.h"
#include "Camera.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine
{
    namespace
    {
        UINT Align256(UINT size)
        {
            return (size + 255) & ~255;
        }

        struct OBJIndex
        {
            int position = 0;
            int uv = 0;
            int normal = 0;

            bool operator==(const OBJIndex& rhs) const
            {
                return position == rhs.position &&
                    uv == rhs.uv &&
                    normal == rhs.normal;
            }
        };

        struct OBJIndexHash
        {
            size_t operator()(const OBJIndex& key) const
            {
                size_t h1 = std::hash<int>()(key.position);
                size_t h2 = std::hash<int>()(key.uv);
                size_t h3 = std::hash<int>()(key.normal);

                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };

        std::string ToString(const std::wstring& str)
        {
            return std::string(str.begin(), str.end());
        }

        int ResolveOBJIndex(
            int index,
            int count
        )
        {
            if (index > 0)
            {
                return index - 1;
            }

            if (index < 0)
            {
                return count + index;
            }

            return -1;
        }

        OBJIndex ParseFaceToken(const std::string& token)
        {
            OBJIndex result{};

            std::stringstream ss(token);
            std::string part;

            std::getline(ss, part, '/');
            if (!part.empty())
            {
                result.position = std::stoi(part);
            }

            if (std::getline(ss, part, '/'))
            {
                if (!part.empty())
                {
                    result.uv = std::stoi(part);
                }
            }

            if (std::getline(ss, part, '/'))
            {
                if (!part.empty())
                {
                    result.normal = std::stoi(part);
                }
            }

            return result;
        }

        const char* OBJShaderCode = R"(

cbuffer ObjectCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    int gUseTexture;
    float3 padding;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position =
        mul(float4(input.position, 1.0f), gWorldViewProjection);

    output.normal = input.normal;
    output.uv = input.uv;

    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    if (gUseTexture != 0)
    {
        return gTexture.Sample(gSampler, input.uv) * gColor;
    }

    return gColor;
}

)";
    }

    OBJModel::OBJModel()
        : m_VertexBufferView{}
        , m_IndexBufferView{}
        , m_MappedConstantBuffer(nullptr)
        , m_Position(0.0f, 0.0f, 0.0f)
        , m_Rotation(0.0f, 0.0f, 0.0f)
        , m_Scale(1.0f, 1.0f, 1.0f)
        , m_Color(1.0f, 1.0f, 1.0f, 1.0f)
        , m_UseTexture(false)
    {
    }

    OBJModel::~OBJModel()
    {
        if (m_ConstantBuffer && m_MappedConstantBuffer)
        {
            m_ConstantBuffer->Unmap(
                0,
                nullptr
            );

            m_MappedConstantBuffer = nullptr;
        }
    }

    bool OBJModel::Load(
        DirectX12& dx12,
        const std::wstring& objPath,
        const std::wstring& texturePath
    )
    {
        m_UseTexture = true;
        m_Color = DirectX::XMFLOAT4(
            1.0f,
            1.0f,
            1.0f,
            1.0f
        );

        if (!LoadOBJFile(objPath)) return false;
        if (!CreateRootSignature(dx12)) return false;
        if (!CreatePipelineState(dx12)) return false;
        if (!CreateVertexBuffer(dx12)) return false;
        if (!CreateIndexBuffer(dx12)) return false;
        if (!CreateConstantBuffer(dx12)) return false;

        if (!m_Texture.LoadFromFile(dx12, texturePath))
        {
            m_UseTexture = false;

            if (!m_Texture.CreateWhiteTexture(dx12))
            {
                return false;
            }
        }

        return true;
    }

    bool OBJModel::Load(
        DirectX12& dx12,
        const std::wstring& objPath,
        const DirectX::XMFLOAT4& color
    )
    {
        m_UseTexture = false;
        m_Color = color;

        if (!LoadOBJFile(objPath)) return false;
        if (!CreateRootSignature(dx12)) return false;
        if (!CreatePipelineState(dx12)) return false;
        if (!CreateVertexBuffer(dx12)) return false;
        if (!CreateIndexBuffer(dx12)) return false;
        if (!CreateConstantBuffer(dx12)) return false;

        if (!m_Texture.CreateWhiteTexture(dx12))
        {
            return false;
        }

        return true;
    }

    bool OBJModel::LoadOBJFile(
        const std::wstring& objPath
    )
    {
        m_Vertices.clear();
        m_Indices.clear();

        std::ifstream file(ToString(objPath));

        if (!file)
        {
            return false;
        }

        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT3> normals;
        std::vector<DirectX::XMFLOAT2> uvs;

        std::unordered_map<
            OBJIndex,
            uint32_t,
            OBJIndexHash
        > vertexMap;

        std::string line;

        while (std::getline(file, line))
        {
            std::stringstream ss(line);

            std::string type;
            ss >> type;

            if (type == "v")
            {
                DirectX::XMFLOAT3 p{};
                ss >> p.x >> p.y >> p.z;

                positions.push_back(p);
            }
            else if (type == "vt")
            {
                DirectX::XMFLOAT2 uv{};
                ss >> uv.x >> uv.y;

                // OBJÇÕVï˚å¸Ç™ãtÇ…Ç»ÇÈÇ±Ç∆Ç™ëΩÇ¢ÇΩÇﬂîΩì]
                uv.y = 1.0f - uv.y;

                uvs.push_back(uv);
            }
            else if (type == "vn")
            {
                DirectX::XMFLOAT3 n{};
                ss >> n.x >> n.y >> n.z;

                normals.push_back(n);
            }
            else if (type == "f")
            {
                std::vector<uint32_t> faceIndices;

                std::string token;

                while (ss >> token)
                {
                    OBJIndex rawIndex =
                        ParseFaceToken(token);

                    OBJIndex resolved{};
                    resolved.position =
                        ResolveOBJIndex(
                            rawIndex.position,
                            static_cast<int>(positions.size())
                        );

                    resolved.uv =
                        ResolveOBJIndex(
                            rawIndex.uv,
                            static_cast<int>(uvs.size())
                        );

                    resolved.normal =
                        ResolveOBJIndex(
                            rawIndex.normal,
                            static_cast<int>(normals.size())
                        );

                    auto it = vertexMap.find(resolved);

                    if (it != vertexMap.end())
                    {
                        faceIndices.push_back(it->second);
                    }
                    else
                    {
                        OBJVertex vertex{};

                        if (resolved.position >= 0)
                        {
                            vertex.position =
                                positions[resolved.position];
                        }

                        if (resolved.normal >= 0)
                        {
                            vertex.normal =
                                normals[resolved.normal];
                        }
                        else
                        {
                            vertex.normal =
                                DirectX::XMFLOAT3(
                                    0.0f,
                                    1.0f,
                                    0.0f
                                );
                        }

                        if (resolved.uv >= 0)
                        {
                            vertex.uv =
                                uvs[resolved.uv];
                        }
                        else
                        {
                            vertex.uv =
                                DirectX::XMFLOAT2(
                                    0.0f,
                                    0.0f
                                );
                        }

                        uint32_t newIndex =
                            static_cast<uint32_t>(
                                m_Vertices.size()
                                );

                        m_Vertices.push_back(vertex);

                        vertexMap.emplace(
                            resolved,
                            newIndex
                        );

                        faceIndices.push_back(newIndex);
                    }
                }

                // éOäpå`ÅEéläpå`ÅEëΩäpå`ÇTriangle FanÇ≈ï™â
                for (size_t i = 1; i + 1 < faceIndices.size(); ++i)
                {
                    m_Indices.push_back(faceIndices[0]);
                    m_Indices.push_back(faceIndices[i]);
                    m_Indices.push_back(faceIndices[i + 1]);
                }
            }
        }

        return !m_Vertices.empty() && !m_Indices.empty();
    }

    bool OBJModel::CreateRootSignature(
        DirectX12& dx12
    )
    {
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType =
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0;
        srvRange.RegisterSpace = 0;
        srvRange.OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParameters[2]{};

        rootParameters[0].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].ShaderVisibility =
            D3D12_SHADER_VISIBILITY_ALL;

        rootParameters[1].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[1].DescriptorTable.pDescriptorRanges =
            &srvRange;
        rootParameters[1].ShaderVisibility =
            D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter =
            D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU =
            D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV =
            D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW =
            D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias = 0.0f;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc =
            D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor =
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 2;
        desc.pParameters = rootParameters;
        desc.NumStaticSamplers = 1;
        desc.pStaticSamplers = &sampler;
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

    bool OBJModel::CreatePipelineState(
        DirectX12& dx12
    )
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
            OBJShaderCode,
            std::strlen(OBJShaderCode),
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
            OBJShaderCode,
            std::strlen(OBJShaderCode),
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

        D3D12_INPUT_ELEMENT_DESC inputLayout[3]{};

        inputLayout[0].SemanticName = "POSITION";
        inputLayout[0].Format =
            DXGI_FORMAT_R32G32B32_FLOAT;
        inputLayout[0].InputSlot = 0;
        inputLayout[0].AlignedByteOffset = 0;
        inputLayout[0].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        inputLayout[1].SemanticName = "NORMAL";
        inputLayout[1].Format =
            DXGI_FORMAT_R32G32B32_FLOAT;
        inputLayout[1].InputSlot = 0;
        inputLayout[1].AlignedByteOffset = 12;
        inputLayout[1].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        inputLayout[2].SemanticName = "TEXCOORD";
        inputLayout[2].Format =
            DXGI_FORMAT_R32G32_FLOAT;
        inputLayout[2].InputSlot = 0;
        inputLayout[2].AlignedByteOffset = 24;
        inputLayout[2].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature =
            m_RootSignature.Get();

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
        psoDesc.InputLayout.NumElements = 3;

        psoDesc.PrimitiveTopologyType =
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

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
        blendDesc.SrcBlend =
            D3D12_BLEND_ONE;
        blendDesc.DestBlend =
            D3D12_BLEND_ZERO;
        blendDesc.BlendOp =
            D3D12_BLEND_OP_ADD;
        blendDesc.SrcBlendAlpha =
            D3D12_BLEND_ONE;
        blendDesc.DestBlendAlpha =
            D3D12_BLEND_ZERO;
        blendDesc.BlendOpAlpha =
            D3D12_BLEND_OP_ADD;
        blendDesc.LogicOp =
            D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;

        psoDesc.BlendState.RenderTarget[0] =
            blendDesc;

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

    bool OBJModel::CreateVertexBuffer(
        DirectX12& dx12
    )
    {
        const UINT bufferSize =
            static_cast<UINT>(
                sizeof(OBJVertex) * m_Vertices.size()
                );

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type =
            D3D12_HEAP_TYPE_UPLOAD;
        heap.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bufferSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = dx12.GetDevice()->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
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
            sizeof(OBJVertex);

        return true;
    }

    bool OBJModel::CreateIndexBuffer(
        DirectX12& dx12
    )
    {
        const UINT bufferSize =
            static_cast<UINT>(
                sizeof(uint32_t) * m_Indices.size()
                );

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type =
            D3D12_HEAP_TYPE_UPLOAD;
        heap.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bufferSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = dx12.GetDevice()->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_IndexBuffer)
        );

        if (FAILED(hr))
        {
            return false;
        }

        void* mapped = nullptr;

        hr = m_IndexBuffer->Map(
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
            m_Indices.data(),
            bufferSize
        );

        m_IndexBuffer->Unmap(
            0,
            nullptr
        );

        m_IndexBufferView.BufferLocation =
            m_IndexBuffer->GetGPUVirtualAddress();
        m_IndexBufferView.SizeInBytes =
            bufferSize;
        m_IndexBufferView.Format =
            DXGI_FORMAT_R32_UINT;

        return true;
    }

    bool OBJModel::CreateConstantBuffer(
        DirectX12& dx12
    )
    {
        const UINT bufferSize =
            Align256(sizeof(OBJConstantBuffer));

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type =
            D3D12_HEAP_TYPE_UPLOAD;
        heap.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bufferSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = dx12.GetDevice()->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
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

    void OBJModel::SetPosition(
        const DirectX::XMFLOAT3& position
    )
    {
        m_Position = position;
    }

    void OBJModel::SetRotation(
        const DirectX::XMFLOAT3& rotation
    )
    {
        m_Rotation = rotation;
    }

    void OBJModel::SetScale(
        const DirectX::XMFLOAT3& scale
    )
    {
        m_Scale = scale;
    }

    void OBJModel::SetColor(
        const DirectX::XMFLOAT4& color
    )
    {
        m_Color = color;
    }

    void OBJModel::Update(float deltaTime)
    {
        (void)deltaTime;
    }

    DirectX::XMMATRIX OBJModel::GetWorldMatrix() const
    {
        using namespace DirectX;

        XMMATRIX scale =
            XMMatrixScaling(
                m_Scale.x,
                m_Scale.y,
                m_Scale.z
            );

        XMMATRIX rotation =
            XMMatrixRotationRollPitchYaw(
                m_Rotation.x,
                m_Rotation.y,
                m_Rotation.z
            );

        XMMATRIX translation =
            XMMatrixTranslation(
                m_Position.x,
                m_Position.y,
                m_Position.z
            );

        return scale * rotation * translation;
    }

    void OBJModel::Draw(
        DirectX12& dx12,
        const Camera& camera
    )
    {
        if (!m_PipelineState)
            return;

        if (!m_RootSignature)
            return;

        if (m_Indices.empty())
            return;

        ID3D12GraphicsCommandList* commandList =
            dx12.GetCommandList();

        using namespace DirectX;

        XMMATRIX world =
            GetWorldMatrix();

        XMMATRIX viewProjection =
            camera.GetViewProjectionMatrix();

        XMMATRIX wvp =
            world * viewProjection;

        XMMATRIX transposed =
            XMMatrixTranspose(wvp);

        XMStoreFloat4x4(
            &m_MappedConstantBuffer->worldViewProjection,
            transposed
        );

        m_MappedConstantBuffer->color =
            m_Color;

        m_MappedConstantBuffer->useTexture =
            m_UseTexture ? 1 : 0;

        ID3D12DescriptorHeap* heaps[] =
        {
            m_Texture.GetSRVHeap()
        };

        commandList->SetDescriptorHeaps(
            1,
            heaps
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

        commandList->SetGraphicsRootDescriptorTable(
            1,
            m_Texture.GetSRVGPUHandle()
        );

        commandList->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
        );

        commandList->IASetVertexBuffers(
            0,
            1,
            &m_VertexBufferView
        );

        commandList->IASetIndexBuffer(
            &m_IndexBufferView
        );

        commandList->DrawIndexedInstanced(
            static_cast<UINT>(m_Indices.size()),
            1,
            0,
            0,
            0
        );
    }
}