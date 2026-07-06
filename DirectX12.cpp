#include "DirectX12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace Engine
{
    DirectX12::DirectX12()
        : m_Width(0)
        , m_Height(0)
        , m_FrameIndex(0)
        , m_BackBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
        , m_DepthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)
        , m_RTVDescriptorSize(0)
        , m_FenceValue(0)
        , m_FenceEvent(nullptr)
    {
        m_Viewport = {};
        m_ScissorRect = {};
    }

    DirectX12::~DirectX12()
    {
        Finalize();
    }

    bool DirectX12::Initialize(HWND hwnd, uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        m_Viewport.TopLeftX = 0.0f;
        m_Viewport.TopLeftY = 0.0f;
        m_Viewport.Width = static_cast<float>(width);
        m_Viewport.Height = static_cast<float>(height);
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;

        m_ScissorRect.left = 0;
        m_ScissorRect.top = 0;
        m_ScissorRect.right = static_cast<LONG>(width);
        m_ScissorRect.bottom = static_cast<LONG>(height);

        if (!CreateFactory()) return false;
        if (!CreateDevice()) return false;
        if (!CreateCommandObjects()) return false;
        if (!CreateSwapChain(hwnd)) return false;
        if (!CreateRenderTargetViews()) return false;
        if (!CreateDepthStencilView()) return false;
        if (!CreateFence()) return false;

        return true;
    }

    void DirectX12::Finalize()
    {
        WaitGPU();

        if (m_FenceEvent)
        {
            CloseHandle(m_FenceEvent);
            m_FenceEvent = nullptr;
        }
    }

    bool DirectX12::CreateFactory()
    {
        UINT flags = 0;

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        HRESULT hr = CreateDXGIFactory2(
            flags,
            IID_PPV_ARGS(&m_Factory)
        );

        return SUCCEEDED(hr);
    }

    bool DirectX12::CreateDevice()
    {
        HRESULT hr = D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_Device)
        );

        return SUCCEEDED(hr);
    }

    bool DirectX12::CreateCommandObjects()
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        HRESULT hr = m_Device->CreateCommandQueue(
            &queueDesc,
            IID_PPV_ARGS(&m_CommandQueue)
        );

        if (FAILED(hr)) return false;

        hr = m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_CommandAllocator)
        );

        if (FAILED(hr)) return false;

        hr = m_Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_CommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_CommandList)
        );

        if (FAILED(hr)) return false;

        m_CommandList->Close();

        return true;
    }

    bool DirectX12::CreateSwapChain(HWND hwnd)
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.Width = m_Width;
        swapChainDesc.Height = m_Height;
        swapChainDesc.Format = m_BackBufferFormat;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = BackBufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags = 0;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;

        HRESULT hr = m_Factory->CreateSwapChainForHwnd(
            m_CommandQueue.Get(),
            hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        );

        if (FAILED(hr)) return false;

        hr = swapChain1.As(&m_SwapChain);

        if (FAILED(hr)) return false;

        m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

        return true;
    }

    bool DirectX12::CreateRenderTargetViews()
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = BackBufferCount;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;

        HRESULT hr = m_Device->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(&m_RTVHeap)
        );

        if (FAILED(hr)) return false;

        m_RTVDescriptorSize =
            m_Device->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV
            );

        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

        for (uint32_t i = 0; i < BackBufferCount; ++i)
        {
            hr = m_SwapChain->GetBuffer(
                i,
                IID_PPV_ARGS(&m_BackBuffers[i])
            );

            if (FAILED(hr)) return false;

            m_Device->CreateRenderTargetView(
                m_BackBuffers[i].Get(),
                nullptr,
                handle
            );

            handle.ptr += m_RTVDescriptorSize;
        }

        return true;
    }

    bool DirectX12::CreateDepthStencilView()
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;

        HRESULT hr = m_Device->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(&m_DSVHeap)
        );

        if (FAILED(hr)) return false;

        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = m_Width;
        resourceDesc.Height = m_Height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = m_DepthStencilFormat;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = m_DepthStencilFormat;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        hr = m_Device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_DepthStencilBuffer)
        );

        if (FAILED(hr)) return false;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = m_DepthStencilFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        m_Device->CreateDepthStencilView(
            m_DepthStencilBuffer.Get(),
            &dsvDesc,
            m_DSVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        return true;
    }

    bool DirectX12::CreateFence()
    {
        HRESULT hr = m_Device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&m_Fence)
        );

        if (FAILED(hr)) return false;

        m_FenceValue = 1;

        m_FenceEvent = CreateEvent(
            nullptr,
            FALSE,
            FALSE,
            nullptr
        );

        return m_FenceEvent != nullptr;
    }

    void DirectX12::BeginFrame(const float clearColor[4])
    {
        m_CommandAllocator->Reset();

        m_CommandList->Reset(
            m_CommandAllocator.Get(),
            nullptr
        );

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_BackBuffers[m_FrameIndex].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        m_CommandList->ResourceBarrier(
            1,
            &barrier
        );

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            GetCurrentRTVHandle();

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
            GetDSVHandle();

        m_CommandList->OMSetRenderTargets(
            1,
            &rtvHandle,
            FALSE,
            &dsvHandle
        );

        m_CommandList->RSSetViewports(
            1,
            &m_Viewport
        );

        m_CommandList->RSSetScissorRects(
            1,
            &m_ScissorRect
        );

        m_CommandList->ClearRenderTargetView(
            rtvHandle,
            clearColor,
            0,
            nullptr
        );

        m_CommandList->ClearDepthStencilView(
            dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr
        );
    }

    void DirectX12::EndFrame()
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_BackBuffers[m_FrameIndex].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        m_CommandList->ResourceBarrier(
            1,
            &barrier
        );

        m_CommandList->Close();

        ID3D12CommandList* commandLists[] =
        {
            m_CommandList.Get()
        };

        m_CommandQueue->ExecuteCommandLists(
            1,
            commandLists
        );

        m_SwapChain->Present(1, 0);

        WaitGPU();

        m_FrameIndex =
            m_SwapChain->GetCurrentBackBufferIndex();
    }

    void DirectX12::WaitGPU()
    {
        if (!m_CommandQueue || !m_Fence)
            return;

        const uint64_t fence = m_FenceValue;

        m_CommandQueue->Signal(
            m_Fence.Get(),
            fence
        );

        ++m_FenceValue;

        if (m_Fence->GetCompletedValue() < fence)
        {
            m_Fence->SetEventOnCompletion(
                fence,
                m_FenceEvent
            );

            WaitForSingleObject(
                m_FenceEvent,
                INFINITE
            );
        }
    }

    ID3D12Device* DirectX12::GetDevice() const
    {
        return m_Device.Get();
    }

    ID3D12GraphicsCommandList* DirectX12::GetCommandList() const
    {
        return m_CommandList.Get();
    }

    ID3D12CommandQueue* DirectX12::GetCommandQueue() const
    {
        return m_CommandQueue.Get();
    }

    uint32_t DirectX12::GetWidth() const
    {
        return m_Width;
    }

    uint32_t DirectX12::GetHeight() const
    {
        return m_Height;
    }

    DXGI_FORMAT DirectX12::GetBackBufferFormat() const
    {
        return m_BackBufferFormat;
    }

    DXGI_FORMAT DirectX12::GetDepthStencilFormat() const
    {
        return m_DepthStencilFormat;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DirectX12::GetCurrentRTVHandle() const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

        handle.ptr +=
            static_cast<SIZE_T>(m_FrameIndex) *
            static_cast<SIZE_T>(m_RTVDescriptorSize);

        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DirectX12::GetDSVHandle() const
    {
        return m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
    }

    const D3D12_VIEWPORT& DirectX12::GetViewport() const
    {
        return m_Viewport;
    }

    const D3D12_RECT& DirectX12::GetScissorRect() const
    {
        return m_ScissorRect;
    }

    void DirectX12::ExecuteCommandListImmediately(
        const std::function<void(ID3D12GraphicsCommandList*)>& recordFunc
    )
    {
        m_CommandAllocator->Reset();

        m_CommandList->Reset(
            m_CommandAllocator.Get(),
            nullptr
        );

        recordFunc(m_CommandList.Get());

        m_CommandList->Close();

        ID3D12CommandList* commandLists[] =
        {
            m_CommandList.Get()
        };

        m_CommandQueue->ExecuteCommandLists(
            1,
            commandLists
        );

        WaitGPU();
    }

    void DirectX12::TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES beforeState,
        D3D12_RESOURCE_STATES afterState
    )
    {
        if (!resource)
            return;

        if (beforeState == afterState)
            return;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type =
            D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags =
            D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource =
            resource;
        barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore =
            beforeState;
        barrier.Transition.StateAfter =
            afterState;

        m_CommandList->ResourceBarrier(
            1,
            &barrier
        );
    }
}