//|| GameInput.h ||:::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Game描画領域へFocusがある場合だけKeyboard入力を公開する
//||

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <atomic>
#include <cstdint>

namespace Engine
{
    namespace GameInput
    {
        inline std::atomic<HWND> InputWindow = nullptr; //Game入力を受け取る描画子Window

        inline void SetInputWindow(HWND window)
        {
            InputWindow.store(window, std::memory_order_release);
        }

        inline void ClearInputWindow(HWND window)
        {
            HWND Expected = window;
            InputWindow.compare_exchange_strong(
                Expected,
                nullptr,
                std::memory_order_acq_rel
            );
        }

        inline bool HasFocus()
        {
            const HWND Window = InputWindow.load(std::memory_order_acquire); //確認対象描画Window

            if (Window == nullptr || !IsWindow(Window))
            {
                return false;
            }

            const DWORD UIThread = GetWindowThreadProcessId(Window, nullptr); //描画Window所有Thread
            GUITHREADINFO Information{}; //別ThreadのFocus状態
            Information.cbSize = sizeof(GUITHREADINFO);

            if (UIThread == 0 || !GetGUIThreadInfo(UIThread, &Information) ||
                Information.hwndFocus != Window)
            {
                return false;
            }

            const HWND Root = GetAncestor(Window, GA_ROOT); //前面確認対象Editor Window
            return Root != nullptr && GetForegroundWindow() == Root;
        }

        inline bool IsKeyDown(std::uint32_t virtualKey)
        {
            return virtualKey <= 0xffu && HasFocus() &&
                (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
        }
    }
}
