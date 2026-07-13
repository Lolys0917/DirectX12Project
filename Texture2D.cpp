//|| Texture2D.cpp ||:::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  WIC画像をDirectX12のTexture Resourceへ変換する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.30  編集: WIC、GPU転送及びSRV作成失敗をMessageLogへ記録
//||                         WIC寸法取得と即時Command実行結果を検証
//||  2026_07_13  v1.20  C++変数命名と宣言コメントを規則へ統一
//||  2026_07_13  v1.10  COM初期化責務をWinMainへ集約
//||  2026_06_01  v1.00  新規作成
//||

#include "Texture2D.h"

#include "DirectX12.h"
#include "MessageLog.h"

#include <cstdio>
#include <limits>
#include <vector>
#include <wincodec.h>
#include <cstring>

#pragma comment(lib, "windowscodecs.lib")

namespace
{
    /**
     * Texture処理のHRESULT失敗をMessageLogへ追加する
     * @param operation 失敗したWICまたはDirectX処理名
     * @param result 失敗を示すHRESULT
     */
    void AddTextureFailureLog(const char* operation, HRESULT result)
    {
        char Message[320]{}; // 処理名とHRESULTを含む表示用メッセージ
        sprintf_s(
            Message,
            "[Error] Texture2D | %s failed. HRESULT=0x%08lX.",
            operation,
            static_cast<unsigned long>(result)
        );
        Engine::MessageLog::GetInstance().AddLog(Message);
    }
}

namespace Engine
{
    //空のTexture管理Objectを作成する
    Texture2D::Texture2D()
        : Width(0)
        , Height(0)
        , Format(DXGI_FORMAT_R8G8B8A8_UNORM)
    {
    }

    //Texture Resourceを破棄する
    Texture2D::~Texture2D()
    {
    }

    //WICで画像FileをRGBA8へ変換してTextureを作成する
    //引数: dx12 描画基盤、filePath 読み込む画像File
    //戻り値: 読み込みとTexture作成に成功した場合はtrue
    bool Texture2D::LoadFromFile(
        DirectX12& dx12,
        const std::wstring& filePath
    )
    {
        Microsoft::WRL::ComPtr<IWICImagingFactory> Factory; //WIC画像Factory

        HRESULT Result = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&Factory)
        ); //WIC Factory作成結果

        if (FAILED(Result))
        {
            AddTextureFailureLog("CoCreateInstance for WIC factory", Result);
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> Decoder; //画像File Decoder

        Result = Factory->CreateDecoderFromFilename(
            filePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &Decoder
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("IWICImagingFactory::CreateDecoderFromFilename", Result);
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> Frame; //先頭画像Frame

        Result = Decoder->GetFrame(
            0,
            &Frame
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("IWICBitmapDecoder::GetFrame", Result);
            return false;
        }

        UINT ImageWidth = 0; //読み込んだ画像幅
        UINT ImageHeight = 0; //読み込んだ画像高さ

        Result = Frame->GetSize(
            &ImageWidth,
            &ImageHeight
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("IWICBitmapFrameDecode::GetSize", Result);
            return false;
        }

        if (ImageWidth == 0 || ImageHeight == 0 ||
            ImageWidth > (std::numeric_limits<UINT>::max)() / 4 ||
            ImageHeight > (std::numeric_limits<UINT>::max)() / (ImageWidth * 4))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Texture2D | The decoded image dimensions are invalid or too large."
            );
            return false;
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> Converter; //RGBA8変換器

        Result = Factory->CreateFormatConverter(
            &Converter
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("IWICImagingFactory::CreateFormatConverter", Result);
            return false;
        }

        Result = Converter->Initialize(
            Frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("IWICFormatConverter::Initialize", Result);
            return false;
        }

        const UINT Stride =
            ImageWidth * 4; //RGBA8一行のByte数

        const UINT ImageSize =
            Stride * ImageHeight; //画像全体のByte数

        std::vector<unsigned char> Pixels; //RGBA8画像Data
        Pixels.resize(ImageSize);

        Result = Converter->CopyPixels(
            nullptr,
            Stride,
            ImageSize,
            Pixels.data()
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("IWICFormatConverter::CopyPixels", Result);
            return false;
        }

        return CreateFromRGBA(
            dx12,
            Pixels.data(),
            ImageWidth,
            ImageHeight
        );
    }

    //1Pixelの白Textureを作成する
    //引数: dx12 描画基盤
    //戻り値: Texture作成に成功した場合はtrue
    bool Texture2D::CreateWhiteTexture(
        DirectX12& dx12
    )
    {
        const unsigned char Pixel[4] =
        {
            255, 255, 255, 255
        }; //白色RGBA8 Pixel

        return CreateFromRGBA(
            dx12,
            Pixel,
            1,
            1
        );
    }

    //RGBA8 Pixel列からGPU Textureを作成する
    //引数: dx12 描画基盤、pixels RGBA8 Data、width 幅、height 高さ
    //戻り値: TextureとSRVの作成に成功した場合はtrue
    bool Texture2D::CreateFromRGBA(
        DirectX12& dx12,
        const unsigned char* pixels,
        uint32_t width,
        uint32_t height
    )
    {
        if (pixels == nullptr || width == 0 || height == 0 ||
            dx12.GetDevice() == nullptr ||
            width > (std::numeric_limits<UINT>::max)() / 4)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Texture2D | RGBA texture creation received invalid data, size, or device."
            );
            return false;
        }

        Width = width;
        Height = height;
        Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        D3D12_RESOURCE_DESC TextureDescription{}; //GPU Texture Resource設定
        TextureDescription.Dimension =
            D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        TextureDescription.Alignment = 0;
        TextureDescription.Width = width;
        TextureDescription.Height = height;
        TextureDescription.DepthOrArraySize = 1;
        TextureDescription.MipLevels = 1;
        TextureDescription.Format = Format;
        TextureDescription.SampleDesc.Count = 1;
        TextureDescription.SampleDesc.Quality = 0;
        TextureDescription.Layout =
            D3D12_TEXTURE_LAYOUT_UNKNOWN;
        TextureDescription.Flags =
            D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES DefaultHeapProperties{}; //Default Heap設定
        DefaultHeapProperties.Type =
            D3D12_HEAP_TYPE_DEFAULT;
        DefaultHeapProperties.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        DefaultHeapProperties.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        DefaultHeapProperties.CreationNodeMask = 1;
        DefaultHeapProperties.VisibleNodeMask = 1;

        HRESULT Result = dx12.GetDevice()->CreateCommittedResource(
            &DefaultHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &TextureDescription,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&TextureResource)
        ); //GPU Texture作成結果

        if (FAILED(Result))
        {
            AddTextureFailureLog("CreateCommittedResource for texture", Result);
            return false;
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint{}; //Upload用Subresource配置
        UINT RowCount = 0; //Texture行数
        UINT64 RowSizeInBytes = 0; //非Alignment時の一行Byte数
        UINT64 TotalBytes = 0; //Upload Buffer全体のByte数

        dx12.GetDevice()->GetCopyableFootprints(
            &TextureDescription,
            0,
            1,
            0,
            &Footprint,
            &RowCount,
            &RowSizeInBytes,
            &TotalBytes
        );

        D3D12_HEAP_PROPERTIES UploadHeapProperties{}; //Upload Heap設定
        UploadHeapProperties.Type =
            D3D12_HEAP_TYPE_UPLOAD;
        UploadHeapProperties.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        UploadHeapProperties.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        UploadHeapProperties.CreationNodeMask = 1;
        UploadHeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC UploadDescription{}; //Upload Buffer Resource設定
        UploadDescription.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        UploadDescription.Alignment = 0;
        UploadDescription.Width = TotalBytes;
        UploadDescription.Height = 1;
        UploadDescription.DepthOrArraySize = 1;
        UploadDescription.MipLevels = 1;
        UploadDescription.Format = DXGI_FORMAT_UNKNOWN;
        UploadDescription.SampleDesc.Count = 1;
        UploadDescription.SampleDesc.Quality = 0;
        UploadDescription.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        UploadDescription.Flags =
            D3D12_RESOURCE_FLAG_NONE;

        Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer; //Texture転送用Upload Buffer

        Result = dx12.GetDevice()->CreateCommittedResource(
            &UploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &UploadDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&UploadBuffer)
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("CreateCommittedResource for texture upload", Result);
            return false;
        }

        unsigned char* MappedData = nullptr; //Upload BufferのCPU書込先

        Result = UploadBuffer->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&MappedData)
        );

        if (FAILED(Result))
        {
            AddTextureFailureLog("ID3D12Resource::Map for texture upload", Result);
            return false;
        }

        const UINT SourceRowPitch =
            width * 4; //入力RGBA8一行のByte数

        unsigned char* DestinationData =
            MappedData + Footprint.Offset; //配置Offset反映済み転送先

        for (UINT RowIndex = 0; RowIndex < height; ++RowIndex) //各行をAlignment済み領域へコピーする
        {
            std::memcpy(
                DestinationData + RowIndex * Footprint.Footprint.RowPitch,
                pixels + RowIndex * SourceRowPitch,
                SourceRowPitch
            );
        }

        UploadBuffer->Unmap(
            0,
            nullptr
        );

        const bool UploadSucceeded = dx12.ExecuteCommandListImmediately(
            [&](ID3D12GraphicsCommandList* commandList)
            {
                D3D12_TEXTURE_COPY_LOCATION SourceLocation{}; //Upload Buffer側のCopy元
                SourceLocation.pResource =
                    UploadBuffer.Get();
                SourceLocation.Type =
                    D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                SourceLocation.PlacedFootprint =
                    Footprint;

                D3D12_TEXTURE_COPY_LOCATION DestinationLocation{}; //Texture側のCopy先
                DestinationLocation.pResource =
                    TextureResource.Get();
                DestinationLocation.Type =
                    D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                DestinationLocation.SubresourceIndex = 0;

                commandList->CopyTextureRegion(
                    &DestinationLocation,
                    0,
                    0,
                    0,
                    &SourceLocation,
                    nullptr
                );

                D3D12_RESOURCE_BARRIER Barrier{}; //Copy後のTexture状態遷移
                Barrier.Type =
                    D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                Barrier.Transition.pResource =
                    TextureResource.Get();
                Barrier.Transition.Subresource =
                    D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                Barrier.Transition.StateBefore =
                    D3D12_RESOURCE_STATE_COPY_DEST;
                Barrier.Transition.StateAfter =
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

                commandList->ResourceBarrier(
                    1,
                    &Barrier
                );
            }
        );

        if (!UploadSucceeded)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] Texture2D | Texture upload commands were not executed successfully."
            );
            TextureResource.Reset();
            return false;
        }

        return CreateSRV(dx12);
    }

    //Texture参照用Shader Resource Viewを作成する
    //引数: dx12 描画基盤
    //戻り値: Descriptor Heap作成に成功した場合はtrue
    bool Texture2D::CreateSRV(
        DirectX12& dx12
    )
    {
        D3D12_DESCRIPTOR_HEAP_DESC HeapDescription{}; //SRV Descriptor Heap設定
        HeapDescription.Type =
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        HeapDescription.NumDescriptors = 1;
        HeapDescription.Flags =
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HeapDescription.NodeMask = 0;

        HRESULT Result = dx12.GetDevice()->CreateDescriptorHeap(
            &HeapDescription,
            IID_PPV_ARGS(&SRVHeap)
        ); //SRV Descriptor Heap作成結果

        if (FAILED(Result))
        {
            AddTextureFailureLog("CreateDescriptorHeap for texture SRV", Result);
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDescription{}; //Texture2D用SRV設定
        SRVDescription.Format = Format;
        SRVDescription.ViewDimension =
            D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDescription.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDescription.Texture2D.MipLevels = 1;
        SRVDescription.Texture2D.MostDetailedMip = 0;
        SRVDescription.Texture2D.PlaneSlice = 0;
        SRVDescription.Texture2D.ResourceMinLODClamp = 0.0f;

        dx12.GetDevice()->CreateShaderResourceView(
            TextureResource.Get(),
            &SRVDescription,
            SRVHeap->GetCPUDescriptorHandleForHeapStart()
        );

        return true;
    }

    ID3D12DescriptorHeap* Texture2D::GetSRVHeap() const
    {
        return SRVHeap.Get();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE Texture2D::GetSRVGPUHandle() const
    {
        return SRVHeap->GetGPUDescriptorHandleForHeapStart();
    }

    //TextureとSRVが描画可能な状態か判定する
    //戻り値: TextureとSRVが存在する場合はtrue
    bool Texture2D::IsValid() const
    {
        return TextureResource != nullptr && SRVHeap != nullptr;
    }
}
