//|| WinApp.cpp ||:::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DX12エンジン用Windowsエディターウィンドウを実装する
//||  左描画領域とWindows標準コントロールの操作パネルを管理する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v3.10  GameEngine組込みAPIをコード補完走査対象へ追加
//||  2026_08_17  v3.00  Tab式EditorとObject／Script Context操作を追加
//||  2026_07_13  v2.30  修正: 右パネル背景のZ Orderを最背面へ固定
//||                         上段操作、中央FPS、下段ログの配置へ変更
//||  2026_07_13  v2.20  編集: Window、Control及びUI Resource初期化失敗をログへ記録
//||  2026_07_13  v2.10  変更: DPI対応レイアウトとログ表示領域を追加
//||  2026_07_13  v2.00  追加: 可変分割UI、再生操作、動的FPS設定
//||

#include "WinApp.h"

#include "MessageLog.h"
#include "ProgramSuggestionRegistry.h"

#include <CommCtrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
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
        constexpr int EditorTabControlId = 1008;         // Editor TabのコントロールID
        constexpr int ObjectTreeControlId = 1009;        // Object TreeのコントロールID
        constexpr int AddObjectControlId = 1010;         // Object追加ボタンのコントロールID
        constexpr int AddScriptControlId = 1011;         // Script追加ボタンのコントロールID
        constexpr int LoadScriptControlId = 1012;        // Script DLL読込ボタンのコントロールID
        constexpr int SceneSelectorControlId = 1013;     // Scene選択ComboのコントロールID
        constexpr int ObjectNameEditControlId = 1014;    // Object名入力のコントロールID
        constexpr int ObjectActiveControlId = 1015;      // Object有効CheckのコントロールID
        constexpr int TransformEditControlBase = 1016;   // Transform XYZ入力IDの基点
        constexpr int ApplyObjectControlId = 1025;       // Object編集適用ボタンのコントロールID
        constexpr int ProgramFileListControlId = 1030;   // Program File一覧のコントロールID
        constexpr int ProgramFunctionListControlId = 1031; // Program関数一覧のコントロールID
        constexpr int ProgramEditorControlId = 1032;     // Program入力のコントロールID
        constexpr int ProgramErrorListControlId = 1033;  // Compile出力一覧のコントロールID
        constexpr int ProgramFileNameControlId = 1034;   // Program File名入力のコントロールID
        constexpr int NewProgramControlId = 1035;        // Program新規作成ボタンのコントロールID
        constexpr int RenameProgramControlId = 1036;     // Program名前変更ボタンのコントロールID
        constexpr int DeleteProgramControlId = 1037;     // Program削除ボタンのコントロールID
        constexpr int SaveProgramControlId = 1038;       // Program保存ボタンのコントロールID
        constexpr int CompileProgramControlId = 1039;    // Program手動CompileボタンのコントロールID
        constexpr int ProgramStatusControlId = 1040;     // Program状態表示のコントロールID
        constexpr int ProgramSuggestionListControlId = 1041; // Programコード補完一覧ID
        constexpr int ProgramRoleControlId = 1042;       // Main又はScript役割説明Label ID
        constexpr int RestoreProgramControlId = 1043;    // 最終Compile成功Source復元Button ID
        constexpr int PauseControlId = 1044;             // 状態を保持する一時停止Button ID
        constexpr UINT_PTR ProgramEditorSubclassId = 5001; // RichEdit補完Subclass識別子
        constexpr UINT_PTR ProgramAutomationTimerId = 4001; // 自動保存とCompile監視Timer ID
        constexpr UINT ProgramAutomationTimerInterval = 200; // 自動処理確認間隔ms
        constexpr auto ProgramCompileDebounce = std::chrono::milliseconds(900); // 入力停止待機時間
        constexpr auto ProgramVisualRefreshDebounce = std::chrono::milliseconds(280); // 色分けと関数解析の入力停止待機時間
        constexpr UINT CreateObjectMenuBase = 2000;      // Object型Menu IDの基点
        constexpr UINT DuplicateObjectMenuId = 2020;     // Object複製Menu ID
        constexpr UINT RenameItemMenuId = 2021;          // Tree要素名変更Menu ID
        constexpr UINT ToggleActiveMenuId = 2022;        // 有効状態切替Menu ID
        constexpr UINT DeleteItemMenuId = 2023;          // Tree要素削除Menu ID
        constexpr UINT LoadScriptMenuId = 2024;          // Script DLL読込Menu ID
        constexpr UINT RefreshTreeMenuId = 2025;         // Tree再表示Menu ID
        constexpr UINT AddChildObjectMenuId = 2026;      // 子Object追加Menu ID
        constexpr UINT DetachParentMenuId = 2027;        // 親解除Menu ID
        constexpr UINT ScriptMenuBase = 3000;            // Script Factory Menu IDの基点
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
        constexpr int EditorInnerSplitterSize = 7;      // Tab内分割線の論理幅又は高さ
        constexpr float DefaultEngineSplitRatio = 0.57f; // Engine Page内Object Tree高さ比率
        constexpr float DefaultProgramHorizontalRatio = 0.25f; // Program一覧領域の高さ比率
        constexpr float DefaultProgramVerticalRatio = 0.48f; // Program File一覧の横幅比率
        constexpr const wchar_t* ProgramCompletionWords[] =
        {
            L"alignas", L"alignof", L"auto", L"bool", L"break", L"case",
            L"catch", L"char", L"class", L"const", L"constexpr", L"continue",
            L"default", L"delete", L"do", L"double", L"else", L"enum",
            L"explicit", L"extern", L"false", L"float", L"for", L"if",
            L"inline", L"int", L"namespace", L"new", L"noexcept", L"nullptr",
            L"private", L"protected", L"public", L"return", L"sizeof", L"static",
            L"struct", L"switch", L"template", L"this", L"throw", L"true",
            L"try", L"using", L"virtual", L"void", L"volatile", L"while",
            L"std", L"uint32_t", L"uint64_t", L"EngineHostAPI",
            L"EngineExternalObjectType", L"EngineExternalVector3", L"EngineExternalTransform",
            L"EngineExternalSceneInfo", L"EngineExternalObjectInfo",
            L"EngineExternalComponentInfo", L"EngineExternalScriptInfo",
            L"EngineExternalGameObjectTemplateInfo", L"EngineExtensionModuleDescriptor",
            L"EngineExtensionAbiVersion", L"ENGINE_EXTENSION_CALL",
            L"ENGINE_EXTENSION_EXPORT", L"Context", L"AddLog", L"GetFrameNumber",
            L"GetDeltaTime", L"GetSceneCount", L"GetSceneInfo", L"SetSceneActive",
            L"SetViewScene", L"GetObjectCount", L"GetObjectInfo", L"CreateObject",
            L"RemoveObject", L"RenameObject", L"SetObjectActive",
            L"SetObjectTransform", L"SetObjectParent", L"GetComponentCount",
            L"GetComponentInfo", L"RemoveComponent", L"RenameComponent",
            L"SetComponentActive", L"ModuleName", L"ModuleVersion", L"Create",
            L"Destroy", L"Update", L"GetStateSize", L"SaveState", L"LoadState",
            L"GetEngineRevision", L"GetMainSceneID", L"GetViewSceneID",
            L"FindSceneByName", L"CreateScene", L"DuplicateScene", L"RemoveScene",
            L"SetMainScene", L"FindObjectByName", L"GetChildCount",
            L"GetChildObjectID", L"DuplicateObject", L"CreateGameObjectTemplate",
            L"GetGameObjectTemplateInfo", L"SetGameObjectTemplateInfo",
            L"GetScriptCount", L"GetScriptInfo", L"AttachScript",
            L"EngineExternalColor", L"GetObjectColor", L"SetObjectColor",
            L"IsKeyDown", L"EngineScriptHostAPI", L"EngineScriptDescriptor",
            L"EngineScriptModuleDescriptor", L"EngineScriptAbiVersion",
            L"ENGINE_SCRIPT_CALL", L"ENGINE_SCRIPT_EXPORT", L"GetObjectID",
            L"GetActive", L"SetActive", L"GetPosition", L"SetPosition",
            L"GetRotation", L"SetRotation", L"GetScale", L"SetScale",
            L"GetObjectType", L"GetColor", L"SetColor", L"TypeKey",
            L"DisplayName", L"ScriptCount", L"Scripts", L"OnAttach",
            L"OnStart", L"OnUpdate", L"OnStop", L"OnDetach",
            L"GameKey", L"GameObjectType", L"Float3", L"Color4",
            L"ObjectScript", L"PositionProperty", L"ColorProperty",
            L"GetKeyPress", L"IsObjectType", L"Move", L"MoveWhenPressed",
            L"SetColorWhenPressed", L"MakeObjectScriptDescriptor",
            L"MultiplyColor", L"MultiplyColorWhenPressed",
            L"MultiplyObjectColor", L"SetProgramSuggestion",
            L"Init", L"Update", L"End", L"ENGINE_REGISTER_SCENE",
            L"CreateCapsuleModel", L"CreateBox", L"CreateSphere",
            L"SetSize", L"SetPosition", L"SetTransform", L"SetColor",
            L"CreateMany", L"CreateBoxes", L"CreateCapsules", L"ObjectHandle",
            L"ComponentHandle", L"Find", L"FindAll", L"FindByType",
            L"FindByComponent", L"FindByScript", L"GetComponent", L"GetComponents",
            L"HasComponent", L"GetID", L"GetName", L"GetType", L"IsValid",
            L"SetActive", L"Remove", L"Exists", L"AttachScript", L"Advanced",
            L"StartDestroy", L"EndDestroy", L"ENGINE_REGISTER_SCENE_LIFECYCLE",
            L"LeftArrow", L"UpArrow", L"RightArrow", L"DownArrow",
            L"KeyA", L"KeyB", L"KeyD", L"KeyG", L"KeyR", L"KeyS", L"KeyW"
        }; //Program補完へ常時登録するC++及び外部Engine API識別子

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

        //概要：UTF-8文字列をWindows Control用UTF-16へ変換する
        //引数：text=変換するUTF-8文字列
        //戻り値：変換済みUTF-16文字列、失敗時は代替文字列
        std::wstring ConvertEditorTextToWide(const std::string& text)
        {
            if (text.empty())
            {
                return std::wstring();
            }

            int RequiredLength = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0
            ); //変換後に必要なUTF-16文字数
            UINT CodePage = CP_UTF8; //UTF-8失敗時に外部ANSI SourceへFallbackするCode Page
            DWORD Flags = MB_ERR_INVALID_CHARS; //現在Code Pageで使う検証Flag

            if (RequiredLength <= 0)
            {
                CodePage = CP_ACP;
                Flags = 0;
                RequiredLength = MultiByteToWideChar(
                    CodePage,
                    Flags,
                    text.data(),
                    static_cast<int>(text.size()),
                    nullptr,
                    0
                );
            }

            if (RequiredLength <= 0)
            {
                return L"<invalid source text>";
            }

            std::wstring Result(
                static_cast<std::size_t>(RequiredLength),
                L'\0'
            ); //UTF-16出力Buffer
            MultiByteToWideChar(
                CodePage,
                Flags,
                text.data(),
                static_cast<int>(text.size()),
                Result.data(),
                RequiredLength
            );
            return Result;
        }

        //概要：Windows ControlのUTF-16文字列をEngine用UTF-8へ変換する
        //引数：text=変換するUTF-16文字列
        //戻り値：変換済みUTF-8文字列、失敗時は空文字列
        std::string ConvertEditorTextToUtf8(const wchar_t* text)
        {
            if (text == nullptr || *text == L'\0')
            {
                return std::string();
            }

            const int SourceLength = static_cast<int>(std::wcslen(text)); //終端を除く入力文字数
            const int RequiredLength = WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                text,
                SourceLength,
                nullptr,
                0,
                nullptr,
                nullptr
            ); //変換後に必要なUTF-8 Byte数

            if (RequiredLength <= 0)
            {
                return std::string();
            }

            std::string Result(
                static_cast<std::size_t>(RequiredLength),
                '\0'
            ); //UTF-8出力Buffer
            WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                text,
                SourceLength,
                Result.data(),
                RequiredLength,
                nullptr,
                nullptr
            );
            return Result;
        }

        //概要：Object種別を追加Menuの日本語表示名へ変換する
        //引数：type=変換するObject種別
        //戻り値：Object種別の表示名
        const wchar_t* GetObjectMenuName(ObjectType type)
        {
            switch (type)
            {
            case ObjectType::Object: return L"空のObject";
            case ObjectType::Box: return L"Box";
            case ObjectType::Sphere: return L"Sphere";
            case ObjectType::Plane: return L"Plane";
            case ObjectType::Cylinder: return L"Cylinder";
            case ObjectType::HalfSphere: return L"HalfSphere";
            case ObjectType::Capsule: return L"Capsule";
            default: return L"Unknown";
            }
        }
    }

    // Windowsエディターウィンドウを未生成の状態で作成する
    WinApp::WinApp()
        : Hwnd(nullptr)
        , RenderHwnd(nullptr)
        , PanelHwnd(nullptr)
        , TitleLabelHwnd(nullptr)
        , EditorTabHwnd(nullptr)
        , SceneSelectorHwnd(nullptr)
        , ObjectTreeHwnd(nullptr)
        , AddObjectButtonHwnd(nullptr)
        , AddScriptButtonHwnd(nullptr)
        , LoadScriptButtonHwnd(nullptr)
        , ObjectNameLabelHwnd(nullptr)
        , ObjectNameEditHwnd(nullptr)
        , ObjectActiveCheckHwnd(nullptr)
        , ObjectParentLabelHwnd(nullptr)
        , TransformLabelsHwnd{}
        , TransformEditsHwnd{}
        , ApplyObjectButtonHwnd(nullptr)
        , StartButtonHwnd(nullptr)
        , PauseButtonHwnd(nullptr)
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
        , ProgramFileListHwnd(nullptr)
        , ProgramFunctionListHwnd(nullptr)
        , ProgramEditorHwnd(nullptr)
        , ProgramSuggestionListHwnd(nullptr)
        , ProgramErrorListHwnd(nullptr)
        , ProgramFileNameEditHwnd(nullptr)
        , NewProgramButtonHwnd(nullptr)
        , RenameProgramButtonHwnd(nullptr)
        , DeleteProgramButtonHwnd(nullptr)
        , SaveProgramButtonHwnd(nullptr)
        , CompileProgramButtonHwnd(nullptr)
        , RestoreProgramButtonHwnd(nullptr)
        , ProgramStatusLabelHwnd(nullptr)
        , ProgramRoleLabelHwnd(nullptr)
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
        , PauseRequested(false)
        , StopRequested(false)
        , ResizeRequested(false)
        , SplitDragging(false)
        , EngineSplitDragging(false)
        , ProgramHorizontalSplitDragging(false)
        , ProgramVerticalSplitDragging(false)
        , TreeDragging(false)
        , CurrentPlaybackState(PlaybackState::Stopped)
        , UpdatingFrameRate(false)
        , ClearLogsRequested(false)
        , ClassRegistered(false)
        , UpdatingProgramEditor(false)
        , ProgramDirty(false)
        , ProgramWorkspaceReady(false)
        , ProgramCompilePending(false)
        , ProgramPendingManualCompile(false)
        , ProgramVisualRefreshPending(false)
        , SuppressProgramCharacter(false)
        , EditingScriptWorkspace(false)
        , ActiveTabIndex(0)
        , EditorSplitDragOffset(0)
        , EngineSplitRatio(DefaultEngineSplitRatio)
        , ProgramHorizontalSplitRatio(DefaultProgramHorizontalRatio)
        , ProgramVerticalSplitRatio(DefaultProgramVerticalRatio)
        , ClassName(L"DX12EngineEditorWindowClass")
        , SelectedEditorSceneID()
        , DraggedTreeNode()
        , Programs(ProgramWorkspaceKind::MainProgram)
        , ScriptPrograms(ProgramWorkspaceKind::ObjectScript)
        , LastProgramEditTime(std::chrono::steady_clock::now())
        , ProgramEditRevision(0)
        , ProgramSavedRevision(0)
        , ProgramRequestedRevision(0)
        , ExternalSuggestionRevision(0)
        , RichEditModule(nullptr)
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
        CommonControlInformation.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES |
            ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES;

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

        InitializeProgramWorkspace();
        const UINT_PTR AutomationTimer = SetTimer(
            Hwnd,
            ProgramAutomationTimerId,
            ProgramAutomationTimerInterval,
            nullptr
        ); //自動保存とCompile完了監視Timer作成結果

        if (AutomationTimer == 0)
        {
            AddWin32FailureLog("SetTimer for Program automation");
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
        Programs.Shutdown();
        ScriptPrograms.Shutdown();

        if (ProgramEditorHwnd != nullptr)
        {
            RemoveWindowSubclass(
                ProgramEditorHwnd,
                ProgramEditorSubclassProc,
                ProgramEditorSubclassId
            );
        }

        if (Hwnd != nullptr)
        {
            KillTimer(Hwnd, ProgramAutomationTimerId);
            DestroyWindow(Hwnd);
            Hwnd = nullptr;
        }

        RenderHwnd = nullptr;
        PanelHwnd = nullptr;
        TitleLabelHwnd = nullptr;
        EditorTabHwnd = nullptr;
        SceneSelectorHwnd = nullptr;
        ObjectTreeHwnd = nullptr;
        AddObjectButtonHwnd = nullptr;
        AddScriptButtonHwnd = nullptr;
        LoadScriptButtonHwnd = nullptr;
        ObjectNameLabelHwnd = nullptr;
        ObjectNameEditHwnd = nullptr;
        ObjectActiveCheckHwnd = nullptr;
        ObjectParentLabelHwnd = nullptr;
        std::fill(std::begin(TransformLabelsHwnd), std::end(TransformLabelsHwnd), nullptr);
        std::fill(std::begin(TransformEditsHwnd), std::end(TransformEditsHwnd), nullptr);
        ApplyObjectButtonHwnd = nullptr;
        StartButtonHwnd = nullptr;
        PauseButtonHwnd = nullptr;
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
        ProgramFileListHwnd = nullptr;
        ProgramFunctionListHwnd = nullptr;
        ProgramEditorHwnd = nullptr;
        ProgramSuggestionListHwnd = nullptr;
        ProgramErrorListHwnd = nullptr;
        ProgramFileNameEditHwnd = nullptr;
        NewProgramButtonHwnd = nullptr;
        RenameProgramButtonHwnd = nullptr;
        DeleteProgramButtonHwnd = nullptr;
        SaveProgramButtonHwnd = nullptr;
        CompileProgramButtonHwnd = nullptr;
        RestoreProgramButtonHwnd = nullptr;
        ProgramStatusLabelHwnd = nullptr;
        ProgramRoleLabelHwnd = nullptr;

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
        EngineSplitDragging = false;
        ProgramHorizontalSplitDragging = false;
        ProgramVerticalSplitDragging = false;
        TreeDragging = false;
        StartRequested = false;
        PauseRequested = false;
        StopRequested = false;
        PendingTickCount = 0;
        CurrentPlaybackState = PlaybackState::Stopped;
        ClearLogsRequested = false;
        ActiveTabIndex = 0;
        CurrentEditorSnapshot = EditorSnapshot{};
        SelectedEditorSceneID = SceneID();
        SceneSelectorIDs.clear();
        EditorTreeNodes.clear();
        PendingEditorCommands.clear();
        ProgramFiles.clear();
        CurrentProgramPath.clear();
        ProgramFunctions.clear();
        ProgramSuggestions.clear();
        ProgramDirty = false;
        ProgramWorkspaceReady = false;
        ProgramCompilePending = false;
        ProgramPendingManualCompile = false;
        ProgramVisualRefreshPending = false;
        SuppressProgramCharacter = false;
        EditingScriptWorkspace = false;
        ProgramEditRevision = 0;
        ProgramSavedRevision = 0;
        ProgramRequestedRevision = 0;
        ExternalSuggestionRevision = 0;

        if (RichEditModule != nullptr)
        {
            FreeLibrary(RichEditModule);
            RichEditModule = nullptr;
        }
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

    //概要：親Editor Window Handleを取得する
    //引数：なし
    //戻り値：親Window Handle、未作成時はnullptr
    HWND WinApp::GetHWND() const
    {
        return Hwnd;
    }

    //概要：DirectX 12描画対象の子Window Handleを取得する
    //引数：なし
    //戻り値：描画用子Window Handle、未作成時はnullptr
    HWND WinApp::GetRenderHwnd() const
    {
        return RenderHwnd;
    }

    //概要：Editor Windowを所有するProcess Instanceを取得する
    //引数：なし
    //戻り値：Module Instance Handle、未作成時はnullptr
    HINSTANCE WinApp::GetInstance() const
    {
        return Instance;
    }

    //概要：親Editor Client領域の現在幅を取得する
    //引数：なし
    //戻り値：物理Pixel単位のClient幅
    uint32_t WinApp::GetWidth() const
    {
        return Width;
    }

    //概要：親Editor Client領域の現在高さを取得する
    //引数：なし
    //戻り値：物理Pixel単位のClient高さ
    uint32_t WinApp::GetHeight() const
    {
        return Height;
    }

    //概要：DirectX 12描画用子Windowの現在幅を取得する
    //引数：なし
    //戻り値：物理Pixel単位の描画幅
    uint32_t WinApp::GetRenderWidth() const
    {
        return RenderWidth;
    }

    //概要：DirectX 12描画用子Windowの現在高さを取得する
    //引数：なし
    //戻り値：物理Pixel単位の描画高さ
    uint32_t WinApp::GetRenderHeight() const
    {
        return RenderHeight;
    }

    //概要：DirectX 12描画用子Windowの現在寸法をまとめて取得する
    //引数：なし
    //戻り値：物理Pixel単位の幅と高さ
    RenderWindowSize WinApp::GetRenderSize() const
    {
        return RenderWindowSize
        {
            RenderWidth,
            RenderHeight
        };
    }

    //概要：EditとSliderが示す目標Frame Rateを取得する
    //引数：なし
    //戻り値：1から240の目標FPS
    uint32_t WinApp::GetTargetFrameRate() const
    {
        return TargetFrameRate;
    }

    //概要：現在適用中のWindows UI設定を取得する
    //引数：なし
    //戻り値：読み取り専用UI設定
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

    //概要：Engine APIのScene、Object、Component、Script一覧をObject Treeへ反映する
    //引数：snapshot=Engine APIが作成した読み取り専用状態
    //戻り値：なし
    void WinApp::UpdateEditorSnapshot(const EditorSnapshot& snapshot)
    {
        const bool MustRebuild = CurrentEditorSnapshot.Revision != snapshot.Revision ||
            CurrentEditorSnapshot.Scenes.size() != snapshot.Scenes.size() ||
            CurrentEditorSnapshot.Scripts.size() != snapshot.Scripts.size(); //Tree再構築が必要な場合true
        CurrentEditorSnapshot = snapshot;

        if (MustRebuild)
        {
            RebuildSceneSelector();
            RebuildObjectTree();
            UpdateObjectInspector();

            if (ProgramWorkspaceReady)
            {
                const std::size_t PreviousFileCount = Programs.GetSourceFiles().size(); //生成前のProgram数

                for (const EditorSceneInfo& Scene : CurrentEditorSnapshot.Scenes)
                {
                    std::filesystem::path SceneSource; //Sceneに対応するMain Program Path
                    Programs.EnsureSceneSource(Scene.Name, SceneSource);
                }

                if (Programs.GetSourceFiles().size() != PreviousFileCount)
                {
                    RefreshProgramFiles(CurrentProgramPath);
                    ProgramCompilePending = true;
                    LastProgramEditTime = std::chrono::steady_clock::now();

                    if (ProgramEditRevision < (std::numeric_limits<std::uint64_t>::max)())
                    {
                        ++ProgramEditRevision;
                    }
                }
            }
        }
    }

    //概要：エディターで発生したObject又はScript操作をFIFO順で1件取得する
    //引数：command=取得した操作要求の格納先
    //戻り値：未処理要求が存在した場合はtrue
    bool WinApp::ConsumeEditorCommand(EditorCommand& command)
    {
        if (PendingEditorCommands.empty())
        {
            return false;
        }

        command = std::move(PendingEditorCommands.front());
        PendingEditorCommands.pop_front();
        return true;
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

    //Pauseボタンの未処理イベントを1件取得する
    bool WinApp::ConsumePause()
    {
        const bool WasRequested = PauseRequested;
        PauseRequested = false;
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

    //概要：Program RichEditの補完表示、候補移動、確定Keyを処理する
    //引数：hwnd=Program入力欄、message=Windows Message、wparam=追加情報、lparam=追加情報、subclassID=Subclass識別子、referenceData=WinApp Pointer
    //戻り値：補完で消費した場合は0、それ以外はRichEdit既定処理結果
    LRESULT CALLBACK WinApp::ProgramEditorSubclassProc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam,
        UINT_PTR subclassID,
        DWORD_PTR referenceData
    )
    {
        WinApp* Application = reinterpret_cast<WinApp*>(referenceData); //補完状態を所有するWinApp

        if (Application == nullptr)
        {
            return DefSubclassProc(hwnd, message, wparam, lparam);
        }

        if (message == WM_CHAR && Application->SuppressProgramCharacter)
        {
            Application->SuppressProgramCharacter = false;
            return 0;
        }

        if (message == WM_KEYDOWN)
        {
            const bool SuggestionsVisible = Application->ProgramSuggestionListHwnd != nullptr &&
                IsWindowVisible(Application->ProgramSuggestionListHwnd); //候補一覧表示中の場合true
            const bool ControlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0; //編集ShortcutのControl Key状態

            if (ControlPressed && (wparam == L'Z' || wparam == L'Y'))
            {
                Application->HideProgramSuggestions();
                const bool RedoRequested = wparam == L'Y' ||
                    ((GetKeyState(VK_SHIFT) & 0x8000) != 0); //Ctrl+Y又はCtrl+Shift+Zの場合true
                SendMessageW(hwnd, RedoRequested ? EM_REDO : WM_UNDO, 0, 0);
                return 0;
            }

            if (wparam == VK_SPACE && ControlPressed)
            {
                Application->SuppressProgramCharacter = true;
                Application->UpdateProgramSuggestions(true);
                return 0;
            }

            if (SuggestionsVisible && (wparam == VK_UP || wparam == VK_DOWN ||
                wparam == VK_PRIOR || wparam == VK_NEXT))
            {
                const LRESULT Count = SendMessageW(
                    Application->ProgramSuggestionListHwnd,
                    LB_GETCOUNT,
                    0,
                    0
                ); //表示中候補数
                LRESULT Selection = SendMessageW(
                    Application->ProgramSuggestionListHwnd,
                    LB_GETCURSEL,
                    0,
                    0
                ); //現在候補Index

                if (Count > 0)
                {
                    const LRESULT Step = wparam == VK_PRIOR
                        ? -5
                        : (wparam == VK_NEXT ? 5 : (wparam == VK_UP ? -1 : 1)); //Key別移動量
                    Selection = std::clamp<LRESULT>(Selection + Step, 0, Count - 1);
                    SendMessageW(
                        Application->ProgramSuggestionListHwnd,
                        LB_SETCURSEL,
                        Selection,
                        0
                    );
                }

                return 0;
            }

            if (SuggestionsVisible && wparam == VK_TAB)
            {
                Application->SuppressProgramCharacter = true;
                Application->ApplyProgramSuggestion();
                return 0;
            }

            if (SuggestionsVisible && wparam == VK_RETURN)
            {
                Application->HideProgramSuggestions();
            }

            if (SuggestionsVisible && wparam == VK_ESCAPE)
            {
                Application->HideProgramSuggestions();
                return 0;
            }
        }

        const LRESULT Result = DefSubclassProc(hwnd, message, wparam, lparam); //RichEdit既定処理結果

        if (message == WM_KEYUP && Application->SuppressProgramCharacter &&
            (wparam == VK_SPACE || wparam == VK_TAB))
        {
            Application->SuppressProgramCharacter = false;
        }

        if (message == WM_CHAR)
        {
            const wchar_t Character = static_cast<wchar_t>(wparam); //候補更新を判断する入力文字

            if (std::iswalnum(Character) || Character == L'_' || Character == L'\b')
            {
                Application->UpdateProgramSuggestions(false);
            }
            else if (Character == L'.' || Character == L'>')
            {
                Application->UpdateProgramSuggestions(true);
            }
            else
            {
                Application->HideProgramSuggestions();
            }
        }
        else if (message == WM_LBUTTONUP ||
            (message == WM_KEYUP && (wparam == VK_DELETE || wparam == VK_LEFT ||
                wparam == VK_RIGHT || wparam == VK_HOME || wparam == VK_END)))
        {
            Application->UpdateProgramSuggestions(false);
        }
        else if (message == WM_KILLFOCUS &&
            reinterpret_cast<HWND>(wparam) != Application->ProgramSuggestionListHwnd)
        {
            Application->HideProgramSuggestions();
        }
        else if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, ProgramEditorSubclassProc, subclassID);
        }

        return Result;
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

            if (ControlId == RenderControlId && NotificationCode == STN_CLICKED)
            {
                HideProgramSuggestions();
                SetFocus(RenderHwnd);
                return 0;
            }

            if (ControlId == AddObjectControlId && NotificationCode == BN_CLICKED)
            {
                RECT ButtonRectangle{}; //Menu表示位置に使うObject追加Button矩形
                GetWindowRect(AddObjectButtonHwnd, &ButtonRectangle);
                ShowCreateObjectMenu(
                    POINT{ ButtonRectangle.left, ButtonRectangle.bottom },
                    false
                );
                return 0;
            }

            if (ControlId == SceneSelectorControlId && NotificationCode == CBN_SELCHANGE)
            {
                const LRESULT Selection = SendMessageW(
                    SceneSelectorHwnd,
                    CB_GETCURSEL,
                    0,
                    0
                ); //利用者が選択したScene Combo Index

                if (Selection >= 0 &&
                    static_cast<std::size_t>(Selection) < SceneSelectorIDs.size())
                {
                    SelectedEditorSceneID = SceneSelectorIDs[
                        static_cast<std::size_t>(Selection)
                    ];
                    RebuildObjectTree();
                    UpdateObjectInspector();
                    EditorCommand Command; //View Scene切替要求
                    Command.Type = EditorCommandType::SetViewScene;
                    Command.Scene = SelectedEditorSceneID;
                    QueueEditorCommand(std::move(Command));
                }

                return 0;
            }

            if (ControlId == ApplyObjectControlId && NotificationCode == BN_CLICKED)
            {
                ApplyObjectInspector();
                return 0;
            }

            if (ControlId == AddScriptControlId && NotificationCode == BN_CLICKED)
            {
                RECT ButtonRectangle{}; //Menu表示位置に使うScript差込Button矩形
                GetWindowRect(AddScriptButtonHwnd, &ButtonRectangle);
                ShowScriptMenu(POINT{ ButtonRectangle.left, ButtonRectangle.bottom });
                return 0;
            }

            if (ControlId == LoadScriptControlId && NotificationCode == BN_CLICKED)
            {
                OpenScriptModuleDialog();
                return 0;
            }

            if (ControlId == StartControlId && NotificationCode == BN_CLICKED)
            {
                StartRequested = true;
                PauseRequested = false;
                StopRequested = false;
                PendingTickCount = 0;
                CurrentPlaybackState = PlaybackState::Playing;
                UpdatePlaybackControls();
                return 0;
            }

            if (ControlId == PauseControlId && NotificationCode == BN_CLICKED)
            {
                PauseRequested = true;
                StartRequested = false;
                StopRequested = false;
                PendingTickCount = 0;
                CurrentPlaybackState = PlaybackState::Paused;
                UpdatePlaybackControls();
                return 0;
            }

            if (ControlId == StopControlId && NotificationCode == BN_CLICKED)
            {
                StopRequested = true;
                StartRequested = false;
                PauseRequested = false;
                PendingTickCount = 0;
                CurrentPlaybackState = PlaybackState::Stopped;
                UpdatePlaybackControls();
                return 0;
            }

            if (ControlId == TickControlId && NotificationCode == BN_CLICKED)
            {
                if (CurrentPlaybackState != PlaybackState::Playing &&
                    PendingTickCount < std::numeric_limits<uint32_t>::max())
                {
                    ++PendingTickCount;
                    CurrentPlaybackState = PlaybackState::Paused;
                    UpdatePlaybackControls();
                }

                return 0;
            }

            if (ControlId == ClearLogsControlId && NotificationCode == BN_CLICKED)
            {
                ClearLogsRequested = true;
                return 0;
            }

            if (ControlId == ProgramFileListControlId && NotificationCode == LBN_SELCHANGE)
            {
                if (ProgramDirty)
                {
                    SaveCurrentProgram();
                }

                LoadSelectedProgram();
                return 0;
            }

            if (ControlId == ProgramFunctionListControlId && NotificationCode == LBN_SELCHANGE)
            {
                const LRESULT Selection = SendMessageW(
                    ProgramFunctionListHwnd,
                    LB_GETCURSEL,
                    0,
                    0
                ); //選択された関数一覧Index

                if (Selection >= 0 &&
                    static_cast<std::size_t>(Selection) < ProgramFunctions.size())
                {
                    const long CharacterIndex = ProgramFunctions[
                        static_cast<std::size_t>(Selection)
                    ].CharacterIndex; //移動先関数名の文字位置
                    SendMessageW(ProgramEditorHwnd, EM_SETSEL, CharacterIndex, CharacterIndex);
                    SendMessageW(ProgramEditorHwnd, EM_SCROLLCARET, 0, 0);
                    SetFocus(ProgramEditorHwnd);
                }

                return 0;
            }

            if (ControlId == ProgramEditorControlId && NotificationCode == EN_CHANGE &&
                !UpdatingProgramEditor)
            {
                ProgramDirty = true;
                ProgramCompilePending = true;
                LastProgramEditTime = std::chrono::steady_clock::now();

                if (ProgramEditRevision < (std::numeric_limits<std::uint64_t>::max)())
                {
                    ++ProgramEditRevision;
                }

                SetWindowTextW(
                    ProgramStatusLabelHwnd,
                    L"変更を検出しました（自動保存待ち）"
                );

                ProgramVisualRefreshPending = true;

                return 0;
            }

            if (ControlId == ProgramSuggestionListControlId &&
                NotificationCode == LBN_DBLCLK)
            {
                ApplyProgramSuggestion();
                return 0;
            }

            if (ControlId == ProgramSuggestionListControlId &&
                NotificationCode == LBN_SELCHANGE &&
                GetFocus() == ProgramSuggestionListHwnd)
            {
                ApplyProgramSuggestion();
                return 0;
            }

            if (ControlId == ProgramSuggestionListControlId &&
                NotificationCode == LBN_KILLFOCUS &&
                GetFocus() != ProgramEditorHwnd)
            {
                HideProgramSuggestions();
                return 0;
            }

            if (ControlId == NewProgramControlId && NotificationCode == BN_CLICKED)
            {
                if (ProgramDirty)
                {
                    SaveCurrentProgram();
                }

                std::filesystem::path CreatedPath; //新規作成されたProgram Path

                if (GetActiveProgramWorkspace().CreateSourceFile(CreatedPath))
                {
                    RefreshProgramFiles(CreatedPath);
                    ProgramCompilePending = true;
                    LastProgramEditTime = std::chrono::steady_clock::now();

                    if (ProgramEditRevision < (std::numeric_limits<std::uint64_t>::max)())
                    {
                        ++ProgramEditRevision;
                    }

                    ProgramSavedRevision = ProgramEditRevision;
                }

                return 0;
            }

            if (ControlId == RenameProgramControlId && NotificationCode == BN_CLICKED)
            {
                RenameCurrentProgram();
                return 0;
            }

            if (ControlId == DeleteProgramControlId && NotificationCode == BN_CLICKED)
            {
                DeleteCurrentProgram();
                return 0;
            }

            if (ControlId == SaveProgramControlId && NotificationCode == BN_CLICKED)
            {
                SaveCurrentProgram();
                return 0;
            }

            if (ControlId == CompileProgramControlId && NotificationCode == BN_CLICKED)
            {
                CompilePrograms();
                return 0;
            }

            if (ControlId == RestoreProgramControlId && NotificationCode == BN_CLICKED)
            {
                RestoreLastSuccessfulProgram();
                return 0;
            }

            if (ControlId == ProgramErrorListControlId && NotificationCode == LBN_DBLCLK)
            {
                const LRESULT Selection = SendMessageW(
                    ProgramErrorListHwnd,
                    LB_GETCURSEL,
                    0,
                    0
                ); //選択されたCompile出力行

                if (Selection >= 0)
                {
                    const LRESULT Length = SendMessageW(
                        ProgramErrorListHwnd,
                        LB_GETTEXTLEN,
                        Selection,
                        0
                    ); //Compile出力行の文字数
                    std::wstring Diagnostic(
                        Length > 0 ? static_cast<std::size_t>(Length) + 1 : 1,
                        L'\0'
                    ); //Compile出力行Buffer
                    SendMessageW(
                        ProgramErrorListHwnd,
                        LB_GETTEXT,
                        Selection,
                        reinterpret_cast<LPARAM>(Diagnostic.data())
                    );
                    Diagnostic.resize(std::wcslen(Diagnostic.c_str()));

                    for (std::size_t Index = 0; Index < ProgramFiles.size(); ++Index)
                    {
                        const std::wstring FileName = ProgramFiles[Index].filename().wstring(); //検索するSource名

                        if (Diagnostic.find(FileName) != std::wstring::npos)
                        {
                            SendMessageW(ProgramFileListHwnd, LB_SETCURSEL, Index, 0);
                            LoadSelectedProgram();
                            break;
                        }
                    }

                    const std::size_t Open = Diagnostic.find(L'('); //MSBuild診断の行番号開始位置

                    if (Open != std::wstring::npos)
                    {
                        wchar_t* End = nullptr; //行番号変換後位置
                        const long Line = std::wcstol(Diagnostic.c_str() + Open + 1, &End, 10);

                        if (Line > 0)
                        {
                            const LRESULT CharacterIndex = SendMessageW(
                                ProgramEditorHwnd,
                                EM_LINEINDEX,
                                Line - 1,
                                0
                            ); //診断行の文字位置
                            SendMessageW(ProgramEditorHwnd, EM_SETSEL, CharacterIndex, CharacterIndex);
                            SendMessageW(ProgramEditorHwnd, EM_SCROLLCARET, 0, 0);
                            SetFocus(ProgramEditorHwnd);
                        }
                    }
                }

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

        case WM_TIMER:
            if (wparam == ProgramAutomationTimerId)
            {
                ProcessProgramAutomation();
                return 0;
            }
            break;

        case WM_NOTIFY:
        {
            const NMHDR* Header = reinterpret_cast<const NMHDR*>(lparam); //通知元Control情報

            if (Header == nullptr)
            {
                break;
            }

            if (Header->hwndFrom == EditorTabHwnd && Header->code == TCN_SELCHANGE)
            {
                ActiveTabIndex = TabCtrl_GetCurSel(EditorTabHwnd);

                if (ActiveTabIndex == 3 || ActiveTabIndex == 4)
                {
                    SwitchProgramWorkspace(ActiveTabIndex == 4);
                }

                UpdateTabVisibility();
                LayoutControls();
                return 0;
            }

            if (Header->hwndFrom == ObjectTreeHwnd && Header->code == NM_RCLICK)
            {
                POINT ScreenPosition{}; //Context Menuを表示する画面座標
                GetCursorPos(&ScreenPosition);
                POINT ClientPosition = ScreenPosition; //Tree Hit Test用Client座標
                ScreenToClient(ObjectTreeHwnd, &ClientPosition);

                TVHITTESTINFO HitTest{}; //右Click位置にあるTree Itemの検索情報
                HitTest.pt = ClientPosition;
                TreeView_HitTest(ObjectTreeHwnd, &HitTest);
                TreeView_SelectItem(ObjectTreeHwnd, HitTest.hItem);
                ShowTreeContextMenu(ScreenPosition);
                return 0;
            }

            if (Header->hwndFrom == ObjectTreeHwnd && Header->code == TVN_SELCHANGEDW)
            {
                UpdateObjectInspector();
                return 0;
            }

            if (Header->hwndFrom == ObjectTreeHwnd && Header->code == TVN_BEGINDRAGW)
            {
                const NMTREEVIEWW* DragInformation = reinterpret_cast<const NMTREEVIEWW*>(
                    lparam
                ); //Tree Drag開始情報
                const EditorTreeNode* Node = DragInformation == nullptr
                    ? nullptr
                    : GetTreeNode(DragInformation->itemNew.hItem); //Drag対象Object情報

                if (Node != nullptr && Node->Kind == EditorTreeNodeKind::Object)
                {
                    DraggedTreeNode = *Node;
                    TreeDragging = true;
                    SetCapture(Hwnd);
                }

                return 0;
            }

            if (Header->hwndFrom == ObjectTreeHwnd && Header->code == TVN_BEGINLABELEDITW)
            {
                const EditorTreeNode* Node = GetSelectedTreeNode(); //名前編集を開始するEngine要素

                if (Node == nullptr || Node->Kind == EditorTreeNodeKind::Scene)
                {
                    return TRUE;
                }

                std::string CurrentName; //装飾を除いてEditへ設定する現在名

                for (const EditorSceneInfo& Scene : CurrentEditorSnapshot.Scenes)
                {
                    if (Scene.ID != Node->Scene)
                    {
                        continue;
                    }

                    for (const EditorObjectInfo& Object : Scene.Objects)
                    {
                        if (Object.ID != Node->Object)
                        {
                            continue;
                        }

                        if (Node->Kind == EditorTreeNodeKind::Object)
                        {
                            CurrentName = Object.Name;
                            break;
                        }

                        for (const EditorComponentInfo& Component : Object.Components)
                        {
                            if (Component.ID == Node->Component)
                            {
                                CurrentName = Component.Name;
                                break;
                            }
                        }
                    }
                }

                HWND EditControl = TreeView_GetEditControl(ObjectTreeHwnd); //Treeが作成したLabel Edit

                if (EditControl != nullptr && !CurrentName.empty())
                {
                    const std::wstring WideName = ConvertEditorTextToWide(CurrentName); //UTF-16の現在名
                    SetWindowTextW(EditControl, WideName.c_str());
                    SendMessageW(EditControl, EM_SETSEL, 0, -1);
                }

                return FALSE;
            }

            if (Header->hwndFrom == ObjectTreeHwnd && Header->code == TVN_ENDLABELEDITW)
            {
                const NMTVDISPINFOW* EditInformation =
                    reinterpret_cast<const NMTVDISPINFOW*>(lparam); //Label編集結果

                if (EditInformation == nullptr || EditInformation->item.pszText == nullptr ||
                    EditInformation->item.lParam <= 0)
                {
                    return FALSE;
                }

                const std::size_t NodeIndex = static_cast<std::size_t>(
                    EditInformation->item.lParam - 1
                ); //Tree Itemに対応するNode表位置

                if (NodeIndex >= EditorTreeNodes.size())
                {
                    return FALSE;
                }

                const EditorTreeNode& Node = EditorTreeNodes[NodeIndex]; //名前を変更するEngine要素
                EditorCommand Command; //EngineAPIへ渡す名前変更要求
                Command.Scene = Node.Scene;
                Command.Object = Node.Object;
                Command.Component = Node.Component;
                Command.Text = ConvertEditorTextToUtf8(EditInformation->item.pszText);

                if (Command.Text.empty() || Node.Kind == EditorTreeNodeKind::Scene)
                {
                    return FALSE;
                }

                Command.Type = Node.Kind == EditorTreeNodeKind::Object
                    ? EditorCommandType::RenameObject
                    : EditorCommandType::RenameComponent;
                QueueEditorCommand(std::move(Command));
                return TRUE;
            }

            if (Header->hwndFrom == ObjectTreeHwnd && Header->code == TVN_KEYDOWN)
            {
                const NMTVKEYDOWN* KeyInformation =
                    reinterpret_cast<const NMTVKEYDOWN*>(lparam); //Tree Key入力情報
                const EditorTreeNode* Node = GetSelectedTreeNode(); //現在選択中のEngine要素

                if (KeyInformation != nullptr && KeyInformation->wVKey == VK_DELETE &&
                    Node != nullptr && Node->Kind != EditorTreeNodeKind::Scene)
                {
                    EditorCommand Command; //EngineAPIへ渡す削除要求
                    Command.Scene = Node->Scene;
                    Command.Object = Node->Object;
                    Command.Component = Node->Component;
                    Command.Type = Node->Kind == EditorTreeNodeKind::Object
                        ? EditorCommandType::DeleteObject
                        : EditorCommandType::DeleteComponent;
                    QueueEditorCommand(std::move(Command));
                }

                return 0;
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

            if (!UIResources.SetTexturePath(UISettings.TexturePath))
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

            const POINT CursorPoint{ CursorPosition.x, CursorPosition.y }; //Tab内分割線判定座標
            const RECT EngineSplitterRectangle = GetEngineSplitterRect(); //Engine上下分割線矩形
            const RECT ProgramHorizontalRectangle = GetProgramHorizontalSplitterRect(); //Program上下分割線矩形
            const RECT ProgramVerticalRectangle = GetProgramVerticalSplitterRect(); //Program左右分割線矩形

            if ((ActiveTabIndex == 0 && PtInRect(&EngineSplitterRectangle, CursorPoint)) ||
                (IsProgramSourceTab() && PtInRect(&ProgramHorizontalRectangle, CursorPoint)))
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return TRUE;
            }

            if (IsProgramSourceTab() && PtInRect(&ProgramVerticalRectangle, CursorPoint))
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

            const POINT MousePoint{ MouseX, MouseY }; //Editor内分割線判定座標
            const RECT EngineSplitterRectangle = GetEngineSplitterRect(); //Engine内分割線矩形
            const RECT ProgramHorizontalRectangle = GetProgramHorizontalSplitterRect(); //Program上下分割線矩形
            const RECT ProgramVerticalRectangle = GetProgramVerticalSplitterRect(); //Program左右分割線矩形

            if (ActiveTabIndex == 0 && PtInRect(&EngineSplitterRectangle, MousePoint))
            {
                EngineSplitDragging = true;
                EditorSplitDragOffset = MouseY - EngineSplitterRectangle.top;
                SetCapture(Hwnd);
                return 0;
            }

            if (IsProgramSourceTab() && PtInRect(&ProgramHorizontalRectangle, MousePoint))
            {
                ProgramHorizontalSplitDragging = true;
                EditorSplitDragOffset = MouseY - ProgramHorizontalRectangle.top;
                SetCapture(Hwnd);
                return 0;
            }

            if (IsProgramSourceTab() && PtInRect(&ProgramVerticalRectangle, MousePoint))
            {
                ProgramVerticalSplitDragging = true;
                EditorSplitDragOffset = MouseX - ProgramVerticalRectangle.left;
                SetCapture(Hwnd);
                return 0;
            }

            break;
        }

        case WM_MOUSEMOVE:
            if (TreeDragging)
            {
                POINT TreePoint{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; //親Window内Drag位置
                ClientToScreen(Hwnd, &TreePoint);
                ScreenToClient(ObjectTreeHwnd, &TreePoint);
                TVHITTESTINFO HitTest{}; //Drag中に指しているTree Item
                HitTest.pt = TreePoint;
                TreeView_HitTest(ObjectTreeHwnd, &HitTest);

                if (HitTest.hItem != nullptr)
                {
                    TreeView_SelectItem(ObjectTreeHwnd, HitTest.hItem);
                }

                return 0;
            }

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

            if (EngineSplitDragging)
            {
                RECT TreeRectangle{}; //Object Treeの親Window座標矩形
                RECT InspectorRectangle{}; //Inspector末尾の親Window座標矩形
                GetWindowRect(ObjectTreeHwnd, &TreeRectangle);
                GetWindowRect(ApplyObjectButtonHwnd, &InspectorRectangle);
                MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&TreeRectangle), 2);
                MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&InspectorRectangle), 2);
                const int Available = std::max(
                    1,
                    static_cast<int>(InspectorRectangle.bottom - TreeRectangle.top)
                ); //TreeとInspector総高さ
                EngineSplitRatio = std::clamp(
                    static_cast<float>(GET_Y_LPARAM(lparam) - TreeRectangle.top) /
                        static_cast<float>(Available),
                    0.20f,
                    0.76f
                );
                LayoutControls();
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return 0;
            }

            if (ProgramHorizontalSplitDragging)
            {
                RECT ListRectangle{}; //Program一覧の親Window座標矩形
                RECT EditorRectangle{}; //Program Editorの親Window座標矩形
                GetWindowRect(ProgramFileListHwnd, &ListRectangle);
                GetWindowRect(ProgramEditorHwnd, &EditorRectangle);
                MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&ListRectangle), 2);
                MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&EditorRectangle), 2);
                const int Available = std::max(
                    1,
                    static_cast<int>(EditorRectangle.bottom - ListRectangle.top)
                ); //一覧とEditor総高さ
                ProgramHorizontalSplitRatio = std::clamp(
                    static_cast<float>(GET_Y_LPARAM(lparam) - ListRectangle.top) /
                        static_cast<float>(Available),
                    0.14f,
                    0.58f
                );
                LayoutControls();
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return 0;
            }

            if (ProgramVerticalSplitDragging)
            {
                RECT FileRectangle{}; //Program File一覧の親Window座標矩形
                RECT FunctionRectangle{}; //Program関数一覧の親Window座標矩形
                GetWindowRect(ProgramFileListHwnd, &FileRectangle);
                GetWindowRect(ProgramFunctionListHwnd, &FunctionRectangle);
                MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&FileRectangle), 2);
                MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&FunctionRectangle), 2);
                const int Available = std::max(
                    1,
                    static_cast<int>(FunctionRectangle.right - FileRectangle.left)
                ); //左右一覧総幅
                ProgramVerticalSplitRatio = std::clamp(
                    static_cast<float>(GET_X_LPARAM(lparam) - FileRectangle.left) /
                        static_cast<float>(Available),
                    0.24f,
                    0.76f
                );
                LayoutControls();
                SetCursor(SplitCursor);
                return 0;
            }
            break;

        case WM_LBUTTONUP:
            if (TreeDragging)
            {
                POINT TreePoint{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; //親Window内Drop位置
                ClientToScreen(Hwnd, &TreePoint);
                ScreenToClient(ObjectTreeHwnd, &TreePoint);
                TVHITTESTINFO HitTest{}; //Drop位置にあるTree Item
                HitTest.pt = TreePoint;
                TreeView_HitTest(ObjectTreeHwnd, &HitTest);
                const EditorTreeNode* DropNode = GetTreeNode(HitTest.hItem); //新しい親候補

                if (DropNode != nullptr && DropNode->Scene == DraggedTreeNode.Scene)
                {
                    EditorCommand Command; //World姿勢を維持する親変更要求
                    Command.Type = EditorCommandType::SetObjectParent;
                    Command.Scene = DraggedTreeNode.Scene;
                    Command.Object = DraggedTreeNode.Object;
                    Command.Parent = DropNode->Kind == EditorTreeNodeKind::Scene
                        ? ObjectID()
                        : DropNode->Object;
                    Command.KeepWorldTransform = true;
                    QueueEditorCommand(std::move(Command));
                }

                TreeDragging = false;

                if (GetCapture() == Hwnd)
                {
                    ReleaseCapture();
                }

                return 0;
            }

            if (SplitDragging)
            {
                SplitDragging = false;

                if (GetCapture() == Hwnd)
                {
                    ReleaseCapture();
                }

                return 0;
            }

            if (EngineSplitDragging || ProgramHorizontalSplitDragging ||
                ProgramVerticalSplitDragging)
            {
                EngineSplitDragging = false;
                ProgramHorizontalSplitDragging = false;
                ProgramVerticalSplitDragging = false;

                if (GetCapture() == Hwnd)
                {
                    ReleaseCapture();
                }

                return 0;
            }
            break;

        case WM_CAPTURECHANGED:
            SplitDragging = false;
            EngineSplitDragging = false;
            ProgramHorizontalSplitDragging = false;
            ProgramVerticalSplitDragging = false;
            TreeDragging = false;
            return 0;

        case WM_PAINT:
            PaintSplitter();
            return 0;

        case WM_CLOSE:
            DestroyWindow(Hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(Hwnd, ProgramAutomationTimerId);
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
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                SS_BLACKRECT | SS_NOTIFY,
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

        EditorTabHwnd = CreateWindowExW(
            0,
            WC_TABCONTROLW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(EditorTabControlId),
            Instance,
            nullptr
        );

        ObjectTreeHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_TREEVIEWW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                WS_VSCROLL | WS_HSCROLL | TVS_HASBUTTONS | TVS_HASLINES |
                TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_EDITLABELS,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ObjectTreeControlId),
            Instance,
            nullptr
        );

        SceneSelectorHwnd = CreateWindowExW(
            0,
            WC_COMBOBOXW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                CBS_DROPDOWNLIST | WS_VSCROLL,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(SceneSelectorControlId),
            Instance,
            nullptr
        );

        AddObjectButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"Object追加",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(AddObjectControlId),
            Instance,
            nullptr
        );

        AddScriptButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"Script差込",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(AddScriptControlId),
            Instance,
            nullptr
        );

        LoadScriptButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"DLL読込",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(LoadScriptControlId),
            Instance,
            nullptr
        );

        ObjectNameLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"Object",
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

        ObjectNameEditHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_EDITW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                ES_AUTOHSCROLL,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ObjectNameEditControlId),
            Instance,
            nullptr
        );

        ObjectActiveCheckHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"有効",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                BS_AUTOCHECKBOX,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ObjectActiveControlId),
            Instance,
            nullptr
        );

        ObjectParentLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"Parent: -",
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

        constexpr const wchar_t* TransformNames[] =
        {
            L"Position",
            L"Rotation",
            L"Scale"
        }; //Inspectorへ表示するTransform項目名

        for (int Row = 0; Row < 3; ++Row)
        {
            TransformLabelsHwnd[Row] = CreateWindowExW(
                0,
                WC_STATICW,
                TransformNames[Row],
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

            for (int Column = 0; Column < 3; ++Column)
            {
                const int Index = Row * 3 + Column; //Transform Edit配列内の位置
                TransformEditsHwnd[Index] = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    WC_EDITW,
                    Row == 2 ? L"1" : L"0",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                        ES_AUTOHSCROLL,
                    0,
                    0,
                    1,
                    1,
                    Hwnd,
                    ToControlMenu(TransformEditControlBase + Index),
                    Instance,
                    nullptr
                );
            }
        }

        ApplyObjectButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"Objectへ適用",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ApplyObjectControlId),
            Instance,
            nullptr
        );

        ProgramFileListHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTBOXW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ProgramFileListControlId),
            Instance,
            nullptr
        );

        ProgramFunctionListHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTBOXW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ProgramFunctionListControlId),
            Instance,
            nullptr
        );

        ProgramFileNameEditHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_EDITW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                ES_AUTOHSCROLL,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ProgramFileNameControlId),
            Instance,
            nullptr
        );

        RichEditModule = LoadLibraryW(L"Msftedit.dll");
        ProgramEditorHwnd = RichEditModule == nullptr
            ? nullptr
            : CreateWindowExW(
                WS_EX_CLIENTEDGE,
                MSFTEDIT_CLASS,
                L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                    WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_WANTRETURN |
                    ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
                0,
                0,
                1,
                1,
                Hwnd,
                ToControlMenu(ProgramEditorControlId),
                Instance,
                nullptr
            );

        ProgramSuggestionListHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTBOXW,
            L"",
            WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL |
                LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ProgramSuggestionListControlId),
            Instance,
            nullptr
        );

        ProgramErrorListHwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTBOXW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ProgramErrorListControlId),
            Instance,
            nullptr
        );

        const struct
        {
            HWND* Handle;
            const wchar_t* Text;
            int ID;
        } ProgramButtons[] =
        {
            { &NewProgramButtonHwnd, L"新規", NewProgramControlId },
            { &RenameProgramButtonHwnd, L"名前変更", RenameProgramControlId },
            { &DeleteProgramButtonHwnd, L"削除", DeleteProgramControlId },
            { &SaveProgramButtonHwnd, L"保存", SaveProgramControlId },
            { &CompileProgramButtonHwnd, L"コンパイル", CompileProgramControlId },
            { &RestoreProgramButtonHwnd, L"正常版", RestoreProgramControlId }
        }; //Program Pageへ作成する操作Button定義

        for (const auto& Button : ProgramButtons)
        {
            *Button.Handle = CreateWindowExW(
                0,
                WC_BUTTONW,
                Button.Text,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
                0,
                0,
                1,
                1,
                Hwnd,
                ToControlMenu(Button.ID),
                Instance,
                nullptr
            );
        }

        ProgramStatusLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"Program Workspaceを準備中",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ProgramStatusControlId),
            Instance,
            nullptr
        );

        ProgramRoleLabelHwnd = CreateWindowExW(
            0,
            WC_STATICW,
            L"メイン：毎フレームの最初に実行し、Scene／Object／Script全体を制御します。",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(ProgramRoleControlId),
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

        PauseButtonHwnd = CreateWindowExW(
            0,
            WC_BUTTONW,
            L"Pause",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            1,
            1,
            Hwnd,
            ToControlMenu(PauseControlId),
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
            EditorTabHwnd != nullptr &&
            SceneSelectorHwnd != nullptr &&
            ObjectTreeHwnd != nullptr &&
            AddObjectButtonHwnd != nullptr &&
            AddScriptButtonHwnd != nullptr &&
            LoadScriptButtonHwnd != nullptr &&
            ObjectNameLabelHwnd != nullptr &&
            ObjectNameEditHwnd != nullptr &&
            ObjectActiveCheckHwnd != nullptr &&
            ObjectParentLabelHwnd != nullptr &&
            ApplyObjectButtonHwnd != nullptr &&
            StartButtonHwnd != nullptr &&
            PauseButtonHwnd != nullptr &&
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
            ClearLogsButtonHwnd != nullptr &&
            ProgramFileListHwnd != nullptr &&
            ProgramFunctionListHwnd != nullptr &&
            ProgramFileNameEditHwnd != nullptr &&
            ProgramEditorHwnd != nullptr &&
            ProgramSuggestionListHwnd != nullptr &&
            ProgramErrorListHwnd != nullptr &&
            NewProgramButtonHwnd != nullptr &&
            RenameProgramButtonHwnd != nullptr &&
            DeleteProgramButtonHwnd != nullptr &&
            SaveProgramButtonHwnd != nullptr &&
            CompileProgramButtonHwnd != nullptr &&
            RestoreProgramButtonHwnd != nullptr &&
            ProgramStatusLabelHwnd != nullptr &&
            ProgramRoleLabelHwnd != nullptr &&
            std::all_of(
                std::begin(TransformLabelsHwnd),
                std::end(TransformLabelsHwnd),
                [](HWND handle) { return handle != nullptr; }
            ) &&
            std::all_of(
                std::begin(TransformEditsHwnd),
                std::end(TransformEditsHwnd),
                [](HWND handle) { return handle != nullptr; }
            ); // 必須コントロールを全て作成できたか

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
        SendMessageW(ProgramEditorHwnd, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
        SendMessageW(ProgramEditorHwnd, EM_EXLIMITTEXT, 0, 8 * 1024 * 1024);
        SendMessageW(ProgramEditorHwnd, EM_SETUNDOLIMIT, 512, 0);

        if (!SetWindowSubclass(
            ProgramEditorHwnd,
            ProgramEditorSubclassProc,
            ProgramEditorSubclassId,
            reinterpret_cast<DWORD_PTR>(this)
        ))
        {
            AddWin32FailureLog("SetWindowSubclass for Program completion");
            return false;
        }

        TCITEMW TabItem{}; //Editor Tabへ登録する表示情報
        TabItem.mask = TCIF_TEXT;
        TabItem.pszText = const_cast<wchar_t*>(L"Engine");
        TabCtrl_InsertItem(EditorTabHwnd, 0, &TabItem);
        TabItem.pszText = const_cast<wchar_t*>(L"再生");
        TabCtrl_InsertItem(EditorTabHwnd, 1, &TabItem);
        TabItem.pszText = const_cast<wchar_t*>(L"ログ");
        TabCtrl_InsertItem(EditorTabHwnd, 2, &TabItem);
        TabItem.pszText = const_cast<wchar_t*>(L"メイン");
        TabCtrl_InsertItem(EditorTabHwnd, 3, &TabItem);
        TabItem.pszText = const_cast<wchar_t*>(L"スクリプト");
        TabCtrl_InsertItem(EditorTabHwnd, 4, &TabItem);
        TabCtrl_SetCurSel(EditorTabHwnd, ActiveTabIndex);
        UpdateTabVisibility();

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
        const int ButtonWidth = std::max(1, (ContentWidth - Gap * 3) / 4); // 各再生ボタンの幅
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
            PauseButtonHwnd,
            ContentLeft + ButtonWidth + Gap,
            ContentTop,
            ButtonWidth,
            ScaledButtonHeight,
            TRUE
        );
        MoveWindow(
            StopButtonHwnd,
            ContentLeft + (ButtonWidth + Gap) * 2,
            ContentTop,
            ButtonWidth,
            ScaledButtonHeight,
            TRUE
        );
        MoveWindow(
            TickButtonHwnd,
            ContentLeft + (ButtonWidth + Gap) * 3,
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

        LayoutTabbedControls(PanelLeft, PanelWidth, ClientHeight);

        const HWND ForegroundControls[] =
        {
            TitleLabelHwnd,
            EditorTabHwnd,
            SceneSelectorHwnd,
            ObjectTreeHwnd,
            AddObjectButtonHwnd,
            AddScriptButtonHwnd,
            LoadScriptButtonHwnd,
            ObjectNameLabelHwnd,
            ObjectNameEditHwnd,
            ObjectActiveCheckHwnd,
            ObjectParentLabelHwnd,
            TransformLabelsHwnd[0],
            TransformLabelsHwnd[1],
            TransformLabelsHwnd[2],
            TransformEditsHwnd[0],
            TransformEditsHwnd[1],
            TransformEditsHwnd[2],
            TransformEditsHwnd[3],
            TransformEditsHwnd[4],
            TransformEditsHwnd[5],
            TransformEditsHwnd[6],
            TransformEditsHwnd[7],
            TransformEditsHwnd[8],
            ApplyObjectButtonHwnd,
            StartButtonHwnd,
            PauseButtonHwnd,
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
            LogListHwnd,
            ProgramFileListHwnd,
            ProgramFunctionListHwnd,
            ProgramEditorHwnd,
            ProgramSuggestionListHwnd,
            ProgramErrorListHwnd,
            ProgramFileNameEditHwnd,
            NewProgramButtonHwnd,
            RenameProgramButtonHwnd,
            DeleteProgramButtonHwnd,
            SaveProgramButtonHwnd,
            CompileProgramButtonHwnd,
            RestoreProgramButtonHwnd,
            ProgramStatusLabelHwnd,
            ProgramRoleLabelHwnd
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

    //概要：最新SnapshotからScene、Object、Componentの階層Treeを再構築する
    //引数：なし
    //戻り値：なし
    void WinApp::RebuildObjectTree()
    {
        if (ObjectTreeHwnd == nullptr)
        {
            return;
        }

        EditorTreeNode PreviousSelection; //再構築後に復元する従来選択要素
        const EditorTreeNode* SelectedNode = GetSelectedTreeNode(); //再構築前の選択要素
        const bool HadSelection = SelectedNode != nullptr; //選択を復元する必要がある場合true

        if (SelectedNode != nullptr)
        {
            PreviousSelection = *SelectedNode;
        }

        const EditorSceneInfo* SelectedScene = GetSelectedSceneInfo(); //Treeへ表示する選択Scene
        std::size_t NodeCount = SelectedScene == nullptr ? 0 : 1; //事前確保するTree Node総数

        if (SelectedScene != nullptr)
        {
            NodeCount += SelectedScene->Objects.size();

            for (const EditorObjectInfo& Object : SelectedScene->Objects)
            {
                NodeCount += Object.Components.size();
            }
        }

        SendMessageW(ObjectTreeHwnd, WM_SETREDRAW, FALSE, 0);
        TreeView_DeleteAllItems(ObjectTreeHwnd);
        EditorTreeNodes.clear();
        EditorTreeNodes.reserve(NodeCount);
        HTREEITEM RestoredSelection = nullptr; //再構築後に選択するTree Item

        const auto InsertNode = [this, &PreviousSelection, HadSelection, &RestoredSelection](
            HTREEITEM parent,
            const std::wstring& label,
            const EditorTreeNode& node
        )
        {
            EditorTreeNodes.emplace_back(node);

            TVINSERTSTRUCTW InsertInformation{}; //Tree Item作成情報
            InsertInformation.hParent = parent;
            InsertInformation.hInsertAfter = TVI_LAST;
            InsertInformation.item.mask = TVIF_TEXT | TVIF_PARAM;
            InsertInformation.item.pszText = const_cast<wchar_t*>(label.c_str());
            InsertInformation.item.lParam = static_cast<LPARAM>(EditorTreeNodes.size());
            HTREEITEM Item = TreeView_InsertItem(
                ObjectTreeHwnd,
                &InsertInformation
            ); //作成したTree Item

            if (HadSelection &&
                node.Kind == PreviousSelection.Kind &&
                node.Scene == PreviousSelection.Scene &&
                node.Object == PreviousSelection.Object &&
                node.Component == PreviousSelection.Component)
            {
                RestoredSelection = Item;
            }

            return Item;
        }; //Node表とTree Itemを同時登録する処理

        if (SelectedScene != nullptr)
        {
            const EditorSceneInfo& Scene = *SelectedScene; //表示する単一Scene
            std::wstring SceneLabel = Scene.ViewScene ? L"[表示] " : L""; //View Scene表示Prefix
            SceneLabel += Scene.Active ? L"" : L"[停止] ";
            SceneLabel += ConvertEditorTextToWide(Scene.Name);

            EditorTreeNode SceneNode; //Scene Rootに対応するNode情報
            SceneNode.Kind = EditorTreeNodeKind::Scene;
            SceneNode.Scene = Scene.ID;
            SceneNode.Active = Scene.Active;
            HTREEITEM SceneItem = InsertNode(TVI_ROOT, SceneLabel, SceneNode); //Scene Root Item

            std::function<void(const EditorObjectInfo&, HTREEITEM)> InsertObject;
            InsertObject = [this, &Scene, &InsertNode, &InsertObject](
                const EditorObjectInfo& Object,
                HTREEITEM ParentItem
            )
            {
                std::wstring ObjectLabel = Object.Active ? L"" : L"[無効] "; //Object有効状態Prefix
                ObjectLabel += ConvertEditorTextToWide(Object.Name);
                ObjectLabel += L" <";
                ObjectLabel += GetObjectMenuName(Object.Type);
                ObjectLabel += L">";

                EditorTreeNode ObjectNode; //Object Itemに対応するNode情報
                ObjectNode.Kind = EditorTreeNodeKind::Object;
                ObjectNode.Scene = Scene.ID;
                ObjectNode.Object = Object.ID;
                ObjectNode.Active = Object.Active;
                HTREEITEM ObjectItem = InsertNode(
                    ParentItem,
                    ObjectLabel,
                    ObjectNode
                ); //Object Tree Item

                for (const EditorObjectInfo& Child : Scene.Objects)
                {
                    if (Child.ParentID == Object.ID)
                    {
                        InsertObject(Child, ObjectItem);
                    }
                }

                for (const EditorComponentInfo& Component : Object.Components)
                {
                    std::wstring ComponentLabel = Component.Active
                        ? L""
                        : L"[無効] "; //Component有効状態Prefix
                    ComponentLabel += Component.Script ? L"[Script] " : L"";
                    ComponentLabel += ConvertEditorTextToWide(Component.Name);
                    ComponentLabel += L" : ";
                    ComponentLabel += ConvertEditorTextToWide(Component.TypeName);

                    EditorTreeNode ComponentNode; //Component Itemに対応するNode情報
                    ComponentNode.Kind = EditorTreeNodeKind::Component;
                    ComponentNode.Scene = Scene.ID;
                    ComponentNode.Object = Object.ID;
                    ComponentNode.Component = Component.ID;
                    ComponentNode.Active = Component.Active;
                    InsertNode(ObjectItem, ComponentLabel, ComponentNode);
                }
            }; //親子順でObjectと所有Componentを挿入する再帰処理

            for (const EditorObjectInfo& Object : Scene.Objects)
            {
                const bool ParentExists = std::any_of(
                    Scene.Objects.begin(),
                    Scene.Objects.end(),
                    [&Object](const EditorObjectInfo& candidate)
                    {
                        return Object.ParentID.IsValid() && candidate.ID == Object.ParentID;
                    }
                ); //Snapshot内に有効な親が存在する場合true

                if (!ParentExists)
                {
                    InsertObject(Object, SceneItem);
                }
            }

            TreeView_Expand(ObjectTreeHwnd, SceneItem, TVE_EXPAND);
        }

        if (RestoredSelection != nullptr)
        {
            TreeView_SelectItem(ObjectTreeHwnd, RestoredSelection);
            TreeView_EnsureVisible(ObjectTreeHwnd, RestoredSelection);
        }

        SendMessageW(ObjectTreeHwnd, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(ObjectTreeHwnd, nullptr, TRUE);
    }

    //概要：SnapshotのScene一覧をObject Tree上部の選択Comboへ反映する
    //引数：なし
    //戻り値：なし
    void WinApp::RebuildSceneSelector()
    {
        if (SceneSelectorHwnd == nullptr)
        {
            return;
        }

        const bool SelectionExists = std::any_of(
            CurrentEditorSnapshot.Scenes.begin(),
            CurrentEditorSnapshot.Scenes.end(),
            [this](const EditorSceneInfo& scene)
            {
                return scene.ID == SelectedEditorSceneID;
            }
        ); //従来選択Sceneが現在も存在する場合true

        if (!SelectionExists)
        {
            SelectedEditorSceneID = CurrentEditorSnapshot.ViewSceneID;

            if (!SelectedEditorSceneID.IsValid() && !CurrentEditorSnapshot.Scenes.empty())
            {
                SelectedEditorSceneID = CurrentEditorSnapshot.Scenes.front().ID;
            }
        }

        SendMessageW(SceneSelectorHwnd, CB_RESETCONTENT, 0, 0);
        SceneSelectorIDs.clear();
        int SelectedIndex = -1; //Comboへ復元するScene位置

        for (const EditorSceneInfo& Scene : CurrentEditorSnapshot.Scenes)
        {
            std::wstring Label = Scene.ViewScene ? L"[表示] " : L""; //View Scene表示Prefix
            Label += ConvertEditorTextToWide(Scene.Name);
            const LRESULT Index = SendMessageW(
                SceneSelectorHwnd,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(Label.c_str())
            ); //追加したCombo Index

            if (Index >= 0)
            {
                SceneSelectorIDs.emplace_back(Scene.ID);

                if (Scene.ID == SelectedEditorSceneID)
                {
                    SelectedIndex = static_cast<int>(Index);
                }
            }
        }

        SendMessageW(SceneSelectorHwnd, CB_SETCURSEL, SelectedIndex, 0);
    }

    //概要：Treeで選択しているObject又はComponent所有ObjectのSnapshot情報を取得する
    //引数：なし
    //戻り値：選択Object情報、Scene又は空白選択時はnullptr
    const EditorObjectInfo* WinApp::GetSelectedObjectInfo() const
    {
        const EditorTreeNode* Node = GetSelectedTreeNode(); //Treeの現在選択要素

        if (Node == nullptr || !Node->Object.IsValid())
        {
            return nullptr;
        }

        for (const EditorSceneInfo& Scene : CurrentEditorSnapshot.Scenes)
        {
            if (Scene.ID != Node->Scene)
            {
                continue;
            }

            for (const EditorObjectInfo& Object : Scene.Objects)
            {
                if (Object.ID == Node->Object)
                {
                    return &Object;
                }
            }
        }

        return nullptr;
    }

    //概要：Object Treeへ表示しているSceneのSnapshot情報を取得する
    //引数：なし
    //戻り値：選択Scene情報、存在しない場合はnullptr
    const EditorSceneInfo* WinApp::GetSelectedSceneInfo() const
    {
        const auto Iterator = std::find_if(
            CurrentEditorSnapshot.Scenes.begin(),
            CurrentEditorSnapshot.Scenes.end(),
            [this](const EditorSceneInfo& scene)
            {
                return scene.ID == SelectedEditorSceneID;
            }
        ); //選択IDに対応するScene
        return Iterator == CurrentEditorSnapshot.Scenes.end()
            ? nullptr
            : &*Iterator;
    }

    //概要：選択Objectの名前、有効状態、Parent、Local TransformをInspectorへ表示する
    //引数：なし
    //戻り値：なし
    void WinApp::UpdateObjectInspector()
    {
        const EditorObjectInfo* Object = GetSelectedObjectInfo(); //Inspectorへ表示するObject
        const BOOL Enabled = Object == nullptr ? FALSE : TRUE; //Object編集Control有効状態
        EnableWindow(ObjectNameEditHwnd, Enabled);
        EnableWindow(ObjectActiveCheckHwnd, Enabled);
        EnableWindow(ApplyObjectButtonHwnd, Enabled);

        for (HWND Edit : TransformEditsHwnd)
        {
            EnableWindow(Edit, Enabled);
        }

        if (Object == nullptr)
        {
            SetWindowTextW(ObjectNameEditHwnd, L"");
            SetWindowTextW(ObjectParentLabelHwnd, L"Parent: -");
            SendMessageW(ObjectActiveCheckHwnd, BM_SETCHECK, BST_UNCHECKED, 0);
            return;
        }

        const std::wstring Name = ConvertEditorTextToWide(Object->Name); //Object名のUTF-16表現
        SetWindowTextW(ObjectNameEditHwnd, Name.c_str());
        SendMessageW(
            ObjectActiveCheckHwnd,
            BM_SETCHECK,
            Object->Active ? BST_CHECKED : BST_UNCHECKED,
            0
        );

        std::wstring ParentLabel = L"Parent: Root"; //親Object表示名

        if (Object->ParentID.IsValid())
        {
            const EditorSceneInfo* Scene = GetSelectedSceneInfo(); //親名を検索する選択Scene

            if (Scene != nullptr)
            {
                const auto Parent = std::find_if(
                    Scene->Objects.begin(),
                    Scene->Objects.end(),
                    [Object](const EditorObjectInfo& candidate)
                    {
                        return candidate.ID == Object->ParentID;
                    }
                ); //親Object検索結果

                if (Parent != Scene->Objects.end())
                {
                    ParentLabel = L"Parent: " + ConvertEditorTextToWide(Parent->Name);
                }
            }
        }

        SetWindowTextW(ObjectParentLabelHwnd, ParentLabel.c_str());
        constexpr float RadiansToDegrees = 57.29577951308232f; //回転角UI表示変換率
        const float Values[] =
        {
            Object->LocalTransform.Position.X,
            Object->LocalTransform.Position.Y,
            Object->LocalTransform.Position.Z,
            Object->LocalTransform.Rotation.X * RadiansToDegrees,
            Object->LocalTransform.Rotation.Y * RadiansToDegrees,
            Object->LocalTransform.Rotation.Z * RadiansToDegrees,
            Object->LocalTransform.Scale.X,
            Object->LocalTransform.Scale.Y,
            Object->LocalTransform.Scale.Z
        }; //Inspectorへ表示するPosition、Degree Rotation、Scale

        for (std::size_t Index = 0; Index < std::size(Values); ++Index)
        {
            wchar_t Buffer[48]{}; //Transform数値表示Buffer
            _snwprintf_s(Buffer, std::size(Buffer), _TRUNCATE, L"%.4f", Values[Index]);
            SetWindowTextW(TransformEditsHwnd[Index], Buffer);
        }
    }

    //概要：Inspector入力を名前、有効状態、Local Transform操作としてQueueへ登録する
    //引数：なし
    //戻り値：なし
    void WinApp::ApplyObjectInspector()
    {
        const EditorTreeNode* Node = GetSelectedTreeNode(); //操作対象Tree要素
        const EditorObjectInfo* Object = GetSelectedObjectInfo(); //変更前Object情報

        if (Node == nullptr || Object == nullptr)
        {
            return;
        }

        float Values[9]{}; //入力されたPosition、Degree Rotation、Scale

        for (std::size_t Index = 0; Index < std::size(Values); ++Index)
        {
            const std::wstring Text = GetControlText(TransformEditsHwnd[Index]); //現在Edit文字列
            wchar_t* End = nullptr; //数値変換後の未処理位置
            Values[Index] = std::wcstof(Text.c_str(), &End);

            if (End == Text.c_str() || *End != L'\0' || !std::isfinite(Values[Index]))
            {
                MessageLog::GetInstance().AddLog(
                    "[Warning] Editor | Transformには有効な数値を入力してください。"
                );
                return;
            }
        }

        const std::string RequestedName = ConvertEditorTextToUtf8(
            GetControlText(ObjectNameEditHwnd).c_str()
        ); //Inspectorへ入力されたObject名

        if (!RequestedName.empty() && RequestedName != Object->Name)
        {
            EditorCommand Rename; //Object名前変更要求
            Rename.Type = EditorCommandType::RenameObject;
            Rename.Scene = Node->Scene;
            Rename.Object = Node->Object;
            Rename.Text = RequestedName;
            QueueEditorCommand(std::move(Rename));
        }

        const bool RequestedActive = SendMessageW(
            ObjectActiveCheckHwnd,
            BM_GETCHECK,
            0,
            0
        ) == BST_CHECKED; //Inspectorへ入力された有効状態

        if (RequestedActive != Object->Active)
        {
            EditorCommand Toggle; //Object有効状態切替要求
            Toggle.Type = EditorCommandType::ToggleObjectActive;
            Toggle.Scene = Node->Scene;
            Toggle.Object = Node->Object;
            QueueEditorCommand(std::move(Toggle));
        }

        constexpr float DegreesToRadians = 0.017453292519943295f; //回転角Engine格納変換率
        EditorCommand TransformCommand; //Local Transform変更要求
        TransformCommand.Type = EditorCommandType::SetObjectTransform;
        TransformCommand.Scene = Node->Scene;
        TransformCommand.Object = Node->Object;
        TransformCommand.Transform.Position = { Values[0], Values[1], Values[2] };
        TransformCommand.Transform.Rotation =
        {
            Values[3] * DegreesToRadians,
            Values[4] * DegreesToRadians,
            Values[5] * DegreesToRadians
        };
        TransformCommand.Transform.Scale = { Values[6], Values[7], Values[8] };
        QueueEditorCommand(std::move(TransformCommand));
    }

    //概要：現在選択中Tabに属するControlだけを表示する
    //引数：なし
    //戻り値：なし
    void WinApp::UpdateTabVisibility()
    {
        const HWND EngineControls[] =
        {
            SceneSelectorHwnd,
            ObjectTreeHwnd,
            AddObjectButtonHwnd,
            AddScriptButtonHwnd,
            LoadScriptButtonHwnd,
            ObjectNameLabelHwnd,
            ObjectNameEditHwnd,
            ObjectActiveCheckHwnd,
            ObjectParentLabelHwnd,
            TransformLabelsHwnd[0],
            TransformLabelsHwnd[1],
            TransformLabelsHwnd[2],
            TransformEditsHwnd[0],
            TransformEditsHwnd[1],
            TransformEditsHwnd[2],
            TransformEditsHwnd[3],
            TransformEditsHwnd[4],
            TransformEditsHwnd[5],
            TransformEditsHwnd[6],
            TransformEditsHwnd[7],
            TransformEditsHwnd[8],
            ApplyObjectButtonHwnd
        }; //Engine Tabで表示するControl
        const HWND PlaybackControls[] =
        {
            StartButtonHwnd,
            PauseButtonHwnd,
            StopButtonHwnd,
            TickButtonHwnd,
            StatusLabelHwnd,
            FrameRateLabelHwnd,
            FrameRateEditHwnd,
            FrameRateSliderHwnd,
            PreviewLabelHwnd,
            PreviewImageHwnd
        }; //再生Tabで表示するControl
        const HWND LogControls[] =
        {
            LogLabelHwnd,
            LogListHwnd,
            ClearLogsButtonHwnd
        }; //Log Tabで表示するControl
        const HWND ProgramControls[] =
        {
            ProgramFileListHwnd,
            ProgramFunctionListHwnd,
            ProgramEditorHwnd,
            ProgramErrorListHwnd,
            ProgramFileNameEditHwnd,
            NewProgramButtonHwnd,
            RenameProgramButtonHwnd,
            DeleteProgramButtonHwnd,
            SaveProgramButtonHwnd,
            CompileProgramButtonHwnd,
            RestoreProgramButtonHwnd,
            ProgramStatusLabelHwnd,
            ProgramRoleLabelHwnd
        }; //Program Tabで表示するControl

        const auto SetVisibility = [](const HWND* controls, std::size_t count, bool visible)
        {
            for (std::size_t Index = 0; Index < count; ++Index)
            {
                if (controls[Index] != nullptr)
                {
                    ShowWindow(controls[Index], visible ? SW_SHOW : SW_HIDE);
                }
            }
        }; //Control配列の表示状態をまとめて切り替える処理

        SetVisibility(
            EngineControls,
            std::size(EngineControls),
            ActiveTabIndex == 0
        );
        SetVisibility(
            PlaybackControls,
            std::size(PlaybackControls),
            ActiveTabIndex == 1
        );
        SetVisibility(
            LogControls,
            std::size(LogControls),
            ActiveTabIndex == 2
        );
        SetVisibility(
            ProgramControls,
            std::size(ProgramControls),
            IsProgramSourceTab()
        );

        if (!IsProgramSourceTab())
        {
            HideProgramSuggestions();
        }
    }

    //概要：右PanelをEngine、再生、LogのTab Pageとして再配置する
    //引数：panelLeft=右Panel左端、panelWidth=右Panel幅、clientHeight=Client高さ
    //戻り値：なし
    void WinApp::LayoutTabbedControls(
        int panelLeft,
        int panelWidth,
        int clientHeight
    )
    {
        const int Margin = ScaleByDpi(PanelMargin); //右Panel内側余白
        const int Gap = ScaleByDpi(ControlGap); //Control間余白
        const int HeaderHeight = ScaleByDpi(TitleHeight); //Editor見出し高さ
        const int TabHeaderHeight = ScaleByDpi(34); //Tab見出しと内側上余白
        const int ContentLeft = panelLeft + Margin; //Tab外枠左位置
        const int ContentWidth = std::max(1, panelWidth - Margin * 2); //Tab外枠幅
        const int TabTop = Margin + HeaderHeight + Gap; //Tab外枠上位置
        const int TabHeight = std::max(1, clientHeight - TabTop - Margin); //Tab外枠高さ
        const int PageLeft = ContentLeft + Gap; //Tab Page内左位置
        const int PageTop = TabTop + TabHeaderHeight; //Tab Page内上位置
        const int PageWidth = std::max(1, ContentWidth - Gap * 2); //Tab Page内幅
        const int PageBottom = TabTop + TabHeight - Gap; //Tab Page内下位置

        SetWindowTextW(TitleLabelHwnd, L"DirectX 12 Engine Editor");
        MoveWindow(
            EditorTabHwnd,
            ContentLeft,
            TabTop,
            ContentWidth,
            TabHeight,
            TRUE
        );

        const int ActionButtonHeight = ScaleByDpi(ButtonHeight); //Engine操作Button高さ
        const int ActionButtonWidth = std::max(1, (PageWidth - Gap * 2) / 3); //Engine操作Button幅
        const int SceneSelectorHeight = ScaleByDpi(30); //Scene選択Combo高さ
        MoveWindow(
            SceneSelectorHwnd,
            PageLeft,
            PageTop,
            PageWidth,
            ScaleByDpi(240),
            TRUE
        );
        const int EngineButtonTop = PageTop + SceneSelectorHeight + Gap; //Engine操作Button上位置
        MoveWindow(AddObjectButtonHwnd, PageLeft, EngineButtonTop, ActionButtonWidth, ActionButtonHeight, TRUE);
        MoveWindow(
            AddScriptButtonHwnd,
            PageLeft + ActionButtonWidth + Gap,
            EngineButtonTop,
            ActionButtonWidth,
            ActionButtonHeight,
            TRUE
        );
        MoveWindow(
            LoadScriptButtonHwnd,
            PageLeft + (ActionButtonWidth + Gap) * 2,
            EngineButtonTop,
            ActionButtonWidth,
            ActionButtonHeight,
            TRUE
        );
        const int EngineContentTop = EngineButtonTop + ActionButtonHeight + Gap; //Tree上位置
        const int InnerSplitter = ScaleByDpi(EditorInnerSplitterSize); //Engine内分割線高さ
        const int EngineAvailableHeight = std::max(1, PageBottom - EngineContentTop); //TreeとInspectorの総高さ
        const int MinimumTreeHeight = ScaleByDpi(110); //Object Tree最小高さ
        const int MinimumInspectorHeight = ScaleByDpi(205); //Inspector最小高さ
        const int MaximumTreeHeight = std::max(
            MinimumTreeHeight,
            EngineAvailableHeight - InnerSplitter - MinimumInspectorHeight
        ); //Inspectorを確保したTree最大高さ
        const int ObjectTreeHeight = std::clamp(
            static_cast<int>(std::lround(EngineAvailableHeight * EngineSplitRatio)),
            std::min(MinimumTreeHeight, MaximumTreeHeight),
            MaximumTreeHeight
        ); //現在比率を制約したTree高さ
        MoveWindow(ObjectTreeHwnd, PageLeft, EngineContentTop, PageWidth, ObjectTreeHeight, TRUE);

        const int InspectorTop = EngineContentTop + ObjectTreeHeight + InnerSplitter; //Inspector上位置
        const int InspectorHeight = std::max(1, PageBottom - InspectorTop); //Inspector利用可能高さ
        const int InspectorRowHeight = ScaleByDpi(27); //Inspector一行高さ
        const int InspectorLabelWidth = ScaleByDpi(68); //Transform項目名幅
        const int ActiveWidth = ScaleByDpi(58); //有効Check幅
        const int NameEditLeft = PageLeft + InspectorLabelWidth; //Object名入力左位置
        const int NameEditWidth = std::max(
            1,
            PageWidth - InspectorLabelWidth - ActiveWidth - Gap
        ); //Object名入力幅
        int InspectorRowTop = InspectorTop; //次のInspector行上位置
        MoveWindow(ObjectNameLabelHwnd, PageLeft, InspectorRowTop, InspectorLabelWidth, InspectorRowHeight, TRUE);
        MoveWindow(ObjectNameEditHwnd, NameEditLeft, InspectorRowTop, NameEditWidth, InspectorRowHeight, TRUE);
        MoveWindow(
            ObjectActiveCheckHwnd,
            NameEditLeft + NameEditWidth + Gap,
            InspectorRowTop,
            ActiveWidth,
            InspectorRowHeight,
            TRUE
        );
        InspectorRowTop += InspectorRowHeight + Gap;
        MoveWindow(ObjectParentLabelHwnd, PageLeft, InspectorRowTop, PageWidth, InspectorRowHeight, TRUE);
        InspectorRowTop += InspectorRowHeight + Gap;
        const int TransformEditWidth = std::max(
            1,
            (PageWidth - InspectorLabelWidth - Gap * 3) / 3
        ); //Transform一成分の入力幅

        for (int Row = 0; Row < 3; ++Row)
        {
            MoveWindow(
                TransformLabelsHwnd[Row],
                PageLeft,
                InspectorRowTop,
                InspectorLabelWidth,
                InspectorRowHeight,
                TRUE
            );

            for (int Column = 0; Column < 3; ++Column)
            {
                MoveWindow(
                    TransformEditsHwnd[Row * 3 + Column],
                    PageLeft + InspectorLabelWidth + Gap +
                        Column * (TransformEditWidth + Gap),
                    InspectorRowTop,
                    TransformEditWidth,
                    InspectorRowHeight,
                    TRUE
                );
            }

            InspectorRowTop += InspectorRowHeight + Gap;
        }

        MoveWindow(
            ApplyObjectButtonHwnd,
            PageLeft,
            std::min(InspectorRowTop, PageBottom - ActionButtonHeight),
            PageWidth,
            std::min(ActionButtonHeight, InspectorHeight),
            TRUE
        );

        const int PlaybackButtonWidth = std::max(1, (PageWidth - Gap * 3) / 4); //再生操作Button幅
        MoveWindow(StartButtonHwnd, PageLeft, PageTop, PlaybackButtonWidth, ActionButtonHeight, TRUE);
        MoveWindow(
            PauseButtonHwnd,
            PageLeft + PlaybackButtonWidth + Gap,
            PageTop,
            PlaybackButtonWidth,
            ActionButtonHeight,
            TRUE
        );
        MoveWindow(
            StopButtonHwnd,
            PageLeft + (PlaybackButtonWidth + Gap) * 2,
            PageTop,
            PlaybackButtonWidth,
            ActionButtonHeight,
            TRUE
        );
        MoveWindow(
            TickButtonHwnd,
            PageLeft + (PlaybackButtonWidth + Gap) * 3,
            PageTop,
            PlaybackButtonWidth,
            ActionButtonHeight,
            TRUE
        );

        const int LabelHeightValue = ScaleByDpi(LabelHeight); //通常Label高さ
        int PlaybackTop = PageTop + ActionButtonHeight + Gap; //次の再生Control上位置
        MoveWindow(StatusLabelHwnd, PageLeft, PlaybackTop, PageWidth, LabelHeightValue, TRUE);
        PlaybackTop += LabelHeightValue + Gap;

        const int EditWidthValue = ScaleByDpi(EditWidth); //FPS Edit幅
        MoveWindow(
            FrameRateLabelHwnd,
            PageLeft,
            PlaybackTop,
            std::max(1, PageWidth - EditWidthValue - Gap),
            LabelHeightValue,
            TRUE
        );
        MoveWindow(
            FrameRateEditHwnd,
            PageLeft + PageWidth - EditWidthValue,
            PlaybackTop,
            EditWidthValue,
            LabelHeightValue,
            TRUE
        );
        PlaybackTop += LabelHeightValue + Gap;

        const int SliderHeightValue = ScaleByDpi(SliderHeight); //FPS Slider高さ
        MoveWindow(
            FrameRateSliderHwnd,
            PageLeft,
            PlaybackTop,
            PageWidth,
            SliderHeightValue,
            TRUE
        );
        PlaybackTop += SliderHeightValue + Gap;
        MoveWindow(PreviewLabelHwnd, PageLeft, PlaybackTop, PageWidth, LabelHeightValue, TRUE);
        PlaybackTop += LabelHeightValue + Gap;

        const int PreviewWidthValue = std::min(
            PageWidth,
            ScaleByDpi(static_cast<int>(UISettings.PreviewWidth))
        ); //Tab内に収めたPreview幅
        const int PreviewHeightValue = std::max(
            1,
            std::min(
                PageBottom - PlaybackTop,
                ScaleByDpi(static_cast<int>(UISettings.PreviewHeight))
            )
        ); //Tab内に収めたPreview高さ
        MoveWindow(
            PreviewImageHwnd,
            PageLeft + std::max(0, (PageWidth - PreviewWidthValue) / 2),
            PlaybackTop,
            PreviewWidthValue,
            PreviewHeightValue,
            TRUE
        );

        const int ClearButtonWidth = ScaleByDpi(ClearLogsButtonWidth); //Log消去Button幅
        const int LogHeaderHeightValue = ScaleByDpi(LogHeaderHeight); //Log見出し高さ
        MoveWindow(
            LogLabelHwnd,
            PageLeft,
            PageTop,
            std::max(1, PageWidth - ClearButtonWidth - Gap),
            LogHeaderHeightValue,
            TRUE
        );
        MoveWindow(
            ClearLogsButtonHwnd,
            PageLeft + std::max(0, PageWidth - ClearButtonWidth),
            PageTop,
            std::min(PageWidth, ClearButtonWidth),
            LogHeaderHeightValue,
            TRUE
        );
        MoveWindow(
            LogListHwnd,
            PageLeft,
            PageTop + LogHeaderHeightValue + Gap,
            PageWidth,
            std::max(1, PageBottom - PageTop - LogHeaderHeightValue - Gap),
            TRUE
        );

        const int ProgramRoleHeight = ScaleByDpi(38); //Main又はScriptの役割説明高さ
        MoveWindow(
            ProgramRoleLabelHwnd,
            PageLeft,
            PageTop,
            PageWidth,
            ProgramRoleHeight,
            TRUE
        );
        const int ProgramButtonTop = PageTop + ProgramRoleHeight + Gap; //Program操作Button上位置
        const int ProgramButtonWidth = std::max(1, (PageWidth - Gap * 2) / 3); //三列二段のProgram操作Button幅
        const HWND ProgramButtons[] =
        {
            NewProgramButtonHwnd,
            RenameProgramButtonHwnd,
            DeleteProgramButtonHwnd,
            SaveProgramButtonHwnd,
            CompileProgramButtonHwnd,
            RestoreProgramButtonHwnd
        }; //左から配置するProgram操作Button

        for (int Index = 0; Index < 6; ++Index)
        {
            const int ButtonColumn = Index % 3; //操作Buttonの列Index
            const int ButtonRow = Index / 3; //操作Buttonの段Index
            MoveWindow(
                ProgramButtons[Index],
                PageLeft + ButtonColumn * (ProgramButtonWidth + Gap),
                ProgramButtonTop + ButtonRow * (ActionButtonHeight + Gap),
                ProgramButtonWidth,
                ActionButtonHeight,
                TRUE
            );
        }

        const int ProgramNameTop = ProgramButtonTop +
            (ActionButtonHeight + Gap) * 2; //Program File名入力上位置
        MoveWindow(
            ProgramFileNameEditHwnd,
            PageLeft,
            ProgramNameTop,
            PageWidth,
            InspectorRowHeight,
            TRUE
        );
        const int ProgramContentTop = ProgramNameTop + InspectorRowHeight + Gap; //一覧領域上位置
        const int ProgramStatusHeight = ScaleByDpi(LabelHeight); //Program状態表示高さ
        const int ProgramErrorHeight = ScaleByDpi(105); //Compile出力一覧高さ
        const int ProgramErrorTop = std::max(
            ProgramContentTop,
            PageBottom - ProgramErrorHeight
        ); //Compile出力一覧上位置
        const int ProgramStatusTop = std::max(
            ProgramContentTop,
            ProgramErrorTop - ProgramStatusHeight - Gap
        ); //Program状態表示上位置
        const int ProgramSplitAreaHeight = std::max(
            1,
            ProgramStatusTop - ProgramContentTop - Gap
        ); //一覧とEditorが共有する高さ
        const int MinimumProgramListHeight = ScaleByDpi(75); //File及び関数一覧最小高さ
        const int MinimumProgramEditorHeight = ScaleByDpi(120); //Program Editor最小高さ
        const int MaximumProgramListHeight = std::max(
            MinimumProgramListHeight,
            ProgramSplitAreaHeight - InnerSplitter - MinimumProgramEditorHeight
        ); //Editorを確保した一覧最大高さ
        const int ProgramListHeight = std::clamp(
            static_cast<int>(std::lround(
                ProgramSplitAreaHeight * ProgramHorizontalSplitRatio
            )),
            std::min(MinimumProgramListHeight, MaximumProgramListHeight),
            MaximumProgramListHeight
        ); //現在比率を制約した一覧高さ
        const int MinimumProgramColumnWidth = ScaleByDpi(100); //File又は関数一覧最小幅
        const int MaximumFileListWidth = std::max(
            MinimumProgramColumnWidth,
            PageWidth - InnerSplitter - MinimumProgramColumnWidth
        ); //関数一覧を確保したFile一覧最大幅
        const int ProgramFileListWidth = std::clamp(
            static_cast<int>(std::lround(PageWidth * ProgramVerticalSplitRatio)),
            std::min(MinimumProgramColumnWidth, MaximumFileListWidth),
            MaximumFileListWidth
        ); //現在比率を制約したFile一覧幅
        MoveWindow(
            ProgramFileListHwnd,
            PageLeft,
            ProgramContentTop,
            ProgramFileListWidth,
            ProgramListHeight,
            TRUE
        );
        MoveWindow(
            ProgramFunctionListHwnd,
            PageLeft + ProgramFileListWidth + InnerSplitter,
            ProgramContentTop,
            std::max(1, PageWidth - ProgramFileListWidth - InnerSplitter),
            ProgramListHeight,
            TRUE
        );
        const int ProgramEditorTop = ProgramContentTop + ProgramListHeight + InnerSplitter; //Editor上位置
        MoveWindow(
            ProgramEditorHwnd,
            PageLeft,
            ProgramEditorTop,
            PageWidth,
            std::max(1, ProgramStatusTop - ProgramEditorTop - Gap),
            TRUE
        );
        MoveWindow(
            ProgramStatusLabelHwnd,
            PageLeft,
            ProgramStatusTop,
            PageWidth,
            ProgramStatusHeight,
            TRUE
        );
        MoveWindow(
            ProgramErrorListHwnd,
            PageLeft,
            ProgramErrorTop,
            PageWidth,
            ProgramErrorHeight,
            TRUE
        );

        if (ProgramSuggestionListHwnd != nullptr &&
            IsWindowVisible(ProgramSuggestionListHwnd))
        {
            UpdateProgramSuggestions(false);
        }

        UpdateTabVisibility();
    }

    //概要：指定画面座標へ汎用Object型の追加Menuを表示して操作要求を登録する
    //引数：screenPosition=Menu左上の画面座標
    //戻り値：なし
    void WinApp::ShowCreateObjectMenu(const POINT& screenPosition, bool addAsChild)
    {
        HMENU Menu = CreatePopupMenu(); //Object型を選択するPopup Menu

        if (Menu == nullptr)
        {
            return;
        }

        constexpr ObjectType CreatableTypes[] =
        {
            ObjectType::Object,
            ObjectType::Box,
            ObjectType::Sphere,
            ObjectType::Plane,
            ObjectType::Cylinder,
            ObjectType::HalfSphere,
            ObjectType::Capsule
        }; //Editorから既定構築できるObject型

        for (ObjectType Type : CreatableTypes)
        {
            AppendMenuW(
                Menu,
                MF_STRING,
                CreateObjectMenuBase + static_cast<UINT>(Type),
                GetObjectMenuName(Type)
            );
        }

        const UINT SelectedCommand = TrackPopupMenu(
            Menu,
            TPM_RETURNCMD | TPM_RIGHTBUTTON,
            screenPosition.x,
            screenPosition.y,
            0,
            Hwnd,
            nullptr
        ); //利用者が選択したObject型Menu ID
        DestroyMenu(Menu);

        if (SelectedCommand < CreateObjectMenuBase ||
            SelectedCommand > CreateObjectMenuBase + static_cast<UINT>(ObjectType::Capsule))
        {
            return;
        }

        const EditorTreeNode* Node = GetSelectedTreeNode(); //作成先Scene又は親Objectの選択情報
        EditorCommand Command; //EngineAPIへ渡すObject作成要求
        Command.Type = EditorCommandType::CreateObject;
        Command.Scene = Node != nullptr ? Node->Scene : SelectedEditorSceneID;
        Command.Parent = addAsChild && Node != nullptr &&
            Node->Kind != EditorTreeNodeKind::Scene
            ? Node->Object
            : ObjectID();
        Command.ObjectKind = static_cast<ObjectType>(SelectedCommand - CreateObjectMenuBase);
        QueueEditorCommand(std::move(Command));
    }

    //概要：選択Objectへ追加可能な全Native及びDLL Script Menuを表示する
    //引数：screenPosition=Menu左上の画面座標
    //戻り値：なし
    void WinApp::ShowScriptMenu(const POINT& screenPosition)
    {
        const EditorTreeNode* Node = GetSelectedTreeNode(); //Script所有先の選択情報

        if (Node == nullptr || Node->Kind == EditorTreeNodeKind::Scene || !Node->Object.IsValid())
        {
            MessageLog::GetInstance().AddLog(
                "[Info] Editor | Scriptを差し込むObjectを選択してください。"
            );
            return;
        }

        HMENU Menu = CreatePopupMenu(); //登録済みScriptを表示するPopup Menu

        if (Menu == nullptr)
        {
            return;
        }

        for (std::size_t Index = 0; Index < CurrentEditorSnapshot.Scripts.size(); ++Index)
        {
            const EditorScriptInfo& Script = CurrentEditorSnapshot.Scripts[Index]; //Menuへ追加するScript情報
            std::wstring Label = ConvertEditorTextToWide(Script.DisplayName); //Script表示名
            Label += L" [" + ConvertEditorTextToWide(Script.ModuleName) + L"]";
            AppendMenuW(
                Menu,
                MF_STRING,
                ScriptMenuBase + static_cast<UINT>(Index),
                Label.c_str()
            );
        }

        if (CurrentEditorSnapshot.Scripts.empty())
        {
            AppendMenuW(Menu, MF_STRING | MF_GRAYED, ScriptMenuBase, L"Scriptがありません");
        }

        const UINT SelectedCommand = TrackPopupMenu(
            Menu,
            TPM_RETURNCMD | TPM_RIGHTBUTTON,
            screenPosition.x,
            screenPosition.y,
            0,
            Hwnd,
            nullptr
        ); //利用者が選択したScript Menu ID
        DestroyMenu(Menu);

        if (SelectedCommand < ScriptMenuBase)
        {
            return;
        }

        const std::size_t ScriptIndex = static_cast<std::size_t>(
            SelectedCommand - ScriptMenuBase
        ); //Snapshot内のScript位置

        if (ScriptIndex >= CurrentEditorSnapshot.Scripts.size())
        {
            return;
        }

        EditorCommand Command; //EngineAPIへ渡すScript差込要求
        Command.Type = EditorCommandType::AttachScript;
        Command.Scene = Node->Scene;
        Command.Object = Node->Object;
        Command.Text = CurrentEditorSnapshot.Scripts[ScriptIndex].Key;
        QueueEditorCommand(std::move(Command));
    }

    //概要：Treeの空白、Scene、Object、Componentに応じた汎用右Click Menuを表示する
    //引数：screenPosition=Menu左上の画面座標
    //戻り値：なし
    void WinApp::ShowTreeContextMenu(const POINT& screenPosition)
    {
        const EditorTreeNode* Node = GetSelectedTreeNode(); //Context Menu対象のTree要素

        if (Node == nullptr || Node->Kind == EditorTreeNodeKind::Scene)
        {
            HMENU Menu = CreatePopupMenu(); //空白又はScene用Menu
            AppendMenuW(Menu, MF_STRING, CreateObjectMenuBase, L"Object追加...");
            AppendMenuW(Menu, MF_STRING, LoadScriptMenuId, L"Script DLL読込...");
            AppendMenuW(Menu, MF_STRING, RefreshTreeMenuId, L"更新");

            const UINT SelectedCommand = TrackPopupMenu(
                Menu,
                TPM_RETURNCMD | TPM_RIGHTBUTTON,
                screenPosition.x,
                screenPosition.y,
                0,
                Hwnd,
                nullptr
            ); //選択された空白又はScene操作
            DestroyMenu(Menu);

            if (SelectedCommand == CreateObjectMenuBase)
            {
                ShowCreateObjectMenu(screenPosition, false);
            }
            else if (SelectedCommand == LoadScriptMenuId)
            {
                OpenScriptModuleDialog();
            }
            else if (SelectedCommand == RefreshTreeMenuId)
            {
                EditorCommand Command; //Engine Snapshot更新要求
                Command.Type = EditorCommandType::Refresh;
                QueueEditorCommand(std::move(Command));
            }

            return;
        }

        HMENU Menu = CreatePopupMenu(); //Object又はComponent用Menu
        HMENU ScriptMenu = nullptr; //Object用のScript差込Sub Menu

        if (Node->Kind == EditorTreeNodeKind::Object)
        {
            ScriptMenu = CreatePopupMenu();
            AppendMenuW(Menu, MF_STRING, AddChildObjectMenuId, L"子Objectを追加...");

            const EditorObjectInfo* ObjectInformation = GetSelectedObjectInfo(); //親解除可否を確認するObject

            if (ObjectInformation != nullptr && ObjectInformation->ParentID.IsValid())
            {
                AppendMenuW(Menu, MF_STRING, DetachParentMenuId, L"親から外す");
            }

            AppendMenuW(Menu, MF_SEPARATOR, 0, nullptr);

            for (std::size_t Index = 0; Index < CurrentEditorSnapshot.Scripts.size(); ++Index)
            {
                const EditorScriptInfo& Script = CurrentEditorSnapshot.Scripts[Index]; //Sub Menuへ追加するScript
                std::wstring Label = ConvertEditorTextToWide(Script.DisplayName); //Script表示名
                Label += L" [" + ConvertEditorTextToWide(Script.ModuleName) + L"]";
                AppendMenuW(
                    ScriptMenu,
                    MF_STRING,
                    ScriptMenuBase + static_cast<UINT>(Index),
                    Label.c_str()
                );
            }

            if (CurrentEditorSnapshot.Scripts.empty())
            {
                AppendMenuW(ScriptMenu, MF_STRING | MF_GRAYED, ScriptMenuBase, L"Scriptがありません");
            }

            AppendMenuW(
                Menu,
                MF_POPUP,
                reinterpret_cast<UINT_PTR>(ScriptMenu),
                L"Scriptを差し込む"
            );
            AppendMenuW(Menu, MF_STRING, DuplicateObjectMenuId, L"複製");
        }

        AppendMenuW(Menu, MF_STRING, RenameItemMenuId, L"名前変更");
        AppendMenuW(
            Menu,
            MF_STRING,
            ToggleActiveMenuId,
            Node->Active ? L"無効化" : L"有効化"
        );
        AppendMenuW(Menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(Menu, MF_STRING, DeleteItemMenuId, L"削除");

        const UINT SelectedCommand = TrackPopupMenu(
            Menu,
            TPM_RETURNCMD | TPM_RIGHTBUTTON,
            screenPosition.x,
            screenPosition.y,
            0,
            Hwnd,
            nullptr
        ); //選択されたObject又はComponent操作

        if (SelectedCommand == RenameItemMenuId)
        {
            TreeView_EditLabel(ObjectTreeHwnd, TreeView_GetSelection(ObjectTreeHwnd));
        }
        else if (SelectedCommand == AddChildObjectMenuId)
        {
            ShowCreateObjectMenu(screenPosition, true);
        }
        else if (SelectedCommand == DetachParentMenuId)
        {
            EditorCommand Command; //World姿勢を維持する親解除要求
            Command.Type = EditorCommandType::SetObjectParent;
            Command.Scene = Node->Scene;
            Command.Object = Node->Object;
            Command.Parent = ObjectID();
            Command.KeepWorldTransform = true;
            QueueEditorCommand(std::move(Command));
        }
        else if (SelectedCommand >= ScriptMenuBase)
        {
            const std::size_t ScriptIndex = static_cast<std::size_t>(
                SelectedCommand - ScriptMenuBase
            ); //Snapshot内のScript位置

            if (ScriptIndex < CurrentEditorSnapshot.Scripts.size())
            {
                EditorCommand Command; //EngineAPIへ渡すScript差込要求
                Command.Type = EditorCommandType::AttachScript;
                Command.Scene = Node->Scene;
                Command.Object = Node->Object;
                Command.Text = CurrentEditorSnapshot.Scripts[ScriptIndex].Key;
                QueueEditorCommand(std::move(Command));
            }
        }
        else if (SelectedCommand != 0)
        {
            EditorCommand Command; //EngineAPIへ渡すObject又はComponent操作
            Command.Scene = Node->Scene;
            Command.Object = Node->Object;
            Command.Component = Node->Component;

            if (SelectedCommand == DuplicateObjectMenuId)
            {
                Command.Type = EditorCommandType::DuplicateObject;
            }
            else if (SelectedCommand == ToggleActiveMenuId)
            {
                Command.Type = Node->Kind == EditorTreeNodeKind::Object
                    ? EditorCommandType::ToggleObjectActive
                    : EditorCommandType::ToggleComponentActive;
            }
            else if (SelectedCommand == DeleteItemMenuId)
            {
                Command.Type = Node->Kind == EditorTreeNodeKind::Object
                    ? EditorCommandType::DeleteObject
                    : EditorCommandType::DeleteComponent;
            }

            if (Command.Type != EditorCommandType::None)
            {
                QueueEditorCommand(std::move(Command));
            }
        }

        DestroyMenu(Menu);
    }

    //概要：Windows File DialogからDLLを選びScript Module読込要求を登録する
    //引数：なし
    //戻り値：なし
    void WinApp::OpenScriptModuleDialog()
    {
        wchar_t SelectedPath[32768]{}; //利用者が選択したDLL絶対Path
        OPENFILENAMEW DialogInformation{}; //Script DLL選択Dialog設定
        DialogInformation.lStructSize = sizeof(OPENFILENAMEW);
        DialogInformation.hwndOwner = Hwnd;
        DialogInformation.lpstrFilter = L"Script DLL (*.dll)\0*.dll\0すべてのファイル (*.*)\0*.*\0\0";
        DialogInformation.lpstrFile = SelectedPath;
        DialogInformation.nMaxFile = static_cast<DWORD>(std::size(SelectedPath));
        DialogInformation.lpstrTitle = L"Script Module DLLを読み込む";
        DialogInformation.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
            OFN_EXPLORER | OFN_DONTADDTORECENT;

        if (!GetOpenFileNameW(&DialogInformation))
        {
            return;
        }

        EditorCommand Command; //EngineAPIへ渡すDLL読込要求
        Command.Type = EditorCommandType::LoadScriptModule;
        Command.Path = SelectedPath;
        QueueEditorCommand(std::move(Command));
    }

    //概要：TreeView ItemのlParamから現在選択中のEngine要素を取得する
    //引数：なし
    //戻り値：選択要素、未選択又は不整合時はnullptr
    const WinApp::EditorTreeNode* WinApp::GetSelectedTreeNode() const
    {
        if (ObjectTreeHwnd == nullptr)
        {
            return nullptr;
        }

        HTREEITEM SelectedItem = TreeView_GetSelection(ObjectTreeHwnd); //現在選択中のTree Item

        if (SelectedItem == nullptr)
        {
            return nullptr;
        }

        TVITEMW ItemInformation{}; //Node表Indexを取得するTree Item情報
        ItemInformation.mask = TVIF_PARAM;
        ItemInformation.hItem = SelectedItem;

        if (!TreeView_GetItem(ObjectTreeHwnd, &ItemInformation) || ItemInformation.lParam <= 0)
        {
            return nullptr;
        }

        const std::size_t NodeIndex = static_cast<std::size_t>(
            ItemInformation.lParam - 1
        ); //EditorTreeNodes内の位置
        return NodeIndex < EditorTreeNodes.size()
            ? &EditorTreeNodes[NodeIndex]
            : nullptr;
    }

    //概要：指定Tree ItemのlParamからEngine要素情報を取得する
    //引数：item=検索するTree Item
    //戻り値：対応要素、空Item又は不整合時はnullptr
    const WinApp::EditorTreeNode* WinApp::GetTreeNode(HTREEITEM item) const
    {
        if (ObjectTreeHwnd == nullptr || item == nullptr)
        {
            return nullptr;
        }

        TVITEMW Information{}; //Node表Indexを取得するTree Item情報
        Information.mask = TVIF_PARAM;
        Information.hItem = item;

        if (!TreeView_GetItem(ObjectTreeHwnd, &Information) || Information.lParam <= 0)
        {
            return nullptr;
        }

        const std::size_t Index = static_cast<std::size_t>(
            Information.lParam - 1
        ); //EditorTreeNodes内の位置
        return Index < EditorTreeNodes.size() ? &EditorTreeNodes[Index] : nullptr;
    }

    //概要：EngineAPIへ渡すEditor操作をFIFO Queueへ追加する
    //引数：command=追加するObject、Component又はScript操作
    //戻り値：なし
    void WinApp::QueueEditorCommand(EditorCommand command)
    {
        PendingEditorCommands.emplace_back(std::move(command));
    }

    //概要：Main ProgramとObject Scriptの保存先及びCompile環境を初期化する
    //引数：なし
    //戻り値：Program Workspaceを利用可能にできた場合はtrue
    bool WinApp::InitializeProgramWorkspace()
    {
        const bool MainReady = Programs.Initialize(); //Main Program Workspace初期化結果
        const bool ScriptReady = ScriptPrograms.Initialize(); //Object Script Workspace初期化結果
        ProgramWorkspaceReady = MainReady && ScriptReady;

        if (!ProgramWorkspaceReady)
        {
            SetWindowTextW(
                ProgramStatusLabelHwnd,
                L"メイン又はスクリプトのWorkspaceを初期化できませんでした"
            );
            EnableWindow(NewProgramButtonHwnd, FALSE);
            EnableWindow(RenameProgramButtonHwnd, FALSE);
            EnableWindow(DeleteProgramButtonHwnd, FALSE);
            EnableWindow(SaveProgramButtonHwnd, FALSE);
            EnableWindow(CompileProgramButtonHwnd, FALSE);
            EnableWindow(RestoreProgramButtonHwnd, FALSE);
            return false;
        }

        SetWindowTextW(
            ProgramStatusLabelHwnd,
            Programs.GetDirectory().wstring().c_str()
        );
        SetWindowTextW(
            ProgramRoleLabelHwnd,
            L"メイン：毎フレームの最初に実行し、Scene／Object／Script全体を制御します。"
        );
        EditingScriptWorkspace = false;
        RefreshExternalProgramSuggestions();
        RefreshProgramFiles();
        ProgramEditRevision = 1;
        ProgramSavedRevision = 1;
        ProgramCompilePending = true;
        ProgramPendingManualCompile = false;
        LastProgramEditTime = std::chrono::steady_clock::now();
        ScriptPrograms.StartBackgroundCompile(0, true);
        EnableWindow(
            RestoreProgramButtonHwnd,
            Programs.HasLastSuccessfulSnapshot()
        );
        return true;
    }

    //概要：現在Tabが編集対象とするMain又はScript Workspaceを取得する
    //引数：なし
    //戻り値：現在表示中のProgram Workspace参照
    ProgramWorkspace& WinApp::GetActiveProgramWorkspace()
    {
        return EditingScriptWorkspace ? ScriptPrograms : Programs;
    }

    //概要：現在TabがMain又はScriptのSource編集Pageか判定する
    //引数：なし
    //戻り値：メイン又はスクリプトTabの場合true
    bool WinApp::IsProgramSourceTab() const
    {
        return ActiveTabIndex == 3 || ActiveTabIndex == 4;
    }

    //概要：未保存内容を確定してMainとObject Scriptの編集Workspaceを切り替える
    //引数：scriptWorkspace=スクリプトTabを表示する場合true
    //戻り値：なし
    void WinApp::SwitchProgramWorkspace(bool scriptWorkspace)
    {
        if (EditingScriptWorkspace == scriptWorkspace)
        {
            return;
        }

        if (ProgramDirty && !SaveCurrentProgram())
        {
            ActiveTabIndex = EditingScriptWorkspace ? 4 : 3;
            TabCtrl_SetCurSel(EditorTabHwnd, ActiveTabIndex);
            return;
        }

        HideProgramSuggestions();
        EditingScriptWorkspace = scriptWorkspace;
        CurrentProgramPath.clear();
        ProgramFiles.clear();
        ProgramFunctions.clear();
        ProgramSuggestions.clear();
        ProgramDirty = false;
        ProgramCompilePending = false;
        ProgramPendingManualCompile = false;

        if (ProgramEditRevision < (std::numeric_limits<std::uint64_t>::max)())
        {
            ++ProgramEditRevision;
        }

        ProgramSavedRevision = ProgramEditRevision;
        SetWindowTextW(
            ProgramRoleLabelHwnd,
            scriptWorkspace
                ? L"スクリプト（サブ）：ObjectへAttachし、Unityのスクリプト相当として毎フレーム実行します。"
                : L"メイン：SceneごとのInit／Update／Endを名前指定の簡易APIで実装します。"
        );
        RefreshProgramFiles();
        EnableWindow(
            CompileProgramButtonHwnd,
            !GetActiveProgramWorkspace().IsCompiling()
        );
        EnableWindow(
            RestoreProgramButtonHwnd,
            GetActiveProgramWorkspace().HasLastSuccessfulSnapshot() &&
                !GetActiveProgramWorkspace().IsCompiling()
        );
    }

    //概要：Engine API Header、Template、外部追加Sourceから補完可能な識別子を再検出する
    //引数：なし
    //戻り値：なし
    void WinApp::RefreshExternalProgramSuggestions()
    {
        ExternalProgramSuggestions.clear();

        if (!ProgramWorkspaceReady)
        {
            return;
        }

        const std::filesystem::path ProjectRoot = Programs.GetDirectory().parent_path(); //Engine Project Root
        std::vector<std::filesystem::path> SourceFiles
        {
            ProjectRoot / L"EngineAPI.h",
            ProjectRoot / L"EngineExtensionAPI.h",
            ProjectRoot / L"GameEngineAPI.h",
            ProjectRoot / L"ScriptModuleAPI.h",
            ProjectRoot / L"GameScriptAPI.h",
            ProjectRoot / L"GameObjectTemplate.h",
            ProjectRoot / L"Engine.h",
            ProjectRoot / L"GameApp.h",
            ProjectRoot / L"Object.h",
            ProjectRoot / L"Transform.h",
            ProjectRoot / L"Scene.h",
            ProjectRoot / L"SceneManager.h",
            ProjectRoot / L"ObjectManager.h",
            ProjectRoot / L"PrimitiveObject.h",
            ProjectRoot / L"DirectX12.h"
        }; //常に候補化する内部及び外部API Header
        const std::filesystem::path ScanRoots[] =
        {
            ProjectRoot / L"Templates",
            Programs.GetDirectory(),
            ScriptPrograms.GetDirectory()
        }; //利用者が追加したTemplateとSourceを検出するDirectory
        std::error_code Error; //例外を使わないFile走査結果

        for (const std::filesystem::path& Root : ScanRoots)
        {
            if (!std::filesystem::exists(Root, Error))
            {
                Error.clear();
                continue;
            }

            for (std::filesystem::recursive_directory_iterator Iterator(
                    Root,
                    std::filesystem::directory_options::skip_permission_denied,
                    Error
                ), End;
                !Error && Iterator != End && SourceFiles.size() < 256;
                Iterator.increment(Error))
            {
                if (!Iterator->is_regular_file(Error))
                {
                    continue;
                }

                std::wstring Extension = Iterator->path().extension().wstring(); //小文字化して判定する拡張子
                std::transform(
                    Extension.begin(),
                    Extension.end(),
                    Extension.begin(),
                    [](wchar_t character)
                    {
                        return static_cast<wchar_t>(std::towlower(character));
                    }
                );

                if (Extension == L".h" || Extension == L".hpp" ||
                    Extension == L".cpp" || Extension == L".cxx")
                {
                    SourceFiles.emplace_back(Iterator->path());
                }
            }

            Error.clear();
        }

        std::unordered_set<std::wstring> Identifiers; //全Fileから重複を除いた補完候補
        std::unordered_set<std::wstring> ScannedPaths; //重複したHeader Pathを除く集合

        for (const std::filesystem::path& SourcePath : SourceFiles)
        {
            const std::wstring NormalPath = SourcePath.lexically_normal().wstring(); //重複確認用Path

            if (!ScannedPaths.emplace(NormalPath).second)
            {
                continue;
            }

            const std::uintmax_t FileSize = std::filesystem::file_size(SourcePath, Error); //過大File除外用Byte数

            if (Error || FileSize > 2u * 1024u * 1024u)
            {
                Error.clear();
                continue;
            }

            std::ifstream Stream(SourcePath, std::ios::binary); //UTF-8又はANSI Source入力

            if (!Stream)
            {
                continue;
            }

            const std::string Bytes(
                (std::istreambuf_iterator<char>(Stream)),
                std::istreambuf_iterator<char>()
            ); //識別子を抽出するSource Byte列
            const std::wstring Text = ConvertEditorTextToWide(Bytes); //走査用UTF-16 Source
            std::size_t Index = 0; //現在の識別子走査位置

            while (Index < Text.size())
            {
                const wchar_t First = Text[Index]; //識別子先頭候補

                if (!((First >= L'A' && First <= L'Z') ||
                    (First >= L'a' && First <= L'z') || First == L'_'))
                {
                    ++Index;
                    continue;
                }

                const std::size_t Begin = Index++; //識別子開始位置

                while (Index < Text.size())
                {
                    const wchar_t Character = Text[Index]; //識別子継続候補

                    if (!((Character >= L'A' && Character <= L'Z') ||
                        (Character >= L'a' && Character <= L'z') ||
                        (Character >= L'0' && Character <= L'9') || Character == L'_'))
                    {
                        break;
                    }

                    ++Index;
                }

                const std::size_t Length = Index - Begin; //候補識別子の文字数

                if (Length >= 2 && Length <= 80)
                {
                    Identifiers.emplace(Text.substr(Begin, Length));
                }
            }
        }

        ExternalProgramSuggestions.assign(Identifiers.begin(), Identifiers.end());

        for (const std::string& Suggestion :
            ProgramSuggestionRegistry::GetInstance().GetSnapshot())
        {
            const std::wstring WideSuggestion = ConvertEditorTextToWide(Suggestion); //外部Set API候補のUTF-16名

            if (!WideSuggestion.empty())
            {
                ExternalProgramSuggestions.emplace_back(WideSuggestion);
            }
        }

        std::sort(ExternalProgramSuggestions.begin(), ExternalProgramSuggestions.end());
        ExternalProgramSuggestions.erase(
            std::unique(
                ExternalProgramSuggestions.begin(),
                ExternalProgramSuggestions.end()
            ),
            ExternalProgramSuggestions.end()
        );
        ExternalSuggestionRevision =
            ProgramSuggestionRegistry::GetInstance().GetRevision();
    }

    //概要：Programs DirectoryのSource一覧を更新して希望ファイルを選択する
    //引数：preferredPath=更新後に選択するPath、空の場合は現在Path
    //戻り値：なし
    void WinApp::RefreshProgramFiles(const std::filesystem::path& preferredPath)
    {
        if (!ProgramWorkspaceReady || ProgramFileListHwnd == nullptr)
        {
            return;
        }

        const std::filesystem::path SelectionPath = preferredPath.empty()
            ? CurrentProgramPath
            : preferredPath; //一覧更新後に復元するSource Path
        ProgramFiles = GetActiveProgramWorkspace().GetSourceFiles();
        SendMessageW(ProgramFileListHwnd, LB_RESETCONTENT, 0, 0);
        int SelectionIndex = -1; //一覧更新後に選択するIndex

        for (std::size_t Index = 0; Index < ProgramFiles.size(); ++Index)
        {
            const std::wstring FileName = ProgramFiles[Index].filename().wstring(); //一覧表示名
            SendMessageW(
                ProgramFileListHwnd,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(FileName.c_str())
            );

            if (!SelectionPath.empty() &&
                ProgramFiles[Index].lexically_normal() == SelectionPath.lexically_normal())
            {
                SelectionIndex = static_cast<int>(Index);
            }
        }

        if (SelectionIndex < 0 && !ProgramFiles.empty())
        {
            SelectionIndex = 0;
        }

        SendMessageW(ProgramFileListHwnd, LB_SETCURSEL, SelectionIndex, 0);

        if (SelectionIndex >= 0)
        {
            LoadSelectedProgram();
        }
        else
        {
            CurrentProgramPath.clear();
            HideProgramSuggestions();
            UpdatingProgramEditor = true;
            SetWindowTextW(ProgramEditorHwnd, L"");
            SetWindowTextW(ProgramFileNameEditHwnd, L"");
            UpdatingProgramEditor = false;
            SendMessageW(ProgramFunctionListHwnd, LB_RESETCONTENT, 0, 0);
            ProgramFunctions.clear();
            SetWindowTextW(
                ProgramStatusLabelHwnd,
                EditingScriptWorkspace
                    ? L"Scriptファイルがありません"
                    : L"Main Programファイルがありません"
            );
        }
    }

    //概要：現在Editor内容を選択ProgramファイルへUTF-8保存する
    //引数：なし
    //戻り値：Sourceを保存できた場合はtrue
    bool WinApp::SaveCurrentProgram()
    {
        if (!ProgramWorkspaceReady || CurrentProgramPath.empty())
        {
            return false;
        }

        ProgramWorkspace& Workspace = GetActiveProgramWorkspace(); //明示保存する現在Workspace
        Workspace.WaitForBackgroundSave();
        ProgramSaveResult DiscardedResult; //明示保存で置換する未取得Background結果
        Workspace.PollBackgroundSave(DiscardedResult);
        const std::wstring Text = GetControlText(ProgramEditorHwnd); //保存するEditor全文

        if (!Workspace.SaveSourceFile(CurrentProgramPath, Text))
        {
            SetWindowTextW(ProgramStatusLabelHwnd, L"保存に失敗しました");
            return false;
        }

        ProgramDirty = false;
        ProgramSavedRevision = ProgramEditRevision;
        SetWindowTextW(ProgramStatusLabelHwnd, L"保存しました");
        return true;
    }

    //概要：現在Editor全文を一度だけ複製してBackground保存Workerへ渡す
    //引数：なし
    //戻り値：非同期保存を開始できた場合true
    bool WinApp::StartBackgroundSaveCurrentProgram()
    {
        if (!ProgramWorkspaceReady || CurrentProgramPath.empty())
        {
            return false;
        }

        ProgramWorkspace& Workspace = GetActiveProgramWorkspace(); //保存先Main又はScript Workspace

        if (Workspace.IsSaving())
        {
            return false;
        }

        std::wstring TextSnapshot = GetControlText(ProgramEditorHwnd); //UI Threadで一度だけ確定する全文Copy

        if (!Workspace.StartBackgroundSave(
            CurrentProgramPath,
            std::move(TextSnapshot),
            ProgramEditRevision
        ))
        {
            return false;
        }

        ProgramDirty = false;
        SetWindowTextW(ProgramStatusLabelHwnd, L"別スレッドで自動保存中...");
        return true;
    }

    //概要：Main又はScriptのBackground保存結果をEditor状態へ反映する
    //引数：workspace=結果を確認するWorkspace、scriptWorkspace=Script側の場合true
    //戻り値：なし
    void WinApp::ProcessWorkspaceSaveResult(
        ProgramWorkspace& workspace,
        bool scriptWorkspace
    )
    {
        ProgramSaveResult Result; //Workerから受け取る保存結果

        if (!workspace.PollBackgroundSave(Result))
        {
            return;
        }

        const bool ActiveWorkspace = EditingScriptWorkspace == scriptWorkspace; //現在表示中Workspaceの場合true

        if (!Result.Succeeded)
        {
            if (ActiveWorkspace &&
                Result.SourcePath.lexically_normal() == CurrentProgramPath.lexically_normal())
            {
                ProgramDirty = true;
                SetWindowTextW(ProgramStatusLabelHwnd, L"バックグラウンド保存に失敗しました");
            }

            MessageLog::GetInstance().AddLog(
                "[Error] Program | Background source save failed."
            );
            return;
        }

        if (ActiveWorkspace)
        {
            ProgramSavedRevision = std::max(ProgramSavedRevision, Result.Revision);

            if (Result.Revision == ProgramEditRevision &&
                Result.SourcePath.lexically_normal() == CurrentProgramPath.lexically_normal())
            {
                ProgramDirty = false;
                SetWindowTextW(ProgramStatusLabelHwnd, L"自動保存済み");
            }
        }
    }

    //概要：File一覧で選択したProgramをEditorへ読み込む
    //引数：なし
    //戻り値：なし
    void WinApp::LoadSelectedProgram()
    {
        const LRESULT Selection = SendMessageW(
            ProgramFileListHwnd,
            LB_GETCURSEL,
            0,
            0
        ); //読み込むProgram一覧Index

        if (Selection < 0 || static_cast<std::size_t>(Selection) >= ProgramFiles.size())
        {
            return;
        }

        const std::filesystem::path SourcePath = ProgramFiles[
            static_cast<std::size_t>(Selection)
        ]; //読み込むSource Path
        std::wstring Text; //読み込んだSource全文

        if (!GetActiveProgramWorkspace().LoadSourceFile(SourcePath, Text))
        {
            SetWindowTextW(ProgramStatusLabelHwnd, L"Programファイルを読み込めませんでした");
            return;
        }

        CurrentProgramPath = SourcePath;
        HideProgramSuggestions();
        UpdatingProgramEditor = true;
        SetWindowTextW(ProgramEditorHwnd, Text.c_str());
        SetWindowTextW(
            ProgramFileNameEditHwnd,
            CurrentProgramPath.filename().wstring().c_str()
        );
        UpdatingProgramEditor = false;
        ProgramDirty = false;
        SendMessageW(ProgramEditorHwnd, EM_EMPTYUNDOBUFFER, 0, 0);
        HighlightProgramText();
        RebuildProgramFunctionList();
        SetWindowTextW(ProgramStatusLabelHwnd, L"読込済み");
    }

    //概要：File名入力へ従って現在Programの名前を変更する
    //引数：なし
    //戻り値：なし
    void WinApp::RenameCurrentProgram()
    {
        if (CurrentProgramPath.empty() || !SaveCurrentProgram())
        {
            return;
        }

        const std::wstring RequestedName = GetControlText(
            ProgramFileNameEditHwnd
        ); //利用者が入力した新しいファイル名
        std::filesystem::path RenamedPath; //名前変更後のSource Path

        if (!GetActiveProgramWorkspace().RenameSourceFile(
            CurrentProgramPath,
            RequestedName,
            RenamedPath
        ))
        {
            SetWindowTextW(
                ProgramStatusLabelHwnd,
                L"名前変更に失敗しました。重複名と拡張子を確認してください"
            );
            return;
        }

        CurrentProgramPath = RenamedPath;
        RefreshProgramFiles(RenamedPath);
        ProgramCompilePending = true;
        LastProgramEditTime = std::chrono::steady_clock::now();

        if (ProgramEditRevision < (std::numeric_limits<std::uint64_t>::max)())
        {
            ++ProgramEditRevision;
        }

        ProgramSavedRevision = ProgramEditRevision;
        SetWindowTextW(ProgramStatusLabelHwnd, L"名前を変更しました");
    }

    //概要：確認後に現在Programファイルを削除する
    //引数：なし
    //戻り値：なし
    void WinApp::DeleteCurrentProgram()
    {
        if (CurrentProgramPath.empty())
        {
            return;
        }

        const int Answer = MessageBoxW(
            Hwnd,
            (L"削除しますか？\n" + CurrentProgramPath.filename().wstring()).c_str(),
            L"Programファイルの削除",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
        ); //利用者の削除確認結果

        if (Answer != IDYES)
        {
            return;
        }

        if (!GetActiveProgramWorkspace().DeleteSourceFile(CurrentProgramPath))
        {
            SetWindowTextW(ProgramStatusLabelHwnd, L"削除に失敗しました");
            return;
        }

        CurrentProgramPath.clear();
        ProgramDirty = false;
        ProgramCompilePending = true;
        LastProgramEditTime = std::chrono::steady_clock::now();

        if (ProgramEditRevision < (std::numeric_limits<std::uint64_t>::max)())
        {
            ++ProgramEditRevision;
        }

        ProgramSavedRevision = ProgramEditRevision;
        RefreshProgramFiles();
    }

    //概要：現在Source一式を退避して最後にCompile成功したWorkspace状態へ復元する
    //引数：なし
    //戻り値：なし
    void WinApp::RestoreLastSuccessfulProgram()
    {
        ProgramWorkspace& Workspace = GetActiveProgramWorkspace(); //復元対象のMain又はScript Workspace

        if (Workspace.IsCompiling())
        {
            MessageBoxW(
                Hwnd,
                L"コンパイル完了後に正常版への復元を実行してください。",
                L"正常版へ戻す",
                MB_OK | MB_ICONINFORMATION
            );
            return;
        }

        if (!Workspace.HasLastSuccessfulSnapshot())
        {
            MessageBoxW(
                Hwnd,
                L"このWorkspaceにはコンパイル成功済みのSourceがまだありません。",
                L"正常版へ戻す",
                MB_OK | MB_ICONINFORMATION
            );
            return;
        }

        if (ProgramDirty && !SaveCurrentProgram())
        {
            return;
        }

        const int Answer = MessageBoxW(
            Hwnd,
            L"Source一式を最後にコンパイル成功した状態へ戻します。\n現在の内容は.recoveryへ退避されます。",
            L"正常版へ戻す",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
        ); //利用者のWorkspace復元確認結果

        if (Answer != IDYES)
        {
            return;
        }

        const std::filesystem::path PreferredPath = CurrentProgramPath; //復元後に再選択するSource Path
        std::filesystem::path RecoveryDirectory; //復元前Source一式の退避先

        if (!Workspace.RestoreLastSuccessfulSnapshot(RecoveryDirectory))
        {
            SetWindowTextW(ProgramStatusLabelHwnd, L"正常版への復元に失敗しました");
            MessageLog::GetInstance().AddLog(
                "[Error] Program | Failed to restore the last successful source snapshot."
            );
            return;
        }

        ProgramDirty = false;
        ProgramCompilePending = false;
        ProgramPendingManualCompile = false;

        if (ProgramEditRevision < (std::numeric_limits<std::uint64_t>::max)())
        {
            ++ProgramEditRevision;
        }

        ProgramSavedRevision = ProgramEditRevision;
        RefreshProgramFiles(PreferredPath);
        SetWindowTextW(ProgramStatusLabelHwnd, L"最後にコンパイル成功した正常版へ復元しました");
        MessageLog::GetInstance().AddLog(
            "[Info] Program | Restored the last successful source snapshot; previous files were kept in .recovery."
        );
    }

    //概要：未保存Sourceを保存して手動Background Compileを直ちに要求する
    //引数：なし
    //戻り値：なし
    void WinApp::CompilePrograms()
    {
        RequestProgramCompile(false);
    }

    //概要：Timer上で自動保存DebounceとCompile完了、再要求を非Blocking処理する
    //引数：なし
    //戻り値：なし
    void WinApp::ProcessProgramAutomation()
    {
        if (!ProgramWorkspaceReady)
        {
            return;
        }

        ProcessWorkspaceSaveResult(Programs, false);
        ProcessWorkspaceSaveResult(ScriptPrograms, true);
        ProcessWorkspaceCompileResult(Programs, false);
        ProcessWorkspaceCompileResult(ScriptPrograms, true);

        const auto IdleTime = std::chrono::steady_clock::now() -
            LastProgramEditTime; //最終入力からの経過時間

        if (ProgramVisualRefreshPending &&
            IdleTime >= ProgramVisualRefreshDebounce &&
            IsProgramSourceTab())
        {
            ProgramVisualRefreshPending = false;
            HighlightProgramText();
            RebuildProgramFunctionList();
        }

        const std::uint64_t SuggestionRevision =
            ProgramSuggestionRegistry::GetInstance().GetRevision(); //外部Set APIの最新候補Revision

        if (SuggestionRevision != ExternalSuggestionRevision)
        {
            RefreshExternalProgramSuggestions();
        }

        if (!ProgramCompilePending)
        {
            return;
        }

        const bool Manual = ProgramPendingManualCompile; //Debounceを省略する手動要求

        if (!Manual && IdleTime < ProgramCompileDebounce)
        {
            return;
        }

        ProgramWorkspace& Workspace = GetActiveProgramWorkspace(); //自動保存とCompileを行う現在Workspace

        if (ProgramDirty)
        {
            if (!Workspace.IsSaving())
            {
                StartBackgroundSaveCurrentProgram();
            }

            return;
        }

        if (Workspace.IsSaving() || ProgramSavedRevision < ProgramEditRevision)
        {
            SetWindowTextW(ProgramStatusLabelHwnd, L"自動保存の完了待ち...");
            return;
        }

        if (Workspace.IsCompiling())
        {
            SetWindowTextW(
                ProgramStatusLabelHwnd,
                Manual
                    ? L"自動保存済み・手動コンパイル待ち"
                    : L"自動保存済み・再コンパイル待ち"
            );
            return;
        }

        RequestProgramCompile(!Manual);
    }

    //概要：Main又はScript WorkspaceのCompile結果を対応するHot Reload要求へ変換する
    //引数：workspace=結果を確認するWorkspace、scriptWorkspace=Script DLLの場合true
    //戻り値：なし
    void WinApp::ProcessWorkspaceCompileResult(
        ProgramWorkspace& workspace,
        bool scriptWorkspace
    )
    {
        ProgramCompileResult Result; //今回受け取ったBackground Compile結果

        if (!workspace.PollBackgroundCompile(Result))
        {
            return;
        }

        const bool ActiveWorkspace = EditingScriptWorkspace == scriptWorkspace; //結果を現在UIへ表示する場合true

        if (ActiveWorkspace)
        {
            EnableWindow(CompileProgramButtonHwnd, TRUE);
            EnableWindow(
                RestoreProgramButtonHwnd,
                workspace.HasLastSuccessfulSnapshot()
            );
            ShowProgramDiagnostics(Result);
        }

        if (Result.Succeeded && !Result.ModulePath.empty())
        {
            EditorCommand Command; //Main Thread境界へ戻すHot Reload要求
            Command.Type = scriptWorkspace
                ? EditorCommandType::LoadScriptModule
                : EditorCommandType::LoadExtensionModule;
            Command.Path = Result.ModulePath.wstring();
            QueueEditorCommand(std::move(Command));
            RefreshExternalProgramSuggestions();

            if (ActiveWorkspace)
            {
                SetWindowTextW(
                    ProgramStatusLabelHwnd,
                    scriptWorkspace
                        ? L"コンパイル成功（Script登録待ち）"
                        : L"コンパイル成功（Main Hot Reload待ち）"
                );
            }
        }

        MessageLog::GetInstance().AddLog(
            scriptWorkspace
                ? (Result.Succeeded
                    ? "[Info] ScriptProgram | External Script module compiled."
                    : "[Error] ScriptProgram | External Script module compile failed.")
                : (Result.Succeeded
                    ? "[Info] MainProgram | Main extension compiled."
                    : "[Error] MainProgram | Main extension compile failed.")
        );

        if (ActiveWorkspace && Result.Revision < ProgramEditRevision)
        {
            ProgramCompilePending = true;
        }
    }

    //概要：Sourceを保存し簡易構文判定通過時だけBackground DLL Buildを開始する
    //引数：automatic=入力停止後の自動要求の場合true
    //戻り値：Compile Workerを開始できた場合はtrue
    bool WinApp::RequestProgramCompile(bool automatic)
    {
        if (!ProgramWorkspaceReady)
        {
            return false;
        }

        ProgramWorkspace& Workspace = GetActiveProgramWorkspace(); //現在CompileするMain又はScript Workspace

        if (ProgramDirty)
        {
            ProgramCompilePending = true;
            ProgramPendingManualCompile = ProgramPendingManualCompile || !automatic;

            if (!Workspace.IsSaving())
            {
                StartBackgroundSaveCurrentProgram();
            }

            return false;
        }

        if (Workspace.IsSaving() || ProgramSavedRevision < ProgramEditRevision)
        {
            ProgramCompilePending = true;
            ProgramPendingManualCompile = ProgramPendingManualCompile || !automatic;
            SetWindowTextW(ProgramStatusLabelHwnd, L"保存完了後にコンパイルします...");
            return false;
        }

        if (Workspace.IsCompiling())
        {
            ProgramCompilePending = true;
            ProgramPendingManualCompile = ProgramPendingManualCompile || !automatic;
            SetWindowTextW(ProgramStatusLabelHwnd, L"コンパイル要求を待機中...");
            return false;
        }

        const std::wstring Text = GetControlText(ProgramEditorHwnd); //簡易構文判定する現在Source
        const ProgramPreflightResult Preflight = Workspace.AnalyzeCompileReadiness(Text); //実Compile前判定

        if (!Preflight.Ready)
        {
            ProgramCompileResult Failure; //Error一覧へ渡す事前判定失敗結果
            Failure.Revision = ProgramEditRevision;
            Failure.Automatic = automatic;
            Failure.Output = L"簡易構文判定: 行 " + std::to_wstring(Preflight.Line) +
                L" - " + Preflight.Message;
            ShowProgramDiagnostics(Failure);
            ProgramCompilePending = false;
            ProgramPendingManualCompile = false;
            return false;
        }

        ProgramRequestedRevision = ProgramEditRevision;

        if (!Workspace.StartBackgroundCompile(ProgramRequestedRevision, automatic))
        {
            ProgramCompilePending = true;
            ProgramPendingManualCompile = ProgramPendingManualCompile || !automatic;
            return false;
        }

        ProgramCompilePending = false;
        ProgramPendingManualCompile = false;
        SetWindowTextW(
            ProgramStatusLabelHwnd,
            automatic
                ? L"自動保存済み・バックグラウンドコンパイル中..."
                : L"手動バックグラウンドコンパイル中..."
        );
        EnableWindow(CompileProgramButtonHwnd, FALSE);
        EnableWindow(RestoreProgramButtonHwnd, FALSE);
        return true;
    }

    //概要：C++字句を分類し行高を変えずRichEditへ文字色を設定する
    //引数：なし
    //戻り値：なし
    void WinApp::HighlightProgramText()
    {
        if (ProgramEditorHwnd == nullptr)
        {
            return;
        }

        const std::wstring Text = GetControlText(ProgramEditorHwnd); //色分けするSource全文
        CHARRANGE PreviousSelection{}; //色設定後に復元する選択範囲
        SendMessageW(
            ProgramEditorHwnd,
            EM_EXGETSEL,
            0,
            reinterpret_cast<LPARAM>(&PreviousSelection)
        );
        POINT PreviousScrollPosition{}; //色設定後に復元するRichEdit Scroll位置
        SendMessageW(
            ProgramEditorHwnd,
            EM_GETSCROLLPOS,
            0,
            reinterpret_cast<LPARAM>(&PreviousScrollPosition)
        );
        const bool EditorVisible = IsWindowVisible(ProgramEditorHwnd) != FALSE; //非表示Editorを再表示しないための現在状態
        const bool PreviousUpdating = UpdatingProgramEditor; //呼出前の通知抑止状態
        IRichEditOle* RichEditOle = nullptr; //RichEditのText Object Model取得元
        ITextDocument* TextDocument = nullptr; //色分けをUndo履歴から除外するDocument API
        UpdatingProgramEditor = true;

        if (SendMessageW(
                ProgramEditorHwnd,
                EM_GETOLEINTERFACE,
                0,
                reinterpret_cast<LPARAM>(&RichEditOle)
            ) != 0 && RichEditOle != nullptr)
        {
            RichEditOle->QueryInterface(IID_PPV_ARGS(&TextDocument));
            RichEditOle->Release();
        }

        if (TextDocument != nullptr)
        {
            TextDocument->Undo(tomSuspend, nullptr);
        }

        if (EditorVisible)
        {
            SendMessageW(ProgramEditorHwnd, WM_SETREDRAW, FALSE, 0);
        }

        const auto ApplyColor = [this](
            long begin,
            long end,
            COLORREF color
        )
        {
            CHARRANGE Range{ begin, end }; //色を適用する文字範囲
            SendMessageW(
                ProgramEditorHwnd,
                EM_EXSETSEL,
                0,
                reinterpret_cast<LPARAM>(&Range)
            );
            CHARFORMAT2W Format{}; //指定範囲へ適用する文字書式
            Format.cbSize = sizeof(CHARFORMAT2W);
            Format.dwMask = CFM_COLOR;
            Format.crTextColor = color;
            SendMessageW(
                ProgramEditorHwnd,
                EM_SETCHARFORMAT,
                SCF_SELECTION,
                reinterpret_cast<LPARAM>(&Format)
            );
        }; //RichEditの指定範囲へ文字色だけを設定する処理

        ApplyColor(0, static_cast<long>(Text.size()), RGB(30, 30, 30));
        const std::unordered_set<std::wstring> Keywords =
        {
            L"alignas", L"alignof", L"auto", L"break", L"case", L"catch",
            L"class", L"const", L"constexpr", L"continue", L"default", L"delete",
            L"do", L"else", L"enum", L"explicit", L"false", L"for", L"friend",
            L"if", L"inline", L"namespace", L"new", L"noexcept", L"nullptr",
            L"override", L"private", L"protected", L"public", L"return", L"sizeof",
            L"static", L"struct", L"switch", L"template", L"this", L"throw", L"true",
            L"try", L"typedef", L"typename", L"using", L"virtual", L"while"
        }; //C++制御及び宣言Keyword
        const std::unordered_set<std::wstring> Types =
        {
            L"bool", L"char", L"double", L"float", L"int", L"long", L"short",
            L"signed", L"unsigned", L"void", L"wchar_t", L"size_t", L"string",
            L"wstring", L"Object", L"Scene", L"Transform", L"EngineAPI"
        }; //標準及びEngine主要型名
        bool ExpectVariable = false; //型名直後の識別子を変数色にする場合true
        std::size_t Index = 0; //現在解析する文字位置

        while (Index < Text.size())
        {
            const std::size_t Begin = Index; //現在Token開始位置

            if (Text[Index] == L'/' && Index + 1 < Text.size() && Text[Index + 1] == L'/')
            {
                Index += 2;

                while (Index < Text.size() && Text[Index] != L'\n') ++Index;

                ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(0, 128, 0));
                continue;
            }

            if (Text[Index] == L'/' && Index + 1 < Text.size() && Text[Index + 1] == L'*')
            {
                Index += 2;

                while (Index + 1 < Text.size() && !(Text[Index] == L'*' && Text[Index + 1] == L'/')) ++Index;

                Index = std::min(Text.size(), Index + 2);
                ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(0, 128, 0));
                continue;
            }

            if (Text[Index] == L'\"' || Text[Index] == L'\'')
            {
                const wchar_t Quote = Text[Index++]; //文字列又は文字Literal終端記号

                while (Index < Text.size())
                {
                    if (Text[Index] == L'\\' && Index + 1 < Text.size())
                    {
                        Index += 2;
                        continue;
                    }

                    if (Text[Index++] == Quote) break;
                }

                ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(163, 73, 21));
                continue;
            }

            if (Text[Index] == L'#')
            {
                while (Index < Text.size() && Text[Index] != L'\n') ++Index;
                ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(128, 0, 128));
                continue;
            }

            if (std::iswdigit(Text[Index]))
            {
                ++Index;

                while (Index < Text.size() &&
                    (std::iswalnum(Text[Index]) || Text[Index] == L'.' || Text[Index] == L'_')) ++Index;

                ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(128, 0, 160));
                continue;
            }

            if (std::iswalpha(Text[Index]) || Text[Index] == L'_')
            {
                ++Index;

                while (Index < Text.size() &&
                    (std::iswalnum(Text[Index]) || Text[Index] == L'_')) ++Index;

                const std::wstring Token = Text.substr(Begin, Index - Begin); //現在識別子
                std::size_t Next = Index; //空白後の次記号位置

                while (Next < Text.size() && std::iswspace(Text[Next])) ++Next;

                if (Keywords.contains(Token))
                {
                    ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(0, 70, 190));
                    ExpectVariable = false;
                }
                else if (Types.contains(Token) || (!Token.empty() && std::iswupper(Token.front())))
                {
                    ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(0, 90, 170));
                    ExpectVariable = true;
                }
                else if (Next < Text.size() && Text[Next] == L'(')
                {
                    ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(125, 45, 170));
                    ExpectVariable = false;
                }
                else if (ExpectVariable)
                {
                    ApplyColor(static_cast<long>(Begin), static_cast<long>(Index), RGB(0, 130, 145));
                    ExpectVariable = false;
                }

                continue;
            }

            if (!std::iswspace(Text[Index]) && Text[Index] != L'*' && Text[Index] != L'&' &&
                Text[Index] != L':' && Text[Index] != L',')
            {
                ExpectVariable = false;
            }

            ++Index;
        }

        SendMessageW(
            ProgramEditorHwnd,
            EM_EXSETSEL,
            0,
            reinterpret_cast<LPARAM>(&PreviousSelection)
        );
        SendMessageW(
            ProgramEditorHwnd,
            EM_SETSCROLLPOS,
            0,
            reinterpret_cast<LPARAM>(&PreviousScrollPosition)
        );

        if (TextDocument != nullptr)
        {
            TextDocument->Undo(tomResume, nullptr);
            TextDocument->Release();
        }

        if (EditorVisible)
        {
            SendMessageW(ProgramEditorHwnd, WM_SETREDRAW, TRUE, 0);
        }

        UpdatingProgramEditor = PreviousUpdating;

        if (EditorVisible)
        {
            RedrawWindow(
                ProgramEditorHwnd,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_FRAME | RDW_NOERASE
            );
        }
    }

    //概要：Source内の関数定義を抽出してクリック移動一覧を再構築する
    //引数：なし
    //戻り値：なし
    void WinApp::RebuildProgramFunctionList()
    {
        const std::wstring Text = GetControlText(ProgramEditorHwnd); //関数を検索するSource全文
        SendMessageW(ProgramFunctionListHwnd, LB_RESETCONTENT, 0, 0);
        ProgramFunctions.clear();
        const std::unordered_set<std::wstring> Excluded =
        {
            L"if", L"for", L"while", L"switch", L"catch", L"sizeof",
            L"return", L"static_cast", L"dynamic_cast", L"reinterpret_cast"
        }; //関数定義として扱わないKeyword
        std::size_t Index = 0; //現在解析する文字位置
        int Line = 1; //現在位置の一始まり行番号

        while (Index < Text.size())
        {
            if (Text[Index] == L'\n')
            {
                ++Line;
                ++Index;
                continue;
            }

            if (Text[Index] == L'/' && Index + 1 < Text.size() && Text[Index + 1] == L'/')
            {
                while (Index < Text.size() && Text[Index] != L'\n') ++Index;
                continue;
            }

            if (Text[Index] == L'/' && Index + 1 < Text.size() && Text[Index + 1] == L'*')
            {
                Index += 2;

                while (Index + 1 < Text.size() && !(Text[Index] == L'*' && Text[Index + 1] == L'/'))
                {
                    if (Text[Index] == L'\n') ++Line;
                    ++Index;
                }

                Index = std::min(Text.size(), Index + 2);
                continue;
            }

            if (Text[Index] == L'\"' || Text[Index] == L'\'')
            {
                const wchar_t Quote = Text[Index++]; //読み飛ばすLiteral終端記号

                while (Index < Text.size())
                {
                    if (Text[Index] == L'\\' && Index + 1 < Text.size())
                    {
                        Index += 2;
                        continue;
                    }

                    if (Text[Index] == L'\n') ++Line;
                    if (Text[Index++] == Quote) break;
                }

                continue;
            }

            if (!std::iswalpha(Text[Index]) && Text[Index] != L'_')
            {
                ++Index;
                continue;
            }

            const std::size_t NameBegin = Index++; //関数名候補開始位置

            while (Index < Text.size() &&
                (std::iswalnum(Text[Index]) || Text[Index] == L'_')) ++Index;

            const std::wstring Name = Text.substr(NameBegin, Index - NameBegin); //関数名候補
            std::size_t Open = Index; //引数開始括弧候補位置

            while (Open < Text.size() && std::iswspace(Text[Open])) ++Open;

            if (Open >= Text.size() || Text[Open] != L'(' || Excluded.contains(Name))
            {
                continue;
            }

            int Depth = 1; //対応する閉じ括弧検索深さ
            std::size_t Close = Open + 1; //引数終端検索位置

            while (Close < Text.size() && Depth > 0)
            {
                if (Text[Close] == L'(') ++Depth;
                else if (Text[Close] == L')') --Depth;
                ++Close;
            }

            if (Depth != 0)
            {
                continue;
            }

            std::size_t Definition = Close; //関数本体開始候補位置

            while (Definition < Text.size() && Definition < Close + 240 &&
                Text[Definition] != L'{' && Text[Definition] != L';') ++Definition;

            if (Definition >= Text.size() || Text[Definition] != L'{')
            {
                continue;
            }

            ProgramFunctionInfo Information; //一覧へ登録する関数位置情報
            Information.Name = Name;
            Information.CharacterIndex = static_cast<long>(NameBegin);
            Information.Line = Line;
            ProgramFunctions.emplace_back(Information);
            const std::wstring Label = Name + L"  (行 " + std::to_wstring(Line) + L")"; //関数一覧表示
            SendMessageW(
                ProgramFunctionListHwnd,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(Label.c_str())
            );
        }
    }

    //概要：現在の識別子へ一致するC++、Engine API、Source内候補をキャレット付近へ表示する
    //引数：forceDisplay=Prefixが空でもCtrl+Space候補を表示する場合true
    //戻り値：なし
    void WinApp::UpdateProgramSuggestions(bool forceDisplay)
    {
        if (ProgramEditorHwnd == nullptr || ProgramSuggestionListHwnd == nullptr ||
            !IsProgramSourceTab() || UpdatingProgramEditor)
        {
            HideProgramSuggestions();
            return;
        }

        long TokenBegin = 0; //入力中識別子の開始位置
        long CaretPosition = 0; //補完文字列を挿入するCaret位置
        std::wstring Prefix; //候補を絞り込む入力済み識別子

        if (!GetProgramCompletionRange(TokenBegin, CaretPosition, Prefix))
        {
            HideProgramSuggestions();
            return;
        }

        if (!forceDisplay && Prefix.size() < 2)
        {
            HideProgramSuggestions();
            return;
        }

        std::wstring Owner; //Member補完時の`.`又は`->`直前にある識別子

        if (TokenBegin > 0)
        {
            const long ContextBegin = std::max(0L, TokenBegin - 96);
            std::wstring Context(
                static_cast<std::size_t>(TokenBegin - ContextBegin) + 1,
                L'\0'
            );
            TEXTRANGEW Range{};
            Range.chrg = CHARRANGE{ ContextBegin, TokenBegin };
            Range.lpstrText = Context.data();
            const LRESULT Length = SendMessageW(
                ProgramEditorHwnd,
                EM_GETTEXTRANGE,
                0,
                reinterpret_cast<LPARAM>(&Range)
            );

            if (Length > 0)
            {
                Context.resize(static_cast<std::size_t>(Length));
                std::size_t End = Context.size();

                while (End > 0 && std::iswspace(Context[End - 1]))
                {
                    --End;
                }

                if (End > 0 && Context[End - 1] == L'.')
                {
                    --End;
                }
                else if (End > 1 && Context[End - 2] == L'-' &&
                    Context[End - 1] == L'>')
                {
                    End -= 2;
                }
                else
                {
                    End = 0;
                }

                std::size_t Begin = End;

                while (Begin > 0 && (std::iswalnum(Context[Begin - 1]) ||
                    Context[Begin - 1] == L'_'))
                {
                    --Begin;
                }

                if (Begin < End)
                {
                    Owner = Context.substr(Begin, End - Begin);
                }
            }
        }

        std::unordered_set<std::wstring> CandidateSet; //重複を除いた全候補

        const auto AddCandidates = [&CandidateSet](
            std::initializer_list<const wchar_t*> candidates)
            {
                for (const wchar_t* Candidate : candidates)
                {
                    CandidateSet.emplace(Candidate);
                }
            };

        if (Owner == L"AddObject")
        {
            AddCandidates({ L"Create", L"CreateCapsuleModel", L"CreateBox",
                L"CreateSphere", L"CreatePlane", L"CreateCylinder", L"CreateMany",
                L"CreateBoxes", L"CreateCapsules" });
        }
        else if (Owner == L"Object")
        {
            AddCandidates({ L"Find", L"FindAll", L"FindByType", L"FindByComponent",
                L"FindByScript", L"Exists", L"SetSize", L"SetPosition",
                L"SetTransform", L"Move", L"SetColor", L"MultiplyColor",
                L"Remove", L"AttachScript" });
        }
        else if (Owner == L"Scene")
        {
            AddCandidates({ L"GetID", L"SetActive", L"SetView" });
        }
        else if (Owner == L"Input")
        {
            AddCandidates({ L"IsKeyDown" });
        }
        else if (Owner == L"Advanced")
        {
            AddCandidates({ L"Host", L"Program" });
        }
        else if (!Owner.empty())
        {
            AddCandidates({ L"GetID", L"GetSceneID", L"GetName", L"GetType",
                L"IsValid", L"SetSize", L"SetPosition", L"SetTransform", L"Move",
                L"SetColor", L"MultiplyColor", L"SetActive", L"GetComponent",
                L"GetComponents", L"HasComponent", L"AttachScript", L"Remove" });
        }

        if (Owner.empty())
        {
            for (const wchar_t* Word : ProgramCompletionWords)
            {
                CandidateSet.emplace(Word);
            }
        }

        if (forceDisplay)
        {
            RefreshExternalProgramSuggestions();
        }

        for (const std::wstring& Word : ExternalProgramSuggestions)
        {
            if (Owner.empty())
            {
                CandidateSet.emplace(Word);
            }
        }

        for (const ProgramFunctionInfo& Function : ProgramFunctions)
        {
            if (Owner.empty())
            {
                CandidateSet.emplace(Function.Name);
            }
        }

        ProgramSuggestions.clear();

        for (const std::wstring& Candidate : CandidateSet)
        {
            const bool PrefixMatches = Prefix.empty() ||
                (Candidate.size() >= Prefix.size() && std::equal(
                    Prefix.begin(),
                    Prefix.end(),
                    Candidate.begin(),
                    [](wchar_t left, wchar_t right)
                    {
                        return std::towlower(left) == std::towlower(right);
                    }
                )); //大文字小文字を区別しないAPI Prefix一致

            if (PrefixMatches && Candidate != Prefix)
            {
                ProgramSuggestions.emplace_back(Candidate);
            }
        }

        std::sort(ProgramSuggestions.begin(), ProgramSuggestions.end());

        if (ProgramSuggestions.size() > 128)
        {
            ProgramSuggestions.resize(128);
        }

        if (ProgramSuggestions.empty())
        {
            HideProgramSuggestions();
            return;
        }

        SendMessageW(ProgramSuggestionListHwnd, WM_SETREDRAW, FALSE, 0);
        SendMessageW(ProgramSuggestionListHwnd, LB_RESETCONTENT, 0, 0);
        std::size_t MaximumLength = 0; //候補一覧の横幅計算用最大文字数

        for (const std::wstring& Suggestion : ProgramSuggestions)
        {
            SendMessageW(
                ProgramSuggestionListHwnd,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(Suggestion.c_str())
            );
            MaximumLength = std::max(MaximumLength, Suggestion.size());
        }

        SendMessageW(ProgramSuggestionListHwnd, LB_SETCURSEL, 0, 0);
        SendMessageW(
            ProgramSuggestionListHwnd,
            LB_SETHORIZONTALEXTENT,
            ScaleByDpi(static_cast<int>(MaximumLength * 9)),
            0
        );
        SendMessageW(ProgramSuggestionListHwnd, WM_SETREDRAW, TRUE, 0);

        POINTL RichEditPoint{}; //CaretのRichEdit Client座標
        SendMessageW(
            ProgramEditorHwnd,
            EM_POSFROMCHAR,
            reinterpret_cast<WPARAM>(&RichEditPoint),
            CaretPosition
        );
        POINT PopupPoint
        {
            static_cast<LONG>(RichEditPoint.x),
            static_cast<LONG>(RichEditPoint.y + ScaleByDpi(22))
        }; //親Windowへ変換する候補左上位置
        MapWindowPoints(ProgramEditorHwnd, Hwnd, &PopupPoint, 1);
        RECT ClientRectangle{}; //候補を収める親Client範囲
        GetClientRect(Hwnd, &ClientRectangle);
        const int ClientWidth = std::max(
            1,
            static_cast<int>(ClientRectangle.right - ClientRectangle.left)
        ); //候補幅を制約する親Client幅
        const int SuggestionWidth = std::min(
            ScaleByDpi(320),
            std::max(ScaleByDpi(180), ClientWidth)
        ); //候補一覧幅
        const int VisibleRows = static_cast<int>(std::min<std::size_t>(
            ProgramSuggestions.size(),
            8
        )); //同時表示候補行数
        const int SuggestionHeight = ScaleByDpi(22 * VisibleRows + 4); //候補一覧高さ

        if (PopupPoint.y + SuggestionHeight > ClientRectangle.bottom)
        {
            PopupPoint.y = std::max(
                0L,
                PopupPoint.y - SuggestionHeight - ScaleByDpi(24)
            );
        }

        PopupPoint.x = std::clamp<LONG>(
            PopupPoint.x,
            0,
            std::max(0L, ClientRectangle.right - SuggestionWidth)
        );
        SetWindowPos(
            ProgramSuggestionListHwnd,
            HWND_TOP,
            PopupPoint.x,
            PopupPoint.y,
            SuggestionWidth,
            SuggestionHeight,
            SWP_SHOWWINDOW | SWP_NOACTIVATE
        );
        InvalidateRect(ProgramSuggestionListHwnd, nullptr, TRUE);
    }

    //概要：表示中のProgramコード補完一覧を消去して非表示にする
    //引数：なし
    //戻り値：なし
    void WinApp::HideProgramSuggestions()
    {
        ProgramSuggestions.clear();

        if (ProgramSuggestionListHwnd != nullptr)
        {
            ShowWindow(ProgramSuggestionListHwnd, SW_HIDE);
            SendMessageW(ProgramSuggestionListHwnd, LB_RESETCONTENT, 0, 0);
        }
    }

    //概要：選択中候補で入力中識別子を置換してProgram Sourceへ書き込む
    //引数：なし
    //戻り値：なし
    void WinApp::ApplyProgramSuggestion()
    {
        if (ProgramSuggestionListHwnd == nullptr || ProgramEditorHwnd == nullptr)
        {
            return;
        }

        const LRESULT Selection = SendMessageW(
            ProgramSuggestionListHwnd,
            LB_GETCURSEL,
            0,
            0
        ); //確定する候補Index

        if (Selection < 0 ||
            static_cast<std::size_t>(Selection) >= ProgramSuggestions.size())
        {
            HideProgramSuggestions();
            return;
        }

        long TokenBegin = 0; //置換する識別子開始位置
        long CaretPosition = 0; //置換する識別子終端位置
        std::wstring Prefix; //現在入力済みPrefix

        if (!GetProgramCompletionRange(TokenBegin, CaretPosition, Prefix))
        {
            HideProgramSuggestions();
            return;
        }

        const std::wstring Suggestion = ProgramSuggestions[
            static_cast<std::size_t>(Selection)
        ]; //Sourceへ書き込む候補
        CHARRANGE ReplacementRange{ TokenBegin, CaretPosition }; //Prefix置換範囲
        SendMessageW(
            ProgramEditorHwnd,
            EM_EXSETSEL,
            0,
            reinterpret_cast<LPARAM>(&ReplacementRange)
        );
        SendMessageW(
            ProgramEditorHwnd,
            EM_REPLACESEL,
            TRUE,
            reinterpret_cast<LPARAM>(Suggestion.c_str())
        );
        HideProgramSuggestions();
        SetFocus(ProgramEditorHwnd);
    }

    //概要：Caret直前にあるC++識別子の置換範囲とPrefixを取得する
    //引数：tokenBegin=開始位置格納先、caretPosition=Caret位置格納先、prefix=入力済み文字列格納先
    //戻り値：単一Caretから有効な置換範囲を取得できた場合はtrue
    bool WinApp::GetProgramCompletionRange(
        long& tokenBegin,
        long& caretPosition,
        std::wstring& prefix
    ) const
    {
        if (ProgramEditorHwnd == nullptr)
        {
            return false;
        }

        CHARRANGE Selection{}; //現在のRichEdit選択又はCaret範囲
        SendMessageW(
            ProgramEditorHwnd,
            EM_EXGETSEL,
            0,
            reinterpret_cast<LPARAM>(&Selection)
        );

        if (Selection.cpMin != Selection.cpMax)
        {
            return false;
        }

        caretPosition = std::max(0L, Selection.cpMax);
        tokenBegin = caretPosition;

        while (tokenBegin > 0)
        {
            wchar_t CharacterBuffer[2]{}; //RichEdit座標でCaret直前一文字を受け取るBuffer
            TEXTRANGEW CharacterRange{}; //RichEdit座標の一文字取得範囲
            CharacterRange.chrg = CHARRANGE{ tokenBegin - 1, tokenBegin };
            CharacterRange.lpstrText = CharacterBuffer;
            const LRESULT CopiedLength = SendMessageW(
                ProgramEditorHwnd,
                EM_GETTEXTRANGE,
                0,
                reinterpret_cast<LPARAM>(&CharacterRange)
            ); //取得した文字数

            if (CopiedLength != 1 ||
                (!std::iswalnum(CharacterBuffer[0]) && CharacterBuffer[0] != L'_'))
            {
                break;
            }

            --tokenBegin;
        }

        std::wstring PrefixBuffer(
            static_cast<std::size_t>(caretPosition - tokenBegin) + 1,
            L'\0'
        ); //終端文字を含むPrefix取得Buffer
        TEXTRANGEW PrefixRange{}; //RichEdit座標でのPrefix取得範囲
        PrefixRange.chrg = CHARRANGE{ tokenBegin, caretPosition };
        PrefixRange.lpstrText = PrefixBuffer.data();
        const LRESULT PrefixLength = SendMessageW(
            ProgramEditorHwnd,
            EM_GETTEXTRANGE,
            0,
            reinterpret_cast<LPARAM>(&PrefixRange)
        ); //取得したPrefix文字数

        if (PrefixLength < 0)
        {
            return false;
        }

        prefix.assign(PrefixBuffer.data(), static_cast<std::size_t>(PrefixLength));

        if (!prefix.empty() && !std::iswalpha(prefix.front()) && prefix.front() != L'_')
        {
            return false;
        }

        std::wstring Text(static_cast<std::size_t>(tokenBegin) + 1, L'\0'); //字句状態を調べるCaret手前のSource Buffer
        TEXTRANGEW ContextRange{}; //Source先頭からToken手前までの取得範囲
        ContextRange.chrg = CHARRANGE{ 0, tokenBegin };
        ContextRange.lpstrText = Text.data();
        const LRESULT ContextLength = SendMessageW(
            ProgramEditorHwnd,
            EM_GETTEXTRANGE,
            0,
            reinterpret_cast<LPARAM>(&ContextRange)
        ); //取得した字句解析対象文字数

        if (ContextLength < 0)
        {
            return false;
        }

        Text.resize(static_cast<std::size_t>(ContextLength));
        const std::size_t LocalTokenBegin = Text.size(); //字句解析Buffer内のToken開始位置
        std::size_t LineBegin = LocalTokenBegin; //字句解析Buffer内の現在行開始位置

        while (LineBegin > 0 && Text[LineBegin - 1] != L'\n' &&
            Text[LineBegin - 1] != L'\r')
        {
            --LineBegin;
        }

        std::size_t FirstLineToken = LineBegin; //空白を除いた現在行の先頭位置

        while (FirstLineToken < LocalTokenBegin &&
            (Text[FirstLineToken] == L' ' || Text[FirstLineToken] == L'\t'))
        {
            ++FirstLineToken;
        }

        if (FirstLineToken < Text.size() && Text[FirstLineToken] == L'#')
        {
            return false;
        }

        enum class CompletionLexicalState : std::uint8_t
        {
            Code,
            String,
            Character,
            LineComment,
            BlockComment
        }; //補完を禁止するC++字句状態

        CompletionLexicalState State = CompletionLexicalState::Code; //Caret直前までの字句状態
        bool Escaped = false; //文字列内で直前文字がEscapeの場合true

        for (std::size_t Index = 0; Index < LocalTokenBegin; ++Index)
        {
            const wchar_t Character = Text[Index]; //現在解析する文字
            const wchar_t Next = Index + 1 < LocalTokenBegin
                ? Text[Index + 1]
                : L'\0'; //Token開始より前にある次文字

            if (State == CompletionLexicalState::LineComment)
            {
                if (Character == L'\n' || Character == L'\r')
                {
                    State = CompletionLexicalState::Code;
                }

                continue;
            }

            if (State == CompletionLexicalState::BlockComment)
            {
                if (Character == L'*' && Next == L'/')
                {
                    State = CompletionLexicalState::Code;
                    ++Index;
                }

                continue;
            }

            if (State == CompletionLexicalState::String ||
                State == CompletionLexicalState::Character)
            {
                const wchar_t Terminator = State == CompletionLexicalState::String
                    ? L'"'
                    : L'\''; //現在Literalの終端記号

                if (!Escaped && Character == Terminator)
                {
                    State = CompletionLexicalState::Code;
                }

                Escaped = !Escaped && Character == L'\\';

                if (Character != L'\\')
                {
                    Escaped = false;
                }

                continue;
            }

            if (Character == L'/' && Next == L'/')
            {
                State = CompletionLexicalState::LineComment;
                ++Index;
            }
            else if (Character == L'/' && Next == L'*')
            {
                State = CompletionLexicalState::BlockComment;
                ++Index;
            }
            else if (Character == L'"')
            {
                State = CompletionLexicalState::String;
                Escaped = false;
            }
            else if (Character == L'\'')
            {
                State = CompletionLexicalState::Character;
                Escaped = false;
            }
        }

        if (State != CompletionLexicalState::Code)
        {
            return false;
        }

        return true;
    }

    //概要：Compile標準出力を行単位でError一覧と状態表示へ反映する
    //引数：result=Background Compile又は事前判定の終了状態と標準出力
    //戻り値：なし
    void WinApp::ShowProgramDiagnostics(const ProgramCompileResult& result)
    {
        SendMessageW(ProgramErrorListHwnd, LB_RESETCONTENT, 0, 0);
        std::wistringstream Stream(result.Output); //行単位で読むCompile出力
        std::wstring Line; //現在追加するCompile出力行
        std::size_t MaximumLength = 0; //横Scroll幅計算用最大文字数

        while (std::getline(Stream, Line))
        {
            if (!Line.empty() && Line.back() == L'\r')
            {
                Line.pop_back();
            }

            if (Line.empty())
            {
                continue;
            }

            SendMessageW(
                ProgramErrorListHwnd,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(Line.c_str())
            );
            MaximumLength = std::max(MaximumLength, Line.size());
        }

        if (SendMessageW(ProgramErrorListHwnd, LB_GETCOUNT, 0, 0) == 0)
        {
            const wchar_t* Message = result.Succeeded
                ? L"コンパイルは正常に完了しました"
                : L"コンパイルを開始できませんでした"; //出力が空の場合の診断
            SendMessageW(
                ProgramErrorListHwnd,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(Message)
            );
        }

        SendMessageW(
            ProgramErrorListHwnd,
            LB_SETHORIZONTALEXTENT,
            ScaleByDpi(static_cast<int>(MaximumLength * 9)),
            0
        );
        SetWindowTextW(
            ProgramStatusLabelHwnd,
            result.Succeeded
                ? L"コンパイル成功"
                : (result.Started ? L"コンパイル失敗" : L"コンパイルを開始できませんでした")
        );
    }

    //概要：Windows Controlの全文を安全なUTF-16文字列として取得する
    //引数：control=文字列を取得するWindow Handle
    //戻り値：Control全文、無効Handleの場合は空文字列
    std::wstring WinApp::GetControlText(HWND control) const
    {
        if (control == nullptr)
        {
            return std::wstring();
        }

        const int Length = GetWindowTextLengthW(control); //終端を除くControl文字数
        std::wstring Result(static_cast<std::size_t>(Length) + 1, L'\0'); //終端を含む取得Buffer
        GetWindowTextW(control, Result.data(), Length + 1);
        Result.resize(static_cast<std::size_t>(Length));
        return Result;
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
            EditorTabHwnd,
            SceneSelectorHwnd,
            ObjectTreeHwnd,
            AddObjectButtonHwnd,
            AddScriptButtonHwnd,
            LoadScriptButtonHwnd,
            ObjectNameLabelHwnd,
            ObjectNameEditHwnd,
            ObjectActiveCheckHwnd,
            ObjectParentLabelHwnd,
            TransformLabelsHwnd[0],
            TransformLabelsHwnd[1],
            TransformLabelsHwnd[2],
            TransformEditsHwnd[0],
            TransformEditsHwnd[1],
            TransformEditsHwnd[2],
            TransformEditsHwnd[3],
            TransformEditsHwnd[4],
            TransformEditsHwnd[5],
            TransformEditsHwnd[6],
            TransformEditsHwnd[7],
            TransformEditsHwnd[8],
            ApplyObjectButtonHwnd,
            StartButtonHwnd,
            PauseButtonHwnd,
            StopButtonHwnd,
            TickButtonHwnd,
            StatusLabelHwnd,
            FrameRateLabelHwnd,
            FrameRateEditHwnd,
            FrameRateSliderHwnd,
            PreviewLabelHwnd,
            LogLabelHwnd,
            LogListHwnd,
            ClearLogsButtonHwnd,
            ProgramFileListHwnd,
            ProgramFunctionListHwnd,
            ProgramEditorHwnd,
            ProgramSuggestionListHwnd,
            ProgramErrorListHwnd,
            ProgramFileNameEditHwnd,
            NewProgramButtonHwnd,
            RenameProgramButtonHwnd,
            DeleteProgramButtonHwnd,
            SaveProgramButtonHwnd,
            CompileProgramButtonHwnd,
            RestoreProgramButtonHwnd,
            ProgramStatusLabelHwnd,
            ProgramRoleLabelHwnd
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
        const bool Playing = CurrentPlaybackState == PlaybackState::Playing;
        const bool Stopped = CurrentPlaybackState == PlaybackState::Stopped;

        if (StartButtonHwnd != nullptr)
        {
            SetWindowTextW(StartButtonHwnd, CurrentPlaybackState == PlaybackState::Paused
                ? L"Resume"
                : L"Start");
            EnableWindow(StartButtonHwnd, Playing ? FALSE : TRUE);
        }

        if (PauseButtonHwnd != nullptr)
        {
            EnableWindow(PauseButtonHwnd, Playing ? TRUE : FALSE);
        }

        if (StopButtonHwnd != nullptr)
        {
            EnableWindow(StopButtonHwnd, Stopped ? FALSE : TRUE);
        }

        if (TickButtonHwnd != nullptr)
        {
            EnableWindow(TickButtonHwnd, Playing ? FALSE : TRUE);
        }

        if (StatusLabelHwnd != nullptr)
        {
            SetWindowTextW(
                StatusLabelHwnd,
                Playing
                    ? L"状態: 再生中"
                    : (Stopped ? L"状態: 停止（初期状態）" : L"状態: 一時停止")
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

    //概要：Object TreeとInspectorの間にある上下分割線矩形を取得する
    //引数：なし
    //戻り値：親Window座標の分割線矩形
    RECT WinApp::GetEngineSplitterRect() const
    {
        RECT TreeRectangle{}; //Object Treeの画面座標矩形
        RECT InspectorRectangle{}; //Inspector先頭Controlの画面座標矩形

        if (ObjectTreeHwnd == nullptr || ObjectNameLabelHwnd == nullptr)
        {
            return TreeRectangle;
        }

        GetWindowRect(ObjectTreeHwnd, &TreeRectangle);
        GetWindowRect(ObjectNameLabelHwnd, &InspectorRectangle);
        MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&TreeRectangle), 2);
        MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&InspectorRectangle), 2);
        return RECT
        {
            TreeRectangle.left,
            TreeRectangle.bottom,
            TreeRectangle.right,
            InspectorRectangle.top
        };
    }

    //概要：Program一覧とSource Editorの間にある上下分割線矩形を取得する
    //引数：なし
    //戻り値：親Window座標の分割線矩形
    RECT WinApp::GetProgramHorizontalSplitterRect() const
    {
        RECT ListRectangle{}; //Program一覧の画面座標矩形
        RECT EditorRectangle{}; //Source Editorの画面座標矩形

        if (ProgramFileListHwnd == nullptr || ProgramEditorHwnd == nullptr)
        {
            return ListRectangle;
        }

        GetWindowRect(ProgramFileListHwnd, &ListRectangle);
        GetWindowRect(ProgramEditorHwnd, &EditorRectangle);
        MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&ListRectangle), 2);
        MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&EditorRectangle), 2);
        return RECT
        {
            ListRectangle.left,
            ListRectangle.bottom,
            EditorRectangle.right,
            EditorRectangle.top
        };
    }

    //概要：Program File一覧と関数一覧の間にある左右分割線矩形を取得する
    //引数：なし
    //戻り値：親Window座標の分割線矩形
    RECT WinApp::GetProgramVerticalSplitterRect() const
    {
        RECT FileRectangle{}; //File一覧の画面座標矩形
        RECT FunctionRectangle{}; //関数一覧の画面座標矩形

        if (ProgramFileListHwnd == nullptr || ProgramFunctionListHwnd == nullptr)
        {
            return FileRectangle;
        }

        GetWindowRect(ProgramFileListHwnd, &FileRectangle);
        GetWindowRect(ProgramFunctionListHwnd, &FunctionRectangle);
        MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&FileRectangle), 2);
        MapWindowPoints(HWND_DESKTOP, Hwnd, reinterpret_cast<POINT*>(&FunctionRectangle), 2);
        return RECT
        {
            FileRectangle.right,
            FileRectangle.top,
            FunctionRectangle.left,
            FileRectangle.bottom
        };
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
            PaintEditorSplitters(PaintDc);
        }

        EndPaint(Hwnd, &PaintInformation);
    }

    //概要：現在Tabで使用する内部分割線を親Windowへ描画する
    //引数：paintDC=WM_PAINT中の描画先Device Context
    //戻り値：なし
    void WinApp::PaintEditorSplitters(HDC paintDC)
    {
        if (paintDC == nullptr)
        {
            return;
        }

        if (ActiveTabIndex == 0)
        {
            RECT Rectangle = GetEngineSplitterRect(); //Engine上下分割線
            FillRect(paintDC, &Rectangle, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
            DrawEdge(paintDC, &Rectangle, EDGE_RAISED, BF_TOP | BF_BOTTOM);
        }
        else if (IsProgramSourceTab())
        {
            RECT Horizontal = GetProgramHorizontalSplitterRect(); //Program上下分割線
            RECT Vertical = GetProgramVerticalSplitterRect(); //Program左右分割線
            FillRect(paintDC, &Horizontal, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
            DrawEdge(paintDC, &Horizontal, EDGE_RAISED, BF_TOP | BF_BOTTOM);
            FillRect(paintDC, &Vertical, reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
            DrawEdge(paintDC, &Vertical, EDGE_RAISED, BF_LEFT | BF_RIGHT);
        }
    }
}
