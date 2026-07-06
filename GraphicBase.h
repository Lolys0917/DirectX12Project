#pragma once
#include <vector>
#include <memory>
#include <Windows.h>

#include "DirectX12.h"
#include "IRenderable.h"

namespace Engine
{
    class GraphicBase final
    {
    public:
        GraphicBase();
        ~GraphicBase();

        GraphicBase(const GraphicBase&) = delete;
        GraphicBase& operator=(const GraphicBase&) = delete;

        bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
        void Finalize();

        void BeginDraw();
        void DrawAll();
        void EndDraw();

        void AddRenderable(IRenderable* renderable);
        void ClearRenderables();

        DirectX12& GetDirectX12();

    private:
        DirectX12 m_DirectX12;

        std::vector<IRenderable*> m_Renderables;

        float m_ClearColor[4];
    };
}