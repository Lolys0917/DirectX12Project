#pragma once
#include <string>
#include <DirectXMath.h>

#include "IRenderable.h"
#include "VertexMesh.h"

namespace Engine
{
    class Polygon : public IRenderable
    {
    public:
        Polygon();
        virtual ~Polygon();

        void SetPosition(float x, float y, float z = 0.0f);
        void SetSize(float width, float height);
        void SetColor(const DirectX::XMFLOAT4& color);
        void SetUV(float u0, float v0, float u1, float v1);

        void SetTexturePath(const std::wstring& path);

        virtual void CreateGPUResource(DirectX12& dx12) override;
        virtual void Update(float deltaTime) override;
        virtual void Draw(DirectX12& dx12) override;

    protected:
        void BuildMesh();

    protected:
        DirectX::XMFLOAT3 m_Position;
        DirectX::XMFLOAT2 m_Size;
        DirectX::XMFLOAT4 m_Color;

        float m_U0;
        float m_V0;
        float m_U1;
        float m_V1;

        std::wstring m_TexturePath;

        VertexMesh m_Mesh;
    };
}