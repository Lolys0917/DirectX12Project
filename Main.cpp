// ==========================================================
// Main.cpp
// DX12 Texture Sample
// ==========================================================

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wincodec.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

#include "d3dx12.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")

using namespace Microsoft::WRL;

// ==========================================================
// 設定
// ==========================================================

const int WIDTH = 1280;
const int HEIGHT = 720;
const int FRAME_COUNT = 2;

// ==========================================================
// 構造体
// ==========================================================

struct Vertex
{
    float x, y, z;
    float u, v;
};
struct PolygonState
{
    float x, y, z;
    float r, g, b, a;
};

struct TextureData
{
    ComPtr<ID3D12Resource> resource;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};

    UINT width = 0;
    UINT height = 0;
};

// ==========================================================
// グローバル
// ==========================================================

HWND g_hwnd = nullptr;

ComPtr<ID3D12Device> device;
ComPtr<IDXGISwapChain3> swapChain;
ComPtr<ID3D12CommandQueue> commandQueue;

ComPtr<ID3D12DescriptorHeap> rtvHeap;
ComPtr<ID3D12DescriptorHeap> srvHeap;

ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];

ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12GraphicsCommandList> commandList;

ComPtr<ID3D12Fence> fence;

HANDLE fenceEvent = nullptr;

UINT64 fenceValue = 0;

UINT frameIndex = 0;

UINT rtvDescriptorSize = 0;
UINT srvDescriptorSize = 0;

ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;

ComPtr<ID3D12Resource> vertexBuffer;

D3D12_VERTEX_BUFFER_VIEW vbView{};

UINT vertexCount = 0;

std::unordered_map<std::string, TextureData> g_textures;

// ==========================================================
// ウィンドウ
// ==========================================================

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    switch (msg)
    {
    case WM_DESTROY:

        PostQuitMessage(0);

        return 0;
    }

    return DefWindowProc(
        hwnd,
        msg,
        wparam,
        lparam
    );
}

void CreateWindowApp(HINSTANCE hInstance)
{
    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "DX12WindowClass";

    RegisterClass(&wc);

    g_hwnd =
        CreateWindowEx(
            0,
            wc.lpszClassName,
            "DX12 Texture Sample",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WIDTH,
            HEIGHT,
            nullptr,
            nullptr,
            hInstance,
            nullptr
        );

    ShowWindow(
        g_hwnd,
        SW_SHOW
    );
}

// ==========================================================
// GPU同期
// ==========================================================

void WaitForGPU()
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

// ==========================================================
// DX12初期化
// ==========================================================

void InitD3D()
{
    ComPtr<IDXGIFactory6> factory;

    CreateDXGIFactory1(
        IID_PPV_ARGS(&factory)
    );

    // ======================================================
    // Device
    // ======================================================

    D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)
    );

    // ======================================================
    // CommandQueue
    // ======================================================

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};

    queueDesc.Type =
        D3D12_COMMAND_LIST_TYPE_DIRECT;

    device->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&commandQueue)
    );

    // ======================================================
    // SwapChain
    // ======================================================

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

    frameIndex =
        swapChain->GetCurrentBackBufferIndex();

    // ======================================================
    // RTV Heap
    // ======================================================

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};

    rtvDesc.NumDescriptors =
        FRAME_COUNT;

    rtvDesc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    device->CreateDescriptorHeap(
        &rtvDesc,
        IID_PPV_ARGS(&rtvHeap)
    );

    rtvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV
        );

    // ======================================================
    // RenderTarget
    // ======================================================

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        swapChain->GetBuffer(
            i,
            IID_PPV_ARGS(&renderTargets[i])
        );

        device->CreateRenderTargetView(
            renderTargets[i].Get(),
            nullptr,
            rtvHandle
        );

        rtvHandle.Offset(
            1,
            rtvDescriptorSize
        );
    }

    // ======================================================
    // CommandAllocator
    // ======================================================

    device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator)
    );

    // ======================================================
    // CommandList
    // ======================================================

    device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList)
    );

    commandList->Close();

    // ======================================================
    // SRV Heap
    // ======================================================

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};

    srvDesc.NumDescriptors = 256;

    srvDesc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    srvDesc.Flags =
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    device->CreateDescriptorHeap(
        &srvDesc,
        IID_PPV_ARGS(&srvHeap)
    );

    srvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );

    // ======================================================
    // Fence
    // ======================================================

    device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence)
    );

    fenceValue = 1;

    fenceEvent =
        CreateEvent(
            nullptr,
            FALSE,
            FALSE,
            nullptr
        );
}

// ==========================================================
// テクスチャ読み込み
// ==========================================================

bool LoadTexture(
    std::string name,
    std::wstring filePath
)
{
    HRESULT hr;

    IWICImagingFactory* wicFactory = nullptr;

    CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED
    );

    hr =
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&wicFactory)
        );

    if (FAILED(hr))
    {
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;

    hr =
        wicFactory->CreateDecoderFromFilename(
            filePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &decoder
        );

    if (FAILED(hr))
    {
        MessageBoxA(
            0,
            "Texture Load Failed",
            "Error",
            MB_OK
        );

        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;

    decoder->GetFrame(
        0,
        &frame
    );

    UINT width = 0;
    UINT height = 0;

    frame->GetSize(
        &width,
        &height
    );

    IWICFormatConverter* converter = nullptr;

    wicFactory->CreateFormatConverter(
        &converter
    );

    converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );

    std::vector<BYTE> pixels(
        width * height * 4
    );

    converter->CopyPixels(
        nullptr,
        width * 4,
        (UINT)pixels.size(),
        pixels.data()
    );

    TextureData texData;

    texData.width = width;
    texData.height = height;

    CD3DX12_RESOURCE_DESC texDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            width,
            height
        );

    CD3DX12_HEAP_PROPERTIES defaultHeap(
        D3D12_HEAP_TYPE_DEFAULT
    );

    hr =
        device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&texData.resource)
        );

    if (FAILED(hr))
    {
        return false;
    }

    UINT64 uploadSize =
        GetRequiredIntermediateSize(
            texData.resource.Get(),
            0,
            1
        );

    ComPtr<ID3D12Resource> uploadBuffer;

    CD3DX12_HEAP_PROPERTIES uploadHeap(
        D3D12_HEAP_TYPE_UPLOAD
    );

    CD3DX12_RESOURCE_DESC uploadDesc =
        CD3DX12_RESOURCE_DESC::Buffer(
            uploadSize
        );

    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    D3D12_SUBRESOURCE_DATA subresource = {};

    subresource.pData =
        pixels.data();

    subresource.RowPitch =
        width * 4;

    subresource.SlicePitch =
        subresource.RowPitch * height;

    commandAllocator->Reset();

    commandList->Reset(
        commandAllocator.Get(),
        nullptr
    );

    UpdateSubresources(
        commandList.Get(),
        texData.resource.Get(),
        uploadBuffer.Get(),
        0,
        0,
        1,
        &subresource
    );

    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            texData.resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

    commandList->ResourceBarrier(
        1,
        &barrier
    );

    commandList->Close();

    ID3D12CommandList* lists[] =
    {
        commandList.Get()
    };

    commandQueue->ExecuteCommandLists(
        1,
        lists
    );

    WaitForGPU();

    // ======================================================
    // SRV
    // ======================================================

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    srvDesc.ViewDimension =
        D3D12_SRV_DIMENSION_TEXTURE2D;

    srvDesc.Texture2D.MipLevels = 1;

    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
        srvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    device->CreateShaderResourceView(
        texData.resource.Get(),
        &srvDesc,
        cpuHandle
    );

    texData.gpuHandle =
        srvHeap->GetGPUDescriptorHandleForHeapStart();

    g_textures[name] = texData;

    converter->Release();
    frame->Release();
    decoder->Release();
    wicFactory->Release();

    return true;
}

// ==========================================================
// 頂点生成
// ==========================================================
std::vector<Vertex> CreateQuad()
{
    float posx = 0.75f;
    float posy = 0.75f;
    float size = 0.2f;

    return
    {
        {-size + posx, -size + posy, 0.0f, 0.0f, 1.0f},
        {-size + posx,  size + posy, 0.0f, 0.0f, 0.0f},
        { size + posx, -size + posy, 0.0f, 1.0f, 1.0f},

        { size + posx, -size + posy, 0.0f, 1.0f, 1.0f},
        {-size + posx,  size + posy, 0.0f, 0.0f, 0.0f},
        { size + posx,  size + posy, 0.0f, 1.0f, 0.0f}
    };
}

// ==========================================================
// 描画初期化
// ==========================================================

void InitGraphics()
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

    // ======================================================
    // Texture
    // ======================================================

    LoadTexture(
        "test",
        L"UIDemo.png"
    );

    // ======================================================
    // VertexBuffer
    // ======================================================

    auto vertices =
        CreateQuad();

    vertexCount =
        (UINT)vertices.size();

    UINT size =
        sizeof(Vertex) * vertexCount;

    CD3DX12_HEAP_PROPERTIES heapProp(
        D3D12_HEAP_TYPE_UPLOAD
    );

    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(size);

    device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );

    void* ptr = nullptr;

    vertexBuffer->Map(
        0,
        nullptr,
        &ptr
    );

    memcpy(
        ptr,
        vertices.data(),
        size
    );

    vertexBuffer->Unmap(
        0,
        nullptr
    );

    vbView.BufferLocation =
        vertexBuffer->GetGPUVirtualAddress();

    vbView.SizeInBytes =
        size;

    vbView.StrideInBytes =
        sizeof(Vertex);
}

// ==========================================================
// 描画
// ==========================================================

void Render()
{
    commandAllocator->Reset();

    commandList->Reset(
        commandAllocator.Get(),
        pipelineState.Get()
    );

    // ======================================================
    // RootSignature
    // ======================================================

    commandList->SetGraphicsRootSignature(
        rootSignature.Get()
    );

    // ======================================================
    // DescriptorHeap
    // ======================================================

    ID3D12DescriptorHeap* heaps[] =
    {
        srvHeap.Get()
    };

    commandList->SetDescriptorHeaps(
        1,
        heaps
    );

    // ======================================================
    // Texture
    // ======================================================

    commandList->SetGraphicsRootDescriptorTable(
        0,
        g_textures["test"].gpuHandle
    );

    // ======================================================
    // Barrier
    // ======================================================

    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

    commandList->ResourceBarrier(
        1,
        &barrier
    );

    // ======================================================
    // RTV
    // ======================================================

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        frameIndex,
        rtvDescriptorSize
    );
    commandList->OMSetRenderTargets(
        1,
        &rtvHandle,
        FALSE,
        nullptr
    );

    // ======================================================
    // Clear
    // ======================================================

    float clearColor[] =
    {
        0.1f,
        0.2f,
        0.4f,
        1.0f
    };

    commandList->ClearRenderTargetView(
        rtvHandle,
        clearColor,
        0,
        nullptr
    );

    // ======================================================
    // Viewport
    // ======================================================

    D3D12_VIEWPORT vp =
    {
        0.0f,
        0.0f,
        (float)WIDTH,
        (float)HEIGHT,
        0.0f,
        1.0f
    };

    D3D12_RECT scissor =
    {
        0,
        0,
        WIDTH,
        HEIGHT
    };

    commandList->RSSetViewports(
        1,
        &vp
    );

    commandList->RSSetScissorRects(
        1,
        &scissor
    );

    // ======================================================
    // Draw
    // ======================================================

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    commandList->IASetVertexBuffers(
        0,
        1,
        &vbView
    );

    commandList->DrawInstanced(
        vertexCount,
        1,
        0,
        0
    );

    // ======================================================
    // Barrier
    // ======================================================

    barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        );

    commandList->ResourceBarrier(
        1,
        &barrier
    );

    commandList->Close();

    ID3D12CommandList* lists[] =
    {
        commandList.Get()
    };

    commandQueue->ExecuteCommandLists(
        1,
        lists
    );

    swapChain->Present(
        1,
        0
    );

    WaitForGPU();
}

// ==========================================================
// Main
// ==========================================================

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int
)
{
    CreateWindowApp(hInstance);

    InitD3D();

    InitGraphics();

    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(
            &msg,
            nullptr,
            0,
            0,
            PM_REMOVE
        ))
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

std::vector<PolygonState> Polygon(int sides, float radius)
{
    std::vector<PolygonState> vertices;

    PolygonState center = { 0,0,0, 1,1,1,1 };

    for (int i = 0; i < sides; i++)
    {
        float angle1 = (float)i / sides * 2.0f * 3.141592f;
        float angle2 = (float)(i + 1) / sides * 2.0f * 3.141592f;

        PolygonState v1 = { cosf(angle1) * radius, sinf(angle1) * radius, 0, 0,1,0,1 };
        PolygonState v2 = { cosf(angle2) * radius, sinf(angle2) * radius, 0, 0,1,0,1 };

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

//制作関数用メモ

//頂点を作成する関数
//テクスチャを読込保存させる関数
//テクスチャをGPUに送る関数
//描画情報を送り込む関数←実質Draw関数