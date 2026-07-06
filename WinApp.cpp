#include "WinApp.h"

namespace Engine
{
    WinApp::WinApp()
        : m_HWND(nullptr)
        , m_Instance(nullptr)
        , m_Width(0)
        , m_Height(0)
        , m_ClassName(L"DX12EngineWindowClass")
    {
    }

    WinApp::~WinApp()
    {
        Destroy();
    }

    bool WinApp::Create(
        const wchar_t* title,
        uint32_t width,
        uint32_t height
    )
    {
        m_Instance = GetModuleHandleW(nullptr);
        m_Width = width;
        m_Height = height;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WinApp::WindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = m_Instance;
        wc.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = m_ClassName.c_str();
        wc.hIconSm = LoadIconA(nullptr, IDI_APPLICATION);

        if (!RegisterClassExW(&wc))
        {
            return false;
        }

        RECT windowRect{};
        windowRect.left = 0;
        windowRect.top = 0;
        windowRect.right = static_cast<LONG>(width);
        windowRect.bottom = static_cast<LONG>(height);

        AdjustWindowRect(
            &windowRect,
            WS_OVERLAPPEDWINDOW,
            FALSE
        );

        const int windowWidth =
            windowRect.right - windowRect.left;

        const int windowHeight =
            windowRect.bottom - windowRect.top;

        m_HWND = CreateWindowExW(
            0,
            m_ClassName.c_str(),
            title,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowWidth,
            windowHeight,
            nullptr,
            nullptr,
            m_Instance,
            nullptr
        );

        if (!m_HWND)
        {
            return false;
        }

        ShowWindow(m_HWND, SW_SHOW);
        UpdateWindow(m_HWND);

        return true;
    }

    void WinApp::Destroy()
    {
        if (m_HWND)
        {
            DestroyWindow(m_HWND);
            m_HWND = nullptr;
        }

        if (m_Instance)
        {
            UnregisterClassW(
                m_ClassName.c_str(),
                m_Instance
            );

            m_Instance = nullptr;
        }
    }

    bool WinApp::ProcessMessage()
    {
        MSG msg{};

        while (PeekMessageW(
            &msg,
            nullptr,
            0,
            0,
            PM_REMOVE
        ))
        {
            if (msg.message == WM_QUIT)
            {
                return false;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return true;
    }

    HWND WinApp::GetHWND() const
    {
        return m_HWND;
    }

    HINSTANCE WinApp::GetInstance() const
    {
        return m_Instance;
    }

    uint32_t WinApp::GetWidth() const
    {
        return m_Width;
    }

    uint32_t WinApp::GetHeight() const
    {
        return m_Height;
    }

    LRESULT CALLBACK WinApp::WindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wparam,
        LPARAM lparam
    )
    {
        switch (msg)
        {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(
            hwnd,
            msg,
            wparam,
            lparam
        );
    }
}