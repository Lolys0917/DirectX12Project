//|| Collider.cpp ||::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  基本Collider形状の設定、複製、ワールド境界計算を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Collider.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "Object.h"

namespace Engine
{
    namespace
    {
        //ローカル点を所有Objectのワールド空間へ変換する
        //owner : 所有Object。未登録時はnullptr
        //localPoint : ローカル座標
        //戻り値 : ワールド座標
        DirectX::XMFLOAT3 TransformPoint(
            const Object* owner,
            const DirectX::XMFLOAT3& localPoint
        )
        {
            if (!owner)
            {
                return localPoint;
            }

            DirectX::XMFLOAT3 WorldPoint; //変換後のワールド座標
            DirectX::XMStoreFloat3(
                &WorldPoint,
                DirectX::XMVector3TransformCoord(
                    DirectX::XMLoadFloat3(&localPoint),
                    owner->GetWorldMatrix()
                )
            );
            return WorldPoint;
        }

        //ローカル外接直方体の8頂点からワールドAABBを求める
        //owner : 所有Object。未登録時はnullptr
        //center : ローカル中心
        //halfExtent : ローカル半寸法
        //戻り値 : 回転と拡縮を包摂するワールドAABB
        AxisAlignedBox TransformLocalBox(
            const Object* owner,
            const DirectX::XMFLOAT3& center,
            const DirectX::XMFLOAT3& halfExtent
        )
        {
            const std::array<DirectX::XMFLOAT3, 8> LocalCorners = //ローカル外接直方体の8頂点
            {
                DirectX::XMFLOAT3(center.x - halfExtent.x, center.y - halfExtent.y, center.z - halfExtent.z),
                DirectX::XMFLOAT3(center.x + halfExtent.x, center.y - halfExtent.y, center.z - halfExtent.z),
                DirectX::XMFLOAT3(center.x - halfExtent.x, center.y + halfExtent.y, center.z - halfExtent.z),
                DirectX::XMFLOAT3(center.x + halfExtent.x, center.y + halfExtent.y, center.z - halfExtent.z),
                DirectX::XMFLOAT3(center.x - halfExtent.x, center.y - halfExtent.y, center.z + halfExtent.z),
                DirectX::XMFLOAT3(center.x + halfExtent.x, center.y - halfExtent.y, center.z + halfExtent.z),
                DirectX::XMFLOAT3(center.x - halfExtent.x, center.y + halfExtent.y, center.z + halfExtent.z),
                DirectX::XMFLOAT3(center.x + halfExtent.x, center.y + halfExtent.y, center.z + halfExtent.z)
            };
            const float MaximumValue = (std::numeric_limits<float>::max)(); //初期境界値
            AxisAlignedBox Bounds =
                { { MaximumValue, MaximumValue, MaximumValue },
                  { -MaximumValue, -MaximumValue, -MaximumValue } }; //集計するワールドAABB

            for (const DirectX::XMFLOAT3& LocalCorner : LocalCorners) //8頂点をワールド変換する
            {
                const DirectX::XMFLOAT3 WorldCorner =
                    TransformPoint(owner, LocalCorner); //変換後頂点
                Bounds.Minimum.x = (std::min)(Bounds.Minimum.x, WorldCorner.x);
                Bounds.Minimum.y = (std::min)(Bounds.Minimum.y, WorldCorner.y);
                Bounds.Minimum.z = (std::min)(Bounds.Minimum.z, WorldCorner.z);
                Bounds.Maximum.x = (std::max)(Bounds.Maximum.x, WorldCorner.x);
                Bounds.Maximum.y = (std::max)(Bounds.Maximum.y, WorldCorner.y);
                Bounds.Maximum.z = (std::max)(Bounds.Maximum.z, WorldCorner.z);
            }

            return Bounds;
        }

        //所有Objectの最大絶対拡縮率を取得する
        //owner : 所有Object。未登録時はnullptr
        //戻り値 : XYZで最大の絶対拡縮率
        float GetMaximumScale(const Object* owner)
        {
            if (!owner)
            {
                return 1.0f;
            }

            const DirectX::XMFLOAT3& Scale = owner->GetScale(); //所有Objectの拡縮率
            return (std::max)({ std::abs(Scale.x), std::abs(Scale.y), std::abs(Scale.z) });
        }
    }

    //指定形状種別のコライダーを作成する
    //componentType : 具体的なコライダー種別
    Collider::Collider(ComponentType componentType)
        : Component(componentType)
        , Center(0.0f, 0.0f, 0.0f)
        , Trigger(false)
        , LayerMask(0xFFFFFFFFu)
    {
    }

    //コライダーを破棄する
    Collider::~Collider() = default;

    //複製先へ共通コライダー設定をコピーする
    //destination : 未登録の複製先
    void Collider::CopyColliderDefinitionTo(Collider& destination) const
    {
        CopyDefinitionTo(destination);
        destination.Center = Center;
        destination.Trigger = Trigger;
        destination.LayerMask = LayerMask;
    }

    //一辺1の直方体コライダーを作成する
    BoxCollider::BoxCollider()
        : Collider(ComponentType::BoxCollider)
        , Size(1.0f, 1.0f, 1.0f)
    {
    }

    //概要：Box ColliderのLocal寸法を安全な正値へ補正する
    //引数：size=設定するXYZ寸法
    //戻り値：なし
    void BoxCollider::SetSize(const DirectX::XMFLOAT3& size)
    {
        constexpr float MinimumSize = 0.0001f; //ゼロ形状を防ぐ最小寸法
        Size.x = (std::max)(size.x, MinimumSize);
        Size.y = (std::max)(size.y, MinimumSize);
        Size.z = (std::max)(size.z, MinimumSize);
    }

    //回転と拡縮を含む直方体のワールドAABBを取得する
    //戻り値 : 8頂点を変換して求めたAABB
    AxisAlignedBox BoxCollider::GetWorldBounds() const
    {
        const DirectX::XMFLOAT3 HalfExtent =
            { Size.x * 0.5f, Size.y * 0.5f, Size.z * 0.5f }; //ローカル半寸法
        return TransformLocalBox(GetOwner(), Center, HalfExtent);
    }

    //未登録状態の直方体コライダーを複製する
    //戻り値 : 同じ寸法と共通設定を持つコライダー
    std::unique_ptr<Component> BoxCollider::Clone() const
    {
        std::unique_ptr<BoxCollider> ClonedCollider =
            std::make_unique<BoxCollider>(); //複製コライダー
        ClonedCollider->Size = Size;
        CopyColliderDefinitionTo(*ClonedCollider);
        return ClonedCollider;
    }

    //半径0.5の球コライダーを作成する
    SphereCollider::SphereCollider()
        : Collider(ComponentType::SphereCollider)
        , Radius(0.5f)
    {
    }

    //概要：Sphere ColliderのLocal半径を安全な正値へ補正する
    //引数：radius=設定する球半径
    //戻り値：なし
    void SphereCollider::SetRadius(float radius)
    {
        constexpr float MinimumRadius = 0.0001f; //ゼロ球を防ぐ最小半径
        Radius = (std::max)(radius, MinimumRadius);
    }

    //球を包むワールドAABBを取得する
    //戻り値 : 非一様拡縮時も包摂するAABB
    AxisAlignedBox SphereCollider::GetWorldBounds() const
    {
        const DirectX::XMFLOAT3 WorldCenter = TransformPoint(GetOwner(), Center); //ワールド中心
        const float WorldRadius = Radius * GetMaximumScale(GetOwner()); //保守的ワールド半径
        return {
            { WorldCenter.x - WorldRadius, WorldCenter.y - WorldRadius, WorldCenter.z - WorldRadius },
            { WorldCenter.x + WorldRadius, WorldCenter.y + WorldRadius, WorldCenter.z + WorldRadius }
        };
    }

    //未登録状態の球コライダーを複製する
    //戻り値 : 同じ半径と共通設定を持つコライダー
    std::unique_ptr<Component> SphereCollider::Clone() const
    {
        std::unique_ptr<SphereCollider> ClonedCollider =
            std::make_unique<SphereCollider>(); //複製コライダー
        ClonedCollider->Radius = Radius;
        CopyColliderDefinitionTo(*ClonedCollider);
        return ClonedCollider;
    }

    //半径0.5、全高2のY軸カプセルコライダーを作成する
    CapsuleCollider::CapsuleCollider()
        : Collider(ComponentType::CapsuleCollider)
        , Radius(0.5f)
        , Height(2.0f)
    {
    }

    //概要：Capsule ColliderのLocal半径と全高を安全な値へ補正する
    //引数：radius=Capsule半径、height=両端を含む全高
    //戻り値：なし
    void CapsuleCollider::SetSize(float radius, float height)
    {
        constexpr float MinimumRadius = 0.0001f; //ゼロ形状を防ぐ最小半径
        Radius = (std::max)(radius, MinimumRadius);
        Height = (std::max)(height, Radius * 2.0f);
    }

    //カプセルを包むワールドAABBを取得する
    //戻り値 : 両端球中心と最大拡縮半径から求めたAABB
    AxisAlignedBox CapsuleCollider::GetWorldBounds() const
    {
        const float HalfStraightHeight = (Height - Radius * 2.0f) * 0.5f; //中心線分の半長
        const DirectX::XMFLOAT3 LocalTop =
            { Center.x, Center.y + HalfStraightHeight, Center.z }; //上端球中心
        const DirectX::XMFLOAT3 LocalBottom =
            { Center.x, Center.y - HalfStraightHeight, Center.z }; //下端球中心
        const DirectX::XMFLOAT3 WorldTop = TransformPoint(GetOwner(), LocalTop); //ワールド上端球中心
        const DirectX::XMFLOAT3 WorldBottom = TransformPoint(GetOwner(), LocalBottom); //ワールド下端球中心
        const float WorldRadius = Radius * GetMaximumScale(GetOwner()); //保守的ワールド半径

        return {
            { (std::min)(WorldTop.x, WorldBottom.x) - WorldRadius,
              (std::min)(WorldTop.y, WorldBottom.y) - WorldRadius,
              (std::min)(WorldTop.z, WorldBottom.z) - WorldRadius },
            { (std::max)(WorldTop.x, WorldBottom.x) + WorldRadius,
              (std::max)(WorldTop.y, WorldBottom.y) + WorldRadius,
              (std::max)(WorldTop.z, WorldBottom.z) + WorldRadius }
        };
    }

    //未登録状態のカプセルコライダーを複製する
    //戻り値 : 同じ寸法と共通設定を持つコライダー
    std::unique_ptr<Component> CapsuleCollider::Clone() const
    {
        std::unique_ptr<CapsuleCollider> ClonedCollider =
            std::make_unique<CapsuleCollider>(); //複製コライダー
        ClonedCollider->Radius = Radius;
        ClonedCollider->Height = Height;
        CopyColliderDefinitionTo(*ClonedCollider);
        return ClonedCollider;
    }

    //半径0.5、高さ1のY軸円柱コライダーを作成する
    CylinderCollider::CylinderCollider()
        : Collider(ComponentType::CylinderCollider)
        , Radius(0.5f)
        , Height(1.0f)
    {
    }

    //概要：Cylinder ColliderのLocal半径と高さを安全な正値へ補正する
    //引数：radius=円柱半径、height=円柱高さ
    //戻り値：なし
    void CylinderCollider::SetSize(float radius, float height)
    {
        constexpr float MinimumSize = 0.0001f; //ゼロ形状を防ぐ最小寸法
        Radius = (std::max)(radius, MinimumSize);
        Height = (std::max)(height, MinimumSize);
    }

    //回転と拡縮を含む円柱の保守的ワールドAABBを取得する
    //戻り値 : ローカル外接直方体を変換して求めたAABB
    AxisAlignedBox CylinderCollider::GetWorldBounds() const
    {
        const DirectX::XMFLOAT3 HalfExtent =
            { Radius, Height * 0.5f, Radius }; //円柱のローカル外接半寸法
        return TransformLocalBox(GetOwner(), Center, HalfExtent);
    }

    //未登録状態の円柱コライダーを複製する
    //戻り値 : 同じ寸法と共通設定を持つコライダー
    std::unique_ptr<Component> CylinderCollider::Clone() const
    {
        std::unique_ptr<CylinderCollider> ClonedCollider =
            std::make_unique<CylinderCollider>(); //複製コライダー
        ClonedCollider->Radius = Radius;
        ClonedCollider->Height = Height;
        CopyColliderDefinitionTo(*ClonedCollider);
        return ClonedCollider;
    }

    //所有Object位置を通る上向き無限平面コライダーを作成する
    PlaneCollider::PlaneCollider()
        : Collider(ComponentType::PlaneCollider)
        , Normal(0.0f, 1.0f, 0.0f)
        , Distance(0.0f)
    {
    }

    //概要：Plane ColliderのLocal法線と原点からの距離を設定する
    //引数：normal=正規化する平面法線、distance=Local原点からの距離
    //戻り値：なし
    void PlaneCollider::SetPlane(const DirectX::XMFLOAT3& normal, float distance)
    {
        DirectX::XMVECTOR NormalVector = DirectX::XMLoadFloat3(&normal); //正規化対象法線
        const float LengthSquared = DirectX::XMVectorGetX(
            DirectX::XMVector3LengthSq(NormalVector)); //法線長の二乗

        if (LengthSquared <= 0.00000001f)
        {
            Normal = { 0.0f, 1.0f, 0.0f };
        }
        else
        {
            DirectX::XMStoreFloat3(&Normal, DirectX::XMVector3Normalize(NormalVector));
        }

        Distance = distance;
    }

    //ワールド空間の平面方程式を取得する
    //戻り値 : xyzが単位法線、wが原点からの符号付き項
    DirectX::XMFLOAT4 PlaneCollider::GetWorldPlane() const
    {
        DirectX::XMFLOAT3 WorldNormal = Normal; //ワールド単位法線
        const Object* OwnerObject = GetOwner(); //所有Object

        if (OwnerObject)
        {
            const DirectX::XMFLOAT3& OwnerRotation = OwnerObject->GetRotation(); //所有Object回転
            const DirectX::XMMATRIX RotationMatrix =
                DirectX::XMMatrixRotationRollPitchYaw(
                    OwnerRotation.x,
                    OwnerRotation.y,
                    OwnerRotation.z
                ); //法線変換用回転行列
            DirectX::XMStoreFloat3(
                &WorldNormal,
                DirectX::XMVector3Normalize(
                    DirectX::XMVector3TransformNormal(
                        DirectX::XMLoadFloat3(&Normal),
                        RotationMatrix
                    )
                )
            );
        }

        const DirectX::XMFLOAT3 LocalPoint =
            { Center.x + Normal.x * Distance,
              Center.y + Normal.y * Distance,
              Center.z + Normal.z * Distance }; //ローカル平面上の点
        const DirectX::XMFLOAT3 WorldPoint = TransformPoint(OwnerObject, LocalPoint); //ワールド平面上の点
        const float PlaneTerm = -(
            WorldNormal.x * WorldPoint.x +
            WorldNormal.y * WorldPoint.y +
            WorldNormal.z * WorldPoint.z); //平面方程式の定数項

        return { WorldNormal.x, WorldNormal.y, WorldNormal.z, PlaneTerm };
    }

    //無限平面を示す最大範囲AABBを取得する
    //戻り値 : broad-phaseで常に候補となる保守的AABB
    AxisAlignedBox PlaneCollider::GetWorldBounds() const
    {
        const float Extent = (std::numeric_limits<float>::max)() * 0.25f; //演算余裕を残す最大範囲
        return { { -Extent, -Extent, -Extent }, { Extent, Extent, Extent } };
    }

    //未登録状態の平面コライダーを複製する
    //戻り値 : 同じ平面と共通設定を持つコライダー
    std::unique_ptr<Component> PlaneCollider::Clone() const
    {
        std::unique_ptr<PlaneCollider> ClonedCollider =
            std::make_unique<PlaneCollider>(); //複製コライダー
        ClonedCollider->Normal = Normal;
        ClonedCollider->Distance = Distance;
        CopyColliderDefinitionTo(*ClonedCollider);
        return ClonedCollider;
    }
}
