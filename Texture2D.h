#pragma once

#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi.h>

namespace Engine
{
    class DirectX12;

    class Texture2D
    {
    public:
        Texture2D();
        ~Texture2D();

        Texture2D(const Texture2D&) = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        bool LoadFromFile(
            DirectX12& dx12,
            const std::wstring& filePath
        );

        bool CreateWhiteTexture(
            DirectX12& dx12
        );

        ID3D12DescriptorHeap* GetSRVHeap() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const;

        bool IsValid() const;

    private:
        bool CreateFromRGBA(
            DirectX12& dx12,
            const unsigned char* pixels,
            uint32_t width,
            uint32_t height
        );

        bool CreateSRV(DirectX12& dx12);

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_Texture;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SRVHeap;

        uint32_t m_Width;
        uint32_t m_Height;

        DXGI_FORMAT m_Format;
    };
}