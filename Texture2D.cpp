#include "Texture2D.h"

#include "DirectX12.h"

#include <vector>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

namespace Engine
{
    Texture2D::Texture2D()
        : m_Width(0)
        , m_Height(0)
        , m_Format(DXGI_FORMAT_R8G8B8A8_UNORM)
    {
    }

    Texture2D::~Texture2D()
    {
    }

    bool Texture2D::LoadFromFile(
        DirectX12& dx12,
        const std::wstring& filePath
    )
    {
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED
        );

        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;

        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)
        );

        if (FAILED(hr))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;

        hr = factory->CreateDecoderFromFilename(
            filePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &decoder
        );

        if (FAILED(hr))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;

        hr = decoder->GetFrame(
            0,
            &frame
        );

        if (FAILED(hr))
        {
            return false;
        }

        UINT width = 0;
        UINT height = 0;

        frame->GetSize(
            &width,
            &height
        );

        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;

        hr = factory->CreateFormatConverter(
            &converter
        );

        if (FAILED(hr))
        {
            return false;
        }

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom
        );

        if (FAILED(hr))
        {
            return false;
        }

        const UINT stride =
            width * 4;

        const UINT imageSize =
            stride * height;

        std::vector<unsigned char> pixels;
        pixels.resize(imageSize);

        hr = converter->CopyPixels(
            nullptr,
            stride,
            imageSize,
            pixels.data()
        );

        if (FAILED(hr))
        {
            return false;
        }

        return CreateFromRGBA(
            dx12,
            pixels.data(),
            width,
            height
        );
    }

    bool Texture2D::CreateWhiteTexture(
        DirectX12& dx12
    )
    {
        const unsigned char pixel[4] =
        {
            255, 255, 255, 255
        };

        return CreateFromRGBA(
            dx12,
            pixel,
            1,
            1
        );
    }

    bool Texture2D::CreateFromRGBA(
        DirectX12& dx12,
        const unsigned char* pixels,
        uint32_t width,
        uint32_t height
    )
    {
        m_Width = width;
        m_Height = height;
        m_Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        D3D12_RESOURCE_DESC textureDesc{};
        textureDesc.Dimension =
            D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Alignment = 0;
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = m_Format;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Layout =
            D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDesc.Flags =
            D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type =
            D3D12_HEAP_TYPE_DEFAULT;
        defaultHeap.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeap.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeap.CreationNodeMask = 1;
        defaultHeap.VisibleNodeMask = 1;

        HRESULT hr = dx12.GetDevice()->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_Texture)
        );

        if (FAILED(hr))
        {
            return false;
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 totalBytes = 0;

        dx12.GetDevice()->GetCopyableFootprints(
            &textureDesc,
            0,
            1,
            0,
            &footprint,
            &numRows,
            &rowSizeInBytes,
            &totalBytes
        );

        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type =
            D3D12_HEAP_TYPE_UPLOAD;
        uploadHeap.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeap.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeap.CreationNodeMask = 1;
        uploadHeap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Alignment = 0;
        uploadDesc.Width = totalBytes;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.SampleDesc.Quality = 0;
        uploadDesc.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploadDesc.Flags =
            D3D12_RESOURCE_FLAG_NONE;

        Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;

        hr = dx12.GetDevice()->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer)
        );

        if (FAILED(hr))
        {
            return false;
        }

        unsigned char* mapped = nullptr;

        hr = uploadBuffer->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mapped)
        );

        if (FAILED(hr))
        {
            return false;
        }

        const UINT srcRowPitch =
            width * 4;

        unsigned char* dst =
            mapped + footprint.Offset;

        for (UINT y = 0; y < height; ++y)
        {
            std::memcpy(
                dst + y * footprint.Footprint.RowPitch,
                pixels + y * srcRowPitch,
                srcRowPitch
            );
        }

        uploadBuffer->Unmap(
            0,
            nullptr
        );

        dx12.ExecuteCommandListImmediately(
            [&](ID3D12GraphicsCommandList* commandList)
            {
                D3D12_TEXTURE_COPY_LOCATION src{};
                src.pResource =
                    uploadBuffer.Get();
                src.Type =
                    D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint =
                    footprint;

                D3D12_TEXTURE_COPY_LOCATION dst{};
                dst.pResource =
                    m_Texture.Get();
                dst.Type =
                    D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = 0;

                commandList->CopyTextureRegion(
                    &dst,
                    0,
                    0,
                    0,
                    &src,
                    nullptr
                );

                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type =
                    D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource =
                    m_Texture.Get();
                barrier.Transition.Subresource =
                    D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore =
                    D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter =
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

                commandList->ResourceBarrier(
                    1,
                    &barrier
                );
            }
        );

        return CreateSRV(dx12);
    }

    bool Texture2D::CreateSRV(
        DirectX12& dx12
    )
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type =
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags =
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask = 0;

        HRESULT hr = dx12.GetDevice()->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(&m_SRVHeap)
        );

        if (FAILED(hr))
        {
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = m_Format;
        srvDesc.ViewDimension =
            D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        dx12.GetDevice()->CreateShaderResourceView(
            m_Texture.Get(),
            &srvDesc,
            m_SRVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        return true;
    }

    ID3D12DescriptorHeap* Texture2D::GetSRVHeap() const
    {
        return m_SRVHeap.Get();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE Texture2D::GetSRVGPUHandle() const
    {
        return m_SRVHeap->GetGPUDescriptorHandleForHeapStart();
    }

    bool Texture2D::IsValid() const
    {
        return m_Texture != nullptr && m_SRVHeap != nullptr;
    }
}