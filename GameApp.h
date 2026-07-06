#pragma once

#include <Windows.h>
#include <cstdint>

#include "GraphicBase.h"
#include "Camera.h"
#include "Grid.h"
#include "OBJModel.h"

namespace Engine
{
    class GameApp final
    {
    public:
        GameApp();
        ~GameApp();

        bool Initialize(
            HWND hwnd,
            uint32_t width,
            uint32_t height
        );

        void Finalize();

        void Update(float deltaTime);
        void Draw();

    private:
        HWND m_HWND;

        GraphicBase m_GraphicBase;

        //オブジェクト変数
        Camera m_Camera;
        Grid m_Grid;
        OBJModel m_OBJModel;

        bool m_IsInitialized;
    };
}