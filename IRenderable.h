#pragma once

namespace Engine
{
    class DirectX12;

    class IRenderable
    {
    public:
        virtual ~IRenderable() = default;

        virtual void CreateGPUResource(DirectX12& dx12) = 0;
        virtual void Update(float deltaTime) = 0;
        virtual void Draw(DirectX12& dx12) = 0;
    };
}