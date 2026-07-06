#pragma once

#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

namespace Engine
{
    class DirectX12;
    class Camera;

    struct GridVertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    struct GridConstantBuffer
    {
        DirectX::XMFLOAT4X4 worldViewProjection;
    };

    class Grid
    {
    public:
        Grid();
        ~Grid();

        Grid(const Grid&) = delete;
        Grid& operator=(const Grid&) = delete;

        void SetCamera(Camera* camera);

        bool Initialize(DirectX12& dx12);
        void Update(float deltaTime);
        void Draw(DirectX12& dx12);

    private:
        void BuildGrid();

        bool CreateRootSignature(DirectX12& dx12);
        bool CreatePipelineState(DirectX12& dx12);
        bool CreateVertexBuffer(DirectX12& dx12);
        bool CreateConstantBuffer(DirectX12& dx12);

    private:
        Camera* m_Camera;

        std::vector<GridVertex> m_Vertices;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

        Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;

        Microsoft::WRL::ComPtr<ID3D12Resource> m_ConstantBuffer;
        GridConstantBuffer* m_MappedConstantBuffer;
    };
}