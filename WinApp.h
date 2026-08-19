//|| WinApp.h ||:::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DX12エンジン用のWindowsエディターウィンドウを管理する
//||  描画領域、操作パネル、可変スプリッターをWindows標準機能で提供する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v3.00  Engine Tab、Object Tree、Script操作Menuを追加
//||  2026_07_13  v2.30  修正: 右パネル表示階層と三段レイアウトを明確化
//||  2026_07_13  v2.10  変更: DPI対応レイアウトとログ表示領域を追加
//||  2026_07_13  v2.00  追加: Windows標準エディターUIと操作イベント
//||

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <CommCtrl.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

#include "EditorTypes.h"
#include "ProgramWorkspace.h"
#include "WindowsUIResources.h"

namespace Engine
{
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

        // エンジンのScene、Object、Component、Script一覧をUIへ反映する
        // snapshot: EngineAPIが作成した読み取り専用状態
        void UpdateEditorSnapshot(const EditorSnapshot& snapshot);

        // エディターで発生したObject又はScript操作を1件取得する
        // command: 取得した操作要求の格納先
        // 戻り値: 未処理要求が存在した場合はtrue
        bool ConsumeEditorCommand(EditorCommand& command);

        /**
         * Startボタンの未処理イベントを1件取得する
         * @return Start要求が存在した場合はtrue
         */
        bool ConsumeStart();

        //Pauseボタンの未処理イベントを1件取得する
        bool ConsumePause();

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

        static LRESULT CALLBACK ProgramEditorSubclassProc(
            HWND hwnd,
            UINT message,
            WPARAM wparam,
            LPARAM lparam,
            UINT_PTR subclassID,
            DWORD_PTR referenceData
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

        enum class EditorTreeNodeKind : std::uint8_t
        {
            Scene,
            Object,
            Component
        };

        enum class PlaybackState : std::uint8_t
        {
            Stopped,
            Playing,
            Paused
        };

        struct EditorTreeNode final
        {
            EditorTreeNodeKind Kind = EditorTreeNodeKind::Scene; //Tree上の要素種別
            SceneID Scene; //要素を所有するScene
            ObjectID Object; //要素又は親Object
            ComponentID Component; //Component要素の場合のID
            bool Active = true; //現在の有効状態
        };

        // Object Treeを最新Snapshotから再構築する
        void RebuildObjectTree();

        void RebuildSceneSelector();
        void UpdateObjectInspector();
        void ApplyObjectInspector();
        const EditorObjectInfo* GetSelectedObjectInfo() const;
        const EditorSceneInfo* GetSelectedSceneInfo() const;

        // 選択中Tabに合わせてControlの表示状態を切り替える
        void UpdateTabVisibility();

        // Tab内のEngine、再生、Log Controlを配置する
        // panelLeft: 右Panel左端、panelWidth: 右Panel幅、clientHeight: Client高さ
        void LayoutTabbedControls(int panelLeft, int panelWidth, int clientHeight);

        // 指定画面座標へObject作成Menuを表示する
        // screenPosition: Menu左上の画面座標
        void ShowCreateObjectMenu(const POINT& screenPosition, bool addAsChild);

        // 選択Objectへ追加可能なScript Menuを表示する
        // screenPosition: Menu左上の画面座標
        void ShowScriptMenu(const POINT& screenPosition);

        // Object Treeの選択状態に応じた右Click Menuを表示する
        // screenPosition: Menu左上の画面座標
        void ShowTreeContextMenu(const POINT& screenPosition);

        // DLL選択Dialogを開きLoad Script Module要求を登録する
        void OpenScriptModuleDialog();

        // Object Treeで現在選択中の要素情報を取得する
        // 戻り値: 選択要素、未選択又は不整合時はnullptr
        const EditorTreeNode* GetSelectedTreeNode() const;
        const EditorTreeNode* GetTreeNode(HTREEITEM item) const;

        // EngineAPIへ渡す操作要求をQueueへ追加する
        // command: 追加するEditor操作
        void QueueEditorCommand(EditorCommand command);

        RECT GetEngineSplitterRect() const;
        RECT GetProgramHorizontalSplitterRect() const;
        RECT GetProgramVerticalSplitterRect() const;
        void PaintEditorSplitters(HDC paintDC);

        bool InitializeProgramWorkspace();
        void RefreshProgramFiles(const std::filesystem::path& preferredPath = {});
        bool SaveCurrentProgram();
        bool StartBackgroundSaveCurrentProgram();
        void ProcessWorkspaceSaveResult(
            ProgramWorkspace& workspace,
            bool scriptWorkspace
        );
        void LoadSelectedProgram();
        void RenameCurrentProgram();
        void DeleteCurrentProgram();
        void RestoreLastSuccessfulProgram();
        void CompilePrograms();
        void ProcessProgramAutomation();
        void ProcessWorkspaceCompileResult(
            ProgramWorkspace& workspace,
            bool scriptWorkspace
        );
        bool RequestProgramCompile(bool automatic);
        ProgramWorkspace& GetActiveProgramWorkspace();
        bool IsProgramSourceTab() const;
        void SwitchProgramWorkspace(bool scriptWorkspace);
        void RefreshExternalProgramSuggestions();
        void HighlightProgramText();
        void RebuildProgramFunctionList();
        void UpdateProgramSuggestions(bool forceDisplay);
        void HideProgramSuggestions();
        void ApplyProgramSuggestion();
        bool GetProgramCompletionRange(
            long& tokenBegin,
            long& caretPosition,
            std::wstring& prefix
        ) const;
        void ShowProgramDiagnostics(const ProgramCompileResult& result);
        std::wstring GetControlText(HWND control) const;

        struct ProgramFunctionInfo final
        {
            std::wstring Name;
            long CharacterIndex = 0;
            int Line = 0;
        };

    private:
        HWND Hwnd;                  // 親エディターウィンドウ
        HWND RenderHwnd;            // DX12 SwapChainを接続する左描画ウィンドウ
        HWND PanelHwnd;             // 右操作パネルの範囲管理に使う非表示コントロール
        HWND TitleLabelHwnd;        // 操作パネル見出しテキスト
        HWND EditorTabHwnd;         // Engine、再生、Log、Main、Scriptを切り替えるTab
        HWND SceneSelectorHwnd;      // Object Treeへ表示するScene選択
        HWND ObjectTreeHwnd;         // Scene、Object、Component階層一覧
        HWND AddObjectButtonHwnd;    // Object追加Menuを開くButton
        HWND AddScriptButtonHwnd;    // 選択ObjectへScriptを差し込むButton
        HWND LoadScriptButtonHwnd;   // Script DLL選択Dialogを開くButton
        HWND ObjectNameLabelHwnd;    // 選択Object名の見出し
        HWND ObjectNameEditHwnd;     // 選択Object名の入力
        HWND ObjectActiveCheckHwnd;  // 選択Object有効状態
        HWND ObjectParentLabelHwnd;  // 選択Objectの親表示
        HWND TransformLabelsHwnd[3]; // Position、Rotation、Scale見出し
        HWND TransformEditsHwnd[9];  // Local TransformのXYZ入力
        HWND ApplyObjectButtonHwnd;  // Inspector内容の適用
        HWND StartButtonHwnd;       // ゲーム開始ボタン
        HWND PauseButtonHwnd;       // 状態を保持する一時停止ボタン
        HWND StopButtonHwnd;        // 初期状態へ戻す停止ボタン
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
        HWND ProgramFileListHwnd;    // Programソースファイル一覧
        HWND ProgramFunctionListHwnd;// 選択Source内の関数一覧
        HWND ProgramEditorHwnd;      // Syntax色分け対応Program入力
        HWND ProgramSuggestionListHwnd; // 入力位置へ表示するコード補完候補
        HWND ProgramErrorListHwnd;   // Compile出力とError一覧
        HWND ProgramFileNameEditHwnd;// 選択Sourceファイル名
        HWND NewProgramButtonHwnd;   // Source新規作成
        HWND RenameProgramButtonHwnd;// Source名前変更
        HWND DeleteProgramButtonHwnd;// Source削除
        HWND SaveProgramButtonHwnd;  // Source保存
        HWND CompileProgramButtonHwnd;// 手動Background Compile
        HWND RestoreProgramButtonHwnd;// 最終Compile成功Sourceへ復元
        HWND ProgramStatusLabelHwnd; // 保存及びCompile状態
        HWND ProgramRoleLabelHwnd;   // Main又はScriptの役割説明
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
        bool PauseRequested;        // 未処理のPause要求が存在するか
        bool StopRequested;         // 未処理のStop要求が存在するか
        bool ResizeRequested;       // 未処理の描画領域サイズ変更が存在するか
        bool SplitDragging;         // スプリッターをドラッグ中か
        bool EngineSplitDragging;   // Object TreeとInspectorの分割線をドラッグ中か
        bool ProgramHorizontalSplitDragging; // Program一覧とEditorの分割線をドラッグ中か
        bool ProgramVerticalSplitDragging; // Program Fileと関数一覧の分割線をドラッグ中か
        bool TreeDragging;          // Object親変更のTreeドラッグ中か
        PlaybackState CurrentPlaybackState; //UI上の再生、一時停止、停止状態
        bool UpdatingFrameRate;     // FPSコントロールの相互更新中か
        bool ClearLogsRequested;    // 未処理のログ一括消去要求が存在するか
        bool ClassRegistered;       // 親ウィンドウクラスを登録済みか
        bool UpdatingProgramEditor; // 色設定又はFile読込による変更通知中か
        bool ProgramDirty;          // Editor内容が未保存の場合true
        bool ProgramWorkspaceReady; // Program保存先を初期化済みの場合true
        bool ProgramCompilePending; // 入力停止後又は手動要求のCompile待ち
        bool ProgramPendingManualCompile; // 待機中要求を手動Compileとして扱う場合true
        bool ProgramVisualRefreshPending; // 入力処理後のProgram全面再描画Message待ちの場合true
        bool SuppressProgramCharacter; // 補完確定Key由来WM_CHARを破棄する場合true
        bool EditingScriptWorkspace; // ScriptProgramsをEditorへ表示中の場合true
        int ActiveTabIndex;          // 現在表示中のEditor Tab番号
        int EditorSplitDragOffset;  // Editor内分割線のドラッグ開始差
        float EngineSplitRatio;     // Engine Page高さに対するTree比率
        float ProgramHorizontalSplitRatio; // Program一覧領域の高さ比率
        float ProgramVerticalSplitRatio; // File一覧の横幅比率
        std::wstring ClassName;     // 親ウィンドウクラス名
        SceneID SelectedEditorSceneID; // Object Treeへ表示するScene
        EditorTreeNode DraggedTreeNode; // 親変更中のObject情報
        std::vector<SceneID> SceneSelectorIDs; // Combo IndexからScene IDへの対応
        EditorSnapshot CurrentEditorSnapshot; // Object TreeとScript Menuの現在情報
        std::vector<EditorTreeNode> EditorTreeNodes; // Tree Item lParamから参照する要素表
        std::deque<EditorCommand> PendingEditorCommands; // EngineAPIへ渡す未処理操作Queue
        ProgramWorkspace Programs; // Program保存、簡易判定、Background Compile
        ProgramWorkspace ScriptPrograms; // Object Script保存、Compile、DLL読込
        std::vector<std::filesystem::path> ProgramFiles; // File List Index対応Path
        std::filesystem::path CurrentProgramPath; // 現在Editorへ表示中のSource
        std::vector<ProgramFunctionInfo> ProgramFunctions; // Function List Index対応位置
        std::vector<std::wstring> ProgramSuggestions; // 表示中候補のList Index対応文字列
        std::vector<std::wstring> ExternalProgramSuggestions; // API Headerと外部Templateから検出した候補
        std::chrono::steady_clock::time_point LastProgramEditTime; // 最終Source変更時刻
        std::uint64_t ProgramEditRevision; // Source変更ごとに増える版番号
        std::uint64_t ProgramSavedRevision; // 最後に保存成功した版番号
        std::uint64_t ProgramRequestedRevision; // 最後にCompileへ渡した版番号
        std::uint64_t ExternalSuggestionRevision; // 最後に反映した外部登録候補Revision
        HMODULE RichEditModule; // RichEdit 4.1 Window Class Module
        WindowsUISettings UISettings;       // UI画像とフォントの現在設定
        WindowsUIResources UIResources;     // Windows用フォントと画像リソース
    };
}
