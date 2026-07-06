#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "Texture2D.h"

/*
åƒÇ—èoÇµó·

if (!m_OBJModel.Load(
    m_GraphicBase.GetDirectX12(),
    L"Assets/Models/sample.obj",
    L"Assets/Textures/sample.png"))
{
    return false;
}

m_OBJModel.SetPosition(
    DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)
);

m_OBJModel.SetRotation(
    DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)
);

m_OBJModel.SetScale(
    DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)
);

*/

namespace Engine
{
    class DirectX12;
    class Camera;

    struct OBJVertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
    };

    struct OBJConstantBuffer
    {
        DirectX::XMFLOAT4X4 worldViewProjection;
        DirectX::XMFLOAT4 color;

        int useTexture;
        float padding[3];
    };

    class OBJModel
    {
    public:
        OBJModel();
        ~OBJModel();

        OBJModel(const OBJModel&) = delete;
        OBJModel& operator=(const OBJModel&) = delete;

        bool Load(
            DirectX12& dx12,
            const std::wstring& objPath,
            const std::wstring& texturePath
        );

        bool Load(
            DirectX12& dx12,
            const std::wstring& objPath,
            const DirectX::XMFLOAT4& color
        );

        void SetPosition(const DirectX::XMFLOAT3& position);
        void SetRotation(const DirectX::XMFLOAT3& rotation);
        void SetScale(const DirectX::XMFLOAT3& scale);
        void SetColor(const DirectX::XMFLOAT4& color);

        void Update(float deltaTime);
        void Draw(DirectX12& dx12, const Camera& camera);

    private:
        bool LoadOBJFile(const std::wstring& objPath);

        bool CreateRootSignature(DirectX12& dx12);
        bool CreatePipelineState(DirectX12& dx12);
        bool CreateVertexBuffer(DirectX12& dx12);
        bool CreateIndexBuffer(DirectX12& dx12);
        bool CreateConstantBuffer(DirectX12& dx12);

        DirectX::XMMATRIX GetWorldMatrix() const;

    private:
        std::vector<OBJVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

        Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_ConstantBuffer;

        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
        D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

        OBJConstantBuffer* m_MappedConstantBuffer;

        Texture2D m_Texture;

        DirectX::XMFLOAT3 m_Position;
        DirectX::XMFLOAT3 m_Rotation;
        DirectX::XMFLOAT3 m_Scale;
        DirectX::XMFLOAT4 m_Color;

        bool m_UseTexture;
    };
}