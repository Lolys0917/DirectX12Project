//|| Camera.h ||::::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Objectへ付加するCamera Componentと専用RenderTextureを管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Component化、専用Depth、複数Camera描画へ対応
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <memory>

#include "Component.h"
#include "RenderTexture.h"

namespace Engine
{
    class Camera final : public Component
    {
    public:
        static constexpr ComponentType StaticType = ComponentType::Camera; //Manager登録で使用するComponent型

        //指定した初期描画解像度でCamera Componentを作成する
        //引数: width 初期描画幅、height 初期描画高さ
        explicit Camera(uint32_t width = 1, uint32_t height = 1);

        //Camera専用RenderTextureを解放してComponentを破棄する
        ~Camera() override;

        //RenderTextureの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元Camera
        Camera(const Camera&) = delete;

        //RenderTextureの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元Camera
        //戻り値: 代入先Cameraへの参照
        Camera& operator=(const Camera&) = delete;

        //専用Color/Depth RenderTextureを作成する
        //引数: dx12 GPU Resource作成に使用する描画基盤
        //戻り値: Resource作成に成功した場合はtrue
        bool Initialize(DirectX12& dx12) override;

        //入力に応じて所有ObjectのCamera位置を更新する
        //引数: deltaTime 前回更新からの秒数
        void Update(float deltaTime) override;

        //Camera自体は描画対象ではないため何も描画しない
        //引数: renderContext 現在の描画Context
        void Draw(const RenderContext& renderContext) override;

        //Camera専用RenderTextureを解放する
        void Finalize() override;

        //未登録状態のCamera定義を複製する
        //戻り値: GPU Resourceを持たない複製Component
        std::unique_ptr<Component> Clone() const override;

        //専用RenderTextureを安全に再作成する
        //引数: dx12 描画基盤、width 新しい幅、height 新しい高さ
        //戻り値: 再作成に成功した場合はtrue
        bool Resize(DirectX12& dx12, uint32_t width, uint32_t height);

        void SetTarget(const DirectX::XMFLOAT3& target);
        void SetUp(const DirectX::XMFLOAT3& up);
        void SetMoveSpeed(float moveSpeed);
        void SetPreviewView(const DirectX::XMFLOAT3& position, float aspect)
        { DetachedPosition = position; Aspect = aspect; }

        DirectX::XMMATRIX GetViewMatrix() const;
        DirectX::XMMATRIX GetProjectionMatrix() const;
        DirectX::XMMATRIX GetViewProjectionMatrix() const;
        DirectX::XMFLOAT3 GetPosition() const;

        //このCamera専用RenderTextureへの描画を開始する
        //引数: dx12 描画基盤、clearColor RGBA消去色
        void BeginRender(DirectX12& dx12, const float clearColor[4]);

        //このCamera専用RenderTextureへの描画を終了する
        //引数: dx12 描画基盤
        void EndRender(DirectX12& dx12);

        RenderTexture* GetRenderTexture();
        const RenderTexture* GetRenderTexture() const;

    private:
        std::unique_ptr<RenderTexture> OutputTexture; //このCameraだけが所有する描画出力
        DirectX::XMFLOAT3 Target; //注視点
        DirectX::XMFLOAT3 Up; //上方向
        DirectX::XMFLOAT3 DetachedPosition; //Owner登録前に使用する代替位置
        uint32_t Width; //RenderTexture幅
        uint32_t Height; //RenderTexture高さ
        float Aspect; //投影行列の縦横比
        float MoveSpeed; //キーボード移動速度
    };
}
