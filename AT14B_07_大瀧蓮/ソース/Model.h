#pragma once

#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

class Core;

enum class ModelFileType
{
    VertexList,
    Obj,
    Fbx,
};

struct ModelLoadDesc
{
    ModelFileType type = ModelFileType::Obj;
    std::wstring modelPath;
    std::wstring texturePath;
    float scale = 1.0f;
    bool normalize = true;
    bool flipTextureV = true;
};

struct ModelVertex
{
    float x, y, z;
    float u, v;
};

class Model
{
public:
    bool Initialize(Core& core, const std::vector<ModelVertex>& vertices, const std::wstring& texturePath);

    bool Initialize(
        Core& core,
        const std::wstring& modelPath,
        const std::wstring& texturePath,
        float scale = 1.0f,
        ModelFileType type = ModelFileType::Obj
    );

    bool Initialize(Core& core, const ModelLoadDesc& desc);

    void Draw(Core& core) const;

private:
    bool LoadVertices(const ModelLoadDesc& desc, std::vector<ModelVertex>& vertices);
    bool LoadObj(const ModelLoadDesc& desc, std::vector<ModelVertex>& vertices);
    bool LoadFbx(const ModelLoadDesc& desc, std::vector<ModelVertex>& vertices);

    bool LoadTexture(Core& core, const std::wstring& filePath);
    bool CreateVertexBuffer(Core& core, const std::vector<ModelVertex>& vertices);

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle{};

    UINT textureWidth = 0;
    UINT textureHeight = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    UINT vertexCount = 0;
};