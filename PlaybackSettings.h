#pragma once
#include <cmath>

namespace Engine
{
    struct PlaybackSettings final
    {
        float TimeScale = 1.0f;
        float GravityX = 0.0f;
        float GravityY = -9.81f;
        float GravityZ = 0.0f;
        float LinearDrag = 0.1f;
        float Restitution = 0.3f;
        float GroundHeight = 0.0f;
        float MaxFallSpeed = 55.0f;
        float CameraSpeed = 5.0f;
        float SkyYaw = 0.0f;
        float SkyExposure = 1.0f;
        bool GroundEnabled = true;

        bool IsValid() const
        {
            const float values[] = { TimeScale, GravityX, GravityY, GravityZ, LinearDrag,
                Restitution, GroundHeight, MaxFallSpeed, CameraSpeed, SkyYaw, SkyExposure };
            for (float value : values) if (!std::isfinite(value)) return false;
            return TimeScale >= 0 && TimeScale <= 10 &&
                std::abs(GravityX) <= 1000 && std::abs(GravityY) <= 1000 && std::abs(GravityZ) <= 1000 &&
                LinearDrag >= 0 && LinearDrag <= 100 && Restitution >= 0 && Restitution <= 1 &&
                std::abs(GroundHeight) <= 100000 && MaxFallSpeed >= 0.1f && MaxFallSpeed <= 10000 &&
                CameraSpeed >= 0 && CameraSpeed <= 1000 && std::abs(SkyYaw) <= 360 &&
                SkyExposure >= 0 && SkyExposure <= 10;
        }
    };
    // UIはCommandの値コピーで送信し、更新・描画を行うスレッドだけが参照する。
    inline thread_local PlaybackSettings ActivePlaybackSettings;
}
