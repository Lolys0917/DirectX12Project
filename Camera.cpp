#include "Camera.h"
#include "DirectX12.h"

#include <Windows.h>

namespace Engine
{
    Camera::Camera()
        : m_Position(0.0f, 6.0f, -10.0f)
        , m_Target(0.0f, 0.0f, 0.0f)
        , m_Up(0.0f, 1.0f, 0.0f)
        , m_Aspect(16.0f / 9.0f)
        , m_MoveSpeed(5.0f)
    {
    }

    void Camera::Initialize(float width, float height)
    {
        if (height <= 0.0f)
        {
            height = 1.0f;
        }

        m_Aspect = width / height;
    }

    void Camera::Update(float deltaTime)
    {
        const float move = m_MoveSpeed * deltaTime;

        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        {
            m_Position.x -= move;
            //m_Target.x -= move;
        }

        if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        {
            m_Position.x += move;
            //m_Target.x += move;
        }

        if (GetAsyncKeyState(VK_UP) & 0x8000)
        {
            m_Position.z += move;
            //m_Target.z += move;
        }

        if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        {
            m_Position.z -= move;
            //m_Target.z -= move;
        }
    }

    bool Camera::CreateRenderTexture(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height
    )
    {
        m_RenderTexture =
            std::make_unique<RenderTexture>();

        return m_RenderTexture->Initialize(
            dx12,
            width,
            height,
            dx12.GetBackBufferFormat()
        );
    }

    void Camera::BeginRender(
        DirectX12& dx12,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
        const float clearColor[4]
    )
    {
        if (!m_RenderTexture)
            return;

        m_RenderTexture->Begin(
            dx12,
            dsvHandle,
            clearColor
        );
    }

    void Camera::EndRender(
        DirectX12& dx12
    )
    {
        if (!m_RenderTexture)
            return;

        m_RenderTexture->End(dx12);
    }

    RenderTexture* Camera::GetRenderTexture()
    {
        return m_RenderTexture.get();
    }

    const RenderTexture* Camera::GetRenderTexture() const
    {
        return m_RenderTexture.get();
    }

    DirectX::XMMATRIX Camera::GetViewMatrix() const
    {
        using namespace DirectX;

        return XMMatrixLookAtLH(
            XMLoadFloat3(&m_Position),
            XMLoadFloat3(&m_Target),
            XMLoadFloat3(&m_Up)
        );
    }

    DirectX::XMMATRIX Camera::GetProjectionMatrix() const
    {
        return DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(45.0f),
            m_Aspect,
            0.1f,
            1000.0f
        );
    }

    DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const
    {
        return GetViewMatrix() * GetProjectionMatrix();
    }

    const DirectX::XMFLOAT3& Camera::GetPosition() const
    {
        return m_Position;
    }
}