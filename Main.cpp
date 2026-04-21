//メインコード

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <d3dcompiler.h>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

// ==========================================================
// 設定
// ==========================================================
const int WIDTH = 1280;
const int HEIGHT = 720;
const int FRAME_COUNT = 2;

// ==========================================================
// グローバル
// ==========================================================
HWND g_hwnd = nullptr;

ComPtr<ID3D12Device> device;
ComPtr<IDXGISwapChain3> swapChain;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<ID3D12DescriptorHeap> rtvHeap;
ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];
ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12GraphicsCommandList> commandList;

ComPtr<ID3D12Fence> fence;
UINT64 fenceValue = 0;
HANDLE fenceEvent;

UINT rtvDescriptorSize;
UINT frameIndex;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;

ComPtr<ID3D12Resource> vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW vbView;
UINT vertexCount = 0;

bool initialized = false;

//プロトタイプ宣言

//！！テスト用プロトタイプ宣言！！
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

std::vector<Vertex> Polygon(int sides, float radius);
void CreateVertexBuffer(int sides);

// ==========================================================
// ウィンドウプロシージャ
// ==========================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

// ==========================================================
// ウィンドウ作成
// ==========================================================
void CreateWindowApp(HINSTANCE hInstance)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "DX12Sample";

    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        "DX12 Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WIDTH, HEIGHT,
        nullptr, nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(g_hwnd, SW_SHOW);
}

// ==========================================================
// 初期化
// ==========================================================
void InitD3D()
{
    ComPtr<IDXGIFactory6> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    // デバイス作成
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

    // コマンドキュー
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

    // スワップチェーン
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.BufferCount = FRAME_COUNT;
    scDesc.Width = WIDTH;
    scDesc.Height = HEIGHT;
    scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> sc1;
    factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        g_hwnd,
        &scDesc,
        nullptr,
        nullptr,
        &sc1
    );

    sc1.As(&swapChain);
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // RTVヒープ
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = FRAME_COUNT;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap));

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // RTV生成
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, handle);
        handle.Offset(1, rtvDescriptorSize);
    }

    // コマンド
    device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator));

    device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList));

    commandList->Close();

    // フェンス
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    fenceValue = 1;
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

// ==========================================================
// フレーム同期
// ==========================================================
void WaitForGPU()
{
    const UINT64 currentFence = fenceValue;
    commandQueue->Signal(fence.Get(), currentFence);
    fenceValue++;

    if (fence->GetCompletedValue() < currentFence)
    {
        fence->SetEventOnCompletion(currentFence, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

// ===================
// 内部描画
// ===================
void Draw()
{
    CreateVertexBuffer(4);
}

// ==========================================================
// 描画
// ==========================================================
void Render()
{
    if (!initialized)
    {
        initialized = true;

        // -----------------------------
        // ルートシグネチャ
        // -----------------------------
        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(0, nullptr, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> sigBlob;
        D3D12SerializeRootSignature(&rsDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &sigBlob, nullptr);

        device->CreateRootSignature(0,
            sigBlob->GetBufferPointer(),
            sigBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature));

        // -----------------------------
        // シェーダ（直書き）
        // -----------------------------
        const char* vs =
            "struct VS_IN { float3 pos : POSITION; float4 col : COLOR; };"
            "struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };"
            "PS_IN main(VS_IN input){"
            "PS_IN o; o.pos=float4(input.pos,1); o.col=input.col; return o;}";

        const char* ps =
            "struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };"
            "float4 main(PS_IN input) : SV_TARGET { return input.col; }";

        ComPtr<ID3DBlob> vsBlob, psBlob;
        D3DCompile(vs, strlen(vs), nullptr, nullptr, nullptr,
            "main", "vs_5_0", 0, 0, &vsBlob, nullptr);

        D3DCompile(ps, strlen(ps), nullptr, nullptr, nullptr,
            "main", "ps_5_0", 0, 0, &psBlob, nullptr);

        // -----------------------------
        // 入力レイアウト
        // -----------------------------
        D3D12_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
            { "COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
        };

        // -----------------------------
        // PSO
        // -----------------------------
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { layout, _countof(layout) };
        psoDesc.pRootSignature = rootSignature.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.NumRenderTargets = 1;
        psoDesc.SampleDesc.Count = 1;

        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE; // 今は深度使わない

        device->CreateGraphicsPipelineState(&psoDesc,
            IID_PPV_ARGS(&pipelineState));

        // -----------------------------
        // 頂点生成（四角形）
        // -----------------------------
        auto vertices = Polygon(4, 0.5f);
        vertexCount = (UINT)vertices.size();

        UINT size = sizeof(Vertex) * vertexCount;

        CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

        device->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer)
        );

        void* ptr;
        vertexBuffer->Map(0, nullptr, &ptr);
        memcpy(ptr, vertices.data(), size);
        vertexBuffer->Unmap(0, nullptr);

        vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vbView.SizeInBytes = size;
        vbView.StrideInBytes = sizeof(Vertex);
    }

    // =========================================
    // 通常描画
    // =========================================
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), pipelineState.Get());

    // バリア
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

    commandList->ResourceBarrier(1, &barrier);

    // RTV
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        frameIndex,
        rtvDescriptorSize);

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // ビューポート
    D3D12_VIEWPORT vp = { 0,0,(float)WIDTH,(float)HEIGHT,0,1 };
    D3D12_RECT scissor = { 0,0,WIDTH,HEIGHT };
    commandList->RSSetViewports(1, &vp);
    commandList->RSSetScissorRects(1, &scissor);

    // 描画設定
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vbView);

    // 描画
    commandList->DrawInstanced(vertexCount, 1, 0, 0);

    // 戻す
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    commandList->ResourceBarrier(1, &barrier);

    commandList->Close();

    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, lists);

    swapChain->Present(1, 0);

    WaitForGPU();
}

// ==========================================================
// エントリポイント
// ==========================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    CreateWindowApp(hInstance);
    InitD3D();

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    WaitForGPU();
    CloseHandle(fenceEvent);

    return 0;
}


//=======================================
//
// ！！！ここから先はテスト用です！！！
//
//=======================================

#include <vector>
#include <cmath>

std::vector<Vertex> Polygon(int sides, float radius)
{
    std::vector<Vertex> vertices;

    Vertex center = { 0,0,0, 1,1,1,1 };

    for (int i = 0; i < sides; i++)
    {
        float angle1 = (float)i / sides * 2.0f * 3.141592f;
        float angle2 = (float)(i + 1) / sides * 2.0f * 3.141592f;

        Vertex v1 = { cosf(angle1) * radius, sinf(angle1) * radius, 0, 0,1,0,1 };
        Vertex v2 = { cosf(angle2) * radius, sinf(angle2) * radius, 0, 0,1,0,1 };

        // 三角形（中心・v1・v2）
        vertices.push_back(center);
        vertices.push_back(v1);
        vertices.push_back(v2);
    }

    return vertices;
}

void CreateVertexBuffer(int sides)
{
    auto vertices = Polygon(sides, 2.0f);

    UINT size = sizeof(Vertex) * (UINT)vertices.size();

    // アップロードヒープ
    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );

    // 書き込み
    void* ptr;
    vertexBuffer->Map(0, nullptr, &ptr);
    memcpy(ptr, vertices.data(), size);
    vertexBuffer->Unmap(0, nullptr);

    vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes = size;
    vbView.StrideInBytes = sizeof(Vertex);
}