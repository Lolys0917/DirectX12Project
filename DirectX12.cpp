//|| DirectX12.cpp ||:::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DirectX 12の初期化、Frame制御、動的な描画領域変更及び
//||  RenderTextureからBackBufferへの安全な転送処理を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.20  編集: DirectX失敗、Present及びGPU同期失敗をMessageLogへ記録
//||                         即時Command実行の失敗を呼び出し元へ返すように変更
//||  2026_07_13  v1.10  編集: SwapChainの安全なResize処理を追加
//||                         BackBuffer再設定及びTexture転送処理を追加
//||                         命名規則、宣言コメント及び失敗時確認を統一
//||

#include "DirectX12.h"
#include "TextureDisplay.h"

#include <cstdio>

#include "MessageLog.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace
{
    /**
     * DirectX APIの失敗結果を共通形式でMessageLogへ追加する
     * @param operation 失敗した処理名
     * @param result 失敗を示すHRESULT
     * @param permanent 一括消去対象外の障害として残す場合はtrue
     */
    void AddDirectXFailureLog(
        const char* operation,
        HRESULT result,
        bool permanent
    )
    {
        char Message[320]{}; // 処理名とHRESULTを含む表示用メッセージ
        sprintf_s(
            Message,
            "[Error] DirectX12 | %s failed. HRESULT=0x%08lX.",
            operation,
            static_cast<unsigned long>(result)
        );

        if (permanent)
        {
            Engine::MessageLog::GetInstance().AddPermanentLog(Message);
            return;
        }

        Engine::MessageLog::GetInstance().AddLog(Message);
    }
}

namespace Engine
{
    // DirectX 12の管理値を初期状態に設定する
    DirectX12::DirectX12()
        : Width(0)                            // BackBufferの初期横幅
        , Height(0)                           // BackBufferの初期縦幅
        , FrameIndex(0)                       // 最初に参照するBackBuffer番号
        , BackBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM) // BackBuffer形式
        , DepthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT) // Depth形式
        , RTVDescriptorSize(0)                // 未生成時のDescriptor幅
        , CurrentBackBufferState(D3D12_RESOURCE_STATE_PRESENT) // 初期State
        , FenceValue(0)                       // 未通知のFence値
        , FenceEvent(nullptr)                 // 未生成のFence Event
        , Initialized(false)                  // 未初期化状態
        , FrameOpen(false)                    // CommandList未記録状態
    {
        Viewport = {};    // 未設定のViewport
        ScissorRect = {}; // 未設定のScissor Rect
    }

    // 使用中のGPU処理を完了させてDirectX 12資源を解放する
    DirectX12::~DirectX12()
    {
        Finalize();
    }

    // DirectX 12描画基盤を初期化する
    // hwnd: SwapChainの表示先となるWindow Handle
    // width: BackBufferの横幅をPixel単位で指定する
    // height: BackBufferの縦幅をPixel単位で指定する
    // 戻り値: 全ての初期化に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::Initialize(
        HWND hwnd,
        uint32_t width,
        uint32_t height
    )
    {
        if (hwnd == nullptr || width == 0 || height == 0)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | Initialize received an invalid window or back-buffer size."
            );
            return false;
        }

        Finalize();

        Width = width;
        Height = height;
        UpdateDrawingArea(width, height);

        if (!CreateFactory() ||
            !CreateDevice() ||
            !CreateCommandObjects() ||
            !CreateSwapChain(hwnd) ||
            !CreateRenderTargetViews() ||
            !CreateDepthStencilView() ||
            !CreateFence())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] DirectX12 | Graphics initialization did not complete."
            );
            Finalize();
            return false;
        }

        FrameIndex = SwapChain->GetCurrentBackBufferIndex();
        CurrentBackBufferState = D3D12_RESOURCE_STATE_PRESENT;
        Initialized = true;

        return true;
    }

    // GPU処理の完了を待ち、保持しているDirectX 12資源を解放する
    void DirectX12::Finalize()
    {
        if (FrameOpen && CommandList)
        {
            const HRESULT CloseResult =
                CommandList->Close(); // 終了時に未完了CommandListを閉じる結果

            if (FAILED(CloseResult))
            {
                AddDirectXFailureLog(
                    "ID3D12GraphicsCommandList::Close during Finalize",
                    CloseResult,
                    false
                );
            }

            FrameOpen = false;
        }

        if (CommandQueue && Fence)
        {
            if (!WaitGPU())
            {
                MessageLog::GetInstance().AddPermanentLog(
                    "[Critical] DirectX12 | Finalize released resources without confirmed GPU synchronization."
                );
            }
        }

        for (Microsoft::WRL::ComPtr<ID3D12Resource>& BackBuffer : BackBuffers) // 個別に解放するBackBuffer
        {
            BackBuffer.Reset();
        }

        SkyTexture.reset();
        DepthStencilBuffer.Reset();
        RTVHeap.Reset();
        DSVHeap.Reset();
        SwapChain.Reset();
        CommandList.Reset();
        CommandAllocator.Reset();
        CommandQueue.Reset();
        Fence.Reset();
        Device.Reset();
        Factory.Reset();

        if (FenceEvent != nullptr)
        {
            CloseHandle(FenceEvent);
            FenceEvent = nullptr;
        }

        Width = 0;
        Height = 0;
        FrameIndex = 0;
        RTVDescriptorSize = 0;
        FenceValue = 0;
        CurrentBackBufferState = D3D12_RESOURCE_STATE_PRESENT;
        Viewport = {};
        ScissorRect = {};
        Initialized = false;
        FrameOpen = false;
    }

    // BackBuffer及びDepthBufferを指定寸法で安全に再生成する
    // width: 新しいBackBufferの横幅をPixel単位で指定する
    // height: 新しいBackBufferの縦幅をPixel単位で指定する
    // 戻り値: 再生成に成功した場合はtrue、描画中又は再生成失敗時はfalse
    bool DirectX12::Resize(uint32_t width, uint32_t height)
    {
        if (!Initialized || !SwapChain || width == 0 || height == 0)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | Resize was rejected because the renderer or requested size was invalid."
            );
            return false;
        }

        if (FrameOpen)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | Resize was rejected while a frame was open."
            );
            return false;
        }

        if (Width == width && Height == height)
        {
            ID3D12Resource* CurrentBackBuffer =
                GetCurrentBackBuffer(); // 現在寸法のBackBuffer Resource
            const bool HasValidBackBuffer =
                CurrentBackBuffer != nullptr && RTVHeap != nullptr; // BackBuffer側の生成状態
            const bool HasValidDepthBuffer =
                DepthStencilBuffer != nullptr && DSVHeap != nullptr &&
                DepthStencilBuffer->GetDesc().Width == width &&
                DepthStencilBuffer->GetDesc().Height == height; // DepthBuffer側の生成状態

            if (HasValidBackBuffer && HasValidDepthBuffer)
            {
                return true;
            }

            if (!WaitGPU())
            {
                return false;
            }

            return CreateRenderTargetViews() && CreateDepthStencilView();
        }

        const uint32_t PreviousWidth = Width;   // 失敗時に維持する横幅
        const uint32_t PreviousHeight = Height; // 失敗時に維持する縦幅

        if (!WaitGPU())
        {
            return false;
        }

        for (Microsoft::WRL::ComPtr<ID3D12Resource>& BackBuffer : BackBuffers) // Resize前に参照を外すBackBuffer
        {
            BackBuffer.Reset();
        }

        HRESULT ResizeResult = SwapChain->ResizeBuffers(
            BackBufferCount,
            width,
            height,
            BackBufferFormat,
            0
        ); // SwapChainの寸法変更結果

        if (FAILED(ResizeResult))
        {
            AddDirectXFailureLog("IDXGISwapChain::ResizeBuffers", ResizeResult, true);
            Width = PreviousWidth;
            Height = PreviousHeight;
            FrameIndex = SwapChain->GetCurrentBackBufferIndex();
            CurrentBackBufferState = D3D12_RESOURCE_STATE_PRESENT;
            if (!CreateRenderTargetViews())
            {
                MessageLog::GetInstance().AddPermanentLog(
                    "[Critical] DirectX12 | Previous back-buffer views could not be restored after Resize failure."
                );
            }
            return false;
        }

        Width = width;
        Height = height;
        FrameIndex = SwapChain->GetCurrentBackBufferIndex();
        CurrentBackBufferState = D3D12_RESOURCE_STATE_PRESENT;
        UpdateDrawingArea(width, height);

        if (!CreateRenderTargetViews() || !CreateDepthStencilView())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] DirectX12 | Back-buffer views could not be rebuilt after Resize."
            );
            return false;
        }

        return true;
    }

    // 1Frame分のCommandList記録を開始しBackBufferを消去する
    // clearColor: BackBufferを消去するRGBA色を指定する
    void DirectX12::BeginFrame(const float clearColor[4])
    {
        if (!Initialized || FrameOpen || !CommandAllocator || !CommandList)
        {
            return;
        }

        HRESULT AllocatorResetResult = CommandAllocator->Reset(); // Allocator再利用結果

        if (FAILED(AllocatorResetResult))
        {
            AddDirectXFailureLog("ID3D12CommandAllocator::Reset for BeginFrame", AllocatorResetResult, false);
            return;
        }

        HRESULT CommandListResetResult = CommandList->Reset(
            CommandAllocator.Get(),
            nullptr
        ); // CommandList再利用結果

        if (FAILED(CommandListResetResult))
        {
            AddDirectXFailureLog("ID3D12GraphicsCommandList::Reset for BeginFrame", CommandListResetResult, false);
            return;
        }

        FrameOpen = true;

        TransitionResource(
            GetCurrentBackBuffer(),
            CurrentBackBufferState,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        CurrentBackBufferState = D3D12_RESOURCE_STATE_RENDER_TARGET;

        if (!BindBackBuffer())
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | BeginFrame could not bind the current back buffer."
            );
            const HRESULT CloseResult =
                CommandList->Close(); // Bind失敗後にCommandListを閉じる結果

            if (FAILED(CloseResult))
            {
                AddDirectXFailureLog(
                    "ID3D12GraphicsCommandList::Close after BindBackBuffer failure",
                    CloseResult,
                    false
                );
            }

            FrameOpen = false;
            return;
        }

        static constexpr float DefaultClearColor[4] =
        {
            0.0f,
            0.0f,
            0.0f,
            1.0f
        }; // nullptr指定時に使用する消去色

        const float* AppliedClearColor =
            clearColor != nullptr ? clearColor : DefaultClearColor; // 実際に使用する消去色

        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle =
            GetCurrentRTVHandle(); // 現在のBackBuffer用RTV Handle
        D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle =
            GetDSVHandle(); // BackBuffer用DSV Handle

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

    // CommandListを実行してBackBufferを画面へ表示する
    void DirectX12::EndFrame()
    {
        if (!Initialized || !FrameOpen || !CommandList || !CommandQueue)
        {
            return;
        }

        TransitionResource(
            GetCurrentBackBuffer(),
            CurrentBackBufferState,
            D3D12_RESOURCE_STATE_PRESENT
        );
        CurrentBackBufferState = D3D12_RESOURCE_STATE_PRESENT;

        HRESULT CloseResult = CommandList->Close(); // CommandList記録の終了結果
        FrameOpen = false;

        if (FAILED(CloseResult))
        {
            AddDirectXFailureLog("ID3D12GraphicsCommandList::Close for EndFrame", CloseResult, false);
            return;
        }

        ID3D12CommandList* CommandLists[] =
        {
            CommandList.Get()
        }; // GPUへ登録するCommandList配列

        CommandQueue->ExecuteCommandLists(1, CommandLists);

        // 更新間隔はFrameRateControllerが管理する。二つの描画先で垂直同期を重ねない。
        HRESULT PresentResult = SwapChain->Present(0, 0);

        if (FAILED(PresentResult))
        {
            AddDirectXFailureLog("IDXGISwapChain::Present", PresentResult, true);
        }

        if (!WaitGPU())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] DirectX12 | EndFrame could not confirm GPU completion."
            );
        }

        FrameIndex = SwapChain->GetCurrentBackBufferIndex();
        CurrentBackBufferState = D3D12_RESOURCE_STATE_PRESENT;
    }

    // 記録中のCommandListへBackBuffer、DepthBuffer及び描画領域を再設定する
    // 戻り値: 再設定に成功した場合はtrue、Frame記録外の場合はfalse
    bool DirectX12::BindBackBuffer()
    {
        if (!FrameOpen || !CommandList || !RTVHeap || !DSVHeap ||
            GetCurrentBackBuffer() == nullptr || !DepthStencilBuffer)
        {
            return false;
        }

        if (CurrentBackBufferState != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            TransitionResource(
                GetCurrentBackBuffer(),
                CurrentBackBufferState,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            CurrentBackBufferState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle =
            GetCurrentRTVHandle(); // 現在のBackBuffer用RTV Handle
        D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle =
            GetDSVHandle(); // BackBuffer用DSV Handle

        CommandList->OMSetRenderTargets(
            1,
            &RTVHandle,
            FALSE,
            &DSVHandle
        );

        CommandList->RSSetViewports(1, &Viewport);
        CommandList->RSSetScissorRects(1, &ScissorRect);

        return true;
    }

    // Textureを現在のBackBufferへ転送し、転送後にBackBufferを再設定する
    // sourceResource: 転送元となる単一Sampleの2D Textureを指定する
    // sourceState: 転送前及び転送後に維持する転送元TextureのResource State
    // 戻り値: 転送に成功した場合はtrue、形式不一致等で転送できない場合はfalse
    bool DirectX12::CopyTextureToBackBuffer(
        ID3D12Resource* sourceResource,
        D3D12_RESOURCE_STATES sourceState
    )
    {
        ID3D12Resource* DestinationResource =
            GetCurrentBackBuffer(); // 転送先となる現在のBackBuffer

        if (!FrameOpen || !CommandList || sourceResource == nullptr ||
            DestinationResource == nullptr || sourceResource == DestinationResource)
        {
            return false;
        }

        D3D12_RESOURCE_DESC SourceDescription =
            sourceResource->GetDesc(); // 転送元Textureの構成
        D3D12_RESOURCE_DESC DestinationDescription =
            DestinationResource->GetDesc(); // BackBufferの構成

        const bool IsSupportedTexture =
            SourceDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
            DestinationDescription.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
            SourceDescription.Format == DestinationDescription.Format &&
            SourceDescription.SampleDesc.Count == 1 &&
            DestinationDescription.SampleDesc.Count == 1 &&
            SourceDescription.DepthOrArraySize == 1 &&
            DestinationDescription.DepthOrArraySize == 1 &&
            SourceDescription.MipLevels == 1 &&
            DestinationDescription.MipLevels == 1; // 安全に直接転送できる構成か

        if (!IsSupportedTexture ||
            SourceDescription.Width == 0 || SourceDescription.Height == 0 ||
            DestinationDescription.Width == 0 || DestinationDescription.Height == 0)
        {
            return false;
        }

        TransitionResource(
            sourceResource,
            sourceState,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        );
        TransitionResource(
            DestinationResource,
            CurrentBackBufferState,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        CurrentBackBufferState = D3D12_RESOURCE_STATE_COPY_DEST;

        const bool HasSameDimensions =
            SourceDescription.Width == DestinationDescription.Width &&
            SourceDescription.Height == DestinationDescription.Height; // 寸法が完全一致するか

        if (HasSameDimensions)
        {
            CommandList->CopyResource(
                DestinationResource,
                sourceResource
            );
        }
        else
        {
            const UINT64 MinimumWidth =
                SourceDescription.Width < DestinationDescription.Width ?
                SourceDescription.Width : DestinationDescription.Width; // 共通領域の横幅
            const UINT MinimumHeight =
                SourceDescription.Height < DestinationDescription.Height ?
                SourceDescription.Height : DestinationDescription.Height; // 共通領域の縦幅
            const UINT CopyWidth =
                static_cast<UINT>(MinimumWidth); // Copy命令へ渡す共通領域の横幅
            const UINT CopyHeight = MinimumHeight; // Copy命令へ渡す共通領域の縦幅

            D3D12_TEXTURE_COPY_LOCATION SourceLocation{}; // 転送元Subresourceの指定
            SourceLocation.pResource = sourceResource;
            SourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            SourceLocation.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION DestinationLocation{}; // 転送先Subresourceの指定
            DestinationLocation.pResource = DestinationResource;
            DestinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            DestinationLocation.SubresourceIndex = 0;

            D3D12_BOX SourceBox{}; // 左上を基準とした安全な転送元領域
            SourceBox.left = 0;
            SourceBox.top = 0;
            SourceBox.front = 0;
            SourceBox.right = CopyWidth;
            SourceBox.bottom = CopyHeight;
            SourceBox.back = 1;

            CommandList->CopyTextureRegion(
                &DestinationLocation,
                0,
                0,
                0,
                &SourceLocation,
                &SourceBox
            );
        }

        TransitionResource(
            sourceResource,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            sourceState
        );
        TransitionResource(
            DestinationResource,
            CurrentBackBufferState,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        CurrentBackBufferState = D3D12_RESOURCE_STATE_RENDER_TARGET;

        return BindBackBuffer();
    }

    // CommandQueueに登録済みの全GPU処理が完了するまで待機する
    // 戻り値: GPU同期に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::WaitGPU()
    {
        if (!CommandQueue || !Fence)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] DirectX12 | GPU synchronization objects are unavailable."
            );
            return false;
        }

        const uint64_t WaitValue = FenceValue; // 今回完了を待つFence値
        HRESULT SignalResult = CommandQueue->Signal(
            Fence.Get(),
            WaitValue
        ); // Fence通知の登録結果

        if (FAILED(SignalResult))
        {
            AddDirectXFailureLog("ID3D12CommandQueue::Signal", SignalResult, true);
            return false;
        }

        ++FenceValue;

        const uint64_t CompletedValue =
            Fence->GetCompletedValue(); // Signal後にGPUが完了しているFence値

        if (CompletedValue == UINT64_MAX)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] DirectX12 | The GPU fence reported a removed device."
            );
            return false;
        }

        if (CompletedValue >= WaitValue)
        {
            return true;
        }

        if (FenceEvent == nullptr)
        {
            while (true)
            {
                const uint64_t CurrentValue =
                    Fence->GetCompletedValue(); // Eventを使わない待機中のFence値

                if (CurrentValue == UINT64_MAX)
                {
                    MessageLog::GetInstance().AddPermanentLog(
                        "[Critical] DirectX12 | The device was removed during GPU fence polling."
                    );
                    return false;
                }

                if (CurrentValue >= WaitValue)
                {
                    break;
                }

                SwitchToThread();
            }

            return true;
        }

        HRESULT EventResult = Fence->SetEventOnCompletion(
            WaitValue,
            FenceEvent
        ); // Fence完了時のEvent登録結果

        if (FAILED(EventResult))
        {
            AddDirectXFailureLog("ID3D12Fence::SetEventOnCompletion", EventResult, true);
            return false;
        }

        const DWORD WaitResult =
            WaitForSingleObject(FenceEvent, INFINITE); // Fence Event待機結果

        if (WaitResult != WAIT_OBJECT_0)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] DirectX12 | Waiting for the GPU fence event failed."
            );
            return false;
        }

        return true;
    }

    // DXGI Factoryを生成する
    // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::CreateFactory()
    {
        UINT FactoryFlags = 0; // Factory生成時に使用するDebug Flag

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug>
            DebugController; // DirectX 12 Debug Layer制御用Interface

        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController))))
        {
            static bool DebugLayerEnabled = false;
            if (!DebugLayerEnabled)
            {
                DebugController->EnableDebugLayer();
                DebugLayerEnabled = true;
            }
            FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        HRESULT CreateResult = CreateDXGIFactory2(
            FactoryFlags,
            IID_PPV_ARGS(&Factory)
        ); // Factory生成結果

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("CreateDXGIFactory2", CreateResult, false);
            return false;
        }

        return true;
    }

    // DirectX 12 Deviceを生成する
    // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::CreateDevice()
    {
        HRESULT CreateResult = D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&Device)
        ); // Device生成結果

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("D3D12CreateDevice", CreateResult, false);
            return false;
        }

        return true;
    }

    // CommandQueue、CommandAllocator及びCommandListを生成する
    // 戻り値: 全ての生成に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::CreateCommandObjects()
    {
        D3D12_COMMAND_QUEUE_DESC QueueDescription{}; // Graphics Queueの構成
        QueueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        QueueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        QueueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        QueueDescription.NodeMask = 0;

        HRESULT CreateResult = Device->CreateCommandQueue(
            &QueueDescription,
            IID_PPV_ARGS(&CommandQueue)
        ); // CommandQueue生成結果

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12Device::CreateCommandQueue", CreateResult, false);
            return false;
        }

        CreateResult = Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&CommandAllocator)
        );

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12Device::CreateCommandAllocator", CreateResult, false);
            return false;
        }

        CreateResult = Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            CommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&CommandList)
        );

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12Device::CreateCommandList", CreateResult, false);
            return false;
        }

        CreateResult = CommandList->Close();

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12GraphicsCommandList::Close during initialization", CreateResult, false);
            return false;
        }

        return true;
    }

    // 指定Windowへ表示するSwapChainを生成する
    // hwnd: SwapChainの表示先となるWindow Handle
    // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::CreateSwapChain(HWND hwnd)
    {
        DXGI_SWAP_CHAIN_DESC1 SwapChainDescription{}; // SwapChainの構成
        SwapChainDescription.Width = Width;
        SwapChainDescription.Height = Height;
        SwapChainDescription.Format = BackBufferFormat;
        SwapChainDescription.Stereo = FALSE;
        SwapChainDescription.SampleDesc.Count = 1;
        SwapChainDescription.SampleDesc.Quality = 0;
        SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDescription.BufferCount = BackBufferCount;
        SwapChainDescription.Scaling = DXGI_SCALING_STRETCH;
        SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDescription.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        SwapChainDescription.Flags = 0;

        Microsoft::WRL::ComPtr<IDXGISwapChain1>
            InitialSwapChain; // CreateSwapChainForHwndが返す基底Interface

        HRESULT CreateResult = Factory->CreateSwapChainForHwnd(
            CommandQueue.Get(),
            hwnd,
            &SwapChainDescription,
            nullptr,
            nullptr,
            &InitialSwapChain
        ); // SwapChain生成結果

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("IDXGIFactory::CreateSwapChainForHwnd", CreateResult, false);
            return false;
        }

        const HRESULT AssociationResult = Factory->MakeWindowAssociation(
            hwnd,
            DXGI_MWA_NO_ALT_ENTER
        ); // Alt+Enter抑止設定の結果

        if (FAILED(AssociationResult))
        {
            AddDirectXFailureLog("IDXGIFactory::MakeWindowAssociation", AssociationResult, false);
        }

        CreateResult = InitialSwapChain.As(&SwapChain);

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("Query IDXGISwapChain3 interface", CreateResult, false);
            return false;
        }

        FrameIndex = SwapChain->GetCurrentBackBufferIndex();

        return true;
    }

    // SwapChainの全BackBufferとRTV Heapを生成する
    // 戻り値: 全ての生成に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::CreateRenderTargetViews()
    {
        if (!Device || !SwapChain)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | RTV creation requires a device and swap chain."
            );
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC HeapDescription{}; // RTV Heapの構成
        HeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        HeapDescription.NumDescriptors = BackBufferCount;
        HeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HeapDescription.NodeMask = 0;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
            NewRTVHeap; // 生成完了後に採用するRTV Heap

        HRESULT CreateResult = Device->CreateDescriptorHeap(
            &HeapDescription,
            IID_PPV_ARGS(&NewRTVHeap)
        ); // RTV Heap生成結果

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12Device::CreateDescriptorHeap for RTV", CreateResult, false);
            return false;
        }

        const uint32_t NewRTVDescriptorSize =
            Device->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV
            ); // 新しいRTV Descriptor一個分のByte幅

        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BackBufferCount>
            NewBackBuffers{}; // 生成完了後に採用するBackBuffer群

        D3D12_CPU_DESCRIPTOR_HANDLE CurrentHandle =
            NewRTVHeap->GetCPUDescriptorHandleForHeapStart(); // 次に生成するRTV位置

        for (uint32_t Index = 0; Index < BackBufferCount; ++Index) // 生成対象のBackBuffer番号
        {
            CreateResult = SwapChain->GetBuffer(
                Index,
                IID_PPV_ARGS(&NewBackBuffers[Index])
            );

            if (FAILED(CreateResult))
            {
                AddDirectXFailureLog("IDXGISwapChain::GetBuffer", CreateResult, false);
                return false;
            }

            Device->CreateRenderTargetView(
                NewBackBuffers[Index].Get(),
                nullptr,
                CurrentHandle
            );

            CurrentHandle.ptr += NewRTVDescriptorSize;
        }

        RTVHeap = NewRTVHeap;
        BackBuffers = NewBackBuffers;
        RTVDescriptorSize = NewRTVDescriptorSize;

        return true;
    }

    // BackBufferと同寸法のDepthBuffer及びDSV Heapを生成する
    // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::CreateDepthStencilView()
    {
        if (!Device || Width == 0 || Height == 0)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | Depth-stencil creation requires a device and valid size."
            );
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC HeapDescription{}; // DSV Heapの構成
        HeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        HeapDescription.NumDescriptors = 1;
        HeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HeapDescription.NodeMask = 0;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
            NewDSVHeap; // 生成完了後に採用するDSV Heap

        HRESULT CreateResult = Device->CreateDescriptorHeap(
            &HeapDescription,
            IID_PPV_ARGS(&NewDSVHeap)
        ); // DSV Heap生成結果

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12Device::CreateDescriptorHeap for DSV", CreateResult, false);
            return false;
        }

        D3D12_HEAP_PROPERTIES HeapProperties{}; // DepthBuffer用Default Heapの構成
        HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDescription{}; // DepthBuffer Resourceの構成
        ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        ResourceDescription.Alignment = 0;
        ResourceDescription.Width = Width;
        ResourceDescription.Height = Height;
        ResourceDescription.DepthOrArraySize = 1;
        ResourceDescription.MipLevels = 1;
        ResourceDescription.Format = DepthStencilFormat;
        ResourceDescription.SampleDesc.Count = 1;
        ResourceDescription.SampleDesc.Quality = 0;
        ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        ResourceDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE ClearValue{}; // DepthBufferの最適化済み消去値
        ClearValue.Format = DepthStencilFormat;
        ClearValue.DepthStencil.Depth = 1.0f;
        ClearValue.DepthStencil.Stencil = 0;

        Microsoft::WRL::ComPtr<ID3D12Resource>
            NewDepthStencilBuffer; // 生成完了後に採用するDepthBuffer

        CreateResult = Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDescription,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &ClearValue,
            IID_PPV_ARGS(&NewDepthStencilBuffer)
        );

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12Device::CreateCommittedResource for depth buffer", CreateResult, false);
            return false;
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC DSVDescription{}; // DepthBuffer用DSVの構成
        DSVDescription.Format = DepthStencilFormat;
        DSVDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        DSVDescription.Flags = D3D12_DSV_FLAG_NONE;

        Device->CreateDepthStencilView(
            NewDepthStencilBuffer.Get(),
            &DSVDescription,
            NewDSVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        DSVHeap = NewDSVHeap;
        DepthStencilBuffer = NewDepthStencilBuffer;

        return true;
    }

    // GPU同期待機に使用するFence及びEventを生成する
    // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
    bool DirectX12::CreateFence()
    {
        HRESULT CreateResult = Device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&Fence)
        ); // Fence生成結果

        if (FAILED(CreateResult))
        {
            AddDirectXFailureLog("ID3D12Device::CreateFence", CreateResult, false);
            return false;
        }

        FenceValue = 1;
        FenceEvent = CreateEventW(
            nullptr,
            FALSE,
            FALSE,
            nullptr
        );

        if (FenceEvent == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | CreateEventW for the GPU fence failed."
            );
            return false;
        }

        return true;
    }

    // BackBuffer寸法に合わせてViewportとScissor Rectを更新する
    // width: 描画領域の横幅をPixel単位で指定する
    // height: 描画領域の縦幅をPixel単位で指定する
    void DirectX12::UpdateDrawingArea(uint32_t width, uint32_t height)
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

    // Deviceを参照する
    // 戻り値: 初期化済みDevice、未初期化の場合はnullptr
    ID3D12Device* DirectX12::GetDevice() const
    {
        return Device.Get();
    }

    // Graphics CommandListを参照する
    // 戻り値: 管理中のGraphics CommandList、未初期化の場合はnullptr
    ID3D12GraphicsCommandList* DirectX12::GetCommandList() const
    {
        return CommandList.Get();
    }

    // Graphics CommandQueueを参照する
    // 戻り値: 管理中のGraphics CommandQueue、未初期化の場合はnullptr
    ID3D12CommandQueue* DirectX12::GetCommandQueue() const
    {
        return CommandQueue.Get();
    }

    // 現在のBackBuffer Resourceを参照する
    // 戻り値: 現在表示対象のBackBuffer、未初期化の場合はnullptr
    ID3D12Resource* DirectX12::GetCurrentBackBuffer() const
    {
        if (FrameIndex >= BackBufferCount)
        {
            return nullptr;
        }

        return BackBuffers[FrameIndex].Get();
    }

    // BackBufferの横幅を取得する
    // 戻り値: BackBufferの横幅をPixel単位で返す
    uint32_t DirectX12::GetWidth() const
    {
        return Width;
    }

    // BackBufferの縦幅を取得する
    // 戻り値: BackBufferの縦幅をPixel単位で返す
    uint32_t DirectX12::GetHeight() const
    {
        return Height;
    }

    // BackBufferのPixel形式を取得する
    // 戻り値: SwapChainで使用しているDXGI形式
    DXGI_FORMAT DirectX12::GetBackBufferFormat() const
    {
        return BackBufferFormat;
    }

    // DepthBufferのPixel形式を取得する
    // 戻り値: DepthBufferで使用しているDXGI形式
    DXGI_FORMAT DirectX12::GetDepthStencilFormat() const
    {
        return DepthStencilFormat;
    }

    // 現在のBackBufferに対応するRTV Handleを取得する
    // 戻り値: 現在のBackBuffer用CPU Descriptor Handle
    D3D12_CPU_DESCRIPTOR_HANDLE DirectX12::GetCurrentRTVHandle() const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE Handle{}; // 未初期化時にも安全なRTV Handle

        if (!RTVHeap || FrameIndex >= BackBufferCount)
        {
            return Handle;
        }

        Handle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
        Handle.ptr +=
            static_cast<SIZE_T>(FrameIndex) *
            static_cast<SIZE_T>(RTVDescriptorSize);

        return Handle;
    }

    // BackBuffer用DepthBufferのDSV Handleを取得する
    // 戻り値: BackBuffer用DepthBufferのCPU Descriptor Handle
    D3D12_CPU_DESCRIPTOR_HANDLE DirectX12::GetDSVHandle() const
    {
        if (!DSVHeap)
        {
            return {};
        }

        return DSVHeap->GetCPUDescriptorHandleForHeapStart();
    }

    // BackBuffer用Viewportを取得する
    // 戻り値: BackBuffer寸法に対応するViewportへの参照
    const D3D12_VIEWPORT& DirectX12::GetViewport() const
    {
        return Viewport;
    }

    // BackBuffer用Scissor Rectを取得する
    // 戻り値: BackBuffer寸法に対応するScissor Rectへの参照
    const D3D12_RECT& DirectX12::GetScissorRect() const
    {
        return ScissorRect;
    }

    // Frame用CommandListが記録中か判定する
    // 戻り値: BeginFrame後からEndFrame前まではtrue、それ以外はfalse
    bool DirectX12::IsFrameOpen() const
    {
        return FrameOpen;
    }

    // 一時CommandListを記録、実行しGPU完了まで待機する
    // recordFunc: CommandListへ命令を記録する処理を指定する
    // 戻り値: Command実行とGPU同期に成功した場合はtrue
    bool DirectX12::ExecuteCommandListImmediately(
        const std::function<void(ID3D12GraphicsCommandList*)>& recordFunc
    )
    {
        if (FrameOpen || !CommandAllocator || !CommandList ||
            !CommandQueue || !recordFunc)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] DirectX12 | Immediate command execution was requested in an invalid state."
            );
            return false;
        }

        HRESULT ResetResult = CommandAllocator->Reset(); // Allocator再利用結果

        if (FAILED(ResetResult))
        {
            AddDirectXFailureLog("ID3D12CommandAllocator::Reset for immediate execution", ResetResult, true);
            return false;
        }

        ResetResult = CommandList->Reset(
            CommandAllocator.Get(),
            nullptr
        );

        if (FAILED(ResetResult))
        {
            AddDirectXFailureLog("ID3D12GraphicsCommandList::Reset for immediate execution", ResetResult, true);
            return false;
        }

        recordFunc(CommandList.Get());

        HRESULT CloseResult = CommandList->Close(); // CommandList記録の終了結果

        if (FAILED(CloseResult))
        {
            AddDirectXFailureLog("ID3D12GraphicsCommandList::Close for immediate execution", CloseResult, true);
            return false;
        }

        ID3D12CommandList* CommandLists[] =
        {
            CommandList.Get()
        }; // GPUへ登録するCommandList配列

        CommandQueue->ExecuteCommandLists(1, CommandLists);
        return WaitGPU();
    }

    // 指定ResourceのStateをCommandList上で変更する
    // resource: Stateを変更するResourceを指定する
    // beforeState: 変更前のResource Stateを指定する
    // afterState: 変更後のResource Stateを指定する
    void DirectX12::TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES beforeState,
        D3D12_RESOURCE_STATES afterState
    )
    {
        if (!CommandList || resource == nullptr || beforeState == afterState)
        {
            return;
        }

        D3D12_RESOURCE_BARRIER Barrier{}; // Resource State変更用Barrier
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        Barrier.Transition.pResource = resource;
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        Barrier.Transition.StateBefore = beforeState;
        Barrier.Transition.StateAfter = afterState;

        CommandList->ResourceBarrier(1, &Barrier);
    }
}
