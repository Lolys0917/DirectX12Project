//|| WinApp.h ||:::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DX12エンジン用のWindowsエディターウィンドウを管理する
//||  描画領域、操作パネル、可変スプリッターをWindows標準機能で提供する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.30  修正: 右パネル表示階層と三段レイアウトを明確化
//||  2026_07_13  v2.10  変更: DPI対応レイアウトとログ表示領域を追加
//||  2026_07_13  v2.00  追加: Windows標準エディターUIと操作イベント
//||

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "WindowsUIResources.h"

namespace Engine
{
    struct RenderWindowSize final
    {
        uint32_t Width;  // DX12描画用子ウィンドウの幅
        uint32_t Height; // DX12描画用子ウィンドウの高さ
    };

    class WinApp final
    {
    public:
        // Windowsエディターウィンドウを未生成の状態で作成する
        WinApp();

        // 作成済みのWindowsウィンドウとUIリソースを解放する
        ~WinApp();

        //Window HandleとGDI Resourceの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元WinApp
        WinApp(const WinApp&) = delete;

        //Window HandleとGDI Resourceの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元WinApp
        //戻り値: 代入先WinAppへの参照
        WinApp& operator=(const WinApp&) = delete;

        /**
         * 親エディターウィンドウとWindows標準コントロールを作成する
         * @param title 親ウィンドウのタイトル
         * @param width 親クライアント領域の論理幅
         * @param height 親クライアント領域の論理高さ
         * @return 全ての必須ウィンドウを作成できた場合はtrue
         */
        bool Create(
            const wchar_t* title,
            uint32_t width,
            uint32_t height
        );

        // 作成済みの親ウィンドウと子コントロールを破棄する
        void Destroy();

        /**
         * Windowsメッセージを処理する
         * @return アプリケーションを継続する場合はtrue
         */
        bool ProcessMessage();

        /**
         * UI画像とフォントの設定を適用する
         * @param settings 新しく適用するWindows標準UI設定
         */
        void SetUISettings(const WindowsUISettings& settings);

        HWND GetHWND() const;
        HWND GetRenderHwnd() const;
        HINSTANCE GetInstance() const;
        uint32_t GetWidth() const;
        uint32_t GetHeight() const;
        uint32_t GetRenderWidth() const;
        uint32_t GetRenderHeight() const;
        RenderWindowSize GetRenderSize() const;
        uint32_t GetTargetFrameRate() const;
        const WindowsUISettings& GetUISettings() const;

        /**
         * メッセージログ一覧のWindows Handleを取得する
         * @return ログ一覧が未作成の場合はnullptr
         */
        HWND GetLogListHwnd() const;

        /**
         * ログ一括消去ボタンの未処理イベントを1件取得する
         * @return 一括消去要求が存在した場合はtrue
         */
        bool ConsumeClearLogs();

        /**
         * 渡された文字列一覧でWindows標準ログ一覧を更新する
         * @param messages 表示順に並んだメッセージ文字列
         */
        void UpdateLogMessages(const std::vector<std::wstring>& messages);

        /**
         * Startボタンの未処理イベントを1件取得する
         * @return Start要求が存在した場合はtrue
         */
        bool ConsumeStart();

        /**
         * Stopボタンの未処理イベントを1件取得する
         * @return Stop要求が存在した場合はtrue
         */
        bool ConsumeStop();

        /**
         * Tickボタンの未処理イベントを1件取得する
         * @return Tick要求が存在した場合はtrue
         */
        bool ConsumeTick();

        /**
         * 描画用子ウィンドウのサイズ変更を取得する
         * @param size 変更後の描画領域サイズを受け取る変数
         * @return 未処理のサイズ変更が存在した場合はtrue
         */
        bool ConsumeResize(RenderWindowSize& size);

    private:
        /**
         * HWNDからWinAppインスタンスへWindowsメッセージを転送する
         * @param hwnd メッセージを受信した親ウィンドウ
         * @param message Windowsメッセージ番号
         * @param wparam メッセージ固有の追加情報
         * @param lparam メッセージ固有の追加情報
         * @return メッセージ処理結果
         */
        static LRESULT CALLBACK WindowProc(
            HWND hwnd,
            UINT message,
            WPARAM wparam,
            LPARAM lparam
        );

        /**
         * WinAppインスタンスに紐付いたWindowsメッセージを処理する
         * @param message Windowsメッセージ番号
         * @param wparam メッセージ固有の追加情報
         * @param lparam メッセージ固有の追加情報
         * @return メッセージ処理結果
         */
        LRESULT HandleMessage(
            UINT message,
            WPARAM wparam,
            LPARAM lparam
        );

        /**
         * 描画領域と右操作パネルのWindows標準コントロールを作成する
         * @return 必須コントロールを全て作成できた場合はtrue
         */
        bool CreateControls();

        // 現在のクライアントサイズと分割位置から全コントロールを配置する
        void LayoutControls();

        // 現在のUIフォントを全ての標準コントロールへ適用する
        void ApplyInterfaceFont();

        // 現在のUIDemoビットマップを画像コントロールへ適用する
        void ApplyDemoBitmap();

        // 再生状態に合わせてボタンと状態テキストを更新する
        void UpdatePlaybackControls();

        // 目標FPSに合わせてEDITとTRACKBARを同期する
        void UpdateFrameRateControls();

        // FPS EDITの有効な数値を目標FPSへ反映する
        void UpdateFrameRateFromEdit();

        /**
         * 目標FPSを有効範囲へ収めて設定する
         * @param frameRate 新しく設定する目標FPS
         */
        void SetTargetFrameRate(uint32_t frameRate);

        // 親ウィンドウの現在クライアントサイズを保存する
        void UpdateClientSize();

        // 描画用子ウィンドウの現在サイズを変更イベントとして記録する
        void UpdateRenderSize();

        /**
         * 現在DPIへ論理ピクセル値を変換する
         * @param value 変換前の96DPI基準値
         * @return 現在DPIへ変換した物理ピクセル値
         */
        int ScaleByDpi(int value) const;

        /**
         * 現在の分割位置からスプリッター矩形を取得する
         * @return 親クライアント座標のスプリッター矩形
         */
        RECT GetSplitterRect() const;

        /**
         * 指定座標がスプリッター内にあるか調べる
         * @param x 親クライアント座標のX位置
         * @param y 親クライアント座標のY位置
         * @return スプリッター内の場合はtrue
         */
        bool IsPointInSplitter(int x, int y) const;

        // 現在のクライアント幅に対して分割位置を安全な範囲へ収める
        void ClampSplitPosition();

        // 親ウィンドウの未使用領域へスプリッターを描画する
        void PaintSplitter();

    private:
        HWND Hwnd;                  // 親エディターウィンドウ
        HWND RenderHwnd;            // DX12 SwapChainを接続する左描画ウィンドウ
        HWND PanelHwnd;             // 右操作パネルの範囲管理に使う非表示コントロール
        HWND TitleLabelHwnd;        // 操作パネル見出しテキスト
        HWND StartButtonHwnd;       // ゲーム開始ボタン
        HWND StopButtonHwnd;        // ゲーム停止ボタン
        HWND TickButtonHwnd;        // ゲーム1フレーム更新ボタン
        HWND StatusLabelHwnd;       // 現在の再生状態テキスト
        HWND FrameRateLabelHwnd;    // FPS設定見出しテキスト
        HWND FrameRateEditHwnd;     // FPS数値入力コントロール
        HWND FrameRateSliderHwnd;   // FPSスライダーコントロール
        HWND PreviewLabelHwnd;      // UI画像の説明テキスト
        HWND PreviewImageHwnd;      // UIDemo.png表示コントロール
        HWND LogLabelHwnd;          // メッセージログの見出しテキスト
        HWND LogListHwnd;           // メッセージログを表示する一覧コントロール
        HWND ClearLogsButtonHwnd;   // 通常ログの一括消去ボタン
        HINSTANCE Instance;         // このプロセスのWindowsインスタンス
        HCURSOR SplitCursor;        // スプリッター上で表示する左右カーソル
        uint32_t Width;             // 親ウィンドウの現在クライアント幅
        uint32_t Height;            // 親ウィンドウの現在クライアント高さ
        uint32_t RenderWidth;       // 左描画ウィンドウの現在幅
        uint32_t RenderHeight;      // 左描画ウィンドウの現在高さ
        uint32_t CurrentDpi;        // 親ウィンドウで現在使用しているDPI
        uint32_t TargetFrameRate;   // EDITとTRACKBARが示す目標FPS
        uint32_t PendingTickCount;  // 未処理のTick要求数
        int SplitPosition;          // スプリッター左端のX位置
        int SplitDragOffset;        // ドラッグ開始位置と分割位置の差
        float SplitRatio;           // ウィンドウ幅に対する左描画領域の比率
        bool StartRequested;        // 未処理のStart要求が存在するか
        bool StopRequested;         // 未処理のStop要求が存在するか
        bool ResizeRequested;       // 未処理の描画領域サイズ変更が存在するか
        bool SplitDragging;         // スプリッターをドラッグ中か
        bool IsPlaying;             // UI上でゲームを再生中として扱うか
        bool UpdatingFrameRate;     // FPSコントロールの相互更新中か
        bool ClearLogsRequested;    // 未処理のログ一括消去要求が存在するか
        bool ClassRegistered;       // 親ウィンドウクラスを登録済みか
        std::wstring ClassName;     // 親ウィンドウクラス名
        WindowsUISettings UISettings;       // UI画像とフォントの現在設定
        WindowsUIResources UIResources;     // Windows用フォントと画像リソース
    };
}
