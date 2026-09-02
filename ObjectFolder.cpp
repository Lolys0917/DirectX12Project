//|| ObjectFolder.cpp ||:::::::::::::::::::::::
//||
//||  Scene Hierarchy整理専用Folder Objectを実装する

#include "ObjectFolder.h"

namespace Engine
{
    ObjectFolder::ObjectFolder()
        : Object(ObjectType::Folder)
    {
    }

    std::unique_ptr<Object> ObjectFolder::Clone() const
    {
        auto Duplicate = std::make_unique<ObjectFolder>();
        CopyDefinitionTo(*Duplicate);
        return Duplicate;
    }
}
