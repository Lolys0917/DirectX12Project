#include "Core.h"

#include <cstring>
#include <climits>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "Model.h"
#include "d3dx12.h"

#include "Grid.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

Core::~Core()
{
    WaitForGPU();

    if (fenceEvent)
    {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }

    if (comInitialized)
    {
        CoUninitialize();
        comInitialized = false;
    }
}
bool Core::Initialize(HINSTANCE hInstance)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (SUCCEEDED(hr))
    {
        comInitialized = true;
    }
    else if (hr != RPC_E_CHANGED_MODE)
    {
        return false;
    }

    if (!CreateWindowApp(hInstance)) return false;
    if (!InitD3D()) return false;
    if (!camera_.Initialize(*this)) return false;
    if (!InitGraphics()) return false;

    return true;
}

void Core::Run(Model& model)
{
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
            Render(model);
        }
    }

    WaitForGPU();
}
void Core::Run(Model& model, Grid& grid)
{
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
            Render(model, grid);
        }
    }

    WaitForGPU();
}

LRESULT CALLBACK Core::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool Core::CreateWindowApp(HINSTANCE hInstance)
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "DX12WindowClass";

    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    hwnd_ = CreateWindowExA(
        0,
        wc.lpszClassName,
        "DX12 Model Sample",
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

    if (!hwnd_) return false;

    ShowWindow(hwnd_, SW_SHOW);
    return true;
}

bool Core::InitD3D()
{
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;

    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    hr = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)
    );
    if (FAILED(hr)) return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.BufferCount = FRAME_COUNT;
    scDesc.Width = WIDTH;
    scDesc.Height = HEIGHT;
    scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;

    hr = factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hwnd_,
        &scDesc,
        nullptr,
        nullptr,
        &swapChain1
    );
    if (FAILED(hr)) return false;

    hr = swapChain1.As(&swapChain);
    if (FAILED(hr)) return false;

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = FRAME_COUNT;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    hr = device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) return false;

    rtvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr)) return false;

        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator)
    );
    if (FAILED(hr)) return false;

    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList)
    );
    if (FAILED(hr)) return false;

    commandList->Close();

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = SRV_MAX;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&srvHeap));
    if (FAILED(hr)) return false;

    srvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) return false;

    fenceValue = 1;

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) return false;

    return true;
}

bool Core::InitGraphics()
{
    CD3DX12_DESCRIPTOR_RANGE range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParam[2];
    rootParam[0].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParam[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(
        _countof(rootParam),
        rootParam,
        1,
        &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rsDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &sigBlob,
        &errorBlob
    );
    if (FAILED(hr)) return false;

    hr = device->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );
    if (FAILED(hr)) return false;

    const char* vs =
        "cbuffer CameraCB:register(b0){row_major float4x4 viewProj;};"
        "struct VS_IN {float3 pos:POSITION;float2 uv:TEXCOORD;};"
        "struct PS_IN {float4 pos:SV_POSITION;float2 uv:TEXCOORD;};"
        "PS_IN main(VS_IN input) {PS_IN o;o.pos=mul(float4(input.pos,1),viewProj);o.uv=input.uv;return o;}";

    const char* ps =
        "Texture2D tex0:register(t0);"
        "SamplerState smp:register(s0);"
        "struct PS_IN{float4 pos:SV_POSITION;float2 uv:TEXCOORD;};"
        "float4 main(PS_IN input):SV_TARGET{return tex0.Sample(smp,input.uv);}";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;

    hr = D3DCompile(vs, std::strlen(vs), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, nullptr);
    if (FAILED(hr)) return false;

    hr = D3DCompile(ps, std::strlen(ps), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, nullptr);
    if (FAILED(hr)) return false;

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { layout, _countof(layout) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    return SUCCEEDED(hr);
}

D3D12_GPU_DESCRIPTOR_HANDLE Core::CreateTextureSrv(ID3D12Resource* resource)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
        srvHeap->GetCPUDescriptorHandleForHeapStart(),
        srvIndex,
        srvDescriptorSize
    );

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
        srvHeap->GetGPUDescriptorHandleForHeapStart(),
        srvIndex,
        srvDescriptorSize
    );

    device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);
    srvIndex++;

    return gpuHandle;
}

void Core::Render(Model& model)
{
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), pipelineState.Get());

    commandList->SetGraphicsRootSignature(rootSignature.Get());

    ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        frameIndex,
        rtvDescriptorSize
    );

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, WIDTH, HEIGHT };

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    camera_.UpdateCamera(
        DirectX::XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)
    );

    commandList->SetGraphicsRootConstantBufferView(
        1,
        camera_.GetGPUVirtualAddress()
    );

    model.Draw(*this);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier);

    commandList->Close();

    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, lists);

    swapChain->Present(1, 0);
    WaitForGPU();
}
void Core::Render(Model& model, Grid& grid)
{
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), pipelineState.Get());

    commandList->SetGraphicsRootSignature(rootSignature.Get());

    ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        frameIndex,
        rtvDescriptorSize
    );

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    float clearColorBuffer[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColorBuffer, 0, nullptr);

    D3D12_VIEWPORT viewport = { 0, 0, (float)WIDTH / 2, (float)HEIGHT / 2, 0, 1 };
    D3D12_RECT scissor = { 0, 0, WIDTH / 2, HEIGHT / 2 };

    float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 1, &scissor);

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    static float angleX = -3.0f;
    static float angleY = 0.0f;
    static float angleZ = 0.0f;

    angleY += 0.001f;

    camera_.UpdateCamera(
        DirectX::XMVectorSet(angleX, sinf(angleY) * 2, angleZ, 1.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)
    );

    commandList->SetGraphicsRootConstantBufferView(
        1,
        camera_.GetGPUVirtualAddress()
    );

    model.Draw(*this);
    //grid.Draw(*this);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier);

    commandList->Close();

    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, lists);

    swapChain->Present(1, 0);
    WaitForGPU();
}
void Core::ExecuteCommandListAndWait()
{
    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, lists);
    WaitForGPU();
}

void Core::WaitForGPU()
{
    if (!commandQueue || !fence || !swapChain) return;

    UINT64 currentFence = fenceValue;

    commandQueue->Signal(fence.Get(), currentFence);
    fenceValue++;

    if (fence->GetCompletedValue() < currentFence)
    {
        fence->SetEventOnCompletion(currentFence, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    frameIndex = swapChain->GetCurrentBackBufferIndex();
}