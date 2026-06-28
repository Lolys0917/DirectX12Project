#pragma once
#include <vector>
#include <wrl.h>
#include <d3d12.h>
using namespace Microsoft::WRL;

struct ModelVertex
{
    float x;
    float y;
    float z;

    float nx;
    float ny;
    float nz;

    float u;
    float v;
};

