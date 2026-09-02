//|| ObjectFolder.h ||:::::::::::::::::::::::::
//||
//||  Scene Hierarchy上でObjectをまとめる非描画Folder Objectを定義する

#pragma once

#include "Object.h"

namespace Engine
{
    class ObjectFolder final : public Object
    {
    public:
        ObjectFolder();
        std::unique_ptr<Object> Clone() const override;
    };
}
