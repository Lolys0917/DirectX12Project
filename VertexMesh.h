#pragma once
#include <vector>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

#include "Vertex.h"

namespace Engine
{
    class DirectX12;

    class VertexMesh
    {
    public:
        VertexMesh();
        ~VertexMesh();

        void Clear();

        void SetVertices(const std::vector<Vertex>& vertices);
        void SetIndices(const std::vector<uint32_t>& indices);

        void AddVertex(const Vertex& vertex);
        void AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2);

        // ‘½ŠpŒ`—p
        // —á: 5ŠpŒ`‚È‚ç {0,1,2,3,4} ‚ð“n‚·
        // “à•”‚Å‚Í triangle fan ‚Æ‚µ‚ÄŽOŠpŒ`‚É•ª‰ð‚·‚é
        void AddPolygonFace(const std::vector<uint32_t>& faceIndices);

        void CreateGPUResource(DirectX12& dx12);
        void Draw(DirectX12& dx12) const;

        const std::vector<Vertex>& GetVertices() const;
        const std::vector<uint32_t>& GetIndices() const;

        uint32_t GetVertexCount() const;
        uint32_t GetIndexCount() const;

    private:
        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;

        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
        D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    };
}