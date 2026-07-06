#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace Engine
{
    class WinApp final
    {
    public:
        WinApp();
        ~WinApp();

        WinApp(const WinApp&) = delete;
        WinApp& operator=(const WinApp&) = delete;

        bool Create(
            const wchar_t* title,
            uint32_t width,
            uint32_t height
        );

        void Destroy();

        bool ProcessMessage();

        HWND GetHWND() const;
        HINSTANCE GetInstance() const;

        uint32_t GetWidth() const;
        uint32_t GetHeight() const;

    private:
        static LRESULT CALLBACK WindowProc(
            HWND hwnd,
            UINT msg,
            WPARAM wparam,
            LPARAM lparam
        );

    private:
        HWND m_HWND;
        HINSTANCE m_Instance;

        uint32_t m_Width;
        uint32_t m_Height;

        std::wstring m_ClassName;
    };
}