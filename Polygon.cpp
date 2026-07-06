#include "Polygon.h"
#include <cmath>

namespace
{
    const float PI = 3.14159265358979323846f;

    ModelVertex CreateCircleVertex(float angle, float radius)
    {
        return
        {
            std::cos(angle) * radius,
            std::sin(angle) * radius,
            0.0f,
            (std::cos(angle) + 1.0f) * 0.5f,
            (1.0f - std::sin(angle)) * 0.5f
        };
    }
}

std::vector<ModelVertex> ClassPolygon::CreateQuad(float x, float y, float size)
{
    return
    {
        {-size + x, -size + y, 0.0f, 0.0f, 1.0f},
        {-size + x,  size + y, 0.0f, 0.0f, 0.0f},
        { size + x, -size + y, 0.0f, 1.0f, 1.0f},

        { size + x, -size + y, 0.0f, 1.0f, 1.0f},
        {-size + x,  size + y, 0.0f, 0.0f, 0.0f},
        { size + x,  size + y, 0.0f, 1.0f, 0.0f}
    };
}

std::vector<ModelVertex> ClassPolygon::CreateRegular(int sides, float radius)
{
    std::vector<ModelVertex> vertices;

    if (sides < 3) return vertices;

    ModelVertex center = { 0.0f, 0.0f, 0.0f, 0.5f, 0.5f };

    for (int i = 0; i < sides; i++)
    {
        float angle1 = static_cast<float>(i) / sides * 2.0f * PI;
        float angle2 = static_cast<float>(i + 1) / sides * 2.0f * PI;

        vertices.push_back(center);
        vertices.push_back(CreateCircleVertex(angle1, radius));
        vertices.push_back(CreateCircleVertex(angle2, radius));
    }

    return vertices;
}