#include "Graphics.h"
#include "Core.h"

void ClassDirectXManager::InitGraphics(ID3D12Device *device)
{
    HRESULT hr;

    // ======================================================
    // RootSignature
    // ======================================================

    CD3DX12_DESCRIPTOR_RANGE range;

    range.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        1,
        0
    );

    CD3DX12_ROOT_PARAMETER rootParam;

    rootParam.InitAsDescriptorTable(
        1,
        &range,
        D3D12_SHADER_VISIBILITY_PIXEL
    );

    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR
    );

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;

    rsDesc.Init(
        1,
        &rootParam,
        1,
        &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> sigBlob;
    ComPtr<ID3DBlob> errorBlob;

    hr =
        D3D12SerializeRootSignature(
            &rsDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &sigBlob,
            &errorBlob
        );

    device->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );

    // ======================================================
    // Shader
    // ======================================================

    const char* vs =
        "struct VS_IN                     "
        "{                                "
        " float3 pos : POSITION;          "
        " float2 uv : TEXCOORD;           "
        "};                               "

        "struct PS_IN                     "
        "{                                "
        " float4 pos : SV_POSITION;       "
        " float2 uv : TEXCOORD;           "
        "};                               "

        "PS_IN main(VS_IN input)          "
        "{                                "
        " PS_IN o;                        "
        " o.pos=float4(input.pos,1);      "
        " o.uv=input.uv;                  "
        " return o;                       "
        "}";

    const char* ps =
        "Texture2D tex0 : register(t0);   "
        "SamplerState smp : register(s0); "

        "struct PS_IN                     "
        "{                                "
        " float4 pos : SV_POSITION;       "
        " float2 uv : TEXCOORD;           "
        "};                               "

        "float4 main(PS_IN input)         "
        " : SV_TARGET                     "
        "{                                "
        " return tex0.Sample(smp,input.uv);"
        "}";

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;

    D3DCompile(
        vs,
        strlen(vs),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        0,
        0,
        &vsBlob,
        nullptr
    );

    D3DCompile(
        ps,
        strlen(ps),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        0,
        0,
        &psBlob,
        nullptr
    );

    // ======================================================
    // InputLayout
    // ======================================================

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };

    // ======================================================
    // PSO
    // ======================================================

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

    psoDesc.InputLayout =
    {
        layout,
        _countof(layout)
    };

    psoDesc.pRootSignature =
        rootSignature.Get();

    psoDesc.VS =
    {
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize()
    };

    psoDesc.PS =
    {
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize()
    };

    psoDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.NumRenderTargets = 1;

    psoDesc.RTVFormats[0] =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    psoDesc.SampleDesc.Count = 1;

    psoDesc.SampleMask = UINT_MAX;

    psoDesc.RasterizerState =
        CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    psoDesc.BlendState =
        CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    device->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&pipelineState)
    );

    //// ======================================================
    //// Texture
    //// ======================================================

    //LoadTexture(
    //    "test",
    //    L"UIDemo.png"
    //);

    //// ======================================================
    //// VertexBuffer
    //// ======================================================

    //auto vertices =
    //    CreateQuad();

    //vertexCount =
    //    (UINT)vertices.size();

    //UINT size =
    //    sizeof(Vertex) * vertexCount;

    //CD3DX12_HEAP_PROPERTIES heapProp(
    //    D3D12_HEAP_TYPE_UPLOAD
    //);

    //CD3DX12_RESOURCE_DESC bufferDesc =
    //    CD3DX12_RESOURCE_DESC::Buffer(size);

    //device->CreateCommittedResource(
    //    &heapProp,
    //    D3D12_HEAP_FLAG_NONE,
    //    &bufferDesc,
    //    D3D12_RESOURCE_STATE_GENERIC_READ,
    //    nullptr,
    //    IID_PPV_ARGS(&vertexBuffer)
    //);

    //void* ptr = nullptr;

    //vertexBuffer->Map(
    //    0,
    //    nullptr,
    //    &ptr
    //);

    //memcpy(
    //    ptr,
    //    vertices.data(),
    //    size
    //);

    //vertexBuffer->Unmap(
    //    0,
    //    nullptr
    //);

    //vbView.BufferLocation =
    //    vertexBuffer->GetGPUVirtualAddress();

    //vbView.SizeInBytes =
    //    size;

    //vbView.StrideInBytes =
    //    sizeof(Vertex);
}


void ClassDirectXManager::WaitForGPU()
{
    UINT64 currentFence = fenceValue;

    commandQueue->Signal(
        fence.Get(),
        currentFence
    );

    fenceValue++;

    if (fence->GetCompletedValue() < currentFence)
    {
        fence->SetEventOnCompletion(
            currentFence,
            fenceEvent
        );

        WaitForSingleObject(
            fenceEvent,
            INFINITE
        );
    }

    frameIndex =
        swapChain->GetCurrentBackBufferIndex();
}