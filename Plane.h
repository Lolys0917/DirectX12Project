//|| Plane.h ||:::::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  XZ平面上の矩形プリミティブオブジェクトを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

#include <memory>

#include "PrimitiveObject.h"

namespace Engine
{
    class Plane final : public PrimitiveObject
    {
    public:
        //幅と奥行きが1の平面を作成する
        Plane();

        //平面を破棄する
        ~Plane() override;

        void SetSize(float width, float depth);

        //未登録状態の平面定義を複製する
        //戻り値 : 同じ寸法と姿勢を持つ平面
        std::unique_ptr<Object> Clone() const override;

    protected:
        //上向き法線を持つ矩形メッシュを再構築する
        void BuildMesh() override;

    private:
        float Width; //X軸方向の幅
        float Depth; //Z軸方向の奥行き
    };
}
