//|| WindowsUIResources.h ||:::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Windows標準UIで使用するテクスチャとフォントを管理する
//||  外部UIライブラリへ依存せずWICとGDIのリソースを提供する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成: UIテクスチャとフォント設定を追加
//||

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <cstdint>
#include <string>

namespace Engine
{
    struct WindowsUISettings final
    {
        // Windows標準UI用の既定設定を作成する
        WindowsUISettings();

        std::wstring TexturePath;       // UIに表示する画像ファイルのパス
        std::wstring PrimaryFontFace;   // 最初に使用を試みるフォント名
        std::wstring SecondaryFontFace; // 代替として使用を試みるフォント名
        uint32_t FontPointSize;          // UIフォントのポイントサイズ
        uint32_t PreviewWidth;           // UI画像プレビューの論理幅
        uint32_t PreviewHeight;          // UI画像プレビューの論理高さ
    };

    class WindowsUIResources final
    {
    public:
        // Windows標準UI用リソース管理を作成する
        WindowsUIResources();

        // 保持中のGDIリソースとCOM初期化を解放する
        ~WindowsUIResources();

        //GDI Resourceの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元UI Resource管理器
        WindowsUIResources(const WindowsUIResources&) = delete;

        //GDI Resourceの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元UI Resource管理器
        //戻り値: 代入先UI Resource管理器への参照
        WindowsUIResources& operator=(const WindowsUIResources&) = delete;

        /**
         * UI設定からフォントと画像を初期化する
         * @param settings 使用する画像、フォント、表示サイズの設定
         * @param dpi リソースを作成するウィンドウのDPI
         * @return UIフォントを使用可能にできた場合はtrue
         */
        bool Initialize(
            const WindowsUISettings& settings,
            uint32_t dpi
        );

        /**
         * DPI変更後のフォントと画像を作り直す
         * @param dpi 新しく適用するウィンドウのDPI
         * @return UIフォントを使用可能にできた場合はtrue
         */
        bool UpdateDpi(uint32_t dpi);

        // 保持中のWindowsリソースを解放する
        void Finalize();

        bool SetTexturePath(const std::wstring& texturePath);

        HFONT GetInterfaceFont() const;
        HBITMAP GetDemoBitmap() const;
        const std::wstring& GetActiveFontFace() const;

        //UI Demo画像をGDI Bitmapとして使用できるか確認する
        //戻り値: Bitmapの読み込みに成功している場合はtrue
        bool IsDemoBitmapLoaded() const;

    private:
        /**
         * WICを利用するためのCOM初期化状態を用意する
         * @return 現在のスレッドでWICを利用できる場合はtrue
         */
        bool InitializeCom();

        /**
         * 設定とDPIからUIフォントを作成する
         * @param dpi フォントを作成するウィンドウのDPI
         * @return フォントを作成できた場合はtrue
         */
        bool CreateInterfaceFont(uint32_t dpi);

        /**
         * 設定されたPNGをWICで読み込みGDIビットマップへ変換する
         * @param dpi プレビュー画像へ適用するウィンドウのDPI
         * @return ビットマップを作成できた場合はtrue
         */
        bool CreateDemoBitmap(uint32_t dpi);

        /**
         * 指定フォントが現在のPCへ登録されているか調べる
         * @param fontFace 調べるフォント名
         * @return 登録済みの場合はtrue
         */
        bool IsFontInstalled(const std::wstring& fontFace) const;

        /**
         * 設定された相対パスを実行ファイル位置も含めて解決する
         * @param filePath 解決するファイルパス
         * @return 読み込みに使用するファイルパス
         */
        std::wstring ResolveResourcePath(const std::wstring& filePath) const;

    private:
        WindowsUISettings Settings; // 現在適用しているUIリソース設定
        HFONT InterfaceFont;         // 標準コントロールへ設定するGDIフォント
        HBITMAP DemoBitmap;          // UIDemo.pngから作成したGDIビットマップ
        std::wstring ActiveFontFace; // 実際に選択されたフォント名
        uint32_t CurrentDpi;         // リソース生成時に適用したDPI
        bool OwnsInterfaceFont;      // InterfaceFontの削除をこのクラスが担当するか
        bool OwnsComInitialization;  // COMの終了処理をこのクラスが担当するか
    };
}
