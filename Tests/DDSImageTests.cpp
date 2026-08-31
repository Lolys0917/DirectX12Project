#include "../DDSImage.h"
#include "../PlaybackSettings.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#include <stdexcept>

namespace
{
    using Bytes = std::vector<unsigned char>;
    void Write32(Bytes& data, int offset, unsigned value)
    { for (int i = 0; i < 4; ++i) data[offset + i] = static_cast<unsigned char>(value >> (8 * i)); }
    Bytes Header(unsigned width, unsigned height, unsigned code, unsigned payload)
    {
        Bytes data(128 + payload);
        Write32(data, 0, 0x20534444); Write32(data, 4, 124); Write32(data, 8, 0x81007);
        Write32(data, 12, height); Write32(data, 16, width); Write32(data, 28, 1);
        Write32(data, 76, 32); Write32(data, 80, 4); Write32(data, 84, code);
        Write32(data, 108, 0x1000); return data;
    }
    void Check(bool ok, const char* name) { if (!ok) throw std::runtime_error(name); std::cout << "PASS " << name << '\n'; }
}

int main(int argc, char** argv)
{
    try
    {
        const auto directory = std::filesystem::path("Tests") / ".output";
        std::filesystem::create_directories(directory);
        const auto fixture = directory / "fixture.dds";
        Bytes pixels;
        unsigned width = 0, height = 0;
        std::string error;
        const auto Decode = [&](const Bytes& bytes) {
            std::ofstream file(fixture, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); file.close();
            return Engine::DecodeDDS(fixture.wstring(), pixels, width, height, error);
        };
        auto bc1 = Header(3, 2, 0x31545844, 8);
        bc1[128] = 0; bc1[129] = 0xf8; // RGB565 red, all selectors zero.
        Check(Decode(bc1) && width == 3 && height == 2 && pixels.size() == 24 && pixels[20] == 255 && pixels[21] == 0 && pixels[23] == 255,
            "BC1 clips padded blocks to non-multiple-of-four dimensions");
        bc1[128] = bc1[129] = 0;
        for (int i = 132; i < 136; ++i) bc1[i] = 255;
        Check(Decode(bc1) && pixels[3] == 0, "BC1 transparent selector");
        auto bc2 = Header(4, 4, 0x33545844, 16);
        for (int i = 128; i < 136; ++i) bc2[i] = 0x88;
        bc2[136] = 0xe0; bc2[137] = 0x07;
        Check(Decode(bc2) && pixels[1] == 255 && pixels[3] == 136, "BC2 explicit alpha");
        auto bc3 = Header(4, 4, 0x35545844, 16);
        bc3[128] = 255; bc3[129] = 0; bc3[130] = 2;
        bc3[136] = 0x1f;
        Check(Decode(bc3) && pixels[2] == 255 && pixels[3] == 218, "BC3 interpolated alpha");
        auto truncated = bc3; truncated.pop_back();
        Check(!Decode(truncated), "Truncated pixel data rejected");
        auto badMip = bc1; Write32(badMip, 28, 4);
        Check(!Decode(badMip), "Invalid mip count rejected");
        auto missingMip = bc3; Write32(missingMip, 28, 2);
        Check(!Decode(missingMip), "Missing lower mip rejected");
        auto cube = bc3; Write32(cube, 112, 0x200);
        Check(!Decode(cube), "Unsupported cubemap rejected explicitly");
        auto huge = bc3; Write32(huge, 16, 0xffffffff);
        Check(!Decode(huge), "Overflow dimensions rejected");
        auto dx10 = Header(4, 4, 0x30315844, 28);
        Write32(dx10, 128, 71); Write32(dx10, 132, 3); Write32(dx10, 140, 1);
        dx10[148] = 0; dx10[149] = 0xf8;
        Check(Decode(dx10) && pixels[0] == 255, "DX10 BC1 header");
        auto rgb = Header(1, 1, 0, 4);
        Write32(rgb, 8, 0x100f); Write32(rgb, 20, 4); Write32(rgb, 80, 0x40);
        Write32(rgb, 88, 24); Write32(rgb, 92, 0xff0000); Write32(rgb, 96, 0xff00); Write32(rgb, 100, 0xff);
        rgb[128] = 10; rgb[129] = 20; rgb[130] = 30;
        Check(Decode(rgb) && pixels[0] == 30 && pixels[1] == 20 && pixels[2] == 10 && pixels[3] == 255,
            "RGB24 padded rows and channel masks");
        Check(Engine::DecodeDDS(L"Assets/Textures/joran-quinten-CRmulUkILVg-unsplash.dds", pixels, width, height, error) &&
            width == 3000 && height == 1998 && pixels.size() == 3000 * 1998 * 4, "Provided DDS image decodes at original dimensions");
        std::ofstream raw(directory / "provided.rgba", std::ios::binary);
        raw.write(reinterpret_cast<const char*>(pixels.data()), pixels.size()); raw.close();
        Engine::PlaybackSettings settings;
        Check(settings.IsValid(), "Playback defaults valid");
        settings.TimeScale = 0; Check(settings.IsValid(), "Zero game speed accepted");
        settings.TimeScale = std::numeric_limits<float>::quiet_NaN(); Check(!settings.IsValid(), "NaN setting rejected");
        settings = {}; settings.Restitution = 2; Check(!settings.IsValid(), "Out-of-range restitution rejected");
        std::filesystem::remove(fixture);
        std::cout << "All DDS and settings checks passed.\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
