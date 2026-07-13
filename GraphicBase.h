//|| GraphicBase.h ||:::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  DirectX12描画基盤の所有とライフサイクルを管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Component描画に合わせて不要な描画リストを削除
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include <Windows.h>
#include <cstdint>

#include "DirectX12.h"

namespace Engine
{
    class GraphicBase final
    {
    public:
        //未初期化のDirectX12描画基盤管理器を作成する
        GraphicBase();

        //DirectX12描画基盤を終了して破棄する
        ~GraphicBase();

        //DirectX12描画基盤の二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元描画基盤管理器
        GraphicBase(const GraphicBase&) = delete;

        //DirectX12描画基盤の二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元描画基盤管理器
        //戻り値: 代入先描画基盤管理器への参照
        GraphicBase& operator=(const GraphicBase&) = delete;

        //指定した子WindowへDirectX12描画基盤を作成する
        //引数: hwnd 描画対象Window、width 描画幅、height 描画高さ
        //戻り値: 初期化に成功した場合はtrue
        bool Initialize(HWND hwnd, uint32_t width, uint32_t height);

        //DirectX12描画基盤を終了する
        void Finalize();

        //SwapChainとDepthBufferを指定サイズへ再作成する
        //引数: width 新しい描画幅、height 新しい描画高さ
        //戻り値: サイズ変更に成功した場合はtrue
        bool Resize(uint32_t width, uint32_t height);

        DirectX12& GetDirectX12();
        const DirectX12& GetDirectX12() const;

    private:
        DirectX12 Graphics; //Scene描画で共有するDirectX12基盤
        bool Initialized; //初期化済みの場合はtrue
    };
}
