#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Engine
{
    // DDSの先頭ミップをRGBAへ展開する。非4倍数の画像寸法も保持する。
    bool DecodeDDS(const std::wstring& path, std::vector<unsigned char>& pixels,
        std::uint32_t& width, std::uint32_t& height, std::string& error);
}
