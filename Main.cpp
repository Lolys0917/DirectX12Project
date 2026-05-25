//メインコード

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <d3dcompiler.h>
#include <vector>
#include <cmath>
#include <string>
#include <unordered_map>
#include <wincodec.h>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

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

HRESULT hr;

ComPtr<ID3D12Device> device;
ComPtr<IDXGISwapChain3> swapChain;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<ID3D12DescriptorHeap> rtvHeap;
ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];
ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12GraphicsCommandList> commandList;
ComPtr<ID3D12DescriptorHeap> srvHeap;
ComPtr<ID3D12Fence> fence;

UINT64 fenceValue = 0;
HANDLE fenceEvent;

UINT srvDescriptorSize = 0;
UINT rtvDescriptorSize;
UINT frameIndex;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;

ComPtr<ID3D12Resource> vertexBuffer;
std::vector < D3D12_VERTEX_BUFFER_VIEW> vbView;
UINT vertexCount = 0;

bool initialized = false;

//プロトタイプ宣言

//！！テスト用グローバル＆プロトタイプ宣言！！
struct ObjectState
{
    float x, y, z;
    float u, v;
};
struct PolygonState
{
    float x, y, z;
    float r, g, b, a;
};
struct Color
{
    int r, g, b, a;
};
struct TextureData
{
    UINT width;
    UINT height;

    std::vector<unsigned char> pixels;

    ComPtr<ID3D12Resource> resource;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
};

std::unordered_map<std::string, TextureData> g_textures;
std::vector<ObjectState> g_vertices;
std::vector<PolygonState> Polygon(int sides, float radius, Color color);
void CreateVertexBuffer(int sides);

void CreateVertexBufferOnTexture(const char* name);

bool LoadTexture(
    const char* name,
    std::wstring filePath);

std::vector<ObjectState> PolygonOnTexture(
    int sides,
    float radius,
    std::string tex);

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
	// DXGIファクトリー作成
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
    //CreateVertexBuffer(4);
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
        // ======================================================
        // Descriptor Range
        // ======================================================

        CD3DX12_DESCRIPTOR_RANGE range;

        range.Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            1,
            0
        );

        // ======================================================
        // Root Parameter
        // ======================================================

        CD3DX12_ROOT_PARAMETER rootParam;

        rootParam.InitAsDescriptorTable(
            1,
            &range,
            D3D12_SHADER_VISIBILITY_PIXEL
        );

        // ======================================================
        // Sampler
        // ======================================================

        CD3DX12_STATIC_SAMPLER_DESC sampler(
            0,
            D3D12_FILTER_MIN_MAG_MIP_LINEAR
        );

        // ======================================================
        // Root Signature
        // ======================================================

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

        if (FAILED(hr))
        {
            MessageBoxA(
                0,
                errorBlob ?
                (char*)errorBlob->GetBufferPointer()
                :
                "RootSignature Error",
                "Error",
                MB_OK
            );

            return;
        }

        device->CreateRootSignature(
            0,
            sigBlob->GetBufferPointer(),
            sigBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)
        );

        // -----------------------------
        // シェーダ
        // -----------------------------
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

        hr = D3DCompile(
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
            &errorBlob
        );

        if (FAILED(hr))
        {
            MessageBoxA(
                0,
                errorBlob ? (char*)errorBlob->GetBufferPointer() : "VS Compile Error",
                "Error",
                MB_OK
            );
            return;
        }

        hr = D3DCompile(
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
            &errorBlob
        );

        if (FAILED(hr))
        {
            MessageBoxA(
                0,
                errorBlob ? (char*)errorBlob->GetBufferPointer() : "PS Compile Error",
                "Error",
                MB_OK
            );
            return;
        }

        // -----------------------------
        // 入力レイアウト
        // -----------------------------
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

        // -----------------------------
        // PSO
        // -----------------------------
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

        // サンプルマスク
        psoDesc.SampleMask = UINT_MAX;

        // ラスタライザーステート
        psoDesc.RasterizerState =
            CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

        // カリング無効
        psoDesc.RasterizerState.CullMode =
            D3D12_CULL_MODE_NONE;

        // ブレンドステート
        psoDesc.BlendState =
            CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        // Depth
        psoDesc.DepthStencilState =
            CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        hr =
            device->CreateGraphicsPipelineState(
                &psoDesc,
                IID_PPV_ARGS(&pipelineState)
            );

        if (FAILED(hr))
        {
            MessageBoxA(
                0,
                "CreateGraphicsPipelineState Failed",
                "Error",
                MB_OK
            );
            return;
        }

        if (FAILED(hr))
        {
            MessageBoxA(0, "PSO Create Failed", "Error", MB_OK);
            return;
        }

        CreateVertexBufferOnTexture("name");
    }

    // ======================================================
    // 描画
    // ======================================================

    // 毎フレーム初期化_________________

	// コマンドリセット
    commandAllocator->Reset();
	// コマンドリストリセット
    commandList->Reset(
        commandAllocator.Get(),
        pipelineState.Get()
    );
    // 描画設定※コマンドリスト
    commandList->SetGraphicsRootSignature(
        rootSignature.Get()
    );

	// SRVヒープセット
    ID3D12DescriptorHeap* heaps[] =
    {
        srvHeap.Get()
    };

	// SRVヒープセット
    commandList->SetDescriptorHeaps(
        1,
        heaps
    );

	// SRVセット
    commandList->SetGraphicsRootDescriptorTable(
        0,
        g_textures.at("test").gpuHandle
    );

    // Present → RenderTarget
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

	// バリアセット
    commandList->ResourceBarrier(1, &barrier);

    // RTV
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        frameIndex,
        rtvDescriptorSize
    );

	// RTVセット
    commandList->OMSetRenderTargets(
        1,
        &rtvHandle,
        FALSE,
        nullptr
    );

    // クリア
    float clearColor[] =
    {
        0.1f,
        0.2f,
        0.4f,
        1.0f
    };

	// RTVクリア※初期背景カラー描画
    commandList->ClearRenderTargetView(
        rtvHandle,
        clearColor,
        0,
        nullptr
    );

    // Viewport
    D3D12_VIEWPORT vp =
    {
        0.0f,
        0.0f,
        (float)WIDTH,
        (float)HEIGHT,
        0.0f,
        1.0f
    };
	// ScissorRect
    D3D12_RECT scissor =
    {
        0,
        0,
        WIDTH,
        HEIGHT
    };

	commandList->RSSetViewports(1, &vp);        // ビューポートセット
	commandList->RSSetScissorRects(1, &scissor);// シザーテストセット

    // 描画設定
    commandList->SetGraphicsRootSignature(
        rootSignature.Get()
    );
	// PSOセット
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );
	// 頂点バッファセット
    commandList->IASetVertexBuffers(
        0,
        1,
        &vbView.back()
    );
    // 描画
    commandList->DrawInstanced(
        vertexCount,
        1,
        0,
        0
    );
    // RenderTarget → Present
    barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        );
	// バリアセット
    commandList->ResourceBarrier(1, &barrier);
	// コマンドリストクローズ
    commandList->Close();
	// コマンドリストセット
    ID3D12CommandList* lists[] =
    {
        commandList.Get()
    };
	// コマンドキューにセット
    commandQueue->ExecuteCommandLists(1, lists);
	// 画面に表示
    swapChain->Present(1, 0);
	// GPU完了待ち
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

std::vector<PolygonState> Polygon(int sides, float radius, Color color)
{
    std::vector<PolygonState> vertices;

    PolygonState center = { 0,0,0, color.r, color.g, color.b, color.a };

    for (int i = 0; i < sides; i++)
    {
        float angle1 = (float)i / sides * 2.0f * 3.141592f;
        float angle2 = (float)(i + 1) / sides * 2.0f * 3.141592f;

        PolygonState v1 = { cosf(angle1) * radius, sinf(angle1) * radius, 0, color.r, color.g, color.b, color.a };
        PolygonState v2 = { cosf(angle2) * radius, sinf(angle2) * radius, 0, color.r, color.g, color.b, color.a };

        // 三角形（中心・v1・v2）
        vertices.push_back(center);
        vertices.push_back(v1);
        vertices.push_back(v2);
    }

    return vertices;
}

void CreateVertexBuffer(int sides)
{
    Color color = { 1.0f, 1.0f, 1.0f, 1.0f };
    auto vertices = Polygon(sides, 2.0f, color);

    UINT size = sizeof(PolygonState) * (UINT)vertices.size();

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

    vbView.push_back({});
    vbView.back().BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vbView.back().SizeInBytes = size;
    vbView.back().StrideInBytes = sizeof(PolygonState);
}

bool LoadTexture(
    const char* name,
    std::wstring filePath
)
{
    HRESULT hr;

    // ======================================================
    // WIC 初期化
    // ======================================================

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

    // ======================================================
    // Decoder
    // ======================================================

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

    // ======================================================
    // Frame
    // ======================================================

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

    // ======================================================
    // RGBA変換
    // ======================================================

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

    // ======================================================
    // DEFAULTヒープTexture
    // ======================================================

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
        MessageBoxA(
            0,
            "Texture Create Failed",
            "Error",
            MB_OK
        );

        return false;
    }

    // ======================================================
    // Upload Buffer
    // ======================================================

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

    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(
            uploadSize
        );

    device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    // ======================================================
    // Subresource
    // ======================================================

    D3D12_SUBRESOURCE_DATA textureData = {};

    textureData.pData = pixels.data();

    textureData.RowPitch =
        width * 4;

    textureData.SlicePitch =
        textureData.RowPitch * height;

    // ======================================================
    // Command Reset
    // ======================================================

    commandAllocator->Reset();

    commandList->Reset(
        commandAllocator.Get(),
        nullptr
    );

    // ======================================================
    // GPU転送
    // ======================================================

    UpdateSubresources(
        commandList.Get(),
        texData.resource.Get(),
        uploadBuffer.Get(),
        0,
        0,
        1,
        &textureData
    );

    // ======================================================
    // COPY_DEST → PIXEL_SHADER_RESOURCE
    // ======================================================

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

    // ======================================================
    // Release
    // ======================================================

    converter->Release();
    frame->Release();
    decoder->Release();
    wicFactory->Release();

    return true;
}

std::vector<ObjectState> CreateVertex(
    int sides,
    float radius)
{

    for (int i = 0; i < sides; i++)
    {
        float angle1 =
            (float)i / sides *
            2.0f *
            3.141592f;

        float angle2 =
            (float)(i + 1) / sides *
            2.0f *
            3.141592f;

        ObjectState center =
        {
            0.0f,
            0.0f,
            0.0f,

            0.5f,
            0.5f
        };

        ObjectState v1 =
        {
            cosf(angle1) * radius,
            sinf(angle1) * radius,
            0.0f,

            (cosf(angle1) + 1.0f) * 0.5f,
            (-sinf(angle1) + 1.0f) * 0.5f
        };

        ObjectState v2 =
        {
            cosf(angle2) * radius,
            sinf(angle2) * radius,
            0.0f,

            (cosf(angle2) + 1.0f) * 0.5f,
            (-sinf(angle2) + 1.0f) * 0.5f
        };

        g_vertices.push_back(center);
        g_vertices.push_back(v1);
        g_vertices.push_back(v2);
    }

    return g_vertices;
}

void CreateVertexBufferOnTexture(
    const char* name
)
{
    // -----------------------------
    // 頂点生成
    // -----------------------------

    // テクスチャ読み込み
    LoadTexture(
        "test",
        L"UIDemo.png"
    );

	// 頂点生成
    // 頂点はCreateVertexで保持
    std::vector<ObjectState> vertices =
        CreateVertex(
            6,
            0.5f);

    vertexCount = (UINT)vertices.size();

    UINT size = sizeof(ObjectState) * vertexCount;

    CD3DX12_HEAP_PROPERTIES heapProp(
        D3D12_HEAP_TYPE_UPLOAD
    );

    CD3DX12_RESOURCE_DESC desc =
        CD3DX12_RESOURCE_DESC::Buffer(size);

    hr = device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );

    if (FAILED(hr))
    {
        MessageBoxA(0, "VertexBuffer Failed", "Error", MB_OK);
        return;
    }

    void* ptr = nullptr;

    vertexBuffer->Map(0, nullptr, &ptr);

    memcpy(ptr, vertices.data(), size);

    vertexBuffer->Unmap(0, nullptr);

    vbView.push_back({});
    vbView.back().BufferLocation =
        vertexBuffer->GetGPUVirtualAddress();

    vbView.back().SizeInBytes = size;

    vbView.back().StrideInBytes = sizeof(ObjectState);
}