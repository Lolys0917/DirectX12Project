#pragma once

#include <Windows.h>
#include <cstdint>
#include <array>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <functional>

namespace Engine
{
    class DirectX12 final
    {
    public:
        static constexpr uint32_t BackBufferCount = 2;

    public:
        DirectX12();
        ~DirectX12();

        DirectX12(const DirectX12&) = delete;
        DirectX12& operator=(const DirectX12&) = delete;

        bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
        void Finalize();

        void BeginFrame(const float clearColor[4]);
        void EndFrame();

        void WaitGPU();

        ID3D12Device* GetDevice() const;
        ID3D12GraphicsCommandList* GetCommandList() const;
        ID3D12CommandQueue* GetCommandQueue() const;

        uint32_t GetWidth() const;
        uint32_t GetHeight() const;

        DXGI_FORMAT GetBackBufferFormat() const;
        DXGI_FORMAT GetDepthStencilFormat() const;

        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;

        const D3D12_VIEWPORT& GetViewport() const;
        const D3D12_RECT& GetScissorRect() const;

        void ExecuteCommandListImmediately(
            const std::function<void(ID3D12GraphicsCommandList*)>& recordFunc
        );

        void TransitionResource(
            ID3D12Resource* resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState
        );

    private:
        bool CreateFactory();
        bool CreateDevice();
        bool CreateCommandObjects();
        bool CreateSwapChain(HWND hwnd);
        bool CreateRenderTargetViews();
        bool CreateDepthStencilView();
        bool CreateFence();

    private:
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_FrameIndex;

        DXGI_FORMAT m_BackBufferFormat;
        DXGI_FORMAT m_DepthStencilFormat;

        D3D12_VIEWPORT m_Viewport;
        D3D12_RECT m_ScissorRect;

        Microsoft::WRL::ComPtr<IDXGIFactory7> m_Factory;
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device;

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

        Microsoft::WRL::ComPtr<IDXGISwapChain4> m_SwapChain;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BackBufferCount> m_BackBuffers;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthStencilBuffer;

        uint32_t m_RTVDescriptorSize;

        Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
        uint64_t m_FenceValue;
        HANDLE m_FenceEvent;
    };
}