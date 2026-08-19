//|| Sphere.cpp ||::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  緯度経度分割によるUV球メッシュを生成する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Sphere.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Engine
{
    //半径0.5のUV球を作成する
    Sphere::Sphere()
        : PrimitiveObject(ObjectType::Sphere)
        , Radius(0.5f)
        , Slice(32)
        , Stack(16)
    {
        BuildMesh();
    }

    //球を破棄する
    Sphere::~Sphere() = default;

    //概要：球半径を安全な正値へ補正してMeshを再構築する
    //引数：radius=新しい球半径
    //戻り値：なし
    void Sphere::SetRadius(float radius)
    {
        constexpr float MinimumRadius = 0.0001f; //ゼロ球を防ぐ最小半径
        Radius = (std::max)(radius, MinimumRadius);
        BuildMesh();
    }

    //概要：球の経度と緯度分割数を変更してMeshを再構築する
    //引数：slice=経度分割数、stack=緯度分割数
    //戻り値：なし
    void Sphere::SetDivision(std::uint32_t slice, std::uint32_t stack)
    {
        Slice = (std::max<std::uint32_t>)(slice, 3);
        Stack = (std::max<std::uint32_t>)(stack, 2);
        BuildMesh();
    }

    //未登録状態の球定義を複製する
    //戻り値 : 同じ半径、分割数、姿勢を持つ球
    std::unique_ptr<Object> Sphere::Clone() const
    {
        std::unique_ptr<Sphere> ClonedObject = std::make_unique<Sphere>(); //複製球
        ClonedObject->Radius = Radius;
        ClonedObject->Slice = Slice;
        ClonedObject->Stack = Stack;
        ClonedObject->Color = Color;
        ClonedObject->BuildMesh();
        CopyDefinitionTo(*ClonedObject);
        return ClonedObject;
    }

    //UV球メッシュを再構築する
    void Sphere::BuildMesh()
    {
        constexpr float Pi = 3.14159265358979323846f; //円周率
        std::vector<Vertex> Vertices; //球面頂点一覧
        std::vector<std::uint32_t> Indices; //三角形インデックス一覧
        const std::uint32_t RingVertexCount = Slice + 1; //継ぎ目を複製した一周の頂点数

        Vertices.reserve(static_cast<std::size_t>(Stack + 1) * RingVertexCount);
        Indices.reserve(static_cast<std::size_t>(Stack) * Slice * 6);

        for (std::uint32_t StackIndex = 0; StackIndex <= Stack; ++StackIndex) //緯度リングを生成する
        {
            const float V = static_cast<float>(StackIndex) / static_cast<float>(Stack); //V座標
            const float Theta = V * Pi; //北極からの角度
            const float RingRadius = std::sin(Theta); //単位球のリング半径
            const float NormalY = std::cos(Theta); //単位球のY法線

            for (std::uint32_t SliceIndex = 0; SliceIndex <= Slice; ++SliceIndex) //経度頂点を生成する
            {
                const float U = static_cast<float>(SliceIndex) / static_cast<float>(Slice); //U座標
                const float Phi = U * Pi * 2.0f; //経度角
                const float NormalX = RingRadius * std::cos(Phi); //単位球のX法線
                const float NormalZ = RingRadius * std::sin(Phi); //単位球のZ法線
                const DirectX::XMFLOAT3 Normal = { NormalX, NormalY, NormalZ }; //頂点法線
                const DirectX::XMFLOAT3 VertexPosition =
                    { NormalX * Radius, NormalY * Radius, NormalZ * Radius }; //頂点座標

                Vertices.push_back({ VertexPosition, Normal, { U, V }, Color });
            }
        }

        for (std::uint32_t StackIndex = 0; StackIndex < Stack; ++StackIndex) //隣接緯度を接続する
        {
            for (std::uint32_t SliceIndex = 0; SliceIndex < Slice; ++SliceIndex) //一セルを二三角形にする
            {
                const std::uint32_t TopLeft = StackIndex * RingVertexCount + SliceIndex; //左上頂点
                const std::uint32_t BottomLeft = TopLeft + RingVertexCount; //左下頂点

                Indices.insert(Indices.end(),
                    { TopLeft, BottomLeft, TopLeft + 1,
                      TopLeft + 1, BottomLeft, BottomLeft + 1 });
            }
        }

        SetMeshData(Vertices, Indices);
    }
}
