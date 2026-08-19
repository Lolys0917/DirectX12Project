//|| MainScene.h ||:::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Engine起動時のMain SceneとDemoModel構成を定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.10  基底更新を維持するNative Main更新処理へ修正
//||  2026_07_13  v1.00  新規作成: Scene継承のMainSceneを追加
//||

#pragma once

#include <memory>

#include "Scene.h"

namespace Engine
{
    class DirectX12;

    class MainScene final : public Scene
    {
    public:
        // 未初期化のMainSceneを作成する
        MainScene();

        // Sceneが所有するComponentを基底Destructorで終了する
        ~MainScene() override;

        // MainプログラムとしてScene全体とデモObjectをネイティブAPIで更新する
        // deltaTime: 前フレームからの経過秒数
        void Update(float deltaTime) override;

    protected:
        // DemoModel ObjectとOBJModel ComponentをMainSceneへ追加する
        // dx12: OBJとTextureのGPU Resource作成に使用する描画基盤
        // 戻り値: DemoModelの構築と読み込みに成功した場合はtrue
        bool OnCreateSceneObjects(DirectX12& dx12) override;

        // Clone先としてMainScene型の未初期化Instanceを作成する
        // 戻り値: 新しいMainScene
        std::unique_ptr<Scene> CreateCloneInstance() const override;
    };
}
