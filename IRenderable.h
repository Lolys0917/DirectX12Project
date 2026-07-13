//|| IRenderable.h ||:::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Componentを持たない基底Primitive Object用の描画契約を定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  RenderContext対応とGPU Resource終了契約を追加
//||  2026_07_13  v1.10  用途と宣言コメントを明確化
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

namespace Engine
{
    class DirectX12;
    struct RenderContext;

    class IRenderable
    {
    public:
        //派生描画Objectを基底Pointerから安全に破棄する
        virtual ~IRenderable() = default;

        //CPU MeshからGPU Resourceを作成する
        //引数: dx12 描画基盤
        //戻り値: Resource作成に成功した場合はtrue
        virtual bool CreateGPUResource(DirectX12& dx12) = 0;

        //描画Object固有の状態を更新する
        //引数: deltaTime 前回更新からの秒数
        virtual void Update(float deltaTime) = 0;

        //現在のCamera passへ描画する
        //引数: renderContext 描画基盤とCameraを持つContext
        virtual void Draw(const RenderContext& renderContext) = 0;

        //所有するGPU Resourceを解放する
        virtual void Finalize() = 0;
    };
}
