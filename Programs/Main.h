#pragma once

#include <cstdint>

namespace Game::Main
{
    //全Scene cppから参照できる共通状態の例
    extern std::uint64_t FrameCount;
    extern float ElapsedTime;
}
