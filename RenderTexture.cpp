//|| RenderTexture.cpp ||:::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Color Texture、同寸法の専用Depth Texture、各Descriptor及び
//||  BackBufferへの安全な転送処理を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.20  編集: Resource生成、GPU待機及び描画先設定失敗をログへ記録
//||  2026_07_13  v1.10  編集: RenderTexture専用Depth及びDSVを追加
//||                         動的ResizeとBackBuffer転送機能を追加
//||                         Beginから外部DSV依存を削除
//||                         命名規則、宣言コメント及び安全確認を統一
//||

#include "RenderTexture.h"

#include "DirectX12.h"
#include "MessageLog.h"

#include <cstdio>

namespace
{
    /**
     * RenderTexture Resource作成失敗をHRESULT付きで記録する
     * @param operation 失敗した処理名
     * @param result 失敗を示すHRESULT
     */
    void AddRenderTextureFailureLog(const char* operation, HRESULT result)
    {
        char Message[320]{}; // 処理名とHRESULTを含む表示用メッセージ
        sprintf_s(
            Message,
            "[Error] RenderTexture | %s failed. HRESULT=0x%08lX.",
            operation,
            static_cast<unsigned long>(result)
        );
        Engine::MessageLog::GetInstance().AddLog(Message);
    }
}

namespace Engine
{
    // RenderTextureの管理値を初期状態に設定する
    RenderTexture::RenderTexture()
        : CurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) // 初期Color State
        , Width(0)                                                  // 未生成時の横幅
        , Height(0)                                                 // 未生成時の縦幅
        , Format(DXGI_FORMAT_R8G8B8A8_UNORM)                        // 初期Color形式
        , DepthFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)                // 初期Depth形式
    {
        Viewport = {};    // 未設定のViewport
        ScissorRect = {}; // 未設定のScissor Rect
    }

    // RenderTextureが所有するGPU Resourceを解放する
    RenderTexture::~RenderTexture()
    {
    }

    // Color Texture、専用Depth Texture及び各Descriptorを生成する
    // dx12: Resource生成に使用するDirectX 12描画基盤
    // width: Textureの横幅をPixel単位で指定する
    // height: Textureの縦幅をPixel単位で指定する
    // format: Color TextureのDXGI形式を指定する
    // 戻り値: 全Resourceの生成に成功した場合はtrue、失敗した場合はfalse
    bool RenderTexture::Initialize(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format
    )
    {
        if (width == 0 || height == 0 || format == DXGI_FORMAT_UNKNOWN ||
            dx12.GetDevice() == nullptr || dx12.IsFrameOpen())
        {
            MessageLog::GetInstance().AddLog(
                "[Error] RenderTexture | Initialize received invalid parameters or was called during a frame."
            );
            return false;
        }

        if (Texture && !dx12.WaitGPU())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] RenderTexture | Existing resources could not be synchronized before replacement."
            );
            return false;
        }

        return CreateResources(dx12, width, height, format);
    }

    // Color Textureと専用Depth Textureを指定寸法で安全に再生成する
    // dx12: GPU待機及びResource生成に使用するDirectX 12描画基盤
    // width: 新しいTextureの横幅をPixel単位で指定する
    // height: 新しいTextureの縦幅をPixel単位で指定する
    // 戻り値: 再生成に成功した場合はtrue、描画中又は再生成失敗時はfalse
    bool RenderTexture::Resize(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height
    )
    {
        if (!Texture || width == 0 || height == 0 || dx12.GetDevice() == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] RenderTexture | Resize was rejected because its resource, size, or device was invalid."
            );
            return false;
        }

        if (Width == width && Height == height)
        {
            return true;
        }

        if (dx12.IsFrameOpen())
        {
            MessageLog::GetInstance().AddLog(
                "[Error] RenderTexture | Resize was rejected while a frame was open."
            );
            return false;
        }

        if (!dx12.WaitGPU())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] RenderTexture | GPU synchronization failed before Resize."
            );
            return false;
        }

        return CreateResources(dx12, width, height, Format);
    }

    // このRenderTextureを描画先に設定しColorとDepthを消去する
    // dx12: CommandListを所有するDirectX 12描画基盤
    // clearColor: Color Textureを消去するRGBA色を指定する
    void RenderTexture::Begin(
        DirectX12& dx12,
        const float clearColor[4]
    )
    {
        ID3D12GraphicsCommandList* CommandList =
            dx12.GetCommandList(); // 描画命令を記録するCommandList

        if (!dx12.IsFrameOpen() || CommandList == nullptr || !Texture ||
            !DepthTexture || !RTVHeap || !DSVHeap)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Error] RenderTexture | Begin could not bind an incomplete render target."
            );
            return;
        }

        dx12.TransitionResource(
            Texture.Get(),
            CurrentState,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        CurrentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle =
            RTVHeap->GetCPUDescriptorHandleForHeapStart(); // Color Texture用RTV Handle
        D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle =
            DSVHeap->GetCPUDescriptorHandleForHeapStart(); // 専用Depth用DSV Handle

        CommandList->OMSetRenderTargets(
            1,
            &RTVHandle,
            FALSE,
            &DSVHandle
        );

        CommandList->RSSetViewports(1, &Viewport);
        CommandList->RSSetScissorRects(1, &ScissorRect);

        static constexpr float DefaultClearColor[4] =
        {
            0.0f,
            0.0f,
            0.0f,
            1.0f
        }; // nullptr指定時に使用する消去色

        const float* AppliedClearColor =
            clearColor != nullptr ? clearColor : DefaultClearColor; // 実際に使用する消去色

        CommandList->ClearRenderTargetView(
            RTVHandle,
            AppliedClearColor,
            0,
            nullptr
        );

        CommandList->ClearDepthStencilView(
            DSVHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            1.0f,
            0,
            0,
            nullptr
        );
    }

    // Color TextureをShader Resourceとして読めるStateへ変更する
    // dx12: CommandListを所有するDirectX 12描画基盤
    void RenderTexture::End(DirectX12& dx12)
    {
        if (!dx12.IsFrameOpen() || !Texture ||
            CurrentState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            return;
        }

        dx12.TransitionResource(
            Texture.Get(),
            CurrentState,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        CurrentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    // このRenderTextureを現在のBackBufferへ転送する
    // dx12: 転送先BackBufferとCommandListを所有するDirectX 12描画基盤
    // 戻り値: 転送に成功した場合はtrue、形式不一致等で失敗した場合はfalse
    bool RenderTexture::CopyToBackBuffer(DirectX12& dx12)
    {
        if (!dx12.IsFrameOpen() || !Texture)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] RenderTexture | CopyToBackBuffer was requested without an open frame or texture."
            );
            return false;
        }

        if (CurrentState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            End(dx12);
        }

        return dx12.CopyTextureToBackBuffer(
            Texture.Get(),
            CurrentState
        );
    }

    // Color、Depth及びDescriptorを一時Resourceへ生成し成功時だけ採用する
    // dx12: Resource生成に使用するDirectX 12描画基盤
    // width: Textureの横幅をPixel単位で指定する
    // height: Textureの縦幅をPixel単位で指定する
    // format: Color TextureのDXGI形式を指定する
    // 戻り値: 全Resourceの生成と採用に成功した場合はtrue、失敗時はfalse
    bool RenderTexture::CreateResources(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format
    )
    {
        ID3D12Device* Device = dx12.GetDevice(); // Resource生成に使用するDevice

        if (Device == nullptr || width == 0 || height == 0 ||
            format == DXGI_FORMAT_UNKNOWN)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] RenderTexture | Resource creation received an invalid device, size, or format."
            );
            return false;
        }

        const DXGI_FORMAT NewDepthFormat =
            dx12.GetDepthStencilFormat(); // 専用Depth Textureに使用する形式

        D3D12_HEAP_PROPERTIES HeapProperties{}; // GPU専用Default Heapの構成
        HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ColorDescription{}; // Color Texture Resourceの構成
        ColorDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        ColorDescription.Alignment = 0;
        ColorDescription.Width = width;
        ColorDescription.Height = height;
        ColorDescription.DepthOrArraySize = 1;
        ColorDescription.MipLevels = 1;
        ColorDescription.Format = format;
        ColorDescription.SampleDesc.Count = 1;
        ColorDescription.SampleDesc.Quality = 0;
        ColorDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        ColorDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE ColorClearValue{}; // Color Textureの最適化済み消去値
        ColorClearValue.Format = format;
        ColorClearValue.Color[0] = 0.0f;
        ColorClearValue.Color[1] = 0.0f;
        ColorClearValue.Color[2] = 0.0f;
        ColorClearValue.Color[3] = 1.0f;

        Microsoft::WRL::ComPtr<ID3D12Resource>
            NewTexture; // 生成完了後に採用するColor Texture

        HRESULT CreateResult = Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ColorDescription,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &ColorClearValue,
            IID_PPV_ARGS(&NewTexture)
        ); // Color Texture生成結果

        if (FAILED(CreateResult))
        {
            AddRenderTextureFailureLog("CreateCommittedResource for color texture", CreateResult);
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDescription{}; // RTV Heapの構成
        RTVHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        RTVHeapDescription.NumDescriptors = 1;
        RTVHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        RTVHeapDescription.NodeMask = 0;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
            NewRTVHeap; // 生成完了後に採用するRTV Heap

        CreateResult = Device->CreateDescriptorHeap(
            &RTVHeapDescription,
            IID_PPV_ARGS(&NewRTVHeap)
        );

        if (FAILED(CreateResult))
        {
            AddRenderTextureFailureLog("CreateDescriptorHeap for RTV", CreateResult);
            return false;
        }

        Device->CreateRenderTargetView(
            NewTexture.Get(),
            nullptr,
            NewRTVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        D3D12_DESCRIPTOR_HEAP_DESC SRVHeapDescription{}; // SRV Heapの構成
        SRVHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        SRVHeapDescription.NumDescriptors = 1;
        SRVHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        SRVHeapDescription.NodeMask = 0;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
            NewSRVHeap; // 生成完了後に採用するSRV Heap

        CreateResult = Device->CreateDescriptorHeap(
            &SRVHeapDescription,
            IID_PPV_ARGS(&NewSRVHeap)
        );

        if (FAILED(CreateResult))
        {
            AddRenderTextureFailureLog("CreateDescriptorHeap for SRV", CreateResult);
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDescription{}; // Color Texture用SRVの構成
        SRVDescription.Format = format;
        SRVDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDescription.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDescription.Texture2D.MostDetailedMip = 0;
        SRVDescription.Texture2D.MipLevels = 1;
        SRVDescription.Texture2D.PlaneSlice = 0;
        SRVDescription.Texture2D.ResourceMinLODClamp = 0.0f;

        Device->CreateShaderResourceView(
            NewTexture.Get(),
            &SRVDescription,
            NewSRVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        D3D12_RESOURCE_DESC DepthDescription{}; // 専用Depth Texture Resourceの構成
        DepthDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        DepthDescription.Alignment = 0;
        DepthDescription.Width = width;
        DepthDescription.Height = height;
        DepthDescription.DepthOrArraySize = 1;
        DepthDescription.MipLevels = 1;
        DepthDescription.Format = NewDepthFormat;
        DepthDescription.SampleDesc.Count = 1;
        DepthDescription.SampleDesc.Quality = 0;
        DepthDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        DepthDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE DepthClearValue{}; // 専用Depthの最適化済み消去値
        DepthClearValue.Format = NewDepthFormat;
        DepthClearValue.DepthStencil.Depth = 1.0f;
        DepthClearValue.DepthStencil.Stencil = 0;

        Microsoft::WRL::ComPtr<ID3D12Resource>
            NewDepthTexture; // 生成完了後に採用する専用Depth Texture

        CreateResult = Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &DepthDescription,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &DepthClearValue,
            IID_PPV_ARGS(&NewDepthTexture)
        );

        if (FAILED(CreateResult))
        {
            AddRenderTextureFailureLog("CreateCommittedResource for depth texture", CreateResult);
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDescription{}; // DSV Heapの構成
        DSVHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        DSVHeapDescription.NumDescriptors = 1;
        DSVHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        DSVHeapDescription.NodeMask = 0;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
            NewDSVHeap; // 生成完了後に採用するDSV Heap

        CreateResult = Device->CreateDescriptorHeap(
            &DSVHeapDescription,
            IID_PPV_ARGS(&NewDSVHeap)
        );

        if (FAILED(CreateResult))
        {
            AddRenderTextureFailureLog("CreateDescriptorHeap for DSV", CreateResult);
            return false;
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC DSVDescription{}; // 専用Depth用DSVの構成
        DSVDescription.Format = NewDepthFormat;
        DSVDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        DSVDescription.Flags = D3D12_DSV_FLAG_NONE;

        Device->CreateDepthStencilView(
            NewDepthTexture.Get(),
            &DSVDescription,
            NewDSVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        Texture = NewTexture;
        DepthTexture = NewDepthTexture;
        RTVHeap = NewRTVHeap;
        SRVHeap = NewSRVHeap;
        DSVHeap = NewDSVHeap;
        Width = width;
        Height = height;
        Format = format;
        DepthFormat = NewDepthFormat;
        CurrentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        UpdateDrawingArea(width, height);

        return true;
    }

    // Texture寸法に合わせてViewportとScissor Rectを更新する
    // width: 描画領域の横幅をPixel単位で指定する
    // height: 描画領域の縦幅をPixel単位で指定する
    void RenderTexture::UpdateDrawingArea(uint32_t width, uint32_t height)
    {
        Viewport.TopLeftX = 0.0f;
        Viewport.TopLeftY = 0.0f;
        Viewport.Width = static_cast<float>(width);
        Viewport.Height = static_cast<float>(height);
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;

        ScissorRect.left = 0;
        ScissorRect.top = 0;
        ScissorRect.right = static_cast<LONG>(width);
        ScissorRect.bottom = static_cast<LONG>(height);
    }

    // Color Texture用SRV Heapを参照する
    // 戻り値: Shader VisibleなSRV Heap、未初期化の場合はnullptr
    ID3D12DescriptorHeap* RenderTexture::GetSRVHeap() const
    {
        return SRVHeap.Get();
    }

    // Color Texture用GPU SRV Handleを取得する
    // 戻り値: Color TextureのGPU Descriptor Handle
    D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::GetSRVGPUHandle() const
    {
        if (!SRVHeap)
        {
            return {};
        }

        return SRVHeap->GetGPUDescriptorHandleForHeapStart();
    }

    // Color Texture Resourceを参照する
    // 戻り値: Color Texture、未初期化の場合はnullptr
    ID3D12Resource* RenderTexture::GetResource() const
    {
        return Texture.Get();
    }

    // Textureの横幅を取得する
    // 戻り値: Textureの横幅をPixel単位で返す
    uint32_t RenderTexture::GetWidth() const
    {
        return Width;
    }

    // Textureの縦幅を取得する
    // 戻り値: Textureの縦幅をPixel単位で返す
    uint32_t RenderTexture::GetHeight() const
    {
        return Height;
    }

    // Color TextureのPixel形式を取得する
    // 戻り値: Color Textureで使用しているDXGI形式
    DXGI_FORMAT RenderTexture::GetFormat() const
    {
        return Format;
    }
}
