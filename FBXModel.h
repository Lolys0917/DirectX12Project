#pragma once
#include "Model.h"

namespace Engine
{
    class FBXModel : public Model
    {
    public:
        FBXModel();
        virtual ~FBXModel();

        virtual bool Load(const std::wstring& filePath) override;

    private:
        bool LoadMeshes(const std::wstring& filePath);
        bool LoadMaterials(const std::wstring& filePath);
        bool LoadTextures(const std::wstring& filePath);
        bool LoadSkeleton(const std::wstring& filePath);
        bool LoadAnimation(const std::wstring& filePath);
    };
}