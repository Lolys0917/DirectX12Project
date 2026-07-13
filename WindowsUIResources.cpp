//|| WindowsUIResources.cpp ||:::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Windows標準UI用の画像とフォント設定を実装する
//||  UIDemo.pngをWICで読み込みYu Gothic UIから順にフォントを選ぶ
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.10  編集: COM、フォント及びUIDemo読込失敗をMessageLogへ記録
//||  2026_07_13  v1.00  新規作成: WIC画像読込とGDIフォント管理を追加
//||

#include "WindowsUIResources.h"

#include "MessageLog.h"

#include <wincodec.h>
#include <wrl.h>

#include <algorithm>
#include <iterator>
#include <limits>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace Engine
{
    namespace
    {
        /**
         * EnumFontFamiliesExWでフォント発見を記録する
         * @param logFont 発見されたフォント情報
         * @param textMetric 発見されたフォントの文字情報
         * @param fontType 発見されたフォントの種類
         * @param lparam 呼び出し元の発見フラグ
         * @return 最初の一致で列挙を止めるため常に0
         */
        int CALLBACK RecordInstalledFont(
            const LOGFONTW* logFont,
            const TEXTMETRICW* textMetric,
            DWORD fontType,
            LPARAM lparam
        )
        {
            (void)logFont;
            (void)textMetric;
            (void)fontType;

            bool* Found = reinterpret_cast<bool*>(lparam); // フォントが見つかったことを返すフラグ

            if (Found != nullptr)
            {
                *Found = true;
            }

            return 0;
        }

        /**
         * ファイルが通常ファイルとして存在するか調べる
         * @param filePath 調べるファイルパス
         * @return 読み込み可能な通常ファイルならtrue
         */
        bool IsExistingFile(const std::wstring& filePath)
        {
            const DWORD Attributes = GetFileAttributesW(filePath.c_str()); // ファイル属性の取得結果

            return
                Attributes != INVALID_FILE_ATTRIBUTES &&
                (Attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }
    }

    // Windows標準UIで使用する既定値を設定する
    WindowsUISettings::WindowsUISettings()
        : TexturePath(L"UIDemo.png")
        , PrimaryFontFace(L"Yu Gothic UI")
        , SecondaryFontFace(L"Meiryo")
        , FontPointSize(10)
        , PreviewWidth(180)
        , PreviewHeight(180)
    {
    }

    // Windowsリソースを保持しない初期状態を作成する
    WindowsUIResources::WindowsUIResources()
        : InterfaceFont(nullptr)
        , DemoBitmap(nullptr)
        , CurrentDpi(USER_DEFAULT_SCREEN_DPI)
        , OwnsInterfaceFont(false)
        , OwnsComInitialization(false)
    {
    }

    // 保持中のGDIリソースとCOM初期化を解放する
    WindowsUIResources::~WindowsUIResources()
    {
        Finalize();
    }

    /**
     * UI設定からフォントと画像を初期化する
     * @param settings 使用する画像、フォント、表示サイズの設定
     * @param dpi リソースを作成するウィンドウのDPI
     * @return UIフォントを使用可能にできた場合はtrue
     */
    bool WindowsUIResources::Initialize(
        const WindowsUISettings& settings,
        uint32_t dpi
    )
    {
        Finalize();

        Settings = settings;
        CurrentDpi = std::max<uint32_t>(dpi, USER_DEFAULT_SCREEN_DPI);

        if (!InitializeCom())
        {
            MessageLog::GetInstance().AddLog(
                "[Error] WindowsUIResources | COM initialization for WIC failed."
            );
            return false;
        }

        const bool FontCreated = CreateInterfaceFont(CurrentDpi); // UIフォントを作成できたか
        const bool BitmapCreated = CreateDemoBitmap(CurrentDpi); // UIDemo画像を作成できたか

        if (!FontCreated)
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] WindowsUIResources | Interface font creation failed."
            );
        }

        if (!BitmapCreated)
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] WindowsUIResources | UIDemo.png could not be loaded; the editor continues without it."
            );
        }

        return FontCreated;
    }

    /**
     * DPI変更後のフォントと画像を作り直す
     * @param dpi 新しく適用するウィンドウのDPI
     * @return UIフォントを使用可能にできた場合はtrue
     */
    bool WindowsUIResources::UpdateDpi(uint32_t dpi)
    {
        CurrentDpi = std::max<uint32_t>(dpi, USER_DEFAULT_SCREEN_DPI);

        if (InterfaceFont != nullptr)
        {
            if (OwnsInterfaceFont)
            {
                DeleteObject(InterfaceFont);
            }

            InterfaceFont = nullptr;
            OwnsInterfaceFont = false;
        }

        if (DemoBitmap != nullptr)
        {
            DeleteObject(DemoBitmap);
            DemoBitmap = nullptr;
        }

        const bool FontCreated = CreateInterfaceFont(CurrentDpi); // DPI変更後にフォントを作成できたか
        const bool BitmapCreated = CreateDemoBitmap(CurrentDpi); // DPI変更後に画像を作成できたか

        if (!FontCreated)
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] WindowsUIResources | Interface font recreation failed after a DPI change."
            );
        }

        if (!BitmapCreated)
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] WindowsUIResources | UIDemo.png recreation failed after a DPI change."
            );
        }

        return FontCreated;
    }

    // 保持中のWindowsリソースを解放する
    void WindowsUIResources::Finalize()
    {
        if (InterfaceFont != nullptr)
        {
            if (OwnsInterfaceFont)
            {
                DeleteObject(InterfaceFont);
            }

            InterfaceFont = nullptr;
            OwnsInterfaceFont = false;
        }

        if (DemoBitmap != nullptr)
        {
            DeleteObject(DemoBitmap);
            DemoBitmap = nullptr;
        }

        ActiveFontFace.clear();

        if (OwnsComInitialization)
        {
            CoUninitialize();
            OwnsComInitialization = false;
        }
    }

    HFONT WindowsUIResources::GetInterfaceFont() const
    {
        return InterfaceFont;
    }

    HBITMAP WindowsUIResources::GetDemoBitmap() const
    {
        return DemoBitmap;
    }

    const std::wstring& WindowsUIResources::GetActiveFontFace() const
    {
        return ActiveFontFace;
    }

    //UI Demo画像をGDI Bitmapとして使用できるか確認する
    //戻り値: Bitmapの読み込みに成功している場合はtrue
    bool WindowsUIResources::IsDemoBitmapLoaded() const
    {
        return DemoBitmap != nullptr;
    }

    /**
     * WICを利用するためのCOM初期化状態を用意する
     * @return 現在のスレッドでWICを利用できる場合はtrue
     */
    bool WindowsUIResources::InitializeCom()
    {
        const HRESULT Result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // COM初期化結果

        if (Result == RPC_E_CHANGED_MODE)
        {
            return true;
        }

        if (FAILED(Result))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] WindowsUIResources | CoInitializeEx failed."
            );
            return false;
        }

        OwnsComInitialization = true;
        return true;
    }

    /**
     * 設定とDPIからUIフォントを作成する
     * @param dpi フォントを作成するウィンドウのDPI
     * @return フォントを作成できた場合はtrue
     */
    bool WindowsUIResources::CreateInterfaceFont(uint32_t dpi)
    {
        NONCLIENTMETRICSW Metrics{}; // Windowsの既定UIフォント情報
        Metrics.cbSize = sizeof(NONCLIENTMETRICSW);

        SystemParametersInfoW(
            SPI_GETNONCLIENTMETRICS,
            sizeof(NONCLIENTMETRICSW),
            &Metrics,
            0
        );

        std::wstring FontFace = Metrics.lfMessageFont.lfFaceName; // 最終フォールバックのシステムフォント名

        if (IsFontInstalled(Settings.PrimaryFontFace))
        {
            FontFace = Settings.PrimaryFontFace;
        }
        else if (IsFontInstalled(Settings.SecondaryFontFace))
        {
            FontFace = Settings.SecondaryFontFace;
        }

        LOGFONTW FontDescription = Metrics.lfMessageFont; // GDIフォント作成に使用する設定
        FontDescription.lfHeight = -MulDiv(
            static_cast<int>(Settings.FontPointSize),
            static_cast<int>(dpi),
            72
        );
        FontDescription.lfWeight = FW_NORMAL;
        FontDescription.lfQuality = CLEARTYPE_QUALITY;

        wcsncpy_s(
            FontDescription.lfFaceName,
            LF_FACESIZE,
            FontFace.c_str(),
            _TRUNCATE
        );

        InterfaceFont = CreateFontIndirectW(&FontDescription);

        if (InterfaceFont == nullptr)
        {
            InterfaceFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            OwnsInterfaceFont = false;
            ActiveFontFace = L"System";
            return InterfaceFont != nullptr;
        }

        OwnsInterfaceFont = true;
        ActiveFontFace = FontFace;
        return true;
    }

    /**
     * 設定されたPNGをWICで読み込みGDIビットマップへ変換する
     * @param dpi プレビュー画像へ適用するウィンドウのDPI
     * @return ビットマップを作成できた場合はtrue
     */
    bool WindowsUIResources::CreateDemoBitmap(uint32_t dpi)
    {
        using Microsoft::WRL::ComPtr;

        ComPtr<IWICImagingFactory> Factory; // WICリソースを作成するファクトリー
        HRESULT Result = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&Factory)
        ); // WICファクトリーの生成結果

        if (FAILED(Result))
        {
            return false;
        }

        const std::wstring TexturePath = ResolveResourcePath(Settings.TexturePath); // 実際に読み込むPNGのパス
        ComPtr<IWICBitmapDecoder> Decoder; // PNGを復号するWICデコーダー
        Result = Factory->CreateDecoderFromFilename(
            TexturePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &Decoder
        );

        if (FAILED(Result))
        {
            return false;
        }

        ComPtr<IWICBitmapFrameDecode> Frame; // PNGの先頭画像フレーム
        Result = Decoder->GetFrame(0, &Frame);

        if (FAILED(Result))
        {
            return false;
        }

        const UINT TargetWidth = std::max<UINT>(
            1,
            static_cast<UINT>(MulDiv(
                static_cast<int>(Settings.PreviewWidth),
                static_cast<int>(dpi),
                USER_DEFAULT_SCREEN_DPI
            ))
        ); // DPI適用後のプレビュー幅
        const UINT TargetHeight = std::max<UINT>(
            1,
            static_cast<UINT>(MulDiv(
                static_cast<int>(Settings.PreviewHeight),
                static_cast<int>(dpi),
                USER_DEFAULT_SCREEN_DPI
            ))
        ); // DPI適用後のプレビュー高さ

        if (
            TargetWidth > std::numeric_limits<UINT>::max() / 4 ||
            TargetHeight > std::numeric_limits<UINT>::max() / (TargetWidth * 4)
        )
        {
            return false;
        }

        ComPtr<IWICBitmapScaler> Scaler; // PNGを表示サイズへ変換するスケーラー
        Result = Factory->CreateBitmapScaler(&Scaler);

        if (FAILED(Result))
        {
            return false;
        }

        Result = Scaler->Initialize(
            Frame.Get(),
            TargetWidth,
            TargetHeight,
            WICBitmapInterpolationModeFant
        );

        if (FAILED(Result))
        {
            return false;
        }

        ComPtr<IWICFormatConverter> Converter; // GDI互換BGRAへ変換するコンバーター
        Result = Factory->CreateFormatConverter(&Converter);

        if (FAILED(Result))
        {
            return false;
        }

        Result = Converter->Initialize(
            Scaler.Get(),
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom
        );

        if (FAILED(Result))
        {
            return false;
        }

        BITMAPINFO BitmapInformation{}; // トップダウンDIBの作成情報
        BitmapInformation.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        BitmapInformation.bmiHeader.biWidth = static_cast<LONG>(TargetWidth);
        BitmapInformation.bmiHeader.biHeight = -static_cast<LONG>(TargetHeight);
        BitmapInformation.bmiHeader.biPlanes = 1;
        BitmapInformation.bmiHeader.biBitCount = 32;
        BitmapInformation.bmiHeader.biCompression = BI_RGB;

        void* PixelData = nullptr; // WICが画像を書き込むDIBピクセル領域
        HDC ScreenDc = GetDC(nullptr); // DIB作成に使用する画面デバイスコンテキスト
        HBITMAP Bitmap = CreateDIBSection(
            ScreenDc,
            &BitmapInformation,
            DIB_RGB_COLORS,
            &PixelData,
            nullptr,
            0
        ); // 作成したGDIビットマップ

        if (ScreenDc != nullptr)
        {
            ReleaseDC(nullptr, ScreenDc);
        }

        if (Bitmap == nullptr || PixelData == nullptr)
        {
            if (Bitmap != nullptr)
            {
                DeleteObject(Bitmap);
            }

            return false;
        }

        const UINT Stride = TargetWidth * 4; // DIBの1行分のバイト数
        const UINT BufferSize = Stride * TargetHeight; // DIB全体のバイト数
        Result = Converter->CopyPixels(
            nullptr,
            Stride,
            BufferSize,
            static_cast<BYTE*>(PixelData)
        );

        if (FAILED(Result))
        {
            DeleteObject(Bitmap);
            return false;
        }

        DemoBitmap = Bitmap;
        return true;
    }

    /**
     * 指定フォントが現在のPCへ登録されているか調べる
     * @param fontFace 調べるフォント名
     * @return 登録済みの場合はtrue
     */
    bool WindowsUIResources::IsFontInstalled(const std::wstring& fontFace) const
    {
        if (fontFace.empty())
        {
            return false;
        }

        LOGFONTW FontQuery{}; // EnumFontFamiliesExWへ渡す検索条件
        FontQuery.lfCharSet = DEFAULT_CHARSET;
        wcsncpy_s(
            FontQuery.lfFaceName,
            LF_FACESIZE,
            fontFace.c_str(),
            _TRUNCATE
        );

        bool Found = false; // 指定フォントが列挙されたか
        HDC ScreenDc = GetDC(nullptr); // システムフォント列挙に使用する画面DC

        if (ScreenDc == nullptr)
        {
            return false;
        }

        EnumFontFamiliesExW(
            ScreenDc,
            &FontQuery,
            RecordInstalledFont,
            reinterpret_cast<LPARAM>(&Found),
            0
        );

        ReleaseDC(nullptr, ScreenDc);
        return Found;
    }

    /**
     * 設定された相対パスを実行ファイル位置も含めて解決する
     * @param filePath 解決するファイルパス
     * @return 読み込みに使用するファイルパス
     */
    std::wstring WindowsUIResources::ResolveResourcePath(
        const std::wstring& filePath
    ) const
    {
        if (filePath.empty() || IsExistingFile(filePath))
        {
            return filePath;
        }

        wchar_t ModulePath[MAX_PATH]{}; // 実行ファイルの絶対パス格納先
        const DWORD ModulePathLength = GetModuleFileNameW(
            nullptr,
            ModulePath,
            static_cast<DWORD>(std::size(ModulePath))
        ); // 実行ファイルパスの文字数

        if (ModulePathLength == 0 || ModulePathLength >= std::size(ModulePath))
        {
            return filePath;
        }

        std::wstring ExecutableDirectory(ModulePath, ModulePathLength); // 実行ファイルを含むディレクトリ
        const std::wstring::size_type SeparatorPosition =
            ExecutableDirectory.find_last_of(L"\\/"); // 最後のディレクトリ区切り位置

        if (SeparatorPosition == std::wstring::npos)
        {
            return filePath;
        }

        ExecutableDirectory.resize(SeparatorPosition + 1);

        const std::wstring CandidatePath = ExecutableDirectory + filePath; // 実行ファイル位置を基準にした候補

        if (IsExistingFile(CandidatePath))
        {
            return CandidatePath;
        }

        return filePath;
    }

    bool WindowsUIResources::SetTexturePath(const std::wstring& texturePath)
    {
        if (texturePath.empty())
        {
            return false;
        }
        const std::wstring ResolvedPath = ResolveResourcePath(texturePath);
        if (!IsExistingFile(ResolvedPath))
        {
            return false;
        }
        Settings.TexturePath = texturePath;
        return true;
	}
}