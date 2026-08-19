//|| HalfSphere.cpp ||::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  上半球曲面と下向き円形底面を生成する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "HalfSphere.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Engine
{
    //半径0.5の上半球を作成する
    HalfSphere::HalfSphere()
        : PrimitiveObject(ObjectType::HalfSphere)
        , Radius(0.5f)
        , Slice(32)
        , Stack(8)
    {
        BuildMesh();
    }

    //半球を破棄する
    HalfSphere::~HalfSphere() = default;

    //概要：半球半径を安全な正値へ補正してMeshを再構築する
    //引数：radius=新しい半球半径
    //戻り値：なし
    void HalfSphere::SetRadius(float radius)
    {
        constexpr float MinimumRadius = 0.0001f; //ゼロ半球を防ぐ最小半径
        Radius = (std::max)(radius, MinimumRadius);
        BuildMesh();
    }

    //概要：半球の経度と緯度分割数を変更してMeshを再構築する
    //引数：slice=経度分割数、stack=緯度分割数
    //戻り値：なし
    void HalfSphere::SetDivision(std::uint32_t slice, std::uint32_t stack)
    {
        Slice = (std::max<std::uint32_t>)(slice, 3);
        Stack = (std::max<std::uint32_t>)(stack, 1);
        BuildMesh();
    }

    //未登録状態の半球定義を複製する
    //戻り値 : 同じ半径、分割数、姿勢を持つ半球
    std::unique_ptr<Object> HalfSphere::Clone() const
    {
        std::unique_ptr<HalfSphere> ClonedObject = std::make_unique<HalfSphere>(); //複製半球
        ClonedObject->Radius = Radius;
        ClonedObject->Slice = Slice;
        ClonedObject->Stack = Stack;
        ClonedObject->Color = Color;
        ClonedObject->BuildMesh();
        CopyDefinitionTo(*ClonedObject);
        return ClonedObject;
    }

    //曲面と底面を持つ上半球メッシュを再構築する
    void HalfSphere::BuildMesh()
    {
        constexpr float Pi = 3.14159265358979323846f; //円周率
        const std::uint32_t RingVertexCount = Slice + 1; //継ぎ目を複製したリング頂点数
        std::vector<Vertex> Vertices; //半球頂点一覧
        std::vector<std::uint32_t> Indices; //三角形インデックス一覧

        for (std::uint32_t StackIndex = 0; StackIndex <= Stack; ++StackIndex) //極から赤道まで生成する
        {
            const float V = static_cast<float>(StackIndex) / static_cast<float>(Stack); //曲面V座標
            const float Theta = V * Pi * 0.5f; //北極から赤道までの角度
            const float RingRadius = std::sin(Theta); //単位半球のリング半径
            const float NormalY = std::cos(Theta); //単位半球のY法線

            for (std::uint32_t SliceIndex = 0; SliceIndex <= Slice; ++SliceIndex) //経度頂点を生成する
            {
                const float U = static_cast<float>(SliceIndex) / static_cast<float>(Slice); //U座標
                const float Phi = U * Pi * 2.0f; //経度角
                const float NormalX = RingRadius * std::cos(Phi); //X法線
                const float NormalZ = RingRadius * std::sin(Phi); //Z法線
                const DirectX::XMFLOAT3 Normal = { NormalX, NormalY, NormalZ }; //曲面法線
                const DirectX::XMFLOAT3 VertexPosition =
                    { NormalX * Radius, NormalY * Radius, NormalZ * Radius }; //曲面座標
                Vertices.push_back({ VertexPosition, Normal, { U, V }, Color });
            }
        }

        for (std::uint32_t StackIndex = 0; StackIndex < Stack; ++StackIndex) //曲面リングを接続する
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

        const std::uint32_t BottomCenter =
            static_cast<std::uint32_t>(Vertices.size()); //底面中心頂点
        Vertices.push_back(
            { { 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.5f, 0.5f }, Color });

        for (std::uint32_t SliceIndex = 0; SliceIndex <= Slice; ++SliceIndex) //底面円周を生成する
        {
            const float Ratio = static_cast<float>(SliceIndex) / static_cast<float>(Slice); //円周比率
            const float Phi = Ratio * Pi * 2.0f; //円周角
            const float Cosine = std::cos(Phi); //X方向単位値
            const float Sine = std::sin(Phi); //Z方向単位値
            Vertices.push_back(
                { { Cosine * Radius, 0.0f, Sine * Radius },
                  { 0.0f, -1.0f, 0.0f },
                  { Cosine * 0.5f + 0.5f, Sine * 0.5f + 0.5f },
                  Color });
        }

        for (std::uint32_t SliceIndex = 0; SliceIndex < Slice; ++SliceIndex) //底面三角形を生成する
        {
            Indices.insert(Indices.end(),
                { BottomCenter,
                  BottomCenter + SliceIndex + 2,
                  BottomCenter + SliceIndex + 1 });
        }

        SetMeshData(Vertices, Indices);
    }
}
