#include "Core.h"

bool ClassWindow::Initialize(
    HINSTANCE instance,
    WNDCLASS wc,
    int width,
    int height,
    const char* title
)
{
    hwnd =
        CreateWindowEx(
            0,
            wc.lpszClassName,
            title,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            width,
            height,
            nullptr,
            nullptr,
            instance,
            nullptr
        );

    ShowWindow(
        hwnd,
        SW_SHOW
    );
}

bool ClassWindow::InitD3D()
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
        hwnd,
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



    return true;
}

HWND ClassWindow::GetHWND()const
{
	return hwnd;
}