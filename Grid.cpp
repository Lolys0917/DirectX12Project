//|| Grid.cpp ||::::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  各Camera passへデバッグ用Gridを描画するComponentを実装する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v2.30  基底契約とGPU Resourceの明示的終了を追加
//||  2026_07_13  v2.20  Camera pass別WVPをRoot Constantsへ変更
//||  2026_07_13  v2.10  C++変数命名と宣言コメントを規則へ統一
//||  2026_07_13  v2.00  Component化し固定Camera依存を削除
//||  2026_06_01  v1.00  新規作成
//||

#include "Grid.h"

#include "DirectX12.h"
#include "Camera.h"
#include "RenderContext.h"

#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine
{
    namespace
    {
        constexpr UINT GridRootConstantCount = 16; //WVP行列を構成する32bit値数
        static_assert(sizeof(GridConstantBuffer) ==
            sizeof(UINT) * GridRootConstantCount);

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

)"; //Grid描画用HLSL Source
    }

    //未登録状態のGrid Componentを作成する
    Grid::Grid()
        : Component(StaticType)
        , VertexBufferView{}
    {
    }

    //Grid Componentを破棄する
    Grid::~Grid()
    {
        Finalize();
    }

    //Gridの終了処理を行う
    void Grid::Finalize()
    {
        VertexBuffer.Reset();
        PipelineState.Reset();
        RootSignature.Reset();
        VertexBufferView = {};
        Component::Finalize();
    }

    //未登録状態のGrid定義を複製する
    //戻り値: GPU Resourceを持たない複製Component
    std::unique_ptr<Component> Grid::Clone() const
    {
        auto Duplicate = std::make_unique<Grid>(); //未登録の複製Grid
        CopyDefinitionTo(*Duplicate);
        return Duplicate;
    }

    //Grid用GPU Resourceを作成する
    //引数: dx12 描画基盤
    //戻り値: 全Resource作成に成功した場合はtrue
    bool Grid::Initialize(DirectX12& dx12)
    {
        if (!Component::Initialize(dx12))
        {
            return false;
        }

        VertexBuffer.Reset();
        PipelineState.Reset();
        RootSignature.Reset();
        VertexBufferView = {};
        BuildGrid();

        if (!CreateRootSignature(dx12)) return false;
        if (!CreatePipelineState(dx12)) return false;
        if (!CreateVertexBuffer(dx12)) return false;

        return true;
    }

    //Gridの時間依存情報を更新する
    //引数: deltaTime 前回更新からの秒数
    void Grid::Update(float deltaTime)
    {
        (void)deltaTime;
    }

    //RenderContextのCameraでGridを描画する
    //引数: renderContext 描画基盤と現在のCamera
    void Grid::Draw(const RenderContext& renderContext)
    {
        if (!PipelineState)
            return;

        if (!RootSignature)
            return;

        DirectX12& Dx12 = renderContext.Graphics; //このpassで使用する描画基盤
        const Camera& ViewCamera = renderContext.ViewCamera; //このpassで使用するCamera
        ID3D12GraphicsCommandList* CommandList =
            Dx12.GetCommandList(); //描画命令の記録先

        using namespace DirectX;

        XMMATRIX World =
            XMMatrixIdentity(); //GridのWorld行列

        XMMATRIX ViewProjection =
            ViewCamera.GetViewProjectionMatrix(); //現在CameraのViewProjection行列

        XMMATRIX WorldViewProjection =
            World * ViewProjection; //GridのWVP行列

        XMMATRIX TransposedWorldViewProjection =
            XMMatrixTranspose(WorldViewProjection); //Shader転送用の転置済みWVP行列

        GridConstantBuffer RootConstants{}; //今回のCamera passだけで使用する描画定数
        XMStoreFloat4x4(
            &RootConstants.WorldViewProjection,
            TransposedWorldViewProjection
        );

        CommandList->SetGraphicsRootSignature(
            RootSignature.Get()
        );

        CommandList->SetPipelineState(
            PipelineState.Get()
        );

        CommandList->SetGraphicsRoot32BitConstants(
            0,
            GridRootConstantCount,
            &RootConstants,
            0
        );

        CommandList->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_LINELIST
        );

        CommandList->IASetVertexBuffers(
            0,
            1,
            &VertexBufferView
        );

        CommandList->DrawInstanced(
            static_cast<UINT>(Vertices.size()),
            1,
            0,
            0
        );
    }

    //デバッグ表示用GridのLine List頂点を構築する
    void Grid::BuildGrid()
    {
        Vertices.clear();

        const float HalfSize = 5.0f; //Gridの中心から端までの長さ

        const DirectX::XMFLOAT4 Black =
        {
            0.0f, 0.0f, 0.0f, 1.0f
        }; //通常Grid線の色

        const DirectX::XMFLOAT4 Red =
        {
            1.0f, 0.0f, 0.0f, 1.0f
        }; //X軸線の色

        const DirectX::XMFLOAT4 Green =
        {
            0.0f, 1.0f, 0.0f, 1.0f
        }; //Y軸線の色

        const DirectX::XMFLOAT4 Blue =
        {
            0.0f, 0.0f, 1.0f, 1.0f
        }; //Z軸線の色

        auto AddLine =
            [this](
                const DirectX::XMFLOAT3& start,
                const DirectX::XMFLOAT3& end,
                const DirectX::XMFLOAT4& color)
            {
                GridVertex Vertex0{}; //線分の始点頂点
                Vertex0.Position = start;
                Vertex0.Color = color;

                GridVertex Vertex1{}; //線分の終点頂点
                Vertex1.Position = end;
                Vertex1.Color = color;

                Vertices.push_back(Vertex0);
                Vertices.push_back(Vertex1);
            }; //線分をLine Listへ追加する処理

        /*
            X-Z平面の10×10グリッド

            範囲：
                X = -5 ～ +5
                Z = -5 ～ +5

            10マス作るため、線は11本ずつ。
        */
        for (int Index = -5; Index <= 5; ++Index) //X軸とZ軸の平行線を生成する
        {
            const float GridPosition = static_cast<float>(Index); //現在線の軸上位置

            /*
                X方向の線

                z = 0 の線をX軸として赤にする。
            */
            AddLine(
                DirectX::XMFLOAT3(-HalfSize, 0.0f, GridPosition),
                DirectX::XMFLOAT3(HalfSize, 0.0f, GridPosition),
                Index == 0 ? Red : Black
            );

            /*
                Z方向の線

                x = 0 の線をZ軸として青にする。
            */
            AddLine(
                DirectX::XMFLOAT3(GridPosition, 0.0f, -HalfSize),
                DirectX::XMFLOAT3(GridPosition, 0.0f, HalfSize),
                Index == 0 ? Blue : Black
            );
        }

        /*
            Y軸

            地面から上方向に伸びる緑線。
        */
        AddLine(
            DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 5.0f, 0.0f),
            Green
        );
    }

    //Grid描画用RootSignatureを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool Grid::CreateRootSignature(DirectX12& dx12)
    {
        D3D12_ROOT_PARAMETER RootParameter{}; //WVP Root Constants用Parameter
        RootParameter.ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        RootParameter.Constants.ShaderRegister = 0;
        RootParameter.Constants.RegisterSpace = 0;
        RootParameter.Constants.Num32BitValues = GridRootConstantCount;
        RootParameter.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_VERTEX;

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
        ); //RootSignatureのSerialize結果

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

    //Grid描画用PipelineStateを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool Grid::CreatePipelineState(DirectX12& dx12)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> VertexShaderBlob; //Compile済みVertex Shader
        Microsoft::WRL::ComPtr<ID3DBlob> PixelShaderBlob; //Compile済みPixel Shader
        Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob; //Shader Compile失敗内容

        UINT CompileFlags = 0; //Shader Compile Option

#if defined(_DEBUG)
        CompileFlags =
            D3DCOMPILE_DEBUG |
            D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT Result = D3DCompile(
            GridShaderCode,
            std::strlen(GridShaderCode),
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
            GridShaderCode,
            std::strlen(GridShaderCode),
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

        D3D12_INPUT_ELEMENT_DESC InputLayout[2]{}; //PositionとColorの頂点入力定義

        InputLayout[0].SemanticName = "POSITION";
        InputLayout[0].SemanticIndex = 0;
        InputLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        InputLayout[0].InputSlot = 0;
        InputLayout[0].AlignedByteOffset = 0;
        InputLayout[0].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        InputLayout[0].InstanceDataStepRate = 0;

        InputLayout[1].SemanticName = "COLOR";
        InputLayout[1].SemanticIndex = 0;
        InputLayout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        InputLayout[1].InputSlot = 0;
        InputLayout[1].AlignedByteOffset = 12;
        InputLayout[1].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        InputLayout[1].InstanceDataStepRate = 0;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDescription{}; //Grid描画Pipeline設定
        PipelineDescription.pRootSignature = RootSignature.Get();

        PipelineDescription.VS.pShaderBytecode =
            VertexShaderBlob->GetBufferPointer();
        PipelineDescription.VS.BytecodeLength =
            VertexShaderBlob->GetBufferSize();

        PipelineDescription.PS.pShaderBytecode =
            PixelShaderBlob->GetBufferPointer();
        PipelineDescription.PS.BytecodeLength =
            PixelShaderBlob->GetBufferSize();

        PipelineDescription.InputLayout.pInputElementDescs =
            InputLayout;
        PipelineDescription.InputLayout.NumElements = 2;

        PipelineDescription.PrimitiveTopologyType =
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

        PipelineDescription.NumRenderTargets = 1;
        PipelineDescription.RTVFormats[0] =
            dx12.GetBackBufferFormat();

        PipelineDescription.DSVFormat =
            dx12.GetDepthStencilFormat();

        PipelineDescription.SampleDesc.Count = 1;
        PipelineDescription.SampleDesc.Quality = 0;
        PipelineDescription.SampleMask = UINT_MAX;

        PipelineDescription.RasterizerState.FillMode =
            D3D12_FILL_MODE_SOLID;
        PipelineDescription.RasterizerState.CullMode =
            D3D12_CULL_MODE_NONE;
        PipelineDescription.RasterizerState.FrontCounterClockwise = FALSE;
        PipelineDescription.RasterizerState.DepthBias =
            D3D12_DEFAULT_DEPTH_BIAS;
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

        D3D12_RENDER_TARGET_BLEND_DESC BlendDescription{}; //単一RenderTargetのBlend設定
        BlendDescription.BlendEnable = FALSE;
        BlendDescription.LogicOpEnable = FALSE;
        BlendDescription.SrcBlend = D3D12_BLEND_ONE;
        BlendDescription.DestBlend = D3D12_BLEND_ZERO;
        BlendDescription.BlendOp = D3D12_BLEND_OP_ADD;
        BlendDescription.SrcBlendAlpha = D3D12_BLEND_ONE;
        BlendDescription.DestBlendAlpha = D3D12_BLEND_ZERO;
        BlendDescription.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        BlendDescription.LogicOp = D3D12_LOGIC_OP_NOOP;
        BlendDescription.RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;

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

    //Grid頂点Bufferを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool Grid::CreateVertexBuffer(DirectX12& dx12)
    {
        const UINT BufferSize =
            static_cast<UINT>(
                sizeof(GridVertex) * Vertices.size()
                ); //頂点BufferのByte数

        D3D12_HEAP_PROPERTIES HeapProperties{}; //Upload Heap設定
        HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        HeapProperties.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProperties.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDescription{}; //頂点Buffer Resource設定
        ResourceDescription.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDescription.Alignment = 0;
        ResourceDescription.Width = BufferSize;
        ResourceDescription.Height = 1;
        ResourceDescription.DepthOrArraySize = 1;
        ResourceDescription.MipLevels = 1;
        ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDescription.SampleDesc.Count = 1;
        ResourceDescription.SampleDesc.Quality = 0;
        ResourceDescription.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ResourceDescription.Flags =
            D3D12_RESOURCE_FLAG_NONE;

        HRESULT Result = dx12.GetDevice()->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&VertexBuffer)
        ); //頂点Buffer作成結果

        if (FAILED(Result))
        {
            return false;
        }

        void* MappedData = nullptr; //頂点BufferのCPU書込先

        Result = VertexBuffer->Map(
            0,
            nullptr,
            &MappedData
        );

        if (FAILED(Result))
        {
            return false;
        }

        std::memcpy(
            MappedData,
            Vertices.data(),
            BufferSize
        );

        VertexBuffer->Unmap(
            0,
            nullptr
        );

        VertexBufferView.BufferLocation =
            VertexBuffer->GetGPUVirtualAddress();

        VertexBufferView.SizeInBytes =
            BufferSize;

        VertexBufferView.StrideInBytes =
            sizeof(GridVertex);

        return true;
    }

}
