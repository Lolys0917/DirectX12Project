//|| DirectX12.h ||:::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DirectX 12のデバイス、SwapChain、コマンド及びBackBufferを管理する
//||  描画基盤クラスを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.20  編集: GPU同期と即時Command実行の成否を返すように変更
//||  2026_07_13  v1.10  編集: 描画領域の動的変更機能を追加
//||                         BackBufferの再設定、参照及びTexture転送機能を追加
//||                         命名規則、宣言コメント及び安全確認を統一
//||

#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <functional>
#include <wrl.h>

namespace Engine
{
    class DirectX12 final
    {
    public:
        static constexpr uint32_t BackBufferCount = 2; // SwapChainが保持するBackBufferの数

    public:
        // DirectX 12の管理値を初期状態に設定する
        DirectX12();

        // 使用中のGPU処理を完了させてDirectX 12資源を解放する
        ~DirectX12();

        //GPU Resourceの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元DirectX 12描画基盤
        DirectX12(const DirectX12&) = delete;

        //GPU Resourceの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元DirectX 12描画基盤
        //戻り値: 代入先DirectX 12描画基盤への参照
        DirectX12& operator=(const DirectX12&) = delete;

        // DirectX 12描画基盤を初期化する
        // hwnd: SwapChainの表示先となるWindow Handle
        // width: BackBufferの横幅をPixel単位で指定する
        // height: BackBufferの縦幅をPixel単位で指定する
        // 戻り値: 全ての初期化に成功した場合はtrue、失敗した場合はfalse
        bool Initialize(HWND hwnd, uint32_t width, uint32_t height);

        // GPU処理の完了を待ち、保持しているDirectX 12資源を解放する
        void Finalize();

        // BackBuffer及びDepthBufferを指定寸法で安全に再生成する
        // width: 新しいBackBufferの横幅をPixel単位で指定する
        // height: 新しいBackBufferの縦幅をPixel単位で指定する
        // 戻り値: 再生成に成功した場合はtrue、描画中又は再生成失敗時はfalse
        bool Resize(uint32_t width, uint32_t height);

        // 1Frame分のCommandList記録を開始しBackBufferを消去する
        // clearColor: BackBufferを消去するRGBA色を指定する
        void BeginFrame(const float clearColor[4]);

        // CommandListを実行してBackBufferを画面へ表示する
        void EndFrame();

        // 記録中のCommandListへBackBuffer、DepthBuffer及び描画領域を再設定する
        // 戻り値: 再設定に成功した場合はtrue、Frame記録外の場合はfalse
        bool BindBackBuffer();

        // Textureを現在のBackBufferへ転送し、転送後にBackBufferを再設定する
        // sourceResource: 転送元となる単一Sampleの2D Textureを指定する
        // sourceState: 転送前及び転送後に維持する転送元TextureのResource State
        // 戻り値: 転送に成功した場合はtrue、形式不一致等で転送できない場合はfalse
        // 備考: 寸法が異なる場合は左上を基準に共通領域のみを転送し拡大縮小は行わない
        bool CopyTextureToBackBuffer(
            ID3D12Resource* sourceResource,
            D3D12_RESOURCE_STATES sourceState
        );

        // CommandQueueに登録済みの全GPU処理が完了するまで待機する
        // 戻り値: GPU同期に成功した場合はtrue、失敗した場合はfalse
        bool WaitGPU();

        // Deviceを参照する
        // 戻り値: 初期化済みDevice、未初期化の場合はnullptr
        ID3D12Device* GetDevice() const;

        // Graphics CommandListを参照する
        // 戻り値: 管理中のGraphics CommandList、未初期化の場合はnullptr
        ID3D12GraphicsCommandList* GetCommandList() const;

        // Graphics CommandQueueを参照する
        // 戻り値: 管理中のGraphics CommandQueue、未初期化の場合はnullptr
        ID3D12CommandQueue* GetCommandQueue() const;

        // 現在のBackBuffer Resourceを参照する
        // 戻り値: 現在表示対象のBackBuffer、未初期化の場合はnullptr
        ID3D12Resource* GetCurrentBackBuffer() const;

        // BackBufferの横幅を取得する
        // 戻り値: BackBufferの横幅をPixel単位で返す
        uint32_t GetWidth() const;

        // BackBufferの縦幅を取得する
        // 戻り値: BackBufferの縦幅をPixel単位で返す
        uint32_t GetHeight() const;

        // BackBufferのPixel形式を取得する
        // 戻り値: SwapChainで使用しているDXGI形式
        DXGI_FORMAT GetBackBufferFormat() const;

        // DepthBufferのPixel形式を取得する
        // 戻り値: DepthBufferで使用しているDXGI形式
        DXGI_FORMAT GetDepthStencilFormat() const;

        // 現在のBackBufferに対応するRTV Handleを取得する
        // 戻り値: 現在のBackBuffer用CPU Descriptor Handle
        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const;

        // BackBuffer用DepthBufferのDSV Handleを取得する
        // 戻り値: BackBuffer用DepthBufferのCPU Descriptor Handle
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;

        // BackBuffer用Viewportを取得する
        // 戻り値: BackBuffer寸法に対応するViewportへの参照
        const D3D12_VIEWPORT& GetViewport() const;

        // BackBuffer用Scissor Rectを取得する
        // 戻り値: BackBuffer寸法に対応するScissor Rectへの参照
        const D3D12_RECT& GetScissorRect() const;

        // Frame用CommandListが記録中か判定する
        // 戻り値: BeginFrame後からEndFrame前まではtrue、それ以外はfalse
        bool IsFrameOpen() const;

        // 一時CommandListを記録、実行しGPU完了まで待機する
        // recordFunc: CommandListへ命令を記録する処理を指定する
        // 戻り値: Command実行とGPU同期に成功した場合はtrue
        bool ExecuteCommandListImmediately(
            const std::function<void(ID3D12GraphicsCommandList*)>& recordFunc
        );

        // 指定ResourceのStateをCommandList上で変更する
        // resource: Stateを変更するResourceを指定する
        // beforeState: 変更前のResource Stateを指定する
        // afterState: 変更後のResource Stateを指定する
        void TransitionResource(
            ID3D12Resource* resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState
        );

    private:
        // DXGI Factoryを生成する
        // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
        bool CreateFactory();

        // DirectX 12 Deviceを生成する
        // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
        bool CreateDevice();

        // CommandQueue、CommandAllocator及びCommandListを生成する
        // 戻り値: 全ての生成に成功した場合はtrue、失敗した場合はfalse
        bool CreateCommandObjects();

        // 指定Windowへ表示するSwapChainを生成する
        // hwnd: SwapChainの表示先となるWindow Handle
        // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
        bool CreateSwapChain(HWND hwnd);

        // SwapChainの全BackBufferとRTV Heapを生成する
        // 戻り値: 全ての生成に成功した場合はtrue、失敗した場合はfalse
        bool CreateRenderTargetViews();

        // BackBufferと同寸法のDepthBuffer及びDSV Heapを生成する
        // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
        bool CreateDepthStencilView();

        // GPU同期待機に使用するFence及びEventを生成する
        // 戻り値: 生成に成功した場合はtrue、失敗した場合はfalse
        bool CreateFence();

        // BackBuffer寸法に合わせてViewportとScissor Rectを更新する
        // width: 描画領域の横幅をPixel単位で指定する
        // height: 描画領域の縦幅をPixel単位で指定する
        void UpdateDrawingArea(uint32_t width, uint32_t height);

    private:
        uint32_t Width;                 // BackBufferの横幅
        uint32_t Height;                // BackBufferの縦幅
        uint32_t FrameIndex;            // 現在表示対象のBackBuffer番号

        DXGI_FORMAT BackBufferFormat;   // BackBufferのPixel形式
        DXGI_FORMAT DepthStencilFormat; // DepthBufferのPixel形式

        D3D12_VIEWPORT Viewport;        // BackBuffer用Viewport
        D3D12_RECT ScissorRect;         // BackBuffer用Scissor Rect

        Microsoft::WRL::ComPtr<IDXGIFactory7> Factory;                    // DXGI Factory
        Microsoft::WRL::ComPtr<ID3D12Device> Device;                      // DirectX 12 Device
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue;          // Graphics CommandQueue
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;  // Graphics CommandAllocator
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList;    // Graphics CommandList
        Microsoft::WRL::ComPtr<IDXGISwapChain4> SwapChain;                // Window表示用SwapChain
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RTVHeap;             // BackBuffer用RTV Heap
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DSVHeap;             // BackBuffer用DSV Heap

        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BackBufferCount>
            BackBuffers;                                                  // SwapChainのBackBuffer群
        Microsoft::WRL::ComPtr<ID3D12Resource> DepthStencilBuffer;         // BackBuffer用DepthBuffer

        uint32_t RTVDescriptorSize;                    // RTV Descriptor一個分のByte幅
        D3D12_RESOURCE_STATES CurrentBackBufferState;  // 現在のBackBuffer Resource State

        Microsoft::WRL::ComPtr<ID3D12Fence> Fence; // GPU同期待機用Fence
        uint64_t FenceValue;                       // 次に通知するFence値
        HANDLE FenceEvent;                         // Fence完了通知用Event

        bool Initialized; // DirectX 12描画基盤の初期化完了状態
        bool FrameOpen;   // Frame用CommandListの記録状態
    };
}
