#pragma once
#include <string>
#include <vector>

#include "Object.h"

namespace Engine
{
    struct ModelMesh
    {
        VertexMesh mesh;
        Material material;

        std::wstring diffuseTexturePath;
        std::wstring normalTexturePath;
        std::wstring specularTexturePath;
    };

    class Model : public Object
    {
    public:
        Model();
        virtual ~Model();

        virtual bool Load(const std::wstring& filePath) = 0;

        virtual void CreateGPUResource(DirectX12& dx12) override;
        virtual void Update(float deltaTime) override;
        virtual void Draw(DirectX12& dx12) override;

        const std::vector<ModelMesh>& GetMeshes() const;

    protected:
        virtual void BuildMesh() override;

    protected:
        std::wstring m_FilePath;
        std::vector<ModelMesh> m_Meshes;
    };
}