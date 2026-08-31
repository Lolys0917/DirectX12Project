//|| WinMain.cpp ||:::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Windows標準UI、固定FPS、Scene描画を接続するエントリーポイント
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v3.00  EngineAPIとEditor操作Queue及びObject Tree更新を接続
//||  2026_07_13  v2.10  編集: MessageLog表示、通常ログ消去、常設ログ及び操作ログを接続
//||  2026_07_13  v2.00  Editor UIとStart、Stop、Tick、動的FPSを接続
//||  2026_06_01  v1.00  新規作成
//||

#include <Windows.h>
#include <objbase.h>

#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "GameRuntime.h"
#include "MessageLog.h"
#include "WinApp.h"

namespace
{
    /**
     * UTF-8ログをWindows標準コントロール用UTF-16へ変換する
     * @param text 変換するUTF-8文字列
     * @return 変換済みUTF-16文字列、変換失敗時は代替メッセージ
     */
    std::wstring ConvertLogToWide(const std::string& text)
    {
        if (text.empty())
        {
            return std::wstring();
        }

        const int RequiredLength = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        ); // UTF-16変換後に必要な文字数

        if (RequiredLength <= 0)
        {
            return L"[Error] MessageLog | UTF-8ログの表示変換に失敗しました。";
        }

        std::wstring ConvertedText(
            static_cast<std::size_t>(RequiredLength),
            L'\0'
        ); // Windows LISTBOXへ渡すUTF-16文字列
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            ConvertedText.data(),
            RequiredLength
        );
        return ConvertedText;
    }

    /**
     * MessageLogの現在値を常設、通常の順でWindows UIへ反映する
     * @param window ログ一覧を所有するEditor Window
     * @param displayedRevision 最後に表示した更新番号
     */
    void RefreshLogDisplay(
        Engine::WinApp& window,
        std::uint64_t& displayedRevision
    )
    {
        Engine::MessageLog& Log =
            Engine::MessageLog::GetInstance(); // Process内で共有するログ管理器

        if (displayedRevision == Log.GetRevision())
        {
            return;
        }

        const Engine::MessageLogSnapshot Snapshot =
            Log.GetSnapshot(); // 同一更新時点の常設ログと通常ログ
        std::vector<std::wstring> DisplayMessages; // LISTBOXへ表示するUTF-16ログ一覧
        DisplayMessages.reserve(
            Snapshot.PermanentLogs.size() + Snapshot.Logs.size()
        );

        for (const std::string& Message : Snapshot.PermanentLogs) // 消去対象外ログを先頭へ配置する
        {
            DisplayMessages.emplace_back(
                L"[常設] " + ConvertLogToWide(Message)
            );
        }

        for (const std::string& Message : Snapshot.Logs) // 通常ログを発生順に配置する
        {
            DisplayMessages.emplace_back(ConvertLogToWide(Message));
        }

        window.UpdateLogMessages(DisplayMessages);
        displayedRevision = Snapshot.Revision;
    }

    //起動失敗時にDirectX、Scene、Windowの具体的なErrorログをDialogへまとめる
    std::wstring BuildStartupFailureMessage()
    {
        const Engine::MessageLogSnapshot Snapshot =
            Engine::MessageLog::GetInstance().GetSnapshot();
        std::vector<std::string> Failures;

        const auto CollectFailures = [&Failures](const std::vector<std::string>& messages)
        {
            for (const std::string& Message : messages)
            {
                if (Message.find("[Error]") != std::string::npos ||
                    Message.find("[Critical]") != std::string::npos)
                {
                    Failures.emplace_back(Message);
                }
            }
        };
        CollectFailures(Snapshot.PermanentLogs);
        CollectFailures(Snapshot.Logs);

        std::wstring Result = L"エンジンの起動初期化に失敗しました。";
        if (Failures.empty())
        {
            Result += L"\n詳細ログは取得できませんでした。";
            return Result;
        }

        Result += L"\n\n失敗箇所:\n";
        constexpr std::size_t MaximumDisplayedFailures = 8;
        const std::size_t First = Failures.size() > MaximumDisplayedFailures
            ? Failures.size() - MaximumDisplayedFailures
            : 0;
        for (std::size_t Index = First; Index < Failures.size(); ++Index)
        {
            Result += L"・" + ConvertLogToWide(Failures[Index]) + L"\n";
        }
        return Result;
    }
}

//Windows標準Editor UIとEngineを起動してMessage Loopを実行する
//引数: hInstance 実行Module、hPrevInstance 未使用の旧Instance、commandLine Command Line、showCommand 初期表示方法
//戻り値: 正常終了時は0、初期化失敗時は-1
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR commandLine,
    int showCommand
)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)commandLine;
    (void)showCommand;

    constexpr uint32_t WindowWidth = 1280; //Editor親Windowの初期クライアント幅
    constexpr uint32_t WindowHeight = 720; //Editor親Windowの初期クライアント高さ

    Engine::MessageLog& Log =
        Engine::MessageLog::GetInstance(); // Engine全体で共有するMessage Log
    Log.AddPermanentLog(
        "DirectX 12 Component Engine | Permanent logs are excluded from Clear Logs."
    );
    Log.AddPermanentLog(
        "ViewScene | The selected scene and every camera keep their own RenderTexture."
    );

    if (!SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) &&
        GetLastError() != ERROR_ACCESS_DENIED)
    {
        Log.AddLog(
            "[Warning] WinMain | Per-monitor DPI awareness could not be enabled."
        );
    }

    const HRESULT ComResult = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED
    ); //WICとWindows UIリソースで共有するCOM初期化結果

    const bool MustUninitializeCom =
        ComResult == S_OK || ComResult == S_FALSE; //この関数がCOM解放を担当する場合はtrue

    if (FAILED(ComResult))
    {
        Log.AddLog(
            "[Warning] WinMain | COM initialization failed; WIC image loading may be unavailable."
        );
    }

    Engine::WinApp Window; //Windows標準コントロールを所有するEditor Window

    if (!Window.Create(
        L"DirectX 12 Component Engine",
        WindowWidth,
        WindowHeight))
    {
        Log.AddPermanentLog(
            "[Critical] WinMain | Editor Window creation failed."
        );
        MessageBoxW(
            nullptr,
            L"Editor Windowの作成に失敗しました。",
            L"DirectX 12 Engine",
            MB_OK | MB_ICONERROR
        );

        if (MustUninitializeCom)
        {
            CoUninitialize();
        }

        return -1;
    }

	Engine::WindowsUISettings UISettings; // Windows標準UIの既定設定
    UISettings.TexturePath = L"UIDemo.png";

	Window.SetUISettings(UISettings);

    Log.AddLog("[Info] WinMain | Editor Window created.");

    Engine::GameRuntime Runtime; //UIとは別ThreadでGame更新と描画を所有するRuntime

    if (!Runtime.Start(
        Window.GetRenderHwnd(),
        Window.GetRenderWidth(),
        Window.GetRenderHeight(),
        Window.GetTargetFrameRate(),
        Window.GetPreviewHwnd()))
    {
        Log.AddPermanentLog(
            "[Critical] WinMain | Engine startup initialization failed."
        );
        const std::wstring FailureMessage = BuildStartupFailureMessage();
        MessageBoxW(
            Window.GetHWND(),
            FailureMessage.c_str(),
            L"DirectX 12 Engine",
            MB_OK | MB_ICONERROR
        );

        Window.Destroy();

        if (MustUninitializeCom)
        {
            CoUninitialize();
        }

        return -1;
    }

    std::uint64_t DisplayedLogRevision =
        (std::numeric_limits<std::uint64_t>::max)(); // UIへ最後に反映したログ更新番号
    RefreshLogDisplay(Window, DisplayedLogRevision);
    Engine::EditorSnapshot InitialSnapshot; //Game Threadが初期化した最初のEngine状態

    if (Runtime.PollEditorSnapshot(InitialSnapshot))
    {
        Window.UpdateEditorSnapshot(InitialSnapshot);
    }

    while (Window.ProcessMessage())
    {
        Engine::EditorCommand EditorCommand; //Object Treeで発生した未処理Engine操作

        while (Window.ConsumeEditorCommand(EditorCommand))
        {
            Runtime.QueueEditorCommand(std::move(EditorCommand));
        }

        Engine::EditorSnapshot Snapshot; //Game Threadから受け取る最新Engine構造

        if (Runtime.PollEditorSnapshot(Snapshot))
        {
            Window.UpdateEditorSnapshot(Snapshot);
        }

        if (Window.ConsumeClearLogs())
        {
            Log.ClearLogs();
            Log.AddLog(
                "[Info] MessageLog | Clear Logs removed all non-permanent entries."
            );
        }

        if (Window.ConsumeStart())
        {
            Runtime.RequestPlaybackStart();
        }

        if (Window.ConsumePause())
        {
            Runtime.RequestPlaybackPause();
        }

        if (Window.ConsumeStop())
        {
            Runtime.RequestPlaybackStop();
        }

        if (Window.ConsumeTick())
        {
            Runtime.RequestTick();
            Log.AddLog("[Info] Playback | Single-frame Tick requested.");
        }

        Runtime.SetTargetFrameRate(Window.GetTargetFrameRate());

        Engine::RenderWindowSize RenderSize{}; //未処理のviewportサイズ変更

        if (Window.ConsumeResize(RenderSize))
        {
            Runtime.RequestResize(RenderSize);
        }

        RefreshLogDisplay(Window, DisplayedLogRevision);

        if (!Runtime.IsOperational())
        {
            Log.AddPermanentLog(
                "[Critical] WinMain | Game Runtime Thread stopped unexpectedly."
            );
            RefreshLogDisplay(Window, DisplayedLogRevision);
            break;
        }

        const DWORD WaitResult = MsgWaitForMultipleObjectsEx(
            0,
            nullptr,
            16,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE
        ); // 次の更新時刻又はWindows Messageを待つ結果

        if (WaitResult == WAIT_FAILED)
        {
            Log.AddPermanentLog(
                "[Critical] WinMain | Waiting for Windows messages failed."
            );
            RefreshLogDisplay(Window, DisplayedLogRevision);
            break;
        }
    }

    Runtime.Shutdown();
    Window.Destroy();

    if (MustUninitializeCom)
    {
        CoUninitialize();
    }

    return 0;
}
