// ==========================================================
// Main.cpp
// DX12 Texture Sample
// ==========================================================
#include <windows.h>
#include "Core.h"
#include "Model.h"
#include "Grid.h"
#include "Camera.h"
#include "Polygon.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    Core core;
    if (!core.Initialize(hInstance)) return -1;

    core.GetCamera().UpdateCamera(
        DirectX::XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)
    );
    Camera camera;
    camera.UpdateCamera({ 0.0f, 0.0f, -10.0f, 1.0f }, { 0.0f,0.0f ,0.0f ,0.0f });
    Model model;
    model.Initialize(
        core,
        L"12222_Cat_v1_l3.obj",
        L"Cat_diffuse.jpg",
        0.8f,
        ModelFileType::Obj
    );

    Grid grid;
    if (!grid.Initialize(core, 10, 0.1f)) return -1;

    core.Run(model, grid);
    return 0;
}