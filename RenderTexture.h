#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi.h>
#include <cstdint>

namespace Engine
{
    class DirectX12;

    class RenderTexture
    {
    public:
        RenderTexture();
        ~RenderTexture();

        bool Initialize(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height,
            DXGI_FORMAT format
        );

        void Begin(
            DirectX12& dx12,
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
            const float clearColor[4]
        );

        void End(
            DirectX12& dx12
        );

        ID3D12DescriptorHeap* GetSRVHeap() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const;

        ID3D12Resource* GetResource() const;

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_Texture;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SRVHeap;

        D3D12_VIEWPORT m_Viewport;
        D3D12_RECT m_ScissorRect;

        D3D12_RESOURCE_STATES m_CurrentState;

        uint32_t m_Width;
        uint32_t m_Height;

        DXGI_FORMAT m_Format;
    };
}