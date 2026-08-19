//|| GameRuntime.h ||::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Game更新、Main、Sub Script、DirectX描画をEditor UIと別Threadで実行する
//||

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

#include "EditorTypes.h"

namespace Engine
{
    class GameRuntime final
    {
    public:
        //概要：停止済みのGame専用Thread管理器を作成する
        //引数：なし
        //戻り値：なし
        GameRuntime();

        //概要：Game Threadを停止して所有Resourceを解放する
        //引数：なし
        //戻り値：なし
        ~GameRuntime();

        GameRuntime(const GameRuntime&) = delete;
        GameRuntime& operator=(const GameRuntime&) = delete;

        //概要：GameAppとDirectXを専用Thread上で初期化する
        //引数：renderWindow=描画先子Window、width=初期幅、height=初期高さ、targetFrameRate=固定更新FPS
        //戻り値：Game Threadと描画基盤を開始できた場合true
        bool Start(
            HWND renderWindow,
            std::uint32_t width,
            std::uint32_t height,
            std::uint32_t targetFrameRate
        );

        //概要：Game ThreadへEditor操作をFIFO順で渡す
        //引数：command=Object、Component、Script又はDLL操作
        //戻り値：なし
        void QueueEditorCommand(EditorCommand command);

        //概要：Game Threadへ連続再生開始を要求する
        //引数：なし
        //戻り値：なし
        void RequestPlaybackStart();

        //概要：Game Threadへ連続再生停止を要求する
        //引数：なし
        //戻り値：なし
        void RequestPlaybackStop();

        //概要：停止中Gameへ一Frame更新を要求する
        //引数：なし
        //戻り値：なし
        void RequestTick();

        //概要：Game Threadで使用する固定更新FPSを変更する
        //引数：targetFrameRate=1から240の要求FPS
        //戻り値：なし
        void SetTargetFrameRate(std::uint32_t targetFrameRate);

        //概要：描画子Windowの最新SizeをGame Threadへ渡す
        //引数：size=幅と高さ
        //戻り値：なし
        void RequestResize(const RenderWindowSize& size);

        //概要：Game Threadが公開した最新Editor Snapshotを取得する
        //引数：snapshot=所有権付きSnapshot格納先
        //戻り値：新しいSnapshotが存在した場合true
        bool PollEditorSnapshot(EditorSnapshot& snapshot);

        //概要：Game Threadが初期化済みで継続実行可能か判定する
        //引数：なし
        //戻り値：Game Runtimeが正常な場合true
        bool IsOperational() const;

        //概要：Game Threadへ終了を通知してResource解放完了を待つ
        //引数：なし
        //戻り値：なし
        void Shutdown();

    private:
        //概要：GameApp、EngineAPI、Main、Sub Script、描画Loopを専用Threadで実行する
        //引数：なし
        //戻り値：なし
        void ThreadMain();

        //概要：Condition Variableを即時解除すべき未処理要求があるか判定する
        //引数：なし
        //戻り値：Queue又は状態変更要求がある場合true
        bool HasPendingRequestLocked() const;

        HWND RenderWindow; //DirectX SwapChainを接続する子Window
        std::uint32_t InitialWidth; //Game Thread初期描画幅
        std::uint32_t InitialHeight; //Game Thread初期描画高さ
        std::uint32_t RequestedFrameRate; //UIから受け取った最新固定更新FPS
        std::thread Worker; //Game、Script、描画専用Thread
        mutable std::mutex RuntimeMutex; //UIとGame要求及びSnapshotを保護するMutex
        std::condition_variable WakeCondition; //Frame時刻又はUI要求まで待機する条件
        std::deque<EditorCommand> EditorCommands; //Game Thread実行待ちEditor操作
        std::optional<RenderWindowSize> PendingResize; //最新描画Size要求
        std::optional<EditorSnapshot> PublishedSnapshot; //UI未取得の最新Engine状態
        bool Running; //Thread Loopを継続する場合true
        bool InitializationComplete; //Start待機を解除できる場合true
        bool InitializationSucceeded; //GameApp初期化に成功した場合true
        bool RuntimeFailed; //初期化後に継続不能Errorが発生した場合true
        bool StartPending; //再生開始要求
        bool StopPending; //再生停止要求
        std::uint32_t TickPendingCount; //未処理一Frame更新数
        bool FrameRatePending; //RequestedFrameRateを反映する場合true
    };
}
