#define NOMINMAX

#include "Model.h"

#include <cstring>
#include <vector>
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "Core.h"
#include "d3dx12.h"

#pragma comment(lib, "windowscodecs.lib")

namespace
{
    struct ObjVec2 { float u, v; };
    struct ObjVec3 { float x, y, z; };

    struct ObjIndex
    {
        int position = 0;
        int texcoord = 0;
    };

    int ResolveObjIndex(int index, int count)
    {
        if (index > 0) return index - 1;
        if (index < 0) return count + index;
        return -1;
    }

    ObjIndex ParseObjFaceToken(const std::string& token)
    {
        ObjIndex result;

        size_t slash1 = token.find('/');
        size_t slash2 = token.find('/', slash1 == std::string::npos ? slash1 : slash1 + 1);

        if (slash1 == std::string::npos)
        {
            result.position = std::atoi(token.c_str());
            return result;
        }

        result.position = std::atoi(token.substr(0, slash1).c_str());

        if (slash2 == std::string::npos)
        {
            result.texcoord = std::atoi(token.substr(slash1 + 1).c_str());
        }
        else if (slash2 > slash1 + 1)
        {
            result.texcoord = std::atoi(token.substr(slash1 + 1, slash2 - slash1 - 1).c_str());
        }

        return result;
    }
}

bool Model::LoadObj(const ModelLoadDesc& desc, std::vector<ModelVertex>& vertices)
{
    FILE* file = nullptr;

    if (_wfopen_s(&file, desc.modelPath.c_str(), L"rb") != 0 || file == nullptr)
    {
        MessageBoxA(nullptr, "OBJ file open failed.", "Error", MB_OK);
        return false;
    }

    std::vector<ObjVec3> positions;
    std::vector<ObjVec2> texcoords;
    std::vector<std::vector<ObjIndex>> faces;

    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), file))
    {
        std::string line(buffer);
        std::istringstream iss(line);

        std::string tag;
        iss >> tag;

        if (tag == "v")
        {
            ObjVec3 p{};
            iss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (tag == "vt")
        {
            ObjVec2 uv{};
            iss >> uv.u >> uv.v;
            texcoords.push_back(uv);
        }
        else if (tag == "f")
        {
            std::vector<ObjIndex> face;
            std::string token;

            while (iss >> token)
            {
                face.push_back(ParseObjFaceToken(token));
            }

            if (face.size() >= 3)
            {
                faces.push_back(face);
            }
        }
    }

    fclose(file);

    if (positions.empty() || faces.empty())
    {
        return false;
    }

    ObjVec3 minPos = positions[0];
    ObjVec3 maxPos = positions[0];

    for (const auto& p : positions)
    {
        minPos.x = std::min(minPos.x, p.x);
        minPos.y = std::min(minPos.y, p.y);
        minPos.z = std::min(minPos.z, p.z);

        maxPos.x = std::max(maxPos.x, p.x);
        maxPos.y = std::max(maxPos.y, p.y);
        maxPos.z = std::max(maxPos.z, p.z);
    }

    ObjVec3 center =
    {
        (minPos.x + maxPos.x) * 0.5f,
        (minPos.y + maxPos.y) * 0.5f,
        (minPos.z + maxPos.z) * 0.5f
    };

    float width = maxPos.x - minPos.x;
    float height = maxPos.y - minPos.y;
    float depth = maxPos.z - minPos.z;
    float maxSize = std::max(width, std::max(height, depth));

    float scale = desc.scale;

    if (desc.normalize && maxSize > 0.0001f)
    {
        scale = desc.scale * 2.0f / maxSize;
    }

    auto addVertex = [&](const ObjIndex& objIndex)
        {
            int posIndex = ResolveObjIndex(objIndex.position, static_cast<int>(positions.size()));
            if (posIndex < 0 || posIndex >= static_cast<int>(positions.size()))
            {
                return;
            }

            const ObjVec3& p = positions[posIndex];

            float u = 0.0f;
            float v = 0.0f;

            int uvIndex = ResolveObjIndex(objIndex.texcoord, static_cast<int>(texcoords.size()));
            if (uvIndex >= 0 && uvIndex < static_cast<int>(texcoords.size()))
            {
                u = texcoords[uvIndex].u;
                v = desc.flipTextureV ? 1.0f - texcoords[uvIndex].v : texcoords[uvIndex].v;
            }

            vertices.push_back(
                {
                    (p.x - center.x) * scale,
                    (p.y - center.y) * scale,
                    (p.z - center.z) * scale,
                    u,
                    v
                }
            );
        };

    for (const auto& face : faces)
    {
        for (size_t i = 1; i + 1 < face.size(); i++)
        {
            addVertex(face[0]);
            addVertex(face[i]);
            addVertex(face[i + 1]);
        }
    }

    return !vertices.empty();
}

bool Model::LoadFbx(const ModelLoadDesc& desc, std::vector<ModelVertex>& vertices)
{
    MessageBoxA(
        nullptr,
        "FBX loading is not implemented yet. Add FBX SDK loading here.",
        "Model",
        MB_OK
    );

    return false;
}

bool Model::Initialize(
    Core& core,
    const std::wstring& modelPath,
    const std::wstring& texturePath,
    float scale,
    ModelFileType type
)
{
    ModelLoadDesc desc;
    desc.type = type;
    desc.modelPath = modelPath;
    desc.texturePath = texturePath;
    desc.scale = scale;

    return Initialize(core, desc);
}

bool Model::Initialize(Core& core, const ModelLoadDesc& desc)
{
    std::vector<ModelVertex> vertices;

    if (!LoadVertices(desc, vertices))
    {
        return false;
    }

    if (!LoadTexture(core, desc.texturePath))
    {
        return false;
    }

    return CreateVertexBuffer(core, vertices);
}

bool Model::LoadVertices(const ModelLoadDesc& desc, std::vector<ModelVertex>& vertices)
{
    switch (desc.type)
    {
    case ModelFileType::Obj:
        return LoadObj(desc, vertices);

    case ModelFileType::Fbx:
        return LoadFbx(desc, vertices);

    case ModelFileType::VertexList:
    default:
        return false;
    }
}

void Model::Draw(Core& core) const
{
    auto* commandList = core.GetCommandList();

    commandList->SetGraphicsRootDescriptorTable(0, textureHandle);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

bool Model::LoadTexture(Core& core, const std::wstring& filePath)
{
    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wicFactory)
    );
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;

    hr = wicFactory->CreateDecoderFromFilename(
        filePath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );
    if (FAILED(hr))
    {
        MessageBoxA(nullptr, "Texture load failed.", "Error", MB_OK);
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return false;

    frame->GetSize(&textureWidth, &textureHeight);

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return false;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return false;

    std::vector<BYTE> pixels(textureWidth * textureHeight * 4);

    hr = converter->CopyPixels(
        nullptr,
        textureWidth * 4,
        static_cast<UINT>(pixels.size()),
        pixels.data()
    );
    if (FAILED(hr)) return false;

    auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        textureWidth,
        textureHeight
    );

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

    hr = core.GetDevice()->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture)
    );
    if (FAILED(hr)) return false;

    UINT64 uploadSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    hr = core.GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );
    if (FAILED(hr)) return false;

    D3D12_SUBRESOURCE_DATA subresource = {};
    subresource.pData = pixels.data();
    subresource.RowPitch = textureWidth * 4;
    subresource.SlicePitch = subresource.RowPitch * textureHeight;

    core.GetCommandAllocator()->Reset();
    core.GetCommandList()->Reset(core.GetCommandAllocator(), nullptr);

    UpdateSubresources(
        core.GetCommandList(),
        texture.Get(),
        uploadBuffer.Get(),
        0,
        0,
        1,
        &subresource
    );

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    core.GetCommandList()->ResourceBarrier(1, &barrier);
    core.GetCommandList()->Close();

    core.ExecuteCommandListAndWait();

    textureHandle = core.CreateTextureSrv(texture.Get());

    return true;
}

bool Model::CreateVertexBuffer(Core& core, const std::vector<ModelVertex>& vertices)
{
    if (vertices.empty()) return false;

    vertexCount = static_cast<UINT>(vertices.size());
    UINT bufferSize = sizeof(ModelVertex) * vertexCount;

    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    HRESULT hr = core.GetDevice()->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );
    if (FAILED(hr)) return false;

    void* ptr = nullptr;
    vertexBuffer->Map(0, nullptr, &ptr);
    std::memcpy(ptr, vertices.data(), bufferSize);
    vertexBuffer->Unmap(0, nullptr);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = bufferSize;
    vertexBufferView.StrideInBytes = sizeof(ModelVertex);

    return true;
}