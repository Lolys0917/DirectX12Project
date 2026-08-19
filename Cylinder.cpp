//|| Cylinder.cpp ||::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  独立した側面法線と蓋法線を持つ円柱メッシュを生成する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Cylinder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Engine
{
    //半径0.5、高さ1の円柱を作成する
    Cylinder::Cylinder()
        : PrimitiveObject(ObjectType::Cylinder)
        , Radius(0.5f)
        , Height(1.0f)
        , Slice(32)
    {
        BuildMesh();
    }

    //円柱を破棄する
    Cylinder::~Cylinder() = default;

    //概要：円柱の半径と高さを安全な正値へ補正してMeshを再構築する
    //引数：radius=円柱半径、height=Y軸高さ
    //戻り値：なし
    void Cylinder::SetSize(float radius, float height)
    {
        constexpr float MinimumSize = 0.0001f; //ゼロ形状を防ぐ最小寸法
        const float NewRadius = (std::max)(radius, MinimumSize);
        const float NewHeight = (std::max)(height, MinimumSize);

        if (Radius == NewRadius && Height == NewHeight)
        {
            return;
        }

        Radius = NewRadius;
        Height = NewHeight;
        BuildMesh();
    }

    //概要：円柱の円周分割数を変更してMeshを再構築する
    //引数：slice=円周分割数
    //戻り値：なし
    void Cylinder::SetDivision(std::uint32_t slice)
    {
        const std::uint32_t NewSlice = (std::max<std::uint32_t>)(slice, 3);

        if (Slice == NewSlice)
        {
            return;
        }

        Slice = NewSlice;
        BuildMesh();
    }

    //未登録状態の円柱定義を複製する
    //戻り値 : 同じ寸法、分割数、姿勢を持つ円柱
    std::unique_ptr<Object> Cylinder::Clone() const
    {
        std::unique_ptr<Cylinder> ClonedObject = std::make_unique<Cylinder>(); //複製円柱
        ClonedObject->Radius = Radius;
        ClonedObject->Height = Height;
        ClonedObject->Slice = Slice;
        ClonedObject->Color = Color;
        ClonedObject->BuildMesh();
        CopyDefinitionTo(*ClonedObject);
        return ClonedObject;
    }

    //側面と上下蓋を持つ円柱メッシュを再構築する
    void Cylinder::BuildMesh()
    {
        constexpr float Pi = 3.14159265358979323846f; //円周率
        const float HalfHeight = Height * 0.5f; //高さの半分
        std::vector<Vertex> Vertices; //円柱頂点一覧
        std::vector<std::uint32_t> Indices; //三角形インデックス一覧

        for (std::uint32_t SliceIndex = 0; SliceIndex <= Slice; ++SliceIndex) //側面リングを生成する
        {
            const float U = static_cast<float>(SliceIndex) / static_cast<float>(Slice); //U座標
            const float Angle = U * Pi * 2.0f; //円周角
            const float Cosine = std::cos(Angle); //X方向単位値
            const float Sine = std::sin(Angle); //Z方向単位値
            const DirectX::XMFLOAT3 Normal = { Cosine, 0.0f, Sine }; //側面法線

            Vertices.push_back(
                { { Cosine * Radius, -HalfHeight, Sine * Radius }, Normal, { U, 1.0f }, Color });
            Vertices.push_back(
                { { Cosine * Radius, HalfHeight, Sine * Radius }, Normal, { U, 0.0f }, Color });
        }

        for (std::uint32_t SliceIndex = 0; SliceIndex < Slice; ++SliceIndex) //側面セルを接続する
        {
            const std::uint32_t BottomLeft = SliceIndex * 2; //側面左下頂点
            const std::uint32_t TopLeft = BottomLeft + 1; //側面左上頂点
            const std::uint32_t BottomRight = BottomLeft + 2; //側面右下頂点
            const std::uint32_t TopRight = BottomLeft + 3; //側面右上頂点
            Indices.insert(Indices.end(),
                { BottomLeft, TopLeft, BottomRight,
                  BottomRight, TopLeft, TopRight });
        }

        const std::uint32_t BottomCenter =
            static_cast<std::uint32_t>(Vertices.size()); //底面中心頂点
        Vertices.push_back(
            { { 0.0f, -HalfHeight, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.5f, 0.5f }, Color });

        for (std::uint32_t SliceIndex = 0; SliceIndex <= Slice; ++SliceIndex) //底面リングを生成する
        {
            const float Ratio = static_cast<float>(SliceIndex) / static_cast<float>(Slice); //円周比率
            const float Angle = Ratio * Pi * 2.0f; //円周角
            const float Cosine = std::cos(Angle); //X方向単位値
            const float Sine = std::sin(Angle); //Z方向単位値
            Vertices.push_back(
                { { Cosine * Radius, -HalfHeight, Sine * Radius },
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

        const std::uint32_t TopCenter =
            static_cast<std::uint32_t>(Vertices.size()); //上面中心頂点
        Vertices.push_back(
            { { 0.0f, HalfHeight, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.5f, 0.5f }, Color });

        for (std::uint32_t SliceIndex = 0; SliceIndex <= Slice; ++SliceIndex) //上面リングを生成する
        {
            const float Ratio = static_cast<float>(SliceIndex) / static_cast<float>(Slice); //円周比率
            const float Angle = Ratio * Pi * 2.0f; //円周角
            const float Cosine = std::cos(Angle); //X方向単位値
            const float Sine = std::sin(Angle); //Z方向単位値
            Vertices.push_back(
                { { Cosine * Radius, HalfHeight, Sine * Radius },
                  { 0.0f, 1.0f, 0.0f },
                  { Cosine * 0.5f + 0.5f, Sine * 0.5f + 0.5f },
                  Color });
        }

        for (std::uint32_t SliceIndex = 0; SliceIndex < Slice; ++SliceIndex) //上面三角形を生成する
        {
            Indices.insert(Indices.end(),
                { TopCenter,
                  TopCenter + SliceIndex + 1,
                  TopCenter + SliceIndex + 2 });
        }

        SetMeshData(Vertices, Indices);
    }
}
