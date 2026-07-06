#include "RenderTexture.h"

#include "DirectX12.h"

namespace Engine
{
    RenderTexture::RenderTexture()
        : m_CurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        , m_Width(0)
        , m_Height(0)
        , m_Format(DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        m_Viewport = {};
        m_ScissorRect = {};
    }

    RenderTexture::~RenderTexture()
    {
    }

    bool RenderTexture::Initialize(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format
    )
    {
        m_Width = width;
        m_Height = height;
        m_Format = format;

        m_Viewport.TopLeftX = 0.0f;
        m_Viewport.TopLeftY = 0.0f;
        m_Viewport.Width =
            static_cast<float>(width);
        m_Viewport.Height =
            static_cast<float>(height);
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;

        m_ScissorRect.left = 0;
        m_ScissorRect.top = 0;
        m_ScissorRect.right =
            static_cast<LONG>(width);
        m_ScissorRect.bottom =
            static_cast<LONG>(height);

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type =
            D3D12_HEAP_TYPE_DEFAULT;
        heap.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension =
            D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout =
            D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags =
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = format;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 1.0f;

        HRESULT hr = dx12.GetDevice()->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&m_Texture)
        );

        if (FAILED(hr))
        {
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type =
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Flags =
            D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = dx12.GetDevice()->CreateDescriptorHeap(
            &rtvHeapDesc,
            IID_PPV_ARGS(&m_RTVHeap)
        );

        if (FAILED(hr))
        {
            return false;
        }

        dx12.GetDevice()->CreateRenderTargetView(
            m_Texture.Get(),
            nullptr,
            m_RTVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
        srvHeapDesc.Type =
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = 1;
        srvHeapDesc.Flags =
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        hr = dx12.GetDevice()->CreateDescriptorHeap(
            &srvHeapDesc,
            IID_PPV_ARGS(&m_SRVHeap)
        );

        if (FAILED(hr))
        {
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = format;
        srvDesc.ViewDimension =
            D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        dx12.GetDevice()->CreateShaderResourceView(
            m_Texture.Get(),
            &srvDesc,
            m_SRVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        return true;
    }

    void RenderTexture::Begin(
        DirectX12& dx12,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
        const float clearColor[4]
    )
    {
        ID3D12GraphicsCommandList* commandList =
            dx12.GetCommandList();

        dx12.TransitionResource(
            m_Texture.Get(),
            m_CurrentState,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

        m_CurrentState =
            D3D12_RESOURCE_STATE_RENDER_TARGET;

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

        commandList->OMSetRenderTargets(
            1,
            &rtvHandle,
            FALSE,
            &dsvHandle
        );

        commandList->RSSetViewports(
            1,
            &m_Viewport
        );

        commandList->RSSetScissorRects(
            1,
            &m_ScissorRect
        );

        commandList->ClearRenderTargetView(
            rtvHandle,
            clearColor,
            0,
            nullptr
        );

        commandList->ClearDepthStencilView(
            dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr
        );
    }

    void RenderTexture::End(
        DirectX12& dx12
    )
    {
        dx12.TransitionResource(
            m_Texture.Get(),
            m_CurrentState,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

        m_CurrentState =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    ID3D12DescriptorHeap* RenderTexture::GetSRVHeap() const
    {
        return m_SRVHeap.Get();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::GetSRVGPUHandle() const
    {
        return m_SRVHeap->GetGPUDescriptorHandleForHeapStart();
    }

    ID3D12Resource* RenderTexture::GetResource() const
    {
        return m_Texture.Get();
    }
}