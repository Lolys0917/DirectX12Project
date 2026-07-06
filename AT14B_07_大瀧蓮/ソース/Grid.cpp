#include "Grid.h"

#include <climits>
#include <cstring>
#include <d3dcompiler.h>

#include "Core.h"
#include "d3dx12.h"

#pragma comment(lib, "d3dcompiler.lib")

bool Grid::Initialize(Core& core, int halfCount, float interval)
{
    std::vector<GridVertex> vertices;
    float size = halfCount * interval;

    for (int i = -halfCount; i <= halfCount; i++)
    {
        float p = i * interval;

        float r = 0.35f, g = 0.35f, b = 0.35f;
        if (i == 0) { r = 1.0f; g = 0.2f; b = 0.2f; }

        vertices.push_back({ -size, p, 0.0f, r, g, b, 1.0f });
        vertices.push_back({ size, p, 0.0f, r, g, b, 1.0f });

        r = 0.35f; g = 0.35f; b = 0.35f;
        if (i == 0) { r = 0.2f; g = 1.0f; b = 0.2f; }

        vertices.push_back({ p, -size, 0.0f, r, g, b, 1.0f });
        vertices.push_back({ p,  size, 0.0f, r, g, b, 1.0f });
    }

    return CreatePipeline(core) && CreateVertexBuffer(core, vertices);
}

void Grid::Draw(Core& core) const
{
    auto* commandList = core.GetCommandList();

    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}

bool Grid::CreatePipeline(Core& core)
{
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        0,
        nullptr,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );

    if (FAILED(hr)) return false;

    hr = core.GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_)
    );

    if (FAILED(hr)) return false;

    const char* vs =
        "struct VS_IN{float3 pos:POSITION;float4 color:COLOR;};"
        "struct PS_IN{float4 pos:SV_POSITION;float4 color:COLOR;};"
        "PS_IN main(VS_IN input){"
        "PS_IN o;"
        "o.pos=float4(input.pos,1);"
        "o.color=input.color;"
        "return o;"
        "}";

    const char* ps =
        "struct PS_IN{float4 pos:SV_POSITION;float4 color:COLOR;};"
        "float4 main(PS_IN input):SV_TARGET{"
        "return input.color;"
        "}";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;

    hr = D3DCompile(vs, strlen(vs), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, nullptr);
    if (FAILED(hr)) return false;

    hr = D3DCompile(ps, strlen(ps), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, nullptr);
    if (FAILED(hr)) return false;

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { layout, _countof(layout) };
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    hr = core.GetDevice()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&pipelineState_)
    );

    return SUCCEEDED(hr);
}

bool Grid::CreateVertexBuffer(Core& core, const std::vector<GridVertex>& vertices)
{
    if (vertices.empty()) return false;

    vertexCount_ = static_cast<UINT>(vertices.size());
    UINT bufferSize = sizeof(GridVertex) * vertexCount_;

    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    HRESULT hr = core.GetDevice()->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer_)
    );

    if (FAILED(hr)) return false;

    void* ptr = nullptr;
    vertexBuffer_->Map(0, nullptr, &ptr);
    memcpy(ptr, vertices.data(), bufferSize);
    vertexBuffer_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = bufferSize;
    vertexBufferView_.StrideInBytes = sizeof(GridVertex);

    return true;
}