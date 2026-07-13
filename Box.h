//|| Box.h ||:::::::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  直方体プリミティブオブジェクトを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  PrimitiveObject階層とメッシュ生成を適用
//||

#pragma once

#include <memory>

#include "PrimitiveObject.h"

namespace Engine
{
    class Box final : public PrimitiveObject
    {
    public:
        //一辺1の直方体を作成する
        Box();

        //直方体を破棄する
        ~Box() override;

        void SetSize(float width, float height, float depth);

        //未登録状態の直方体定義を複製する
        //戻り値 : 同じ寸法と姿勢を持つ直方体
        std::unique_ptr<Object> Clone() const override;

    protected:
        //直方体の頂点と三角形を再構築する
        void BuildMesh() override;

    private:
        float Width; //X軸方向の幅
        float Height; //Y軸方向の高さ
        float Depth; //Z軸方向の奥行き
    };
}
