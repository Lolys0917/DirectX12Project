//|| Sphere.h ||::::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  UV球プリミティブオブジェクトを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  PrimitiveObject階層とメッシュ生成を適用
//||

#pragma once

#include <cstdint>
#include <memory>

#include "PrimitiveObject.h"

namespace Engine
{
    class Sphere final : public PrimitiveObject
    {
    public:
        //半径0.5のUV球を作成する
        Sphere();

        //球を破棄する
        ~Sphere() override;

        void SetRadius(float radius);
        void SetDivision(std::uint32_t slice, std::uint32_t stack);

        //未登録状態の球定義を複製する
        //戻り値 : 同じ半径、分割数、姿勢を持つ球
        std::unique_ptr<Object> Clone() const override;

    protected:
        //UV球メッシュを再構築する
        void BuildMesh() override;

    private:
        float Radius; //球半径
        std::uint32_t Slice; //経度方向分割数
        std::uint32_t Stack; //緯度方向分割数
    };
}
