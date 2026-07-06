#pragma once
#include <DirectXMath.h>

#include "IRenderable.h"
#include "VertexMesh.h"
#include "Transform.h"

namespace Engine
{
    class Object : public IRenderable
    {
    public:
        Object();
        virtual ~Object();

        void SetPosition(const DirectX::XMFLOAT3& position);
        void SetRotation(const DirectX::XMFLOAT3& rotation);
        void SetScale(const DirectX::XMFLOAT3& scale);

        const Transform& GetTransform() const;
        Transform& GetTransform();

        void SetColor(const DirectX::XMFLOAT4& color);

        virtual void CreateGPUResource(DirectX12& dx12) override;
        virtual void Update(float deltaTime) override;
        virtual void Draw(DirectX12& dx12) override;

    protected:
        virtual void BuildMesh() = 0;

    protected:
        Transform m_Transform;
        DirectX::XMFLOAT4 m_Color;

        VertexMesh m_Mesh;
    };
}