#pragma once

#include <DirectXMath.h>
#include <memory>

#include "RenderTexture.h"

namespace Engine
{
    class Camera
    {
    public:
        Camera();

        void Initialize(float width, float height);
        void Update(float deltaTime);

        DirectX::XMMATRIX GetViewMatrix() const;
        DirectX::XMMATRIX GetProjectionMatrix() const;
        DirectX::XMMATRIX GetViewProjectionMatrix() const;

        const DirectX::XMFLOAT3& GetPosition() const;

        //RenderTexture
        bool CreateRenderTexture(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height
        );

        void BeginRender(
            DirectX12& dx12,
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
            const float clearColor[4]
        );

        void EndRender(
            DirectX12& dx12
        );

        RenderTexture* GetRenderTexture();
        const RenderTexture* GetRenderTexture() const;

    private:
        std::unique_ptr<RenderTexture> m_RenderTexture;

        DirectX::XMFLOAT3 m_Position;
        DirectX::XMFLOAT3 m_Target;
        DirectX::XMFLOAT3 m_Up;

        float m_Aspect;
        float m_MoveSpeed;
    };
}