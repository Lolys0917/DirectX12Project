//|| Capsule.h ||:::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Y軸方向のカプセルプリミティブオブジェクトを定義する
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
    class Capsule final : public PrimitiveObject
    {
    public:
        //半径0.5、全高2のカプセルを作成する
        Capsule();

        //カプセルを破棄する
        ~Capsule() override;

        void SetSize(float radius, float height);
        void SetDivision(std::uint32_t slice, std::uint32_t hemisphereStack);

        //未登録状態のカプセル定義を複製する
        //戻り値 : 同じ寸法、分割数、姿勢を持つカプセル
        std::unique_ptr<Object> Clone() const override;

    protected:
        //円柱部と上下半球を連続リングで再構築する
        void BuildMesh() override;

    private:
        float Radius; //カプセル半径
        float Height; //両端を含む全高
        std::uint32_t Slice; //円周方向分割数
        std::uint32_t HemisphereStack; //片側半球の緯度分割数
    };
}
