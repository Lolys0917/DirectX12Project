//|| RenderContext.h ||::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  コンポーネント描画時に使用する描画対象とカメラを定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

namespace Engine
{
    class Camera;
    class DirectX12;

    struct RenderContext
    {
        DirectX12& Graphics; //描画命令を発行するDirectX12基盤
        Camera& ViewCamera; //現在の描画パスで使用するカメラ
    };
}
