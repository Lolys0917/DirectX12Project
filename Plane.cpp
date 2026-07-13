//|| Plane.cpp ||:::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  XZ平面上の矩形メッシュを生成する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Plane.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Engine
{
    //幅と奥行きが1の平面を作成する
    Plane::Plane()
        : PrimitiveObject(ObjectType::Plane)
        , Width(1.0f)
        , Depth(1.0f)
    {
        BuildMesh();
    }

    //平面を破棄する
    Plane::~Plane() = default;

    void Plane::SetSize(float width, float depth)
    {
        constexpr float MinimumSize = 0.0001f; //ゼロ面を防ぐ最小寸法
        Width = (std::max)(width, MinimumSize);
        Depth = (std::max)(depth, MinimumSize);
        BuildMesh();
    }

    //未登録状態の平面定義を複製する
    //戻り値 : 同じ寸法と姿勢を持つ平面
    std::unique_ptr<Object> Plane::Clone() const
    {
        std::unique_ptr<Plane> ClonedObject = std::make_unique<Plane>(); //複製平面
        ClonedObject->Width = Width;
        ClonedObject->Depth = Depth;
        ClonedObject->Color = Color;
        ClonedObject->BuildMesh();
        CopyDefinitionTo(*ClonedObject);
        return ClonedObject;
    }

    //上向き法線を持つ矩形メッシュを再構築する
    void Plane::BuildMesh()
    {
        const float HalfWidth = Width * 0.5f; //幅の半分
        const float HalfDepth = Depth * 0.5f; //奥行きの半分
        const DirectX::XMFLOAT3 Up = { 0.0f, 1.0f, 0.0f }; //上向き法線
        const std::vector<Vertex> Vertices = //矩形頂点一覧
        {
            { { -HalfWidth, 0.0f, HalfDepth }, Up, { 0.0f, 1.0f }, Color },
            { { -HalfWidth, 0.0f, -HalfDepth }, Up, { 0.0f, 0.0f }, Color },
            { { HalfWidth, 0.0f, -HalfDepth }, Up, { 1.0f, 0.0f }, Color },
            { { HalfWidth, 0.0f, HalfDepth }, Up, { 1.0f, 1.0f }, Color }
        };
        const std::vector<std::uint32_t> Indices = //表面三角形インデックス
        {
            0, 1, 2,
            0, 2, 3
        };

        SetMeshData(Vertices, Indices);
    }
}
