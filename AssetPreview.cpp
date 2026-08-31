#include "AssetPreview.h"
#include "RenderContext.h"
#include <filesystem>
#include <cwctype>
#include <algorithm>
#include <cmath>

namespace Engine
{
    AssetPreview::~AssetPreview()
    {
        if (Ready) Graphics.WaitGPU();
        Model.reset(); Image.reset();
        Graphics.Finalize();
    }
    bool AssetPreview::EnsureGraphics()
    {
        if (Ready) return true;
        RECT rect{};
        if (!Window || !GetClientRect(Window, &rect)) return false;
        Ready = Graphics.Initialize(Window, std::max(1L, rect.right), std::max(1L, rect.bottom));
        return Ready;
    }
    bool AssetPreview::Handle(const EditorCommand& command)
    {
        if (command.Type == EditorCommandType::PreviewView)
        {
            if (!std::isfinite(command.Transform.Rotation.X) || !std::isfinite(command.Transform.Rotation.Y) ||
                !std::isfinite(command.Transform.Scale.X)) return false;
            Pitch = std::remainder(command.Transform.Rotation.X, DirectX::XM_2PI);
            Yaw = std::remainder(command.Transform.Rotation.Y, DirectX::XM_2PI);
            Zoom = std::clamp(command.Transform.Scale.X, 0.25f, 4.0f);
            Dirty = true; return true;
        }
        if (!EnsureGraphics()) { Status = L"プレビューの描画初期化に失敗しました。"; return false; }
        if (command.Type == EditorCommandType::PreviewTexture)
        {
            const bool ok = Model && Model->SetTexture(Graphics, command.Path);
            Status = ok ? L"モデルプレビューにテクスチャを適用しました。" : L"先にOBJを表示してください。画像の形式も確認してください。";
            Dirty = true; return ok;
        }
        std::wstring extension = std::filesystem::path(command.Path).extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        if (extension == L".obj")
        {
            auto candidate = std::make_unique<OBJModel>();
            if (!candidate->Load(Graphics, command.Path, DirectX::XMFLOAT4{0.85f,0.88f,0.94f,1}))
            { Status = L"OBJを読み込めません。直前のプレビューを保持しました。"; return false; }
            Graphics.WaitGPU();
            Model = std::move(candidate); Image.reset();
            Model->GetBounds(Center, Radius);
            Status = L"OBJ / " + std::to_wstring(Model->GetTriangleCount()) + L" 三角形 / 矢印で回転・＋−でズーム";
        }
        else
        {
            auto candidate = std::make_unique<TextureDisplay>();
            if (!candidate->Load(Graphics, command.Path))
            { Status = L"画像を読み込めません。DDSは2DのBC1/2/3・RGBA8に対応。ログも確認してください。"; return false; }
            Graphics.WaitGPU();
            Image = std::move(candidate); Model.reset();
            Status = L"画像 / " + std::to_wstring(Image->GetWidth()) + L" × " + std::to_wstring(Image->GetHeight()) + L" px / 縦横比を保持";
        }
        Yaw = Pitch = 0; Zoom = 1; Dirty = true;
        return true;
    }
    void AssetPreview::DrawIfNeeded()
    {
        if (!Window || !IsWindowVisible(Window)) { WasVisible = false; return; }
        if (!EnsureGraphics()) return;
        if (!WasVisible) Dirty = true;
        WasVisible = true;
        RECT rect{};
        GetClientRect(Window, &rect);
        const unsigned width = std::max(1L, rect.right), height = std::max(1L, rect.bottom);
        if (width != Graphics.GetWidth() || height != Graphics.GetHeight())
        {
            if (!Graphics.Resize(width, height)) return;
            Dirty = true;
        }
        // Tabの再表示やウィンドウ復帰でも画像を再提示する。
        if (!Dirty) return;
        const float clear[] = {0.09f,0.10f,0.12f,1};
        Graphics.BeginFrame(clear);
        if (!Graphics.IsFrameOpen()) return;
        if (Image) Image->Draw(Graphics);
        if (Model)
        {
            const float aspect = float(width) / height;
            const float distance = 3.4f * Zoom / std::min(1.0f, aspect);
            ViewCamera.SetPreviewView({0, 0, -distance}, aspect);
            auto world = DirectX::XMMatrixTranslation(-Center.x, -Center.y, -Center.z) *
                DirectX::XMMatrixScaling(1 / Radius, 1 / Radius, 1 / Radius) *
                DirectX::XMMatrixRotationRollPitchYaw(Pitch, Yaw, 0);
            Model->DrawWithTransform({Graphics, ViewCamera}, world);
        }
        Graphics.EndFrame(); Dirty = false;
    }
}
