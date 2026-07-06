#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>

class Core;

struct GridVertex
{
    float x, y, z;
    float r, g, b, a;
};

class Grid
{
public:
    bool Initialize(Core& core, int halfCount, float interval);
    void Draw(Core& core) const;

private:
    bool CreatePipeline(Core& core);
    bool CreateVertexBuffer(Core& core, const std::vector<GridVertex>& vertices);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    UINT vertexCount_ = 0;
};