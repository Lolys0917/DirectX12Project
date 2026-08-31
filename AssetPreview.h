#pragma once
#include "DirectX12.h"
#include "OBJModel.h"
#include "TextureDisplay.h"
#include "Camera.h"
#include "EditorTypes.h"
#include <memory>

namespace Engine
{
    // GameRuntimeと同じスレッドで動かす、ゲームSceneから独立した描画先。
    class AssetPreview final
    {
    public:
        explicit AssetPreview(HWND window) : Window(window) {}
        ~AssetPreview();
        bool Handle(const EditorCommand& command);
        void DrawIfNeeded();
        const std::wstring& GetStatus() const { return Status; }
        void SetStatus(const std::wstring& status) { Status = status; }
    private:
        bool EnsureGraphics();
        HWND Window = nullptr;
        DirectX12 Graphics;
        Camera ViewCamera;
        std::unique_ptr<OBJModel> Model;
        std::unique_ptr<TextureDisplay> Image;
        DirectX::XMFLOAT3 Center{};
        float Radius = 1, Yaw = 0, Pitch = 0, Zoom = 1;
        bool Ready = false, Dirty = true, WasVisible = false;
        std::wstring Status = L"画像またはOBJモデルを選択してください。";
    };
}
