//|| GameRuntime.cpp ||::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Engine更新とDirectX描画をWindows Editor Threadから分離する
//||

#include "GameRuntime.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

#include "EngineAPI.h"
#include "EngineDiagnostics.h"
#include "FrameRateController.h"
#include "GameApp.h"
#include "MessageLog.h"

namespace Engine
{
    //概要：停止済みのGame専用Thread管理器を作成する
    //引数：なし
    //戻り値：なし
    GameRuntime::GameRuntime()
        : RenderWindow(nullptr)
        , InitialWidth(0)
        , InitialHeight(0)
        , RequestedFrameRate(60)
        , Worker()
        , RuntimeMutex()
        , WakeCondition()
        , EditorCommands()
        , PendingResize()
        , PublishedSnapshot()
        , Running(false)
        , InitializationComplete(false)
        , InitializationSucceeded(false)
        , RuntimeFailed(false)
        , StartPending(false)
        , StopPending(false)
        , TickPendingCount(0)
        , FrameRatePending(false)
    {
    }

    //概要：Game Threadを停止して所有Resourceを解放する
    //引数：なし
    //戻り値：なし
    GameRuntime::~GameRuntime()
    {
        Shutdown();
    }

    //概要：GameAppとDirectXを専用Thread上で初期化する
    //引数：renderWindow=描画先子Window、width=初期幅、height=初期高さ、targetFrameRate=固定更新FPS
    //戻り値：Game Threadと描画基盤を開始できた場合true
    bool GameRuntime::Start(
        HWND renderWindow,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t targetFrameRate
    )
    {
        if (renderWindow == nullptr || width == 0 || height == 0 || Worker.joinable())
        {
            return false;
        }

        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //初期設定公開を保護するGuard
            RenderWindow = renderWindow;
            InitialWidth = width;
            InitialHeight = height;
            RequestedFrameRate = std::clamp(targetFrameRate, 1u, 240u);
            Running = true;
            InitializationComplete = false;
            InitializationSucceeded = false;
            RuntimeFailed = false;
        }

        Worker = std::thread(&GameRuntime::ThreadMain, this);
        std::unique_lock<std::mutex> Lock(RuntimeMutex); //初期化完了通知を待つLock
        WakeCondition.wait(Lock, [this]() { return InitializationComplete; });
        const bool Succeeded = InitializationSucceeded; //Start呼出元へ返す初期化結果
        Lock.unlock();

        if (!Succeeded && Worker.joinable())
        {
            Worker.join();
        }

        return Succeeded;
    }

    //概要：Game ThreadへEditor操作をFIFO順で渡す
    //引数：command=Object、Component、Script又はDLL操作
    //戻り値：なし
    void GameRuntime::QueueEditorCommand(EditorCommand command)
    {
        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //Editor操作Queueを保護するGuard
            EditorCommands.emplace_back(std::move(command));
        }
        WakeCondition.notify_one();
    }

    //概要：Game Threadへ連続再生開始を要求する
    //引数：なし
    //戻り値：なし
    void GameRuntime::RequestPlaybackStart()
    {
        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //再生要求状態を保護するGuard
            StartPending = true;
            StopPending = false;
            TickPendingCount = 0;
        }
        WakeCondition.notify_one();
    }

    //概要：Game Threadへ連続再生停止を要求する
    //引数：なし
    //戻り値：なし
    void GameRuntime::RequestPlaybackStop()
    {
        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //停止要求状態を保護するGuard
            StopPending = true;
            StartPending = false;
            TickPendingCount = 0;
        }
        WakeCondition.notify_one();
    }

    //概要：停止中Gameへ一Frame更新を要求する
    //引数：なし
    //戻り値：なし
    void GameRuntime::RequestTick()
    {
        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //Tick要求数を保護するGuard

            if (TickPendingCount < (std::numeric_limits<std::uint32_t>::max)())
            {
                ++TickPendingCount;
            }
        }
        WakeCondition.notify_one();
    }

    //概要：Game Threadで使用する固定更新FPSを変更する
    //引数：targetFrameRate=1から240の要求FPS
    //戻り値：なし
    void GameRuntime::SetTargetFrameRate(std::uint32_t targetFrameRate)
    {
        const std::uint32_t Clamped = std::clamp(targetFrameRate, 1u, 240u); //Engine許容範囲内のFPS

        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //FPS要求を保護するGuard

            if (RequestedFrameRate == Clamped && !FrameRatePending)
            {
                return;
            }

            RequestedFrameRate = Clamped;
            FrameRatePending = true;
        }
        WakeCondition.notify_one();
    }

    //概要：描画子Windowの最新SizeをGame Threadへ渡す
    //引数：size=幅と高さ
    //戻り値：なし
    void GameRuntime::RequestResize(const RenderWindowSize& size)
    {
        if (size.Width == 0 || size.Height == 0)
        {
            return;
        }

        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //最新Resize要求を保護するGuard
            PendingResize = size;
        }
        WakeCondition.notify_one();
    }

    //概要：Game Threadが公開した最新Editor Snapshotを取得する
    //引数：snapshot=所有権付きSnapshot格納先
    //戻り値：新しいSnapshotが存在した場合true
    bool GameRuntime::PollEditorSnapshot(EditorSnapshot& snapshot)
    {
        const std::lock_guard<std::mutex> Lock(RuntimeMutex); //Snapshot移動を保護するGuard

        if (!PublishedSnapshot.has_value())
        {
            return false;
        }

        snapshot = std::move(*PublishedSnapshot);
        PublishedSnapshot.reset();
        return true;
    }

    //概要：Game Threadが初期化済みで継続実行可能か判定する
    //引数：なし
    //戻り値：Game Runtimeが正常な場合true
    bool GameRuntime::IsOperational() const
    {
        const std::lock_guard<std::mutex> Lock(RuntimeMutex); //Runtime状態読取を保護するGuard
        return InitializationSucceeded && Running && !RuntimeFailed;
    }

    //概要：Game Threadへ終了を通知してResource解放完了を待つ
    //引数：なし
    //戻り値：なし
    void GameRuntime::Shutdown()
    {
        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //終了状態公開を保護するGuard
            Running = false;
        }
        WakeCondition.notify_one();

        if (Worker.joinable())
        {
            Worker.join();
        }
    }

    //概要：GameApp、EngineAPI、Main、Sub Script、描画Loopを専用Threadで実行する
    //引数：なし
    //戻り値：なし
    void GameRuntime::ThreadMain()
    {
        const HRESULT ComResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED); //Game Thread用COM初期化結果
        const bool MustUninitializeCom = SUCCEEDED(ComResult); //終了時CoUninitializeが必要な場合true
        GameApp Application; //Game Threadだけが所有する描画及びScene
        const bool Initialized = Application.Initialize(
            RenderWindow,
            InitialWidth,
            InitialHeight
        ); //DirectXとMain Scene初期化結果

        if (!Initialized)
        {
            {
                const std::lock_guard<std::mutex> Lock(RuntimeMutex); //初期化失敗公開を保護するGuard
                InitializationComplete = true;
                InitializationSucceeded = false;
                RuntimeFailed = true;
                Running = false;
            }
            WakeCondition.notify_all();

            if (MustUninitializeCom)
            {
                CoUninitialize();
            }
            return;
        }

        EngineAPI NativeAPI(Application); //Game Thread内だけでEngine状態を操作するFacade
        const bool DiagnosticMode = IsEngineDiagnosticModeEnabled(); //明示的な実行時診断起動の場合true

        if (DiagnosticMode)
        {
            const EngineDiagnosticResult DiagnosticResult = RunEngineDiagnostics(NativeAPI); //主要LifecycleとAPIの診断結果

            if (!WriteEngineDiagnosticReport(DiagnosticResult))
            {
                MessageLog::GetInstance().AddPermanentLog(
                    "[Critical] Diagnostics | Diagnostic report could not be written."
                );
            }

            MessageLog::GetInstance().AddPermanentLog(
                DiagnosticResult.Passed()
                    ? "[Info] Diagnostics | All runtime checks passed."
                    : "[Critical] Diagnostics | One or more runtime checks failed."
            );
        }

        FrameRateController FrameRate; //Game Thread専用固定更新管理器
        FrameRate.SetTargetFrameRate(RequestedFrameRate);
        std::uint64_t PublishedRevision = NativeAPI.GetRevision(); //最後に公開したEngine構造Revision

        {
            const std::lock_guard<std::mutex> Lock(RuntimeMutex); //初期Snapshotと成功状態を公開するGuard
            PublishedSnapshot = NativeAPI.CreateEditorSnapshot();
            InitializationComplete = true;
            InitializationSucceeded = true;
        }
        WakeCondition.notify_all();

        if (DiagnosticMode)
        {
            HWND RootWindow = GetAncestor(RenderWindow, GA_ROOT); //診断完了後に閉じるEditor親Window
            PostMessageW(RootWindow != nullptr ? RootWindow : RenderWindow, WM_CLOSE, 0, 0);
        }

        bool NeedsDraw = true; //Scene出力を更新する場合true

        while (true)
        {
            std::deque<EditorCommand> Commands; //今回実行するEditor操作
            std::optional<RenderWindowSize> Resize; //今回反映する描画Size
            bool Start = false; //今回再生開始する場合true
            bool Stop = false; //今回再生停止する場合true
            std::uint32_t TickCount = 0; //今回反映する一Frame要求数
            bool ChangeFrameRate = false; //今回FPSを変更する場合true
            std::uint32_t TargetFrameRate = 60; //今回設定するFPS

            {
                const std::lock_guard<std::mutex> Lock(RuntimeMutex); //UI要求を一括取得するGuard

                if (!Running)
                {
                    break;
                }

                Commands.swap(EditorCommands);
                Resize = PendingResize;
                PendingResize.reset();
                Start = StartPending;
                Stop = StopPending;
                StartPending = false;
                StopPending = false;
                TickCount = TickPendingCount;
                TickPendingCount = 0;
                ChangeFrameRate = FrameRatePending;
                FrameRatePending = false;
                TargetFrameRate = RequestedFrameRate;
            }

            for (EditorCommand& Command : Commands)
            {
                if (!NativeAPI.ExecuteEditorCommand(Command))
                {
                    MessageLog::GetInstance().AddLog(
                        "[Warning] Editor | Requested game-thread operation failed."
                    );
                }
                else
                {
                    NeedsDraw = true;
                }
            }

            if (Start)
            {
                FrameRate.Start();
                MessageLog::GetInstance().AddLog("[Info] Playback | Start requested.");
            }

            if (Stop)
            {
                FrameRate.Stop();
                MessageLog::GetInstance().AddLog("[Info] Playback | Stop requested.");
            }

            while (TickCount-- > 0)
            {
                FrameRate.RequestTick();
            }

            if (ChangeFrameRate)
            {
                FrameRate.SetTargetFrameRate(TargetFrameRate);
            }

            if (Resize.has_value() &&
                !Application.Resize(Resize->Width, Resize->Height))
            {
                MessageLog::GetInstance().AddPermanentLog(
                    "[Critical] GameRuntime | Render viewport resize failed."
                );
                const std::lock_guard<std::mutex> Lock(RuntimeMutex); //Runtime失敗状態を公開するGuard
                RuntimeFailed = true;
                Running = false;
                break;
            }

            float DeltaTime = 0.0f; //今回の固定更新秒数

            if (FrameRate.TryConsumeStep(DeltaTime))
            {
                NativeAPI.UpdateExtensions(DeltaTime);
                Application.Update(DeltaTime);
                NeedsDraw = true;
            }

            if (NeedsDraw)
            {
                Application.Draw();
                NeedsDraw = false;
            }

            if (PublishedRevision != NativeAPI.GetRevision())
            {
                EditorSnapshot Snapshot = NativeAPI.CreateEditorSnapshot(); //UIへ公開する一貫したEngine状態
                PublishedRevision = Snapshot.Revision;
                const std::lock_guard<std::mutex> Lock(RuntimeMutex); //最新Snapshot置換を保護するGuard
                PublishedSnapshot = std::move(Snapshot);
            }

            const std::uint32_t WaitMilliseconds = FrameRate.GetWaitMilliseconds(); //次Frameまで待てる時間
            std::unique_lock<std::mutex> Lock(RuntimeMutex); //要求又はFrame時刻を待つLock
            WakeCondition.wait_for(
                Lock,
                std::chrono::milliseconds(WaitMilliseconds),
                [this]() { return !Running || HasPendingRequestLocked(); }
            );
        }

        Application.Finalize();

        if (MustUninitializeCom)
        {
            CoUninitialize();
        }
    }

    //概要：Condition Variableを即時解除すべき未処理要求があるか判定する
    //引数：なし
    //戻り値：Queue又は状態変更要求がある場合true
    bool GameRuntime::HasPendingRequestLocked() const
    {
        return !EditorCommands.empty() || PendingResize.has_value() ||
            StartPending || StopPending || TickPendingCount > 0 || FrameRatePending;
    }
}
