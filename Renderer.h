#pragma once

#define NOMINMAX
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <windows.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <DirectXMath.h>

#include "Graphics.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")

using namespace Microsoft::WRL;

//ポリゴン用________
struct Vertex
{
    float x, y, z;
    float u, v;
};
struct PolygonState
{
    float x, y, z;
    float r, g, b, a;
};
//テクスチャデータ______________
struct TextureData
{
    ComPtr<ID3D12Resource> resource;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};

    UINT width = 0;
    UINT height = 0;

    bool isRendering;
};
//テクスチャを名前で設定
struct TextureSettingName
{
    const char* textureName;
    TextureData textureData;
};
//モデル用頂点情報___________
struct ModelVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
};
//メッシュ___________________
class Mesh
{
public:
    std::vector<ModelVertex> Vertex;
    std::vector<unsigned int> Index;
};
//モデルメッシュ管理_________
class Model
{
public:
    std::vector<Mesh> ModelMesh;
};
//モデルロード_______________
class ModelLoader
{
public:
    static bool LoadFBX(
        const std::string& filename,
        Model& model);
};

class TextureManager
{
public:

    bool Load(
        std::string name,
        std::wstring file
    );

    TextureData* Get(
        std::string name
    );

private:

    std::unordered_map<
        std::string,
        TextureData
    > textures;
};

class ModelLoader
{
public:

    static bool LoadFBX(
        std::string file,
        Model& model
    );

private:

    static void ProcessNode(
        aiNode* node,
        const aiScene* scene,
        Model& model
    );

    static Mesh ProcessMesh(
        aiMesh* mesh,
        const aiScene* scene
    );
};

class PolygonGenerator
{
public:

    static std::vector<PolygonState>
        Create(
            int sides,
            float radius
        );
};

class Renderer
{
public:

    bool Initialize(
        ClassDirectXManager* dx
    );

    void Render();

private:

    void CreatePipeline();

    void CreateVertexBuffer();

private:

    ClassDirectXManager* dx;

    ComPtr<ID3D12PipelineState>
        pipelineState;

    ComPtr<ID3D12RootSignature>
        rootSignature;

    ComPtr<ID3D12Resource>
        vertexBuffer;

    D3D12_VERTEX_BUFFER_VIEW
        vbView;
};