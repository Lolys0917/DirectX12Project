//|| Collider.h ||::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  基本形状の衝突領域を表すColliderコンポーネント群を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

#include <cstdint>
#include <memory>

#include <DirectXMath.h>

#include "Component.h"

namespace Engine
{
    struct AxisAlignedBox
    {
        DirectX::XMFLOAT3 Minimum; //ワールド空間の最小座標
        DirectX::XMFLOAT3 Maximum; //ワールド空間の最大座標
    };

    class Collider : public Component
    {
    public:
        //コライダーを破棄する
        ~Collider() override;

        void SetCenter(const DirectX::XMFLOAT3& center) { Center = center; }
        const DirectX::XMFLOAT3& GetCenter() const { return Center; }
        void SetTrigger(bool trigger) { Trigger = trigger; }

        //Colliderが判定専用Triggerか確認する
        //戻り値: 物理応答を行わないTriggerの場合はtrue
        bool IsTrigger() const { return Trigger; }
        void SetLayerMask(std::uint32_t layerMask) { LayerMask = layerMask; }
        std::uint32_t GetLayerMask() const { return LayerMask; }

        //ブロードフェーズ用ワールドAABBを取得する
        //戻り値 : 所有Objectの姿勢を反映した保守的AABB
        virtual AxisAlignedBox GetWorldBounds() const = 0;

        //未登録状態のコライダー定義を複製する
        //戻り値 : 形状設定を複製したコライダー
        virtual std::unique_ptr<Component> Clone() const override = 0;

    protected:
        //指定形状種別のコライダーを作成する
        //componentType : 具体的なコライダー種別
        explicit Collider(ComponentType componentType);

        //複製先へ共通コライダー設定をコピーする
        //destination : 未登録の複製先
        void CopyColliderDefinitionTo(Collider& destination) const;

        DirectX::XMFLOAT3 Center; //所有Objectからのローカル中心オフセット
        bool Trigger; //物理応答を行わない判定専用の場合true
        std::uint32_t LayerMask; //衝突対象レイヤービット
    };

    class BoxCollider final : public Collider
    {
    public:
        //一辺1の直方体コライダーを作成する
        BoxCollider();

        void SetSize(const DirectX::XMFLOAT3& size);
        const DirectX::XMFLOAT3& GetSize() const { return Size; }

        //回転と拡縮を含む直方体のワールドAABBを取得する
        //戻り値 : 8頂点を変換して求めたAABB
        AxisAlignedBox GetWorldBounds() const override;

        //未登録状態の直方体コライダーを複製する
        //戻り値 : 同じ寸法と共通設定を持つコライダー
        std::unique_ptr<Component> Clone() const override;

    private:
        DirectX::XMFLOAT3 Size; //ローカル空間の全寸法
    };

    class SphereCollider final : public Collider
    {
    public:
        //半径0.5の球コライダーを作成する
        SphereCollider();

        void SetRadius(float radius);
        float GetRadius() const { return Radius; }

        //球を包むワールドAABBを取得する
        //戻り値 : 非一様拡縮時も包摂するAABB
        AxisAlignedBox GetWorldBounds() const override;

        //未登録状態の球コライダーを複製する
        //戻り値 : 同じ半径と共通設定を持つコライダー
        std::unique_ptr<Component> Clone() const override;

    private:
        float Radius; //ローカル空間の半径
    };

    class CapsuleCollider final : public Collider
    {
    public:
        //半径0.5、全高2のY軸カプセルコライダーを作成する
        CapsuleCollider();

        void SetSize(float radius, float height);
        float GetRadius() const { return Radius; }
        float GetHeight() const { return Height; }

        //カプセルを包むワールドAABBを取得する
        //戻り値 : 両端球中心と最大拡縮半径から求めたAABB
        AxisAlignedBox GetWorldBounds() const override;

        //未登録状態のカプセルコライダーを複製する
        //戻り値 : 同じ寸法と共通設定を持つコライダー
        std::unique_ptr<Component> Clone() const override;

    private:
        float Radius; //ローカル空間の半径
        float Height; //両端を含むローカル全高
    };

    class CylinderCollider final : public Collider
    {
    public:
        //半径0.5、高さ1のY軸円柱コライダーを作成する
        CylinderCollider();

        void SetSize(float radius, float height);
        float GetRadius() const { return Radius; }
        float GetHeight() const { return Height; }

        //回転と拡縮を含む円柱の保守的ワールドAABBを取得する
        //戻り値 : ローカル外接直方体を変換して求めたAABB
        AxisAlignedBox GetWorldBounds() const override;

        //未登録状態の円柱コライダーを複製する
        //戻り値 : 同じ寸法と共通設定を持つコライダー
        std::unique_ptr<Component> Clone() const override;

    private:
        float Radius; //ローカル空間の半径
        float Height; //ローカル空間の高さ
    };

    class PlaneCollider final : public Collider
    {
    public:
        //所有Object位置を通る上向き無限平面コライダーを作成する
        PlaneCollider();

        void SetPlane(const DirectX::XMFLOAT3& normal, float distance);
        const DirectX::XMFLOAT3& GetNormal() const { return Normal; }
        float GetDistance() const { return Distance; }

        //ワールド空間の平面方程式を取得する
        //戻り値 : xyzが単位法線、wが原点からの符号付き項
        DirectX::XMFLOAT4 GetWorldPlane() const;

        //無限平面を示す最大範囲AABBを取得する
        //戻り値 : broad-phaseで常に候補となる保守的AABB
        AxisAlignedBox GetWorldBounds() const override;

        //未登録状態の平面コライダーを複製する
        //戻り値 : 同じ平面と共通設定を持つコライダー
        std::unique_ptr<Component> Clone() const override;

    private:
        DirectX::XMFLOAT3 Normal; //ローカル空間の単位法線
        float Distance; //Centerから法線方向への距離
    };
}
