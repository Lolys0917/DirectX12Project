//|| Cylinder.h ||::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Y軸方向の閉じた円柱プリミティブオブジェクトを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

#include <cstdint>
#include <memory>

#include "PrimitiveObject.h"

namespace Engine
{
    class Cylinder final : public PrimitiveObject
    {
    public:
        //半径0.5、高さ1の円柱を作成する
        Cylinder();

        //円柱を破棄する
        ~Cylinder() override;

        void SetSize(float radius, float height);
        void SetDivision(std::uint32_t slice);

        //未登録状態の円柱定義を複製する
        //戻り値 : 同じ寸法、分割数、姿勢を持つ円柱
        std::unique_ptr<Object> Clone() const override;

    protected:
        //側面と上下蓋を持つ円柱メッシュを再構築する
        void BuildMesh() override;

    private:
        float Radius; //円柱半径
        float Height; //Y軸方向の高さ
        std::uint32_t Slice; //円周方向分割数
    };
}
