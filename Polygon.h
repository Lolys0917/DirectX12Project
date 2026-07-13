//|| Polygon.h ||:::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Objectへ付加する矩形Polygon Componentを定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  IRenderable単体からComponentへ変更
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include <DirectXMath.h>

#include <memory>
#include <string>

#include "Component.h"
#include "VertexMesh.h"

namespace Engine
{
    class Polygon final : public Component
    {
    public:
        static constexpr ComponentType StaticType = ComponentType::Polygon; //Manager登録で使用するComponent型

        //既定サイズと白色を持つPolygon Componentを作成する
        Polygon();

        //Polygonが所有するMesh Resourceを解放する
        ~Polygon() override;

        void SetSize(float width, float height);
        void SetColor(const DirectX::XMFLOAT4& color);
        void SetUV(float u0, float v0, float u1, float v1);
        void SetTexturePath(const std::wstring& path);

        //矩形頂点のGPU Resourceを作成する
        //引数: dx12 描画基盤
        //戻り値: CPU形状を構築してResource作成を要求できた場合はtrue
        bool Initialize(DirectX12& dx12) override;

        //Polygonの時間依存情報を更新する
        //引数: deltaTime 前回更新からの秒数
        void Update(float deltaTime) override;

        //現在の描画Contextへ矩形Meshを描画する
        //引数: renderContext 描画基盤とCameraを持つContext
        void Draw(const RenderContext& renderContext) override;

        //Polygonが所有するMesh Resourceを解放する
        void Finalize() override;

        //未登録状態のPolygon定義を複製する
        //戻り値: GPU Resourceを持たない複製Component
        std::unique_ptr<Component> Clone() const override;

    private:
        //現在のサイズ、UV、色から矩形CPU Meshを再構築する
        void BuildMesh();

        DirectX::XMFLOAT2 Size; //Object原点を中心とした矩形サイズ
        DirectX::XMFLOAT4 Color; //全頂点へ設定する色
        DirectX::XMFLOAT4 UVRectangle; //左上U/Vと右下U/V
        std::wstring TexturePath; //将来Texture Componentが読み込む画像パス
        VertexMesh Mesh; //矩形の頂点とIndex
    };
}
