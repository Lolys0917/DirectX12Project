#pragma once
#include "Texture2D.h"
#include <DirectXMath.h>
#include <memory>

namespace Engine
{
    class Camera;
    // 同じ画像を2Dプレビュー又はカメラに追従する全天背景として描画する。
    class TextureDisplay final
    {
    public:
        bool Load(DirectX12& graphics, const std::wstring& path);
        void Draw(DirectX12& graphics, const Camera* camera = nullptr);
        uint32_t GetWidth() const { return Texture.GetWidth(); }
        uint32_t GetHeight() const { return Texture.GetHeight(); }
    private:
        Texture2D Texture;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> Root;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> Pipeline;
    };
}
