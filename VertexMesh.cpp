//|| VertexMesh.cpp ||::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  共通頂点Color MeshのGPU Buffer、Pipeline、Camera描画を管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.10  Mesh Resource作成と遅延再生成失敗をMessageLogへ記録
//||  2026_07_13  v2.00  Upload BufferとRoot Constantsによる共通描画を実装
//||  2026_07_13  v1.10  命名と宣言コメントを規則へ統一
//||  2026_06_01  v1.00  新規作成
//||

#include "VertexMesh.h"

#include <climits>
#include <cstring>

#include <d3dcompiler.h>

#include "Camera.h"
#include "DirectX12.h"
#include "MessageLog.h"
#include "RenderContext.h"

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine
{
    namespace
    {
        const char* VertexMeshShaderCode = R"(

cbuffer MeshConstants : register(b0)
{
    float4x4 WorldViewProjection;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 1.0f), WorldViewProjection);
    output.Color = input.Color;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return input.Color;
}

)"; //共通Vertex Color描画用HLSL Source

        //CPU Dataを保持するUpload Heap Bufferを作成する
        //引数: dx12 描画基盤、sourceData 転送元、dataSize Byte数、resource 作成先
        //戻り値: Buffer作成と転送に成功した場合はtrue
        bool CreateUploadBuffer(
            DirectX12& dx12,
            const void* sourceData,
            std::size_t dataSize,
            Microsoft::WRL::ComPtr<ID3D12Resource>& resource
        )
        {
            if (sourceData == nullptr || dataSize == 0)
            {
                return false;
            }

            D3D12_HEAP_PROPERTIES HeapProperties{}; //Upload Heap設定
            HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
            HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            HeapProperties.CreationNodeMask = 1;
            HeapProperties.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC ResourceDescription{}; //Buffer Resource設定
            ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            ResourceDescription.Alignment = 0;
            ResourceDescription.Width = static_cast<UINT64>(dataSize);
            ResourceDescription.Height = 1;
            ResourceDescription.DepthOrArraySize = 1;
            ResourceDescription.MipLevels = 1;
            ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
            ResourceDescription.SampleDesc.Count = 1;
            ResourceDescription.SampleDesc.Quality = 0;
            ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

            HRESULT Result = dx12.GetDevice()->CreateCommittedResource(
                &HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &ResourceDescription,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&resource)
            ); //Upload Buffer作成結果

            if (FAILED(Result))
            {
                return false;
            }

            void* MappedData = nullptr; //Upload BufferのCPU書込先
            Result = resource->Map(0, nullptr, &MappedData);

            if (FAILED(Result))
            {
                resource.Reset();
                return false;
            }

            std::memcpy(MappedData, sourceData, dataSize);
            resource->Unmap(0, nullptr);
            return true;
        }
    }

    //空のCPU Meshと無効なGPU Resourceを作成する
    VertexMesh::VertexMesh()
        : VertexBufferView{}
        , IndexBufferView{}
        , GPUResourceReady(false)
        , GPUResourceDirty(true)
    {
    }

    //CPU MeshとGPU Resourceを解放する
    VertexMesh::~VertexMesh()
    {
        ReleaseGPUResource();
    }

    //CPU MeshとGPU Resourceを初期状態へ戻す
    void VertexMesh::Clear()
    {
        Vertices.clear();
        Indices.clear();
        ReleaseGPUResource();
    }

    //概要：CPU頂点一覧を置き換えてGPU Bufferを再生成対象にする
    //引数：vertices=新しい頂点一覧
    //戻り値：なし
    void VertexMesh::SetVertices(const std::vector<Vertex>& vertices)
    {
        Vertices = vertices;
        GPUResourceDirty = true;
    }

    //概要：CPU Index一覧を置き換えてGPU Bufferを再生成対象にする
    //引数：indices=新しい三角形Index一覧
    //戻り値：なし
    void VertexMesh::SetIndices(const std::vector<uint32_t>& indices)
    {
        Indices = indices;
        GPUResourceDirty = true;
    }

    //頂点をCPU Mesh末尾へ追加する
    //引数: vertex 追加する頂点
    void VertexMesh::AddVertex(const Vertex& vertex)
    {
        Vertices.push_back(vertex);
        GPUResourceDirty = true;
    }

    //三頂点のIndexからTriangleをCPU Mesh末尾へ追加する
    //引数: i0 Triangle第一頂点Index、i1 第二頂点Index、i2 第三頂点Index
    void VertexMesh::AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2)
    {
        Indices.push_back(i0);
        Indices.push_back(i1);
        Indices.push_back(i2);
        GPUResourceDirty = true;
    }

    //多角形のIndex列をTriangle Fanとして三角形へ分解する
    //引数: faceIndices 多角形を周回順で表す頂点Index列
    void VertexMesh::AddPolygonFace(const std::vector<uint32_t>& faceIndices)
    {
        if (faceIndices.size() < 3)
        {
            return;
        }

        const uint32_t Base = faceIndices[0]; //Triangle Fanの共通先頭頂点

        for (size_t Index = 1; Index + 1 < faceIndices.size(); ++Index) //Triangle Fanを構築する
        {
            Indices.push_back(Base);
            Indices.push_back(faceIndices[Index]);
            Indices.push_back(faceIndices[Index + 1]);
        }

        GPUResourceDirty = true;
    }

    //CPU Meshに対応する全GPU Resourceを作成する
    //引数: dx12 Resource作成に使用する描画基盤
    //戻り値: 全Resource作成に成功した場合はtrue
    bool VertexMesh::CreateGPUResource(DirectX12& dx12)
    {
        if (Vertices.empty() || Indices.empty())
        {
            MessageLog::GetInstance().AddLog(
                "[Error] VertexMesh | GPU resource creation requires vertices and indices."
            );
            ReleaseGPUResource();
            return false;
        }

        ReleaseGPUResource();

        if (!CreateRootSignature(dx12) ||
            !CreatePipelineState(dx12) ||
            !CreateVertexBuffer(dx12) ||
            !CreateIndexBuffer(dx12))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] VertexMesh | Root signature, pipeline, or upload buffer creation failed."
            );
            ReleaseGPUResource();
            return false;
        }

        GPUResourceReady = true;
        GPUResourceDirty = false;
        return true;
    }

    //現在のCamera passへWorld姿勢を適用して描画する
    //引数: renderContext 描画基盤とCamera、world MeshのWorld行列
    void VertexMesh::Draw(
        const RenderContext& renderContext,
        const DirectX::XMMATRIX& world
    )
    {
        DirectX12& Dx12 = renderContext.Graphics; //現在passの描画基盤

        if ((GPUResourceDirty || !GPUResourceReady) && !CreateGPUResource(Dx12))
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Error] VertexMesh | A dirty mesh could not rebuild its GPU resources."
            );
            return;
        }

        if (!RootSignature || !PipelineState || !VertexBuffer || !IndexBuffer)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Error] VertexMesh | Draw was skipped because GPU resources are incomplete."
            );
            return;
        }

        const DirectX::XMMATRIX ViewProjection =
            renderContext.ViewCamera.GetViewProjectionMatrix(); //現在CameraのViewProjection行列
        const DirectX::XMMATRIX WorldViewProjection =
            world * ViewProjection; //MeshのWVP行列
        DirectX::XMFLOAT4X4 RootConstants{}; //CommandListへ直接記録する転置済みWVP
        DirectX::XMStoreFloat4x4(
            &RootConstants,
            DirectX::XMMatrixTranspose(WorldViewProjection)
        );

        ID3D12GraphicsCommandList* CommandList = Dx12.GetCommandList(); //描画命令の記録先
        CommandList->SetGraphicsRootSignature(RootSignature.Get());
        CommandList->SetPipelineState(PipelineState.Get());
        CommandList->SetGraphicsRoot32BitConstants(
            0,
            16,
            &RootConstants,
            0
        );
        CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
        CommandList->IASetIndexBuffer(&IndexBufferView);
        CommandList->DrawIndexedInstanced(
            static_cast<UINT>(Indices.size()),
            1,
            0,
            0,
            0
        );
    }

    //CPU Meshを保持したままGPU Resourceだけを解放する
    void VertexMesh::ReleaseGPUResource()
    {
        PipelineState.Reset();
        RootSignature.Reset();
        VertexBuffer.Reset();
        IndexBuffer.Reset();
        VertexBufferView = {};
        IndexBufferView = {};
        GPUResourceReady = false;
        GPUResourceDirty = true;
    }

    //World行列を受け取るRoot Constants用RootSignatureを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool VertexMesh::CreateRootSignature(DirectX12& dx12)
    {
        D3D12_ROOT_PARAMETER RootParameter{}; //16 DWORD WVP用Root Constants
        RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        RootParameter.Constants.ShaderRegister = 0;
        RootParameter.Constants.RegisterSpace = 0;
        RootParameter.Constants.Num32BitValues = 16;
        RootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC Description{}; //RootSignature設定
        Description.NumParameters = 1;
        Description.pParameters = &RootParameter;
        Description.NumStaticSamplers = 0;
        Description.pStaticSamplers = nullptr;
        Description.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> Signature; //Serialize済みRootSignature
        Microsoft::WRL::ComPtr<ID3DBlob> Error; //Serialize失敗内容
        HRESULT Result = D3D12SerializeRootSignature(
            &Description,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &Signature,
            &Error
        ); //RootSignature Serialize結果

        if (FAILED(Result))
        {
            return false;
        }

        Result = dx12.GetDevice()->CreateRootSignature(
            0,
            Signature->GetBufferPointer(),
            Signature->GetBufferSize(),
            IID_PPV_ARGS(&RootSignature)
        );
        return SUCCEEDED(Result);
    }

    //共通Vertex Color描画用PipelineStateを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool VertexMesh::CreatePipelineState(DirectX12& dx12)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> VertexShaderBlob; //Compile済みVertex Shader
        Microsoft::WRL::ComPtr<ID3DBlob> PixelShaderBlob; //Compile済みPixel Shader
        Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob; //Shader Compile失敗内容
        UINT CompileFlags = 0; //Shader Compile Option

#if defined(_DEBUG)
        CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT Result = D3DCompile(
            VertexMeshShaderCode,
            std::strlen(VertexMeshShaderCode),
            nullptr,
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_0",
            CompileFlags,
            0,
            &VertexShaderBlob,
            &ErrorBlob
        ); //Vertex Shader Compile結果

        if (FAILED(Result))
        {
            return false;
        }

        Result = D3DCompile(
            VertexMeshShaderCode,
            std::strlen(VertexMeshShaderCode),
            nullptr,
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_0",
            CompileFlags,
            0,
            &PixelShaderBlob,
            &ErrorBlob
        );

        if (FAILED(Result))
        {
            return false;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDescription{}; //共通Mesh Pipeline設定
        PipelineDescription.pRootSignature = RootSignature.Get();
        PipelineDescription.VS.pShaderBytecode = VertexShaderBlob->GetBufferPointer();
        PipelineDescription.VS.BytecodeLength = VertexShaderBlob->GetBufferSize();
        PipelineDescription.PS.pShaderBytecode = PixelShaderBlob->GetBufferPointer();
        PipelineDescription.PS.BytecodeLength = PixelShaderBlob->GetBufferSize();
        PipelineDescription.InputLayout.pInputElementDescs = GetVertexInputLayout();
        PipelineDescription.InputLayout.NumElements = GetVertexInputLayoutCount();
        PipelineDescription.PrimitiveTopologyType =
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PipelineDescription.NumRenderTargets = 1;
        PipelineDescription.RTVFormats[0] = dx12.GetBackBufferFormat();
        PipelineDescription.DSVFormat = dx12.GetDepthStencilFormat();
        PipelineDescription.SampleDesc.Count = 1;
        PipelineDescription.SampleDesc.Quality = 0;
        PipelineDescription.SampleMask = UINT_MAX;

        PipelineDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        PipelineDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        PipelineDescription.RasterizerState.FrontCounterClockwise = FALSE;
        PipelineDescription.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        PipelineDescription.RasterizerState.DepthBiasClamp =
            D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        PipelineDescription.RasterizerState.SlopeScaledDepthBias =
            D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        PipelineDescription.RasterizerState.DepthClipEnable = TRUE;
        PipelineDescription.RasterizerState.MultisampleEnable = FALSE;
        PipelineDescription.RasterizerState.AntialiasedLineEnable = FALSE;
        PipelineDescription.RasterizerState.ForcedSampleCount = 0;
        PipelineDescription.RasterizerState.ConservativeRaster =
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        PipelineDescription.BlendState.AlphaToCoverageEnable = FALSE;
        PipelineDescription.BlendState.IndependentBlendEnable = FALSE;
        D3D12_RENDER_TARGET_BLEND_DESC BlendDescription{}; //Vertex Color出力のBlend設定
        BlendDescription.BlendEnable = FALSE;
        BlendDescription.LogicOpEnable = FALSE;
        BlendDescription.SrcBlend = D3D12_BLEND_ONE;
        BlendDescription.DestBlend = D3D12_BLEND_ZERO;
        BlendDescription.BlendOp = D3D12_BLEND_OP_ADD;
        BlendDescription.SrcBlendAlpha = D3D12_BLEND_ONE;
        BlendDescription.DestBlendAlpha = D3D12_BLEND_ZERO;
        BlendDescription.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        BlendDescription.LogicOp = D3D12_LOGIC_OP_NOOP;
        BlendDescription.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        PipelineDescription.BlendState.RenderTarget[0] = BlendDescription;

        PipelineDescription.DepthStencilState.DepthEnable = TRUE;
        PipelineDescription.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ALL;
        PipelineDescription.DepthStencilState.DepthFunc =
            D3D12_COMPARISON_FUNC_LESS_EQUAL;
        PipelineDescription.DepthStencilState.StencilEnable = FALSE;
        PipelineDescription.IBStripCutValue =
            D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        Result = dx12.GetDevice()->CreateGraphicsPipelineState(
            &PipelineDescription,
            IID_PPV_ARGS(&PipelineState)
        );
        return SUCCEEDED(Result);
    }

    //Upload Heapへ頂点Bufferを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool VertexMesh::CreateVertexBuffer(DirectX12& dx12)
    {
        const std::size_t BufferSize = sizeof(Vertex) * Vertices.size(); //頂点DataのByte数

        if (!CreateUploadBuffer(dx12, Vertices.data(), BufferSize, VertexBuffer))
        {
            return false;
        }

        VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
        VertexBufferView.SizeInBytes = static_cast<UINT>(BufferSize);
        VertexBufferView.StrideInBytes = sizeof(Vertex);
        return true;
    }

    //Upload HeapへIndex Bufferを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool VertexMesh::CreateIndexBuffer(DirectX12& dx12)
    {
        const std::size_t BufferSize = sizeof(uint32_t) * Indices.size(); //Index DataのByte数

        if (!CreateUploadBuffer(dx12, Indices.data(), BufferSize, IndexBuffer))
        {
            return false;
        }

        IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
        IndexBufferView.SizeInBytes = static_cast<UINT>(BufferSize);
        IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
        return true;
    }

    //概要：CPU側の頂点一覧を取得する
    //引数：なし
    //戻り値：読み取り専用頂点一覧
    const std::vector<Vertex>& VertexMesh::GetVertices() const
    {
        return Vertices;
    }

    //概要：CPU側の三角形Index一覧を取得する
    //引数：なし
    //戻り値：読み取り専用Index一覧
    const std::vector<uint32_t>& VertexMesh::GetIndices() const
    {
        return Indices;
    }

    //概要：CPU側の頂点数を取得する
    //引数：なし
    //戻り値：登録頂点数
    uint32_t VertexMesh::GetVertexCount() const
    {
        return static_cast<uint32_t>(Vertices.size());
    }

    //概要：CPU側の三角形Index数を取得する
    //引数：なし
    //戻り値：登録Index数
    uint32_t VertexMesh::GetIndexCount() const
    {
        return static_cast<uint32_t>(Indices.size());
    }
}
