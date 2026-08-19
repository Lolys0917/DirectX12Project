//|| Box.cpp ||:::::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  面ごとに独立した法線とUVを持つ直方体メッシュを生成する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Box.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Engine
{
    //一辺1の直方体を作成する
    Box::Box()
        : PrimitiveObject(ObjectType::Box)
        , Width(1.0f)
        , Height(1.0f)
        , Depth(1.0f)
    {
        BuildMesh();
    }

    //直方体を破棄する
    Box::~Box() = default;

    //概要：直方体寸法を安全な正値へ補正してMeshを再構築する
    //引数：width=X軸幅、height=Y軸高さ、depth=Z軸奥行き
    //戻り値：なし
    void Box::SetSize(float width, float height, float depth)
    {
        constexpr float MinimumSize = 0.0001f; //ゼロ面を防ぐ最小寸法
        Width = (std::max)(width, MinimumSize);
        Height = (std::max)(height, MinimumSize);
        Depth = (std::max)(depth, MinimumSize);
        BuildMesh();
    }

    //未登録状態の直方体定義を複製する
    //戻り値 : 同じ寸法と姿勢を持つ直方体
    std::unique_ptr<Object> Box::Clone() const
    {
        std::unique_ptr<Box> ClonedObject = std::make_unique<Box>(); //複製直方体
        ClonedObject->Width = Width;
        ClonedObject->Height = Height;
        ClonedObject->Depth = Depth;
        ClonedObject->Color = Color;
        ClonedObject->BuildMesh();
        CopyDefinitionTo(*ClonedObject);
        return ClonedObject;
    }

    //直方体の頂点と三角形を再構築する
    void Box::BuildMesh()
    {
        const float HalfWidth = Width * 0.5f; //幅の半分
        const float HalfHeight = Height * 0.5f; //高さの半分
        const float HalfDepth = Depth * 0.5f; //奥行きの半分
        std::vector<Vertex> Vertices; //面ごとに独立した頂点一覧
        std::vector<std::uint32_t> Indices; //三角形インデックス一覧

        Vertices.reserve(24);
        Indices.reserve(36);

        auto AddFace = [this, &Vertices, &Indices](
            const DirectX::XMFLOAT3& position0,
            const DirectX::XMFLOAT3& position1,
            const DirectX::XMFLOAT3& position2,
            const DirectX::XMFLOAT3& position3,
            const DirectX::XMFLOAT3& normal)
            {
                const std::uint32_t BaseIndex =
                    static_cast<std::uint32_t>(Vertices.size()); //追加面の先頭頂点

                Vertices.push_back({ position0, normal, { 0.0f, 1.0f }, Color });
                Vertices.push_back({ position1, normal, { 0.0f, 0.0f }, Color });
                Vertices.push_back({ position2, normal, { 1.0f, 0.0f }, Color });
                Vertices.push_back({ position3, normal, { 1.0f, 1.0f }, Color });

                Indices.insert(Indices.end(),
                    { BaseIndex, BaseIndex + 1, BaseIndex + 2,
                      BaseIndex, BaseIndex + 2, BaseIndex + 3 });
            }; //一面分の頂点と三角形を追加する処理

        AddFace(
            { -HalfWidth, -HalfHeight, -HalfDepth },
            { -HalfWidth, HalfHeight, -HalfDepth },
            { HalfWidth, HalfHeight, -HalfDepth },
            { HalfWidth, -HalfHeight, -HalfDepth },
            { 0.0f, 0.0f, -1.0f }
        );
        AddFace(
            { HalfWidth, -HalfHeight, HalfDepth },
            { HalfWidth, HalfHeight, HalfDepth },
            { -HalfWidth, HalfHeight, HalfDepth },
            { -HalfWidth, -HalfHeight, HalfDepth },
            { 0.0f, 0.0f, 1.0f }
        );
        AddFace(
            { -HalfWidth, -HalfHeight, HalfDepth },
            { -HalfWidth, HalfHeight, HalfDepth },
            { -HalfWidth, HalfHeight, -HalfDepth },
            { -HalfWidth, -HalfHeight, -HalfDepth },
            { -1.0f, 0.0f, 0.0f }
        );
        AddFace(
            { HalfWidth, -HalfHeight, -HalfDepth },
            { HalfWidth, HalfHeight, -HalfDepth },
            { HalfWidth, HalfHeight, HalfDepth },
            { HalfWidth, -HalfHeight, HalfDepth },
            { 1.0f, 0.0f, 0.0f }
        );
        AddFace(
            { -HalfWidth, HalfHeight, -HalfDepth },
            { -HalfWidth, HalfHeight, HalfDepth },
            { HalfWidth, HalfHeight, HalfDepth },
            { HalfWidth, HalfHeight, -HalfDepth },
            { 0.0f, 1.0f, 0.0f }
        );
        AddFace(
            { -HalfWidth, -HalfHeight, HalfDepth },
            { -HalfWidth, -HalfHeight, -HalfDepth },
            { HalfWidth, -HalfHeight, -HalfDepth },
            { HalfWidth, -HalfHeight, HalfDepth },
            { 0.0f, -1.0f, 0.0f }
        );

        SetMeshData(Vertices, Indices);
    }
}
