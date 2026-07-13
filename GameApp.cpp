//|| GameApp.cpp ||:::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  GraphicBaseとSceneManagerを接続し、MainScene生成、ViewScene描画及び
//||  描画領域変更を管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.20  編集: DemoModel生成をMainSceneへ移しManagerへ初期化を集約
//||  2026_07_13  v2.10  編集: 初期化、描画、Resizeの異常終了をMessageLogへ記録
//||  2026_07_13  v2.00  編集: 直接Object所有をSceneManager所有へ変更
//||                         ViewScene RenderTexture表示とResizeを追加
//||  2026_06_01  v1.00  新規作成
//||

#include "GameApp.h"

#include "DirectX12.h"
#include "MainScene.h"
#include "MessageLog.h"
#include "RenderTexture.h"

namespace Engine
{
    // 未初期化のApplicationを作成する
    GameApp::GameApp()
        : Graphics()
        , Scenes()
        , Initialized(false)
    {
    }

    // Sceneを先に終了してから描画基盤を破棄する
    GameApp::~GameApp()
    {
        Finalize();
    }

    // 描画基盤を初期化しMainSceneの生成と状態設定をSceneManagerへ依頼する
    // hwnd: DirectX 12 SwapChainの表示対象Window Handle
    // width: 初期描画領域の横幅
    // height: 初期描画領域の縦幅
    // 戻り値: 全初期化に成功した場合はtrue
    bool GameApp::Initialize(
        HWND hwnd,
        uint32_t width,
        uint32_t height
    )
    {
        if (Initialized)
        {
            return true;
        }

        if (hwnd == nullptr || width == 0 || height == 0)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] GameApp | Initialize received an invalid render window or size."
            );
            return false;
        }

        if (!Graphics.Initialize(hwnd, width, height))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] GameApp | DirectX 12 graphics initialization failed."
            );
            return false;
        }

        DirectX12& GraphicsDevice =
            Graphics.GetDirectX12(); // SceneとComponentが共有する描画基盤
        const SceneID MainSceneID = Scenes.CreateMainScene<MainScene>(
            GraphicsDevice,
            "MainScene",
            width,
            height
        ); // Engine起動時にActive、Main、Viewへ設定する派生Scene

        if (!MainSceneID.IsValid() ||
            Scenes.GetMainSceneID() != MainSceneID ||
            Scenes.GetViewSceneID() != MainSceneID ||
            !Scenes.IsActive(MainSceneID))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] GameApp | SceneManager could not create and activate MainScene."
            );
            Finalize();
            return false;
        }

        Initialized = true;
        MessageLog::GetInstance().AddLog(
            "[Info] GameApp | SceneManager initialized the active MainScene."
        );
        return true;
    }

    // Scene GPU Resourceを解放してからDirectX 12描画基盤を終了する
    void GameApp::Finalize()
    {
        Scenes.Finalize();
        Graphics.Finalize();
        Initialized = false;
    }

    // 全Active Sceneを更新する
    // deltaTime: 前Frameからの経過秒数
    void GameApp::Update(float deltaTime)
    {
        if (!Initialized)
        {
            return;
        }
        Scenes.UpdateActiveScenes(deltaTime);
    }

    // 全Active SceneをRenderTextureへ描画しViewSceneをBackBufferへ転送する
    void GameApp::Draw()
    {
        if (!Initialized)
        {
            return;
        }

        DirectX12& GraphicsDevice =
            Graphics.GetDirectX12(); // Frame描画に使用するDirectX 12基盤
        static constexpr float ClearColor[4] =
        {
            0.85f,
            0.85f,
            0.85f,
            1.0f
        }; // BackBufferと各Camera RenderTextureの消去色

        GraphicsDevice.BeginFrame(ClearColor);

        if (!GraphicsDevice.IsFrameOpen())
        {
            MessageLog::GetInstance().AddLog(
                "[Error] GameApp | Draw was skipped because BeginFrame failed."
            );
            return;
        }

        Scenes.RenderActiveScenes(GraphicsDevice, ClearColor);

        RenderTexture* ViewTexture =
            Scenes.GetViewRenderTexture(); // 常時画面へ表示するViewScene出力

        if (ViewTexture == nullptr || !ViewTexture->CopyToBackBuffer(GraphicsDevice))
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] GameApp | ViewScene texture was unavailable; the back buffer was used instead."
            );
            GraphicsDevice.BindBackBuffer();
        }

        GraphicsDevice.EndFrame();
    }

    // BackBufferと全SceneのCamera RenderTextureを同寸法へ変更する
    // width: 新しい描画領域の横幅
    // height: 新しい描画領域の縦幅
    // 戻り値: 全ResourceのResizeに成功した場合はtrue
    bool GameApp::Resize(uint32_t width, uint32_t height)
    {
        if (!Initialized || width == 0 || height == 0)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] GameApp | Resize was rejected because the application or size was invalid."
            );
            return false;
        }

        DirectX12& GraphicsDevice =
            Graphics.GetDirectX12(); // Resizeする描画基盤

        if (GraphicsDevice.IsFrameOpen() || !Graphics.Resize(width, height))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] GameApp | Swap chain Resize failed or was requested during an open frame."
            );
            return false;
        }

        if (!Scenes.ResizeActiveScenes(GraphicsDevice, width, height))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] GameApp | Scene camera RenderTexture Resize failed."
            );
            return false;
        }

        MessageLog::GetInstance().AddLog(
            "[Info] GameApp | Render viewport resources were resized."
        );
        return true;
    }

    // Applicationが所有するSceneManagerを取得する
    // 戻り値: SceneManagerへの参照
    SceneManager& GameApp::GetSceneManager()
    {
        return Scenes;
    }

    // Applicationが所有するSceneManagerを取得する
    // 戻り値: SceneManagerへの参照
    const SceneManager& GameApp::GetSceneManager() const
    {
        return Scenes;
    }
}
