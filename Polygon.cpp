#include "Renderer.h"

std::vector<PolygonState>
PolygonGenerator::Create(
    int sides,
    float radius
)
{
    std::vector<PolygonState> vertices;

    PolygonState center =
    {
        0,0,0,
        1,1,1,1
    };

    for (int i = 0; i < sides; i++)
    {
        float a1 =
            (float)i / sides *
            2 * 3.14159265358979f;

        float a2 =
            (float)(i + 1) / sides *
            2 * 3.14159265358979f;

        vertices.push_back(center);

        vertices.push_back(
            {
                cosf(a1) * radius,
                sinf(a1) * radius,
                0,
                0,1,0,1
            });

        vertices.push_back(
            {
                cosf(a2) * radius,
                sinf(a2) * radius,
                0,
                0,1,0,1
            });
    }

    return vertices;
}