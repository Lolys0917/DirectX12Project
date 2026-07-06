#include "GameApp.h"

#include <string>

namespace Engine
{
    GameApp::GameApp()
        : m_HWND(nullptr)
        , m_IsInitialized(false)
    {
    }

    GameApp::~GameApp()
    {
        Finalize();
    }

    bool GameApp::Initialize(
        HWND hwnd,
        uint32_t width,
        uint32_t height
    )
    {
        m_HWND = hwnd;

        if (!m_GraphicBase.Initialize(
            hwnd,
            width,
            height))
        {
            return false;
        }

        m_Camera.Initialize(
            static_cast<float>(width),
            static_cast<float>(height)
        );

        m_Grid.SetCamera(&m_Camera);

        if (!m_Grid.Initialize(
            m_GraphicBase.GetDirectX12()))
        {
            return false;
        }

        if (!m_OBJModel.Load(
            m_GraphicBase.GetDirectX12(),
            L"12222_Cat_v1_l3.obj", L"Cat_diffuse.jpg"))
        {
            return false;
        }

        m_OBJModel.SetPosition(
            DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)
        );

        m_OBJModel.SetScale(
            DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f)
        );

        m_OBJModel.SetRotation(
            DirectX::XMFLOAT3(
                -1.0f * 3.141592f / 2.0f,
                2.0f * 3.141592f / 2.0f,
                0.0f)
		);

        m_IsInitialized = true;

        return true;
    }

    void GameApp::Finalize()
    {
        if (!m_IsInitialized)
            return;

        m_GraphicBase.Finalize();

        m_HWND = nullptr;
        m_IsInitialized = false;
    }

    void GameApp::Update(float deltaTime)
    {
        m_Camera.Update(deltaTime);
        m_Grid.Update(deltaTime);
        m_OBJModel.Update(deltaTime);

        const DirectX::XMFLOAT3& pos =
            m_Camera.GetPosition();

        std::wstring title =
            L"DX12 Grid  Camera X:" +
            std::to_wstring(pos.x) +
            L" Y:" +
            std::to_wstring(pos.y) +
            L" Z:" +
            std::to_wstring(pos.z);

        SetWindowTextW(
            m_HWND,
            title.c_str()
        );
    }

    void GameApp::Draw()
    {
        DirectX12& dx12 =
            m_GraphicBase.GetDirectX12();

        float clearColor[4] =
        {
            0.85f,
            0.85f,
            0.85f,
            1.0f
        };

        dx12.BeginFrame(clearColor);

        /*
            Ç‹Ç∏CameraêÍópRenderTextureÇ÷ï`âÊ
        */
        m_Camera.BeginRender(
            dx12,
            dx12.GetDSVHandle(),
            clearColor
        );

        m_Grid.Draw(dx12);

        m_OBJModel.Draw(
            dx12,
            m_Camera
        );

        m_Camera.EndRender(dx12);

        RenderTexture* texture =
            m_Camera.GetRenderTexture();

        dx12.EndFrame();
    }
}