//|| GraphicBase.cpp ||:::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  DirectX12描画基盤の所有とライフサイクルを管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Component描画とリサイズに対応
//||  2026_06_01  v1.00  新規作成
//||

#include "GraphicBase.h"

namespace Engine
{
    //未初期化のDirectX12描画基盤管理器を作成する
    GraphicBase::GraphicBase()
        : Initialized(false)
    {
    }

    //DirectX12描画基盤を終了して破棄する
    GraphicBase::~GraphicBase()
    {
        Finalize();
    }

    //指定した子WindowへDirectX12描画基盤を作成する
    //引数: hwnd 描画対象Window、width 描画幅、height 描画高さ
    //戻り値: 初期化に成功した場合はtrue
    bool GraphicBase::Initialize(HWND hwnd, uint32_t width, uint32_t height)
    {
        if (Initialized)
        {
            return true;
        }

        Initialized = Graphics.Initialize(hwnd, width, height);
        return Initialized;
    }

    //DirectX12描画基盤を終了する
    void GraphicBase::Finalize()
    {
        if (!Initialized)
        {
            return;
        }

        Graphics.Finalize();
        Initialized = false;
    }

    //SwapChainとDepthBufferを指定サイズへ再作成する
    //引数: width 新しい描画幅、height 新しい描画高さ
    //戻り値: サイズ変更に成功した場合はtrue
    bool GraphicBase::Resize(uint32_t width, uint32_t height)
    {
        if (!Initialized)
        {
            return false;
        }

        return Graphics.Resize(width, height);
    }

    DirectX12& GraphicBase::GetDirectX12()
    {
        return Graphics;
    }

    const DirectX12& GraphicBase::GetDirectX12() const
    {
        return Graphics;
    }
}
