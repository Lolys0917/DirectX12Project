#include <Windows.h>
#include <chrono>

#include "WinApp.h"
#include "GameApp.h"

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    constexpr uint32_t WindowWidth = 1280;
    constexpr uint32_t WindowHeight = 720;

    Engine::WinApp winApp;

    if (!winApp.Create(
        L"DirectX12 Engine",
        WindowWidth,
        WindowHeight))
    {
        MessageBoxA(
            nullptr,
            "Window Create Failed",
            "Error",
            MB_OK
        );

        return -1;
    }

    Engine::GameApp gameApp;

    if (!gameApp.Initialize(
        winApp.GetHWND(),
        winApp.GetWidth(),
        winApp.GetHeight()))
    {
        MessageBoxA(
            nullptr,
            "GameApp Initialize Failed",
            "Error",
            MB_OK
        );

        return -1;
    }

    auto previousTime =
        std::chrono::high_resolution_clock::now();

    while (winApp.ProcessMessage())
    {
        auto currentTime =
            std::chrono::high_resolution_clock::now();

        std::chrono::duration<float> elapsed =
            currentTime - previousTime;

        previousTime = currentTime;

        float deltaTime = elapsed.count();

        gameApp.Update(deltaTime);
        gameApp.Draw();
    }

    gameApp.Finalize();
    winApp.Destroy();

    return 0;
}