//|| HalfSphere.h ||::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Y軸正方向の閉じた半球プリミティブオブジェクトを定義する
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
    class HalfSphere final : public PrimitiveObject
    {
    public:
        //半径0.5の上半球を作成する
        HalfSphere();

        //半球を破棄する
        ~HalfSphere() override;

        void SetRadius(float radius);
        void SetDivision(std::uint32_t slice, std::uint32_t stack);

        //未登録状態の半球定義を複製する
        //戻り値 : 同じ半径、分割数、姿勢を持つ半球
        std::unique_ptr<Object> Clone() const override;

    protected:
        //曲面と底面を持つ上半球メッシュを再構築する
        void BuildMesh() override;

    private:
        float Radius; //半球半径
        std::uint32_t Slice; //経度方向分割数
        std::uint32_t Stack; //曲面の緯度方向分割数
    };
}
