#include "DDSImage.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>

namespace Engine
{
    namespace
    {
        using Byte = unsigned char;
        std::uint32_t Read32(const Byte* p)
        {
            return p[0] | (std::uint32_t(p[1]) << 8) |
                (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
        }
        constexpr std::uint32_t FourCC(char a, char b, char c, char d)
        {
            return std::uint32_t(a) | (std::uint32_t(b) << 8) |
                (std::uint32_t(c) << 16) | (std::uint32_t(d) << 24);
        }
        std::array<Byte, 4> RGB565(unsigned value)
        {
            unsigned r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
            return { Byte((r << 3) | (r >> 2)), Byte((g << 2) | (g >> 4)),
                Byte((b << 3) | (b >> 2)), 255 };
        }
    }

    bool DecodeDDS(const std::wstring& path, std::vector<Byte>& pixels,
        std::uint32_t& width, std::uint32_t& height, std::string& error)
    {
        auto Fail = [&](const char* text) { error = text; return false; };
        std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file) return Fail("DDS file could not be opened.");
        const auto size = file.tellg();
        if (size < 128 || size > 256 * 1024 * 1024)
            return Fail("DDS is truncated or exceeds the 256 MiB limit.");
        std::vector<Byte> data(static_cast<std::size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) return Fail("DDS read failed.");
        const auto Header = [&](int offset) { return Read32(data.data() + offset); };
        if (Header(0) != FourCC('D','D','S',' ') || Header(4) != 124 || Header(76) != 32)
            return Fail("Invalid DDS header.");
        width = Header(16); height = Header(12);
        if (!width || !height || width > 16384 || height > 16384 ||
            std::uint64_t(width) * height > 64 * 1024 * 1024)
            return Fail("DDS dimensions exceed the supported limit.");
        if ((Header(112) & 0x200200) || Header(24) > 1)
            return Fail("Use a 2D DDS image; volume and cubemap DDS files are not supported.");
        std::size_t offset = 128;
        unsigned bc = 0, bits = Header(88);
        std::uint32_t masks[4] = { Header(92), Header(96), Header(100), Header(104) };
        const auto code = Header(84);
        if (Header(80) & 4)
        {
            if (code == FourCC('D','X','T','1')) bc = 1;
            else if (code == FourCC('D','X','T','3')) bc = 2;
            else if (code == FourCC('D','X','T','5')) bc = 3;
            else if (code == FourCC('D','X','1','0'))
            {
                if (data.size() < 148) return Fail("Truncated DX10 DDS header.");
                if (Header(132) != 3 || Header(140) != 1 || (Header(136) & 4))
                    return Fail("Only single 2D DDS textures are supported.");
                const auto format = Header(128);
                if (format == 71 || format == 72) bc = 1;
                else if (format == 74 || format == 75) bc = 2;
                else if (format == 77 || format == 78) bc = 3;
                else if (format == 28 || format == 29 || format == 87 || format == 91 || format == 88 || format == 93)
                {
                    bits = 32;
                    const bool rgba = format == 28 || format == 29;
                    masks[0] = rgba ? 0xff : 0xff0000;
                    masks[1] = 0xff00; masks[2] = rgba ? 0xff0000 : 0xff;
                    masks[3] = format == 88 || format == 93 ? 0 : 0xff000000;
                }
                else return Fail("DDS format unsupported. Use BC1/BC2/BC3 or RGBA/BGRA8.");
                offset = 148;
            }
            else return Fail("DDS compression unsupported. Use DXT1, DXT3 or DXT5.");
        }
        else if (!(Header(80) & 0x40)) return Fail("DDS must contain RGB color data.");
        if (!bc && bits != 24 && bits != 32) return Fail("DDS requires RGB24 or RGBA32 pixels.");
        const std::size_t blockBytes = bc == 1 ? 8 : 16;
        const std::size_t tightPitch = std::size_t(width) * (bits / 8);
        const std::size_t pitch = !bc && (Header(8) & 8) ? Header(20) : tightPitch;
        if (!bc && (pitch < tightPitch || pitch > tightPitch + 4096)) return Fail("Invalid DDS row pitch.");
        // Validate every declared mip before accessing the top level.
        unsigned mipCount = std::max(1u, Header(28)), maxMips = 1;
        for (unsigned n = std::max(width, height); n > 1; n >>= 1) ++maxMips;
        if (mipCount > maxMips) return Fail("Invalid DDS mip count.");
        std::uint64_t required = offset;
        unsigned w = width, h = height;
        for (unsigned mip = 0; mip < mipCount; ++mip)
        {
            required += bc ? std::uint64_t((w + 3) / 4) * ((h + 3) / 4) * blockBytes
                : std::uint64_t(mip == 0 ? pitch : std::size_t(w) * (bits / 8)) * h;
            w = std::max(1u, w / 2); h = std::max(1u, h / 2);
        }
        if (required > data.size()) return Fail("DDS pixel or mip data is truncated.");
        pixels.assign(std::size_t(width) * height * 4, 255);
        if (!bc)
        {
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = 0; x < width; ++x)
                {
                    const Byte* p = data.data() + offset + y * pitch + x * (bits / 8);
                    std::uint32_t value = p[0] | (unsigned(p[1]) << 8) | (unsigned(p[2]) << 16);
                    if (bits == 32) value |= unsigned(p[3]) << 24;
                    for (unsigned c = 0; c < 4; ++c)
                    {
                        unsigned mask = masks[c], shift = 0;
                        if (!mask) { pixels[(std::size_t(y) * width + x) * 4 + c] = c == 3 ? 255 : 0; continue; }
                        while (!(mask & 1)) { mask >>= 1; ++shift; }
                        if (mask != 255) return Fail("DDS requires 8-bit RGB channels.");
                        pixels[(std::size_t(y) * width + x) * 4 + c] = Byte((value >> shift) & mask);
                    }
                }
            return true;
        }
        const unsigned blocksX = (width + 3) / 4;
        for (unsigned by = 0; by < (height + 3) / 4; ++by)
            for (unsigned bx = 0; bx < blocksX; ++bx)
            {
                const Byte* block = data.data() + offset + (std::size_t(by) * blocksX + bx) * blockBytes;
                const Byte* color = block + (bc == 1 ? 0 : 8);
                const unsigned c0 = color[0] | (unsigned(color[1]) << 8);
                const unsigned c1 = color[2] | (unsigned(color[3]) << 8);
                std::array<std::array<Byte, 4>, 4> colors{ RGB565(c0), RGB565(c1) };
                for (int c = 0; c < 3; ++c)
                {
                    colors[2][c] = Byte(c0 > c1 || bc != 1 ? (2 * colors[0][c] + colors[1][c]) / 3 : (colors[0][c] + colors[1][c]) / 2);
                    colors[3][c] = Byte(c0 > c1 || bc != 1 ? (colors[0][c] + 2 * colors[1][c]) / 3 : 0);
                }
                colors[2][3] = 255; colors[3][3] = c0 > c1 || bc != 1 ? 255 : 0;
                const unsigned indices = Read32(color + 4);
                Byte alpha[8] = { block[0], block[1] };
                std::uint64_t alphaIndices = 0;
                if (bc == 3)
                {
                    for (unsigned i = 0; i < 6; ++i) alphaIndices |= std::uint64_t(block[2 + i]) << (8 * i);
                    const unsigned count = alpha[0] > alpha[1] ? 7 : 5;
                    for (unsigned i = 1; i < count; ++i) alpha[i + 1] = Byte(((count - i) * alpha[0] + i * alpha[1]) / count);
                    if (count == 5) { alpha[6] = 0; alpha[7] = 255; }
                }
                for (unsigned i = 0; i < 16; ++i)
                {
                    unsigned x = bx * 4 + i % 4, y = by * 4 + i / 4;
                    if (x >= width || y >= height) continue;
                    auto pixel = colors[(indices >> (i * 2)) & 3];
                    if (bc == 2) pixel[3] = Byte(((block[i / 2] >> (4 * (i % 2))) & 15) * 17);
                    if (bc == 3) pixel[3] = alpha[(alphaIndices >> (3 * i)) & 7];
                    std::copy(pixel.begin(), pixel.end(), pixels.begin() + (std::size_t(y) * width + x) * 4);
                }
            }
        return true;
    }
}
