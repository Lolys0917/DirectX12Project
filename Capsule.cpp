//|| Capsule.cpp ||:::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  円柱部と上下半球が連続するカプセルメッシュを生成する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Capsule.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Engine
{
    //半径0.5、全高2のカプセルを作成する
    Capsule::Capsule()
        : PrimitiveObject(ObjectType::Capsule)
        , Radius(0.5f)
        , Height(2.0f)
        , Slice(32)
        , HemisphereStack(8)
    {
        BuildMesh();
    }

    //カプセルを破棄する
    Capsule::~Capsule() = default;

    //概要：Capsuleの半径と全高を安全な値へ補正してMeshを再構築する
    //引数：radius=Capsule半径、height=両端を含む全高
    //戻り値：なし
    void Capsule::SetSize(float radius, float height)
    {
        constexpr float MinimumRadius = 0.0001f; //ゼロ形状を防ぐ最小半径
        const float NewRadius = (std::max)(radius, MinimumRadius);
        const float NewHeight = (std::max)(height, NewRadius * 2.0f);

        if (Radius == NewRadius && Height == NewHeight)
        {
            return;
        }

        Radius = NewRadius;
        Height = NewHeight;
        BuildMesh();
    }

    //概要：Capsuleの円周と片側半球分割数を変更してMeshを再構築する
    //引数：slice=円周分割数、hemisphereStack=片側半球分割数
    //戻り値：なし
    void Capsule::SetDivision(std::uint32_t slice, std::uint32_t hemisphereStack)
    {
        const std::uint32_t NewSlice = (std::max<std::uint32_t>)(slice, 3);
        const std::uint32_t NewStack = (std::max<std::uint32_t>)(hemisphereStack, 1);

        if (Slice == NewSlice && HemisphereStack == NewStack)
        {
            return;
        }

        Slice = NewSlice;
        HemisphereStack = NewStack;
        BuildMesh();
    }

    //未登録状態のカプセル定義を複製する
    //戻り値 : 同じ寸法、分割数、姿勢を持つカプセル
    std::unique_ptr<Object> Capsule::Clone() const
    {
        std::unique_ptr<Capsule> ClonedObject = std::make_unique<Capsule>(); //複製カプセル
        ClonedObject->Radius = Radius;
        ClonedObject->Height = Height;
        ClonedObject->Slice = Slice;
        ClonedObject->HemisphereStack = HemisphereStack;
        ClonedObject->Color = Color;
        ClonedObject->BuildMesh();
        CopyDefinitionTo(*ClonedObject);
        return ClonedObject;
    }

    //円柱部と上下半球を連続リングで再構築する
    void Capsule::BuildMesh()
    {
        constexpr float Pi = 3.14159265358979323846f; //円周率
        const float HalfStraightHeight = (Height - Radius * 2.0f) * 0.5f; //中心円柱の半高
        const std::uint32_t RingVertexCount = Slice + 1; //継ぎ目を複製したリング頂点数
        const std::uint32_t RingCount = (HemisphereStack + 1) * 2; //上下半球を含むリング数
        std::vector<Vertex> Vertices; //カプセル頂点一覧
        std::vector<std::uint32_t> Indices; //三角形インデックス一覧

        Vertices.reserve(static_cast<std::size_t>(RingCount) * RingVertexCount);
        Indices.reserve(static_cast<std::size_t>(RingCount - 1) * Slice * 6);

        for (std::uint32_t RingIndex = 0; RingIndex < RingCount; ++RingIndex) //上極から下極までリングを生成する
        {
            const bool IsTop = RingIndex <= HemisphereStack; //上半球リングの場合true
            const std::uint32_t LocalStack = IsTop
                ? RingIndex
                : RingIndex - HemisphereStack - 1; //対象半球内のリング番号
            const float LocalRatio = static_cast<float>(LocalStack) /
                static_cast<float>(HemisphereStack); //対象半球内の比率
            const float Theta = IsTop
                ? LocalRatio * Pi * 0.5f
                : Pi * 0.5f + LocalRatio * Pi * 0.5f; //北極からの角度
            const float CenterY = IsTop ? HalfStraightHeight : -HalfStraightHeight; //半球中心Y
            const float RingRadius = std::sin(Theta); //単位半球リング半径
            const float NormalY = std::cos(Theta); //単位半球Y法線
            const float V = static_cast<float>(RingIndex) /
                static_cast<float>(RingCount - 1); //全体V座標

            for (std::uint32_t SliceIndex = 0; SliceIndex <= Slice; ++SliceIndex) //円周頂点を生成する
            {
                const float U = static_cast<float>(SliceIndex) / static_cast<float>(Slice); //U座標
                const float Phi = U * Pi * 2.0f; //円周角
                const float NormalX = RingRadius * std::cos(Phi); //X法線
                const float NormalZ = RingRadius * std::sin(Phi); //Z法線
                const DirectX::XMFLOAT3 Normal = { NormalX, NormalY, NormalZ }; //頂点法線
                const DirectX::XMFLOAT3 VertexPosition =
                    { NormalX * Radius,
                      CenterY + NormalY * Radius,
                      NormalZ * Radius }; //頂点座標
                Vertices.push_back({ VertexPosition, Normal, { U, V }, Color });
            }
        }

        for (std::uint32_t RingIndex = 0; RingIndex + 1 < RingCount; ++RingIndex) //隣接リングを接続する
        {
            for (std::uint32_t SliceIndex = 0; SliceIndex < Slice; ++SliceIndex) //一セルを二三角形にする
            {
                const std::uint32_t TopLeft = RingIndex * RingVertexCount + SliceIndex; //左上頂点
                const std::uint32_t BottomLeft = TopLeft + RingVertexCount; //左下頂点
                Indices.insert(Indices.end(),
                    { TopLeft, BottomLeft, TopLeft + 1,
                      TopLeft + 1, BottomLeft, BottomLeft + 1 });
            }
        }

        SetMeshData(Vertices, Indices);
    }
}
