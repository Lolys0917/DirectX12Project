//|| Polygon.cpp ||:::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Objectへ付加する矩形Polygon Componentを実装する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.10  Owner姿勢を使用する共通Mesh描画へ変更
//||  2026_07_13  v2.00  Componentとして新規実装
//||

#include "Polygon.h"

#include <algorithm>
#include <vector>

#include "Object.h"
#include "RenderContext.h"

namespace Engine
{
    //既定サイズと白色を持つPolygon Componentを作成する
    Polygon::Polygon()
        : Component(StaticType)
        , Size(1.0f, 1.0f)
        , Color(1.0f, 1.0f, 1.0f, 1.0f)
        , UVRectangle(0.0f, 0.0f, 1.0f, 1.0f)
    {
    }

    //Polygonが所有するMesh Resourceを解放する
    Polygon::~Polygon()
    {
        Finalize();
    }

    //概要：Polygonの幅と高さを安全な正値へ補正してMeshを再構築する
    //引数：width=横幅、height=縦幅
    //戻り値：なし
    void Polygon::SetSize(float width, float height)
    {
        Size.x = (std::max)(0.0f, width);
        Size.y = (std::max)(0.0f, height);
        BuildMesh();
    }

    //概要：PolygonのRGBA頂点色を変更してMeshを再構築する
    //引数：color=設定するRGBA色
    //戻り値：なし
    void Polygon::SetColor(const DirectX::XMFLOAT4& color)
    {
        Color = color;
        BuildMesh();
    }

    //概要：Polygonが使用するUV矩形を変更してMeshを再構築する
    //引数：u0=左U、v0=上V、u1=右U、v1=下V
    //戻り値：なし
    void Polygon::SetUV(float u0, float v0, float u1, float v1)
    {
        UVRectangle = DirectX::XMFLOAT4(u0, v0, u1, v1);
        BuildMesh();
    }

    //概要：次回初期化で読み込むTexture Pathを変更する
    //引数：path=画像ファイルPath
    //戻り値：なし
    void Polygon::SetTexturePath(const std::wstring& path)
    {
        TexturePath = path;
    }

    //矩形頂点のGPU Resourceを作成する
    //引数: dx12 描画基盤
    //戻り値: CPU形状を構築してResource作成を要求できた場合はtrue
    bool Polygon::Initialize(DirectX12& dx12)
    {
        BuildMesh();
        return Mesh.CreateGPUResource(dx12);
    }

    //Polygonの時間依存情報を更新する
    //引数: deltaTime 前回更新からの秒数
    void Polygon::Update(float deltaTime)
    {
        (void)deltaTime;
    }

    //現在の描画Contextへ矩形Meshを描画する
    //引数: renderContext 描画基盤とCameraを持つContext
    void Polygon::Draw(const RenderContext& renderContext)
    {
        const Object* OwnerObject = GetOwner(); //Polygon姿勢を所有するObject
        const DirectX::XMMATRIX World = OwnerObject != nullptr
            ? OwnerObject->GetWorldMatrix()
            : DirectX::XMMatrixIdentity(); //Owner未登録時は単位World行列
        Mesh.Draw(renderContext, World);
    }

    //Polygonが所有するMesh Resourceを解放する
    void Polygon::Finalize()
    {
        Mesh.ReleaseGPUResource();
    }

    //未登録状態のPolygon定義を複製する
    //戻り値: GPU Resourceを持たない複製Component
    std::unique_ptr<Component> Polygon::Clone() const
    {
        auto Duplicate = std::make_unique<Polygon>(); //未登録の複製Component
        Duplicate->Size = Size;
        Duplicate->Color = Color;
        Duplicate->UVRectangle = UVRectangle;
        Duplicate->TexturePath = TexturePath;
        CopyDefinitionTo(*Duplicate);
        return Duplicate;
    }

    //現在のサイズ、UV、色から矩形CPU Meshを再構築する
    void Polygon::BuildMesh()
    {
        const float HalfWidth = Size.x * 0.5f; //矩形中心から左右端までの距離
        const float HalfHeight = Size.y * 0.5f; //矩形中心から上下端までの距離
        const DirectX::XMFLOAT3 Normal(0.0f, 0.0f, -1.0f); //矩形の表面法線

        std::vector<Vertex> Vertices =
        {
            { { -HalfWidth,  HalfHeight, 0.0f }, Normal, { UVRectangle.x, UVRectangle.y }, Color },
            { {  HalfWidth,  HalfHeight, 0.0f }, Normal, { UVRectangle.z, UVRectangle.y }, Color },
            { {  HalfWidth, -HalfHeight, 0.0f }, Normal, { UVRectangle.z, UVRectangle.w }, Color },
            { { -HalfWidth, -HalfHeight, 0.0f }, Normal, { UVRectangle.x, UVRectangle.w }, Color }
        }; //矩形の四頂点

        const std::vector<uint32_t> Indices =
        {
            0, 1, 2,
            0, 2, 3
        }; //表面を構成する二つの三角形

        Mesh.SetVertices(Vertices);
        Mesh.SetIndices(Indices);
    }
}
