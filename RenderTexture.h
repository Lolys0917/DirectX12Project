//|| RenderTexture.h ||:::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Shader Resourceとして再利用できるColor Textureと、同寸法の専用
//||  Depth Textureを所有するRenderTextureクラスを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.10  編集: RenderTexture専用Depth及びDSVを追加
//||                         動的ResizeとBackBuffer転送機能を追加
//||                         Beginから外部DSV依存を削除
//||                         命名規則及び宣言コメントを統一
//||

#pragma once

#include <cstdint>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl.h>

namespace Engine
{
    class DirectX12;

    class RenderTexture
    {
    public:
        // RenderTextureの管理値を初期状態に設定する
        RenderTexture();

        // RenderTextureが所有するGPU Resourceを解放する
        ~RenderTexture();

        //GPU Resourceの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元RenderTexture
        RenderTexture(const RenderTexture&) = delete;

        //GPU Resourceの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元RenderTexture
        //戻り値: 代入先RenderTextureへの参照
        RenderTexture& operator=(const RenderTexture&) = delete;

        // Color Texture、専用Depth Texture及び各Descriptorを生成する
        // dx12: Resource生成に使用するDirectX 12描画基盤
        // width: Textureの横幅をPixel単位で指定する
        // height: Textureの縦幅をPixel単位で指定する
        // format: Color TextureのDXGI形式を指定する
        // 戻り値: 全Resourceの生成に成功した場合はtrue、失敗した場合はfalse
        bool Initialize(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height,
            DXGI_FORMAT format
        );

        // Color Textureと専用Depth Textureを指定寸法で安全に再生成する
        // dx12: GPU待機及びResource生成に使用するDirectX 12描画基盤
        // width: 新しいTextureの横幅をPixel単位で指定する
        // height: 新しいTextureの縦幅をPixel単位で指定する
        // 戻り値: 再生成に成功した場合はtrue、描画中又は再生成失敗時はfalse
        bool Resize(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height
        );

        // このRenderTextureを描画先に設定しColorとDepthを消去する
        // dx12: CommandListを所有するDirectX 12描画基盤
        // clearColor: Color Textureを消去するRGBA色を指定する
        void Begin(
            DirectX12& dx12,
            const float clearColor[4]
        );

        // Color TextureをShader Resourceとして読めるStateへ変更する
        // dx12: CommandListを所有するDirectX 12描画基盤
        void End(DirectX12& dx12);

        // このRenderTextureを現在のBackBufferへ転送する
        // dx12: 転送先BackBufferとCommandListを所有するDirectX 12描画基盤
        // 戻り値: 転送に成功した場合はtrue、形式不一致等で失敗した場合はfalse
        // 備考: 寸法が異なる場合は左上を基準に共通領域のみを転送する
        bool CopyToBackBuffer(DirectX12& dx12);

        // Color Texture用SRV Heapを参照する
        // 戻り値: Shader VisibleなSRV Heap、未初期化の場合はnullptr
        ID3D12DescriptorHeap* GetSRVHeap() const;

        // Color Texture用GPU SRV Handleを取得する
        // 戻り値: Color TextureのGPU Descriptor Handle
        D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const;

        // Color Texture Resourceを参照する
        // 戻り値: Color Texture、未初期化の場合はnullptr
        ID3D12Resource* GetResource() const;

        // Textureの横幅を取得する
        // 戻り値: Textureの横幅をPixel単位で返す
        uint32_t GetWidth() const;

        // Textureの縦幅を取得する
        // 戻り値: Textureの縦幅をPixel単位で返す
        uint32_t GetHeight() const;

        // Color TextureのPixel形式を取得する
        // 戻り値: Color Textureで使用しているDXGI形式
        DXGI_FORMAT GetFormat() const;

    private:
        // Color、Depth及びDescriptorを一時Resourceへ生成し成功時だけ採用する
        // dx12: Resource生成に使用するDirectX 12描画基盤
        // width: Textureの横幅をPixel単位で指定する
        // height: Textureの縦幅をPixel単位で指定する
        // format: Color TextureのDXGI形式を指定する
        // 戻り値: 全Resourceの生成と採用に成功した場合はtrue、失敗時はfalse
        bool CreateResources(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height,
            DXGI_FORMAT format
        );

        // Texture寸法に合わせてViewportとScissor Rectを更新する
        // width: 描画領域の横幅をPixel単位で指定する
        // height: 描画領域の縦幅をPixel単位で指定する
        void UpdateDrawingArea(uint32_t width, uint32_t height);

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> Texture;      // Color Texture Resource
        Microsoft::WRL::ComPtr<ID3D12Resource> DepthTexture; // 専用Depth Texture Resource

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RTVHeap; // Color Texture用RTV Heap
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SRVHeap; // Color Texture用SRV Heap
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DSVHeap; // 専用Depth Texture用DSV Heap

        D3D12_VIEWPORT Viewport;    // Texture寸法に対応するViewport
        D3D12_RECT ScissorRect;     // Texture寸法に対応するScissor Rect
        D3D12_RESOURCE_STATES CurrentState; // Color Textureの現在のResource State

        uint32_t Width;  // Textureの横幅
        uint32_t Height; // Textureの縦幅

        DXGI_FORMAT Format;      // Color TextureのPixel形式
        DXGI_FORMAT DepthFormat; // 専用Depth TextureのPixel形式
    };
}
