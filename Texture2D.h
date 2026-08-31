//|| Texture2D.h ||:::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  WIC画像をDirectX12のTexture Resourceとして管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.30  GPU Textureを明示的に解放するResetを追加
//||  2026_07_13  v1.20  関数宣言コメントを規則へ統一
//||  2026_07_13  v1.10  COM初期化責務をアプリケーションへ分離
//||  2026_06_01  v1.00  新規作成
//||

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
        //空のTexture管理器を作成する
        Texture2D();

        //所有するTexture Resourceを解放する
        ~Texture2D();

        //GPU Resourceの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元Texture
        Texture2D(const Texture2D&) = delete;

        //GPU Resourceの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元Texture
        //戻り値: 代入先Textureへの参照
        Texture2D& operator=(const Texture2D&) = delete;

        //画像FileをWICで読み込みGPU Textureを作成する
        //引数: dx12 Resource作成に使用する描画基盤、filePath 読み込む画像File Path
        //戻り値: Texture作成に成功した場合はtrue
        bool LoadFromFile(
            DirectX12& dx12,
            const std::wstring& filePath
        );

        //Texture未指定時に使用する1 Pixel白Textureを作成する
        //引数: dx12 Resource作成に使用する描画基盤
        //戻り値: Texture作成に成功した場合はtrue
        bool CreateWhiteTexture(
            DirectX12& dx12
        );

        //所有するTexture ResourceとSRVを解放する
        void Reset();

        ID3D12DescriptorHeap* GetSRVHeap() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const;

        //TextureとSRVが描画可能な状態か判定する
        //戻り値: TextureとSRVが存在する場合はtrue
        bool IsValid() const;
        uint32_t GetWidth() const { return Width; }
        uint32_t GetHeight() const { return Height; }
        void Swap(Texture2D& other) noexcept;

    private:
        //RGBA8 Pixel列からUpload済みGPU Textureを作成する
        //引数: dx12 Resource作成に使用する描画基盤、pixels RGBA8 Pixel列、width 画像幅、height 画像高さ
        //戻り値: Texture作成に成功した場合はtrue
        bool CreateFromRGBA(
            DirectX12& dx12,
            const unsigned char* pixels,
            uint32_t width,
            uint32_t height
        );

        //所有TextureをShaderから参照するSRVを作成する
        //引数: dx12 Descriptor作成に使用する描画基盤
        //戻り値: SRV作成に成功した場合はtrue
        bool CreateSRV(DirectX12& dx12);

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> TextureResource; //GPU上の2D Texture
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SRVHeap; //Texture参照用Descriptor Heap
        uint32_t Width; //Texture幅
        uint32_t Height; //Texture高さ
        DXGI_FORMAT Format; //TextureのPixel形式
    };
}
