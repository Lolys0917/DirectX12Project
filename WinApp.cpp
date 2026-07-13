//|| WinApp.cpp ||:::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DX12エンジン用Windowsエディターウィンドウを実装する
//||  左描画領域とWindows標準コントロールの操作パネルを管理する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.30  修正: 右パネル背景のZ Orderを最背面へ固定
//||                         上段操作、中央FPS、下段ログの配置へ変更
//||  2026_07_13  v2.20  編集: Window、Control及びUI Resource初期化失敗をログへ記録
//||  2026_07_13  v2.10  変更: DPI対応レイアウトとログ表示領域を追加
//||  2026_07_13  v2.00  追加: 可変分割UI、再生操作、動的FPS設定
//||

#include "WinApp.h"

#include "MessageLog.h"

#include <CommCtrl.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cwchar>
#include <iterator>
#include <limits>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace Engine
{
    namespace
    {
        constexpr int RenderControlId = 1000;          // 左描画領域のコントロールID
        constexpr int StartControlId = 1001;           // StartボタンのコントロールID
        constexpr int StopControlId = 1002;            // StopボタンのコントロールID
        constexpr int TickControlId = 1003;            // TickボタンのコントロールID
        constexpr int FrameRateEditControlId = 1004;   // FPS EDITのコントロールID
        constexpr int FrameRateSliderControlId = 1005; // FPS TRACKBARのコントロールID
        constexpr int LogListControlId = 1006;          // メッセージログ一覧のコントロールID
        constexpr int ClearLogsControlId = 1007;        // ログ一括消去ボタンのコントロールID
        constexpr uint32_t MinimumFrameRate = 1;        // 設定可能な最小FPS
        constexpr uint32_t MaximumFrameRate = 240;      // 設定可能な最大FPS
        constexpr uint32_t DefaultFrameRate = 60;       // 起動時の目標FPS
        constexpr int MinimumRenderWidth = 320;         // 左描画領域の最小論理幅
        constexpr int MinimumPanelWidth = 380;          // 右操作パネルの最小論理幅
        constexpr int MinimumClientHeight = 640;        // UI全体の最小論理高さ
        constexpr int SplitterWidth = 7;                // スプリッターの論理幅
        constexpr int PanelMargin = 18;                 // 操作パネル内側の論理余白
        constexpr int ControlGap = 8;                   // 標準コントロール間の論理余白
        constexpr int SectionGap = 16;                  // UI設定項目間の論理余白
        constexpr int TitleHeight = 28;                 // 操作パネル見出しの論理高さ
        constexpr int ButtonHeight = 34;                // 再生ボタンの論理高さ
        constexpr int LabelHeight = 24;                 // 通常ラベルの論理高さ
        constexpr int EditWidth = 76;                   // FPS EDITの論理幅
        constexpr int SliderHeight = 38;                // FPS TRACKBARの論理高さ
        constexpr int LogHeaderHeight = 30;             // ログ見出し行の論理高さ
        constexpr int ClearLogsButtonWidth = 112;       // ログ一括消去ボタンの論理幅
        constexpr int MinimumLogListHeight = 120;       // ログ一覧が確保する最小論理高さ
        constexpr int LogAverageCharacterWidth = 10;    // ログ横Scroll幅を見積もる一文字の論理幅
        constexpr float DefaultSplitRatio = 0.68f;      // 起動時に左描画領域が占める比率

        /**
         * 数値をHMENU形式の子コントロールIDへ変換する
         * @param controlId 変換するコントロールID
         * @return CreateWindowExWへ渡すHMENU値
         */
        HMENU ToControlMenu(int controlId)
        {
            return reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId));
        }

        /**
         * Win32 API失敗を直後のGetLastErrorとともにMessageLogへ追加する
         * @param operation 失敗したAPI又はControl作成処理名
         */
        void AddWin32FailureLog(const char* operation)
        {
            const DWORD ErrorCode = GetLastError(); // 失敗したWin32 APIのError Code
            char Message[320]{}; // 処理名とError Codeを含む表示用メッセージ
            sprintf_s(
                Message,
                "[Error] WinApp | %s failed. GetLastError=%lu.",
                operation,
                static_cast<unsigned long>(ErrorCode)
            );
            MessageLog::GetInstance().AddLog(Message);
        }
    }

    // Windowsエディターウィンドウを未生成の状態で作成する
    WinApp::WinApp()
        : Hwnd(nullptr)
        , RenderHwnd(nullptr)
        , PanelHwnd(nullptr)
        , TitleLabelHwnd(nullptr)
        , StartButtonHwnd(nullptr)
        , StopButtonHwnd(nullptr)
        , TickButtonHwnd(nullptr)
        , StatusLabelHwnd(nullptr)
        , FrameRateLabelHwnd(nullptr)
        , FrameRateEditHwnd(nullptr)
        , FrameRateSliderHwnd(nullptr)
        , PreviewLabelHwnd(nullptr)
        , PreviewImageHwnd(nullptr)
        , LogLabelHwnd(nullptr)
        , LogListHwnd(nullptr)
        , ClearLogsButtonHwnd(nullptr)
        , Instance(nullptr)
        , SplitCursor(nullptr)
        , Width(0)
        , Height(0)
        , RenderWidth(0)
        , RenderHeight(0)
        , CurrentDpi(USER_DEFAULT_SCREEN_DPI)
        , TargetFrameRate(DefaultFrameRate)
        , PendingTickCount(0)
        , SplitPosition(0)
        , SplitDragOffset(0)
        , SplitRatio(DefaultSplitRatio)
        , StartRequested(false)
        , StopRequested(false)
        , ResizeRequested(false)
        , SplitDragging(false)
        , IsPlaying(false)
        , UpdatingFrameRate(false)
        , ClearLogsRequested(false)
        , ClassRegistered(false)
        , ClassName(L"DX12EngineEditorWindowClass")
    {
    }

    // 作成済みのWindowsウィンドウとUIリソースを解放する
    WinApp::~WinApp()
    {
        Destroy();
    }

    /**
     * 親エディターウィンドウとWindows標準コントロールを作成する
     * @param title 親ウィンドウのタイトル
     * @param width 親クライアント領域の論理幅
     * @param height 親クライアント領域の論理高さ
     * @return 全ての必須ウィンドウを作成できた場合はtrue
     */
    bool WinApp::Create(
        const wchar_t* title,
        uint32_t width,
        uint32_t height
    )
    {
        Destroy();

        if (title == nullptr || width == 0 || height == 0)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] WinApp | Create received an invalid title or client size."
            );
            return false;
        }

        Instance = GetModuleHandleW(nullptr);

        if (Instance == nullptr)
        {
            AddWin32FailureLog("GetModuleHandleW");
            return false;
        }

        INITCOMMONCONTROLSEX CommonControlInformation{}; // Common Controls初期化情報
        CommonControlInformation.dwSize = sizeof(INITCOMMONCONTROLSEX);
        CommonControlInformation.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;

        if (!InitCommonControlsEx(&CommonControlInformation))
        {
            AddWin32FailureLog("InitCommonControlsEx");
            Destroy();
            return false;
        }

        CurrentDpi = GetDpiForSystem();

        WNDCLASSEXW WindowClass{}; // 親エディターウィンドウのクラス情報
        WindowClass.cbSize = sizeof(WNDCLASSEXW);
        WindowClass.style = CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc = WinApp::WindowProc;
        WindowClass.hInstance = Instance;
        WindowClass.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        WindowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        WindowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        WindowClass.lpszClassName = ClassName.c_str();
        WindowClass.hIconSm = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));

        if (!RegisterClassExW(&WindowClass))
        {
            AddWin32FailureLog("RegisterClassExW");
            Destroy();
            return false;
        }

        ClassRegistered = true;

        const DWORD WindowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN; // 親ウィンドウの表示スタイル
        RECT WindowRectangle{}; // DPI適用後の親ウィンドウ矩形
        WindowRectangle.right = MulDiv(
            static_cast<int>(width),
            static_cast<int>(CurrentDpi),
            USER_DEFAULT_SCREEN_DPI
        );
        WindowRectangle.bottom = MulDiv(
            static_cast<int>(height),
            static_cast<int>(CurrentDpi),
            USER_DEFAULT_SCREEN_DPI
        );

        if (!AdjustWindowRectExForDpi(
            &WindowRectangle,
            WindowStyle,
            FALSE,
            0,
            CurrentDpi))
        {
            AddWin32FailureLog("AdjustWindowRectExForDpi");
            Destroy();
            return false;
        }

        const int WindowWidth = WindowRectangle.right - WindowRectangle.left; // 親ウィンドウ全体の幅
        const int WindowHeight = WindowRectangle.bottom - WindowRectangle.top; // 親ウィンドウ全体の高さ

        Hwnd = CreateWindowExW(
            0,
            ClassName.c_str(),
            title,
            WindowStyle,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WindowWidth,
            WindowHeight,
            nullptr,
            nullptr,
            Instance,
            this
        );

        if (Hwnd == nullptr)
        {
            AddWin32FailureLog("CreateWindowExW for editor window");
            Destroy();
            return false;
        }

        CurrentDpi = GetDpiForWindow(Hwnd);
        SplitCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32644));

        if (!UIResources.Initialize(UISettings, CurrentDpi))
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] WinApp | UI resources were only partially initialized."
            );
        }

        if (!CreateControls())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] WinApp | One or more required editor controls could not be created."
            );
            Destroy();
            return false;
        }

        UpdateClientSize();
        SplitPosition = static_cast<int>(
            std::lround(static_cast<double>(Width) * SplitRatio)
        );

        ApplyInterfaceFont();
        ApplyDemoBitmap();
        UpdatePlaybackControls();
        UpdateFrameRateControls();
        LayoutControls();

        ShowWindow(Hwnd, SW_SHOW);
        UpdateWindow(Hwnd);

        return true;
    }

    // 作成済みの親ウィンドウと子コントロールを破棄する
    void WinApp::Destroy()
    {
        if (Hwnd != nullptr)
        {
            DestroyWindow(Hwnd);
            Hwnd = nullptr;
        }

        RenderHwnd = nullptr;
        PanelHwnd = nullptr;
        TitleLabelHwnd = nullptr;
        StartButtonHwnd = nullptr;
        StopButtonHwnd = nullptr;
        TickButtonHwnd = nullptr;
        StatusLabelHwnd = nullptr;
        FrameRateLabelHwnd = nullptr;
        FrameRateEditHwnd = nullptr;
        FrameRateSliderHwnd = nullptr;
        PreviewLabelHwnd = nullptr;
        PreviewImageHwnd = nullptr;
        LogLabelHwnd = nullptr;
        LogListHwnd = nullptr;
        ClearLogsButtonHwnd = nullptr;

        UIResources.Finalize();

        if (ClassRegistered && Instance != nullptr)
        {
            UnregisterClassW(ClassName.c_str(), Instance);
            ClassRegistered = false;
        }

        Instance = nullptr;
        SplitCursor = nullptr;
        Width = 0;
        Height = 0;
        RenderWidth = 0;
        RenderHeight = 0;
        ResizeRequested = false;
        SplitDragging = false;
        ClearLogsRequested = false;
    }

    /**
     * Windowsメッセージを処理する
     * @return アプリケーションを継続する場合はtrue
     */
    bool WinApp::ProcessMessage()
    {
        MSG Message{}; // 取り出したWindowsメッセージ

        while (PeekMessageW(
            &Message,
            nullptr,
            0,
            0,
            PM_REMOVE
        ))
        {
            if (Message.message == WM_QUIT)
            {
                return false;
            }

            if (Hwnd != nullptr && IsDialogMessageW(Hwnd, &Message))
            {
                continue;
            }

            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }

        return true;
    }

    /**
     * UI画像とフォントの設定を適用する
     * @param settings 新しく適用するWindows標準UI設定
     */
    void WinApp::SetUISettings(const WindowsUISettings& settings)
    {
        UISettings = settings;

        if (Hwnd == nullptr)
        {
            return;
        }

        if (PreviewImageHwnd != nullptr)
        {
            SendMessageW(
                PreviewImageHwnd,
                STM_SETIMAGE,
                IMAGE_BITMAP,
                0
            );
        }

        if (!UIResources.Initialize(UISettings, CurrentDpi))
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] WinApp | UI settings were applied with fallback resources."
            );
        }
        ApplyInterfaceFont();
        ApplyDemoBitmap();
        LayoutControls();
    }

    HWND WinApp::GetHWND() const
    {
        return Hwnd;
    }

    HWND WinApp::GetRenderHwnd() const
    {
        return RenderHwnd;
    }

    HINSTANCE WinApp::GetInstance() const
    {
        return Instance;
    }

    uint32_t WinApp::GetWidth() const
    {
        return Width;
    }

    uint32_t WinApp::GetHeight() const
    {
        return Height;
    }

    uint32_t WinApp::GetRenderWidth() const
    {
        return RenderWidth;
    }

    uint32_t WinApp::GetRenderHeight() const
    {
        return RenderHeight;
    }

    RenderWindowSize WinApp::GetRenderSize() const
    {
        return RenderWindowSize
        {
            RenderWidth,
            RenderHeight
        };
    }

    uint32_t WinApp::GetTargetFrameRate() const
    {
        return TargetFrameRate;
    }

    const WindowsUISettings& WinApp::GetUISettings() const
    {
        return UISettings;
    }

    /**
     * メッセージログ一覧のWindows Handleを取得する
     * @return ログ一覧が未作成の場合はnullptr
     */
    HWND WinApp::GetLogListHwnd() const
    {
        return LogListHwnd;
    }

    /**
     * ログ一括消去ボタンの未処理イベントを1件取得する
     * @return 一括消去要求が存在した場合はtrue
     */
    bool WinApp::ConsumeClearLogs()
    {
        const bool WasRequested = ClearLogsRequested; // 呼び出し時点の一括消去要求
        ClearLogsRequested = false;
        return WasRequested;
    }

    /**
     * 渡された文字列一覧でWindows標準ログ一覧を更新する
     * @param messages 表示順に並んだメッセージ文字列
     */
    void WinApp::UpdateLogMessages(
        const std::vector<std::wstring>& messages
    )
    {
        if (LogListHwnd == nullptr)
        {
            return;
        }

        SendMessageW(LogListHwnd, WM_SETREDRAW, FALSE, 0);
        SendMessageW(LogListHwnd, LB_RESETCONTENT, 0, 0);
        std::size_t LongestMessageLength = 0; // 横Scroll範囲へ使用する最長ログ文字数

        for (const std::wstring& Message : messages) // 表示へ追加するログ文字列
        {
            LongestMessageLength = std::max(
                LongestMessageLength,
                Message.size()
            );
            SendMessageW(
                LogListHwnd,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(Message.c_str())
            );
        }

        const std::size_t MaximumScrollableCharacters =
            static_cast<std::size_t>((std::numeric_limits<int>::max)()) /
            static_cast<std::size_t>(ScaleByDpi(LogAverageCharacterWidth)); // int範囲内に収まる文字数
        const int HorizontalExtent = static_cast<int>(
            std::min(LongestMessageLength, MaximumScrollableCharacters) *
            static_cast<std::size_t>(ScaleByDpi(LogAverageCharacterWidth))
        ); // 長いログ全文を確認するための横Scroll幅
        SendMessageW(
            LogListHwnd,
            LB_SETHORIZONTALEXTENT,
            HorizontalExtent,
            0
        );

        if (!messages.empty())
        {
            SendMessageW(
                LogListHwnd,
                LB_SETTOPINDEX,
                messages.size() - 1,
                0
            );
        }

        SendMessageW(LogListHwnd, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(LogListHwnd, nullptr, TRUE);
    }

    /**
     * Startボタンの未処理イベントを1件取得する
     * @return Start要求が存在した場合はtrue
     */
    bool WinApp::ConsumeStart()
    {
        const bool WasRequested = StartRequested; // 呼び出し時点のStart要求
        StartRequested = false;
        return WasRequested;
    }

    /**
     * Stopボタンの未処理イベントを1件取得する
     * @return Stop要求が存在した場合はtrue
     */
    bool WinApp::ConsumeStop()
    {
        const bool WasRequested = StopRequested; // 呼び出し時点のStop要求
        StopRequested = false;
        return WasRequested;
    }

    /**
     * Tickボタンの未処理イベントを1件取得する
     * @return Tick要求が存在した場合はtrue
     */
    bool WinApp::ConsumeTick()
    {
        if (PendingTickCount == 0)
        {
            return false;
        }

        --PendingTickCount;
        return true;
    }

    /**
     * 描画用子ウィンドウのサイズ変更を取得する
     * @param size 変更後の描画領域サイズを受け取る変数
     * @return 未処理のサイズ変更が存在した場合はtrue
     */
    bool WinApp::ConsumeResize(RenderWindowSize& size)
    {
        if (!ResizeRequested)
        {
            return false;
        }

        size.Width = RenderWidth;
        size.Height = RenderHeight;
        ResizeRequested = false;
        return true;
    }

    /**
     * HWNDからWinAppインスタンスへWindowsメッセージを転送する
     * @param hwnd メッセージを受信した親ウィンドウ
     * @param message Windowsメッセージ番号
     * @param wparam メッセージ固有の追加情報
     * @param lparam メッセージ固有の追加情報
     * @return メッセージ処理結果
     */
    LRESULT CALLBACK WinApp::WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam
    )
    {
        WinApp* Application = reinterpret_cast<WinApp*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
        ); // HWNDへ関連付けられたWinApp

        if (message == WM_NCCREATE)
        {
            CREATESTRUCTW* CreateInformation = reinterpret_cast<CREATESTRUCTW*>(
                lparam
            ); // CreateWindowExWから渡された作成情報

            Application = static_cast<WinApp*>(CreateInformation->lpCreateParams);

            if (Application != nullptr)
            {
                Application->Hwnd = hwnd;
                SetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(Application)
                );
            }
        }

        if (Application != nullptr)
        {
            return Application->HandleMessage(
                message,
                wparam,
                lparam
            );
        }

        return DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam
        );
    }

    /**
     * WinAppインスタンスに紐付いたWindowsメッセージを処理する
     * @param message Windowsメッセージ番号
     * @param wparam メッセージ固有の追加情報
     * @param lparam メッセージ固有の追加情報
     * @return メッセージ処理結果
     */
    LRESULT WinApp::HandleMessage(
        UINT message,
        WPARAM wparam,
        LPARAM lparam
    )
    {
        switch (message)
        {
        case WM_COMMAND:
        {
            const int ControlId = LOWORD(wparam); // 通知を送信したコントロールID
            const int NotificationCode = HIWORD(wparam); // コントロールの通知種類

            if (ControlId == StartControlId && NotificationCode == BN_CLICKED)
            {
                StartRequested = true;
                StopRequested = false;
                PendingTickCount = 0;
                IsPlaying = true;
                UpdatePlaybackControls();
                return 0;
            }

            if (ControlId == StopControlId && NotificationCode == BN_CLICKED)
            {
                StopRequested = true;
                StartRequested = false;
                PendingTickCount = 0;
                IsPlaying = false;
                UpdatePlaybackControls();
                return 0;
            }

            if (ControlId == TickControlId && NotificationCode == BN_CLICKED)
            {
                if (!IsPlaying && PendingTickCount < std::numeric_limits<uint32_t>::max())
                {
                    ++PendingTickCount;
                }

                return 0;
            }

            if (ControlId == ClearLogsControlId && NotificationCode == BN_CLICKED)
            {
                ClearLogsRequested = true;
                return 0;
            }

            if (ControlId == FrameRateEditControlId)
            {
                if (NotificationCode == EN_CHANGE && !UpdatingFrameRate)
                {
                    UpdateFrameRateFromEdit();
                    return 0;
                }

                if (NotificationCode == EN_KILLFOCUS)
                {
                    UpdateFrameRateControls();
                    return 0;
                }
            }

            break;
        }

        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lparam) == FrameRateSliderHwnd)
            {
                const LRESULT SliderPosition = SendMessageW(
                    FrameRateSliderHwnd,
                    TBM_GETPOS,
                    0,
                    0
                ); // TRACKBARが現在示しているFPS

                SetTargetFrameRate(
                    static_cast<uint32_t>(SliderPosition)
                );
                return 0;
            }
            break;

        case WM_SIZE:
            UpdateClientSize();

            if (wparam == SIZE_MINIMIZED)
            {
                ResizeRequested = false;
                return 0;
            }

            LayoutControls();
            return 0;

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* SizeInformation = reinterpret_cast<MINMAXINFO*>(lparam); // 親ウィンドウのサイズ制約
            RECT MinimumWindowRectangle{}; // 最小クライアントサイズを含むウィンドウ矩形
            MinimumWindowRectangle.right = ScaleByDpi(
                MinimumRenderWidth + SplitterWidth + MinimumPanelWidth
            );
            MinimumWindowRectangle.bottom = ScaleByDpi(MinimumClientHeight);

            AdjustWindowRectExForDpi(
                &MinimumWindowRectangle,
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                FALSE,
                0,
                CurrentDpi
            );

            SizeInformation->ptMinTrackSize.x =
                MinimumWindowRectangle.right - MinimumWindowRectangle.left;
            SizeInformation->ptMinTrackSize.y =
                MinimumWindowRectangle.bottom - MinimumWindowRectangle.top;
            return 0;
        }

        case WM_DPICHANGED:
        {
            CurrentDpi = HIWORD(wparam);

            if (PreviewImageHwnd != nullptr)
            {
                SendMessageW(
                    PreviewImageHwnd,
                    STM_SETIMAGE,
                    IMAGE_BITMAP,
                    0
                );
            }

            if(!UIResources.SetTexturePath(L"c_1576721479650-1-2.jpg"))
            {
                MessageLog::GetInstance().AddLog(
                    "[Warning] WinApp | Failed to set texture path for UIDemo.png."
                );
            }

            if (!UIResources.UpdateDpi(CurrentDpi))
            {
                MessageLog::GetInstance().AddLog(
                    "[Warning] WinApp | DPI resource update used fallback UI resources."
                );
            }
            ApplyInterfaceFont();
            ApplyDemoBitmap();

            const RECT* SuggestedRectangle = reinterpret_cast<const RECT*>(lparam); // Windowsが推奨するDPI変更後の矩形
            SetWindowPos(
                Hwnd,
                nullptr,
                SuggestedRectangle->left,
                SuggestedRectangle->top,
                SuggestedRectangle->right - SuggestedRectangle->left,
                SuggestedRectangle->bottom - SuggestedRectangle->top,
                SWP_NOACTIVATE | SWP_NOZORDER
            );
            return 0;
        }

        case WM_SETCURSOR:
        {
            POINT CursorPosition{}; // 親クライアント座標へ変換するカーソル位置
            GetCursorPos(&CursorPosition);
            ScreenToClient(Hwnd, &CursorPosition);

            if (IsPointInSplitter(CursorPosition.x, CursorPosition.y))
            {
                SetCursor(SplitCursor);
                return TRUE;
            }

            break;
        }

        case WM_LBUTTONDOWN:
        {
            const int MouseX = GET_X_LPARAM(lparam); // 親クライアント座標のマウスX位置
            const int MouseY = GET_Y_LPARAM(lparam); // 親クライアント座標のマウスY位置

            if (IsPointInSplitter(MouseX, MouseY))
            {
                SplitDragging = true;
                SplitDragOffset = MouseX - SplitPosition;
                SetCapture(Hwnd);
                SetCursor(SplitCursor);
                return 0;
            }

            break;
        }

        case WM_MOUSEMOVE:
            if (SplitDragging)
            {
                const int MouseX = GET_X_LPARAM(lparam); // ドラッグ中のマウスX位置
                SplitPosition = MouseX - SplitDragOffset;
                ClampSplitPosition();

                if (Width > 0)
                {
                    SplitRatio = static_cast<float>(SplitPosition) /
                        static_cast<float>(Width);
                }

                LayoutControls();
                SetCursor(SplitCursor);
                return 0;
            }
            break;

        case WM_LBUTTONUP:
            if (SplitDragging)
            {
                SplitDragging = false;

                if (GetCapture() == Hwnd)
                {
                    ReleaseCapture();
                }

                return 0;
            }
            break;

        case WM_CAPTURECHANGED:
            SplitDragging = false;
            return 0;

        case WM_PAINT:
            PaintSplitter();
            return 0;

        case WM_CLOSE:
            DestroyWindow(Hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_NCDESTROY:
        {
            const HWND DestroyedHwnd = Hwnd; // DefWindowProcWへ渡す破棄対象HWND
            const LRESULT Result = DefWindowProcW(
                DestroyedHwnd,
                message,
                wparam,
                lparam
            ); // Windows既定の非クライアント破棄結果

            SetWindowLongPtrW(
                DestroyedHwnd,
                GWLP_USERDATA,
                0
            );
            Hwnd = nullptr;
            return Result;
        }

        default:
            break;
        }

        return DefWindowProcW(
            Hwnd,
            message,
            wparam,
            lparam
        );
    }

    /**
     * 描画領域と右操作パネルのWindows標準コントロールを作成する
     * @return 必須コントロールを全て作成できた場合はtrue
     */
    bool WinApp::CreateControls()
    {
        RenderHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_BLACKRECT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(RenderControlId),
            Instance,
            nullptr
        );

        PanelHwnd = CreateWindowExW(
            WS_EX_CONTROLPARENT,
            WC_STATICW,
            L"",
            WS_CHILD | WS_CLIPSIBLINGS | SS_WHITERECT,
            0,
            0,
            1,
            1,
            Hwnd,
            nullptr,
            Instance,
            nullptr
        );

        TitleLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"ゲームコントロール",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT,
            0,
            0,
            1,
            1,
            Hwnd,
            nullptr,
            Instance,
            nullptr
        );

        StartButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"Start",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(StartControlId),
            Instance,
            nullptr
        );

        StopButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"Stop",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(StopControlId),
            Instance,
            nullptr
        );

        TickButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"Tick",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(TickControlId),
            Instance,
            nullptr
        );

        StatusLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"状態: 停止中",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT,
            0,
            0,
            1,
            1,
            Hwnd,
            nullptr,
            Instance,
            nullptr
        );

        FrameRateLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"フレームレート",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT,
            0,
            0,
            1,
            1,
            Hwnd,
            nullptr,
            Instance,
            nullptr
        );

        FrameRateEditHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_EDITW,
            L"60",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(FrameRateEditControlId),
            Instance,
            nullptr
        );

        FrameRateSliderHwnd = CreateWindowExW(
            0,
            TRACKBAR_CLASSW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                TBS_AUTOTICKS | TBS_HORZ,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(FrameRateSliderControlId),
            Instance,
            nullptr
        );

        PreviewLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"UIテクスチャ (UIDemo.png)",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT,
            0,
            0,
            1,
            1,
            Hwnd,
            nullptr,
            Instance,
            nullptr
        );

        PreviewImageHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_STATICW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                SS_BITMAP | SS_CENTERIMAGE,
            0,
            0,
            1,
            1,
            Hwnd,
            nullptr,
            Instance,
            nullptr
        );

        LogLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"メッセージログ",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT,
            0,
            0,
            1,
            1,
            Hwnd,
            nullptr,
            Instance,
            nullptr
        );

        ClearLogsButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"ログを一括消去",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ClearLogsControlId),
            Instance,
            nullptr
        );

        LogListHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTBOXW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(LogListControlId),
            Instance,
            nullptr
        );

        const bool ControlsCreated =
            RenderHwnd != nullptr &&
            PanelHwnd != nullptr &&
            TitleLabelHwnd != nullptr &&
            StartButtonHwnd != nullptr &&
            StopButtonHwnd != nullptr &&
            TickButtonHwnd != nullptr &&
            StatusLabelHwnd != nullptr &&
            FrameRateLabelHwnd != nullptr &&
            FrameRateEditHwnd != nullptr &&
            FrameRateSliderHwnd != nullptr &&
            PreviewLabelHwnd != nullptr &&
            PreviewImageHwnd != nullptr &&
            LogLabelHwnd != nullptr &&
            LogListHwnd != nullptr &&
            ClearLogsButtonHwnd != nullptr; // 必須コントロールを全て作成できたか

        if (!ControlsCreated)
        {
            AddWin32FailureLog("CreateWindowExW for required editor controls");
            return false;
        }

        if (!SetWindowPos(
            PanelHwnd,
            HWND_BOTTOM,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE))
        {
            AddWin32FailureLog("SetWindowPos for editor panel background");
            return false;
        }

        SendMessageW(
            FrameRateEditHwnd,
            EM_SETLIMITTEXT,
            3,
            0
        );
        SendMessageW(
            FrameRateSliderHwnd,
            TBM_SETRANGEMIN,
            FALSE,
            MinimumFrameRate
        );
        SendMessageW(
            FrameRateSliderHwnd,
            TBM_SETRANGEMAX,
            TRUE,
            MaximumFrameRate
        );
        SendMessageW(
            FrameRateSliderHwnd,
            TBM_SETTICFREQ,
            30,
            0
        );

        return true;
    }

    // 現在のクライアントサイズと分割位置から全コントロールを配置する
    void WinApp::LayoutControls()
    {
        if (Hwnd == nullptr || Width == 0 || Height == 0)
        {
            return;
        }

        if (!SplitDragging)
        {
            SplitPosition = static_cast<int>(
                std::lround(static_cast<double>(Width) * SplitRatio)
            );
        }

        ClampSplitPosition();

        const int ClientHeight = static_cast<int>(Height); // レイアウト対象の親クライアント高さ
        const int SplitterThickness = ScaleByDpi(SplitterWidth); // 現在DPIのスプリッター幅
        const int PanelLeft = SplitPosition + SplitterThickness; // 右操作パネルの左位置
        const int PanelWidth = std::max(0, static_cast<int>(Width) - PanelLeft); // 右操作パネルの幅
        const int Margin = ScaleByDpi(PanelMargin); // 右操作パネルの内側余白
        const int Gap = ScaleByDpi(ControlGap); // コントロール間の余白
        const int ScaledSectionGap = ScaleByDpi(SectionGap); // UI設定項目間の余白
        const int ContentLeft = PanelLeft + Margin; // 右パネル内容の左位置
        const int ContentWidth = std::max(0, PanelWidth - Margin * 2); // 右パネル内容の幅
        const int ScaledTitleHeight = ScaleByDpi(TitleHeight); // 見出しの高さ
        const int ScaledButtonHeight = ScaleByDpi(ButtonHeight); // 再生ボタンの高さ
        const int ScaledLabelHeight = ScaleByDpi(LabelHeight); // 通常ラベルの高さ
        const int ScaledEditWidth = ScaleByDpi(EditWidth); // FPS EDITの幅
        const int ScaledSliderHeight = ScaleByDpi(SliderHeight); // FPS TRACKBARの高さ
        const int ScaledLogHeaderHeight = ScaleByDpi(LogHeaderHeight); // ログ見出し行の高さ
        const int ScaledClearButtonWidth = ScaleByDpi(ClearLogsButtonWidth); // ログ消去ボタンの幅
        const int ScaledMinimumLogHeight = ScaleByDpi(MinimumLogListHeight); // ログ一覧の最低高
        const int ButtonWidth = std::max(1, (ContentWidth - Gap * 2) / 3); // 各再生ボタンの幅
        int ContentTop = Margin; // 次に配置するコントロールの上位置

        MoveWindow(
            RenderHwnd,
            0,
            0,
            std::max(0, SplitPosition),
            ClientHeight,
            TRUE
        );
        MoveWindow(
            PanelHwnd,
            PanelLeft,
            0,
            PanelWidth,
            ClientHeight,
            TRUE
        );
        MoveWindow(
            TitleLabelHwnd,
            ContentLeft,
            ContentTop,
            ContentWidth,
            ScaledTitleHeight,
            TRUE
        );

        ContentTop += ScaledTitleHeight + Gap;

        MoveWindow(
            StartButtonHwnd,
            ContentLeft,
            ContentTop,
            ButtonWidth,
            ScaledButtonHeight,
            TRUE
        );
        MoveWindow(
            StopButtonHwnd,
            ContentLeft + ButtonWidth + Gap,
            ContentTop,
            ButtonWidth,
            ScaledButtonHeight,
            TRUE
        );
        MoveWindow(
            TickButtonHwnd,
            ContentLeft + (ButtonWidth + Gap) * 2,
            ContentTop,
            ButtonWidth,
            ScaledButtonHeight,
            TRUE
        );

        ContentTop += ScaledButtonHeight + Gap;

        MoveWindow(
            StatusLabelHwnd,
            ContentLeft,
            ContentTop,
            ContentWidth,
            ScaledLabelHeight,
            TRUE
        );

        ContentTop += ScaledLabelHeight + ScaledSectionGap;

        MoveWindow(
            PreviewLabelHwnd,
            ContentLeft,
            ContentTop,
            ContentWidth,
            ScaledLabelHeight,
            TRUE
        );

        ContentTop += ScaledLabelHeight + Gap;

        const int PreviewWidth = ScaleByDpi(
            static_cast<int>(UISettings.PreviewWidth)
        ); // DPI適用後の画像プレビュー幅
        const int DesiredPreviewHeight = ScaleByDpi(
            static_cast<int>(UISettings.PreviewHeight)
        ); // DPI適用後の画像プレビュー希望高さ
        const int ReservedMiddleAndLogHeight =
            ScaledSectionGap +
            ScaledLabelHeight +
            Gap +
            ScaledSliderHeight +
            ScaledSectionGap +
            ScaledLogHeaderHeight +
            Gap +
            ScaledMinimumLogHeight; // 画像より下へ必ず確保するFPS及びログ領域の高さ
        const int AvailablePreviewHeight = std::max(
            1,
            ClientHeight - ContentTop - Margin - ReservedMiddleAndLogHeight
        ); // 現在のウィンドウ高で画像へ割り当てられる高さ
        const int PreviewHeight = std::min(
            DesiredPreviewHeight,
            AvailablePreviewHeight
        ); // ログ領域を侵食しない画像プレビュー高さ
        const int PreviewLeft = ContentLeft + std::max(
            0,
            (ContentWidth - PreviewWidth) / 2
        ); // 右パネル中央へ配置する画像の左位置

        MoveWindow(
            PreviewImageHwnd,
            PreviewLeft,
            ContentTop,
            std::min(PreviewWidth, ContentWidth),
            PreviewHeight,
            TRUE
        );

        ContentTop += PreviewHeight + ScaledSectionGap;

        MoveWindow(
            FrameRateLabelHwnd,
            ContentLeft,
            ContentTop,
            std::max(1, ContentWidth - ScaledEditWidth - Gap),
            ScaledLabelHeight,
            TRUE
        );
        MoveWindow(
            FrameRateEditHwnd,
            ContentLeft + std::max(0, ContentWidth - ScaledEditWidth),
            ContentTop,
            ScaledEditWidth,
            ScaledLabelHeight,
            TRUE
        );

        ContentTop += ScaledLabelHeight + Gap;

        MoveWindow(
            FrameRateSliderHwnd,
            ContentLeft,
            ContentTop,
            ContentWidth,
            ScaledSliderHeight,
            TRUE
        );

        ContentTop += ScaledSliderHeight + ScaledSectionGap;

        const int LogLabelWidth = std::max(
            1,
            ContentWidth - ScaledClearButtonWidth - Gap
        ); // 一括消去ボタンを除いたログ見出し幅

        MoveWindow(
            LogLabelHwnd,
            ContentLeft,
            ContentTop,
            LogLabelWidth,
            ScaledLogHeaderHeight,
            TRUE
        );
        MoveWindow(
            ClearLogsButtonHwnd,
            ContentLeft + std::max(0, ContentWidth - ScaledClearButtonWidth),
            ContentTop,
            std::min(ScaledClearButtonWidth, ContentWidth),
            ScaledLogHeaderHeight,
            TRUE
        );

        ContentTop += ScaledLogHeaderHeight + Gap;

        MoveWindow(
            LogListHwnd,
            ContentLeft,
            ContentTop,
            ContentWidth,
            std::max(1, ClientHeight - ContentTop - Margin),
            TRUE
        );

        const HWND ForegroundControls[] =
        {
            TitleLabelHwnd,
            StartButtonHwnd,
            StopButtonHwnd,
            TickButtonHwnd,
            StatusLabelHwnd,
            PreviewLabelHwnd,
            PreviewImageHwnd,
            FrameRateLabelHwnd,
            FrameRateEditHwnd,
            FrameRateSliderHwnd,
            LogLabelHwnd,
            ClearLogsButtonHwnd,
            LogListHwnd
        }; // 右パネル背景より常に前面へ配置する機能コントロール

        bool ZOrderSucceeded = SetWindowPos(
            PanelHwnd,
            HWND_BOTTOM,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        ) != FALSE; // 右パネル範囲を機能Controlより背面へ置けた場合はtrue

        for (HWND Control : ForegroundControls) // Panel描画に隠されないようZ Orderを更新するControl
        {
            const bool ControlOrderSucceeded = SetWindowPos(
                Control,
                HWND_TOP,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
            ) != FALSE; // 現在Controlを前面へ移動できた場合はtrue
            ZOrderSucceeded = ControlOrderSucceeded && ZOrderSucceeded;
        }

        if (!ZOrderSucceeded)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Error] WinApp | One or more editor controls could not be moved above the right panel."
            );
        }

        InvalidateRect(Hwnd, nullptr, FALSE);
        UpdateRenderSize();
    }

    // 現在のUIフォントを全ての標準コントロールへ適用する
    void WinApp::ApplyInterfaceFont()
    {
        HFONT Font = UIResources.GetInterfaceFont(); // 標準コントロールへ設定するフォント

        if (Font == nullptr)
        {
            Font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        }

        const HWND Controls[] =
        {
            TitleLabelHwnd,
            StartButtonHwnd,
            StopButtonHwnd,
            TickButtonHwnd,
            StatusLabelHwnd,
            FrameRateLabelHwnd,
            FrameRateEditHwnd,
            FrameRateSliderHwnd,
            PreviewLabelHwnd,
            LogLabelHwnd,
            LogListHwnd,
            ClearLogsButtonHwnd
        }; // UIフォントを設定するWindows標準コントロール

        for (HWND Control : Controls) // 現在フォントを適用するコントロール
        {
            if (Control != nullptr)
            {
                SendMessageW(
                    Control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(Font),
                    TRUE
                );
            }
        }
    }

    // 現在のUIDemoビットマップを画像コントロールへ適用する
    void WinApp::ApplyDemoBitmap()
    {
        if (PreviewImageHwnd == nullptr)
        {
            return;
        }

        SendMessageW(
            PreviewImageHwnd,
            STM_SETIMAGE,
            IMAGE_BITMAP,
            reinterpret_cast<LPARAM>(UIResources.GetDemoBitmap())
        );

        if (PreviewLabelHwnd != nullptr)
        {
            SetWindowTextW(
                PreviewLabelHwnd,
                UIResources.IsDemoBitmapLoaded()
                    ? L"UIテクスチャ (UIDemo.png)"
                    : L"UIDemo.png の読込に失敗"
            );
        }
    }

    // 再生状態に合わせてボタンと状態テキストを更新する
    void WinApp::UpdatePlaybackControls()
    {
        if (StartButtonHwnd != nullptr)
        {
            EnableWindow(StartButtonHwnd, IsPlaying ? FALSE : TRUE);
        }

        if (StopButtonHwnd != nullptr)
        {
            EnableWindow(StopButtonHwnd, IsPlaying ? TRUE : FALSE);
        }

        if (TickButtonHwnd != nullptr)
        {
            EnableWindow(TickButtonHwnd, IsPlaying ? FALSE : TRUE);
        }

        if (StatusLabelHwnd != nullptr)
        {
            SetWindowTextW(
                StatusLabelHwnd,
                IsPlaying ? L"状態: 再生中" : L"状態: 停止中"
            );
        }
    }

    // 目標FPSに合わせてEDITとTRACKBARを同期する
    void WinApp::UpdateFrameRateControls()
    {
        UpdatingFrameRate = true;

        wchar_t FrameRateText[16]{}; // 目標FPSをEDITへ表示する文字列
        _snwprintf_s(
            FrameRateText,
            std::size(FrameRateText),
            _TRUNCATE,
            L"%u",
            TargetFrameRate
        );

        if (FrameRateEditHwnd != nullptr)
        {
            SetWindowTextW(FrameRateEditHwnd, FrameRateText);
        }

        if (FrameRateSliderHwnd != nullptr)
        {
            SendMessageW(
                FrameRateSliderHwnd,
                TBM_SETPOS,
                TRUE,
                TargetFrameRate
            );
        }

        UpdatingFrameRate = false;
    }

    // FPS EDITの有効な数値を目標FPSへ反映する
    void WinApp::UpdateFrameRateFromEdit()
    {
        if (FrameRateEditHwnd == nullptr)
        {
            return;
        }

        wchar_t FrameRateText[16]{}; // FPS EDITから読み取る文字列
        GetWindowTextW(
            FrameRateEditHwnd,
            FrameRateText,
            static_cast<int>(std::size(FrameRateText))
        );

        wchar_t* ParseEnd = nullptr; // FPS数値として読み終えた位置
        const unsigned long ParsedFrameRate = std::wcstoul(
            FrameRateText,
            &ParseEnd,
            10
        ); // EDIT文字列から読み取ったFPS

        if (ParseEnd == FrameRateText || *ParseEnd != L'\0')
        {
            return;
        }

        TargetFrameRate = std::clamp<uint32_t>(
            static_cast<uint32_t>(ParsedFrameRate),
            MinimumFrameRate,
            MaximumFrameRate
        );

        UpdatingFrameRate = true;
        SendMessageW(
            FrameRateSliderHwnd,
            TBM_SETPOS,
            TRUE,
            TargetFrameRate
        );
        UpdatingFrameRate = false;
    }

    /**
     * 目標FPSを有効範囲へ収めて設定する
     * @param frameRate 新しく設定する目標FPS
     */
    void WinApp::SetTargetFrameRate(uint32_t frameRate)
    {
        TargetFrameRate = std::clamp<uint32_t>(
            frameRate,
            MinimumFrameRate,
            MaximumFrameRate
        );
        UpdateFrameRateControls();
    }

    // 親ウィンドウの現在クライアントサイズを保存する
    void WinApp::UpdateClientSize()
    {
        if (Hwnd == nullptr)
        {
            Width = 0;
            Height = 0;
            return;
        }

        RECT ClientRectangle{}; // 親ウィンドウの現在クライアント矩形

        if (!GetClientRect(Hwnd, &ClientRectangle))
        {
            Width = 0;
            Height = 0;
            return;
        }

        Width = static_cast<uint32_t>(
            std::max<LONG>(0, ClientRectangle.right - ClientRectangle.left)
        );
        Height = static_cast<uint32_t>(
            std::max<LONG>(0, ClientRectangle.bottom - ClientRectangle.top)
        );
    }

    // 描画用子ウィンドウの現在サイズを変更イベントとして記録する
    void WinApp::UpdateRenderSize()
    {
        if (RenderHwnd == nullptr)
        {
            return;
        }

        RECT RenderRectangle{}; // 左描画ウィンドウのクライアント矩形

        if (!GetClientRect(RenderHwnd, &RenderRectangle))
        {
            AddWin32FailureLog("GetClientRect for render viewport");
            return;
        }

        const uint32_t NewWidth = static_cast<uint32_t>(
            std::max<LONG>(0, RenderRectangle.right - RenderRectangle.left)
        ); // 左描画ウィンドウの新しい幅
        const uint32_t NewHeight = static_cast<uint32_t>(
            std::max<LONG>(0, RenderRectangle.bottom - RenderRectangle.top)
        ); // 左描画ウィンドウの新しい高さ

        if (NewWidth == RenderWidth && NewHeight == RenderHeight)
        {
            return;
        }

        RenderWidth = NewWidth;
        RenderHeight = NewHeight;

        if (RenderWidth > 0 && RenderHeight > 0)
        {
            ResizeRequested = true;
        }
        else
        {
            ResizeRequested = false;
        }
    }

    /**
     * 現在DPIへ論理ピクセル値を変換する
     * @param value 変換前の96DPI基準値
     * @return 現在DPIへ変換した物理ピクセル値
     */
    int WinApp::ScaleByDpi(int value) const
    {
        return MulDiv(
            value,
            static_cast<int>(CurrentDpi),
            USER_DEFAULT_SCREEN_DPI
        );
    }

    /**
     * 現在の分割位置からスプリッター矩形を取得する
     * @return 親クライアント座標のスプリッター矩形
     */
    RECT WinApp::GetSplitterRect() const
    {
        RECT SplitterRectangle{}; // 親クライアント座標のスプリッター矩形
        SplitterRectangle.left = SplitPosition;
        SplitterRectangle.top = 0;
        SplitterRectangle.right = SplitPosition + ScaleByDpi(SplitterWidth);
        SplitterRectangle.bottom = static_cast<LONG>(Height);
        return SplitterRectangle;
    }

    /**
     * 指定座標がスプリッター内にあるか調べる
     * @param x 親クライアント座標のX位置
     * @param y 親クライアント座標のY位置
     * @return スプリッター内の場合はtrue
     */
    bool WinApp::IsPointInSplitter(int x, int y) const
    {
        const RECT SplitterRectangle = GetSplitterRect(); // 当たり判定に使用するスプリッター矩形

        return
            x >= SplitterRectangle.left &&
            x < SplitterRectangle.right &&
            y >= SplitterRectangle.top &&
            y < SplitterRectangle.bottom;
    }

    // 現在のクライアント幅に対して分割位置を安全な範囲へ収める
    void WinApp::ClampSplitPosition()
    {
        const int ClientWidth = static_cast<int>(Width); // 親ウィンドウの現在クライアント幅
        const int MinimumPosition = ScaleByDpi(MinimumRenderWidth); // 左描画領域を保つ最小分割位置
        const int MaximumPosition = ClientWidth -
            ScaleByDpi(SplitterWidth + MinimumPanelWidth); // 右操作パネルを保つ最大分割位置

        if (MaximumPosition < MinimumPosition)
        {
            SplitPosition = std::max(
                0,
                (ClientWidth - ScaleByDpi(SplitterWidth)) / 2
            );
            return;
        }

        SplitPosition = std::clamp(
            SplitPosition,
            MinimumPosition,
            MaximumPosition
        );
    }

    // 親ウィンドウの未使用領域へスプリッターを描画する
    void WinApp::PaintSplitter()
    {
        PAINTSTRUCT PaintInformation{}; // BeginPaintとEndPaintで使用する描画情報
        HDC PaintDc = BeginPaint(Hwnd, &PaintInformation); // スプリッター描画用DC

        if (PaintDc != nullptr)
        {
            RECT SplitterRectangle = GetSplitterRect(); // 描画するスプリッター矩形
            FillRect(
                PaintDc,
                &SplitterRectangle,
                reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1)
            );
            DrawEdge(
                PaintDc,
                &SplitterRectangle,
                EDGE_RAISED,
                BF_LEFT | BF_RIGHT
            );
        }

        EndPaint(Hwnd, &PaintInformation);
    }
}
