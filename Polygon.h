#pragma once

#include <vector>
#include "Model.h"

class ClassPolygon
{
public:
    static std::vector<ModelVertex> CreateQuad(float x, float y, float size);
    static std::vector<ModelVertex> CreateRegular(int sides, float radius);
};