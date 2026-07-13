//|| FrameRateController.h ||:::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  再生、停止、コマ送りと動的フレームレートを固定刻みで管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

#include <chrono>
#include <cstdint>

namespace Engine
{
    class FrameRateController final
    {
    public:
        //停止状態かつ60 FPSの固定更新管理器を作成する
        FrameRateController();

        //連続固定更新を開始する
        void Start();

        //連続固定更新と保留中のコマ送りを停止する
        void Stop();

        //停止中に一回だけ実行する固定更新を予約する
        void RequestTick();

        //固定更新FPSを許容範囲内へ設定する
        //引数: targetFrameRate 新しい固定更新FPS
        void SetTargetFrameRate(uint32_t targetFrameRate);

        //連続固定更新を実行中か確認する
        //戻り値: Start後かつStop前の場合はtrue
        bool IsPlaying() const;
        uint32_t GetTargetFrameRate() const;

        //更新時刻に達した場合だけ固定DeltaTimeを返す
        //引数: deltaTime 更新に使用する秒数の出力先
        //戻り値: 今回Updateを実行する場合はtrue
        bool TryConsumeStep(float& deltaTime);

        //次の更新またはUI確認まで待機できる時間を求める
        //戻り値: 待機可能なミリ秒数
        uint32_t GetWaitMilliseconds() const;

    private:
        using Clock = std::chrono::steady_clock;

        static constexpr uint32_t MinimumFrameRate = 1; //UIから設定できる最小FPS
        static constexpr uint32_t MaximumFrameRate = 240; //UIから設定できる最大FPS

        Clock::time_point NextFrameTime; //次にUpdateを許可する時刻
        uint32_t TargetFrameRate; //現在設定されている固定FPS
        bool Playing; //連続Updateを行う場合はtrue
        bool TickPending; //停止中の1回更新要求がある場合はtrue
    };
}
