//|| Model.h ||:::::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Objectへ付加する3D Model Componentの共通基底を定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Object継承からComponent継承へ変更
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include "Component.h"

namespace Engine
{
    class Model : public Component
    {
    public:
        static constexpr ComponentType StaticType = ComponentType::Model; //Manager登録で使用するComponent型

        //派生Model Componentを基底Pointerから安全に破棄する
        ~Model() override;

    protected:
        //未登録の共通Model Component状態を作成する
        Model();
    };
}
