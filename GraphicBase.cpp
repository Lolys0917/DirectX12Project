#include "GraphicBase.h"

namespace Engine
{
    GraphicBase::GraphicBase()
    {
        // ”wŒiF
        // ”’‚É‹ß‚¢F‚É‚·‚é‚Æ•Grid‚ªŒ©‚¦‚é
        m_ClearColor[0] = 0.85f;
        m_ClearColor[1] = 0.85f;
        m_ClearColor[2] = 0.85f;
        m_ClearColor[3] = 1.0f;
    }

    GraphicBase::~GraphicBase()
    {
        Finalize();
    }

    bool GraphicBase::Initialize(
        HWND hwnd,
        uint32_t width,
        uint32_t height
    )
    {
        return m_DirectX12.Initialize(
            hwnd,
            width,
            height
        );
    }

    void GraphicBase::Finalize()
    {
        m_DirectX12.Finalize();
    }

    void GraphicBase::BeginDraw()
    {
        m_DirectX12.BeginFrame(
            m_ClearColor
        );
    }

    void GraphicBase::DrawAll()
    {
        for (IRenderable* renderable : m_Renderables)
        {
            if (renderable)
            {
                renderable->Draw(m_DirectX12);
            }
        }
    }

    void GraphicBase::EndDraw()
    {
        m_DirectX12.EndFrame();
    }

    void GraphicBase::AddRenderable(
        IRenderable* renderable
    )
    {
        if (renderable)
        {
            m_Renderables.push_back(renderable);
        }
    }

    void GraphicBase::ClearRenderables()
    {
        m_Renderables.clear();
    }

    DirectX12& GraphicBase::GetDirectX12()
    {
        return m_DirectX12;
    }
}