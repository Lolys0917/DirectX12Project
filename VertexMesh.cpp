#include "VertexMesh.h"
#include "DirectX12.h"

namespace Engine
{
    VertexMesh::VertexMesh()
    {
        m_VertexBufferView = {};
        m_IndexBufferView = {};
    }

    VertexMesh::~VertexMesh()
    {
    }

    void VertexMesh::Clear()
    {
        m_Vertices.clear();
        m_Indices.clear();

        m_VertexBuffer.Reset();
        m_IndexBuffer.Reset();

        m_VertexBufferView = {};
        m_IndexBufferView = {};
    }

    void VertexMesh::SetVertices(const std::vector<Vertex>& vertices)
    {
        m_Vertices = vertices;
    }

    void VertexMesh::SetIndices(const std::vector<uint32_t>& indices)
    {
        m_Indices = indices;
    }

    void VertexMesh::AddVertex(const Vertex& vertex)
    {
        m_Vertices.push_back(vertex);
    }

    void VertexMesh::AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2)
    {
        m_Indices.push_back(i0);
        m_Indices.push_back(i1);
        m_Indices.push_back(i2);
    }

    void VertexMesh::AddPolygonFace(const std::vector<uint32_t>& faceIndices)
    {
        if (faceIndices.size() < 3)
            return;

        const uint32_t base = faceIndices[0];

        for (size_t i = 1; i + 1 < faceIndices.size(); ++i)
        {
            m_Indices.push_back(base);
            m_Indices.push_back(faceIndices[i]);
            m_Indices.push_back(faceIndices[i + 1]);
        }
    }

    void VertexMesh::CreateGPUResource(DirectX12& dx12)
    {
        // Œã‚ÅŽÀ‘•
        // m_Vertices -> VertexBuffer
        // m_Indices  -> IndexBuffer
    }

    void VertexMesh::Draw(DirectX12& dx12) const
    {
        // Œã‚ÅŽÀ‘•
        // IASetVertexBuffers
        // IASetIndexBuffer
        // DrawIndexedInstanced
    }

    const std::vector<Vertex>& VertexMesh::GetVertices() const
    {
        return m_Vertices;
    }

    const std::vector<uint32_t>& VertexMesh::GetIndices() const
    {
        return m_Indices;
    }

    uint32_t VertexMesh::GetVertexCount() const
    {
        return static_cast<uint32_t>(m_Vertices.size());
    }

    uint32_t VertexMesh::GetIndexCount() const
    {
        return static_cast<uint32_t>(m_Indices.size());
    }
}