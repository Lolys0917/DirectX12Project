//|| FrameRateController.cpp ||:::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  再生、停止、コマ送りと動的フレームレートを固定刻みで管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "FrameRateController.h"

#include <algorithm>

namespace Engine
{
    //停止状態かつ60 FPSの固定更新管理器を作成する
    FrameRateController::FrameRateController()
        : NextFrameTime(Clock::now())
        , TargetFrameRate(60)
        , Playing(false)
        , TickPending(false)
    {
    }

    //連続固定更新を開始する
    void FrameRateController::Start()
    {
        Playing = true;
        TickPending = false;
        NextFrameTime = Clock::now();
    }

    //連続固定更新と保留中のコマ送りを停止する
    void FrameRateController::Stop()
    {
        Playing = false;
        TickPending = false;
    }

    //停止中に一回だけ実行する固定更新を予約する
    void FrameRateController::RequestTick()
    {
        if (!Playing)
        {
            TickPending = true;
        }
    }

    //固定更新FPSを許容範囲内へ設定する
    //引数: targetFrameRate 新しい固定更新FPS
    void FrameRateController::SetTargetFrameRate(uint32_t targetFrameRate)
    {
        TargetFrameRate = std::clamp(
            targetFrameRate,
            MinimumFrameRate,
            MaximumFrameRate
        );

        NextFrameTime = Clock::now();
    }

    //連続固定更新を実行中か確認する
    //戻り値: Start後かつStop前の場合はtrue
    bool FrameRateController::IsPlaying() const
    {
        return Playing;
    }

    //概要：固定更新へ使用する目標Frame Rateを取得する
    //引数：なし
    //戻り値：1から240の目標FPS
    uint32_t FrameRateController::GetTargetFrameRate() const
    {
        return TargetFrameRate;
    }

    //更新時刻に達した場合だけ固定DeltaTimeを返す
    //引数: deltaTime 更新に使用する秒数の出力先
    //戻り値: 今回Updateを実行する場合はtrue
    bool FrameRateController::TryConsumeStep(float& deltaTime)
    {
        const float FixedDeltaTime =
            1.0f / static_cast<float>(TargetFrameRate); //設定FPSから求めた固定更新秒数

        if (TickPending && !Playing)
        {
            TickPending = false;
            deltaTime = FixedDeltaTime;
            return true;
        }

        if (!Playing)
        {
            return false;
        }

        const Clock::time_point CurrentTime = Clock::now(); //判定時点の単調時刻

        if (CurrentTime < NextFrameTime)
        {
            return false;
        }

        const Clock::duration FrameDuration =
            std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(FixedDeltaTime)
            ); //固定FPSをClock精度へ変換した期間

        NextFrameTime += FrameDuration;

        if (CurrentTime - NextFrameTime > FrameDuration * 4)
        {
            NextFrameTime = CurrentTime + FrameDuration;
        }

        deltaTime = FixedDeltaTime;
        return true;
    }

    //次の更新またはUI確認まで待機できる時間を求める
    //戻り値: 待機可能なミリ秒数
    uint32_t FrameRateController::GetWaitMilliseconds() const
    {
        if (TickPending)
        {
            return 0;
        }

        if (!Playing)
        {
            return 50;
        }

        const Clock::time_point CurrentTime = Clock::now(); //待機時間を求める現在時刻

        if (CurrentTime >= NextFrameTime)
        {
            return 0;
        }

        const auto Remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            NextFrameTime - CurrentTime
        ); //次の固定更新までの残り時間

        return static_cast<uint32_t>(std::max<int64_t>(1, Remaining.count()));
    }
}
