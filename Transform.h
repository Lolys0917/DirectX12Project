//|| Transform.h ||::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Objectが必ず所有するLocal姿勢と行列変換を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <DirectXMath.h>

namespace Engine
{
    class Transform final
    {
    public:
        Transform();

        void SetLocalPosition(const DirectX::XMFLOAT3& position);
        void SetLocalRotation(const DirectX::XMFLOAT3& rotation);
        void SetLocalScale(const DirectX::XMFLOAT3& scale);

        const DirectX::XMFLOAT3& GetLocalPosition() const;
        const DirectX::XMFLOAT3& GetLocalRotation() const;
        const DirectX::XMFLOAT3& GetLocalScale() const;

        DirectX::XMMATRIX GetLocalMatrix() const;
        bool SetLocalMatrix(DirectX::FXMMATRIX matrix);

    private:
        DirectX::XMFLOAT3 LocalPosition;
        DirectX::XMFLOAT3 LocalRotation;
        DirectX::XMFLOAT3 LocalScale;
    };
}
