#pragma once
#include <DirectXMath.h>

namespace Engine
{
    struct Transform
    {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

        DirectX::XMMATRIX GetWorldMatrix() const
        {
            using namespace DirectX;

            XMMATRIX s = XMMatrixScaling(scale.x, scale.y, scale.z);

            XMMATRIX r =
                XMMatrixRotationRollPitchYaw(
                    rotation.x,
                    rotation.y,
                    rotation.z
                );

            XMMATRIX t =
                XMMatrixTranslation(
                    position.x,
                    position.y,
                    position.z
                );

            return s * r * t;
        }
    };
}