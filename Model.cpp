//|| Model.cpp ||:::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Objectへ付加する3D Model Componentの共通基底を実装する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Component基底として再作成
//||

#include "Model.h"

namespace Engine
{
    //未登録の共通Model Component状態を作成する
    Model::Model()
        : Component(StaticType)
    {
    }

    //派生Model Componentを基底Pointerから安全に破棄する
    Model::~Model() = default;
}
