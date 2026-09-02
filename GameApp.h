//|| GameApp.h ||:::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  GraphicBaseとSceneManagerを接続し、Engineの初期化、更新、描画及び
//||  描画領域変更を管理するApplicationクラスを定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v2.30  Native Main層向けDirectX12取得APIを追加
//||  2026_07_13  v2.20  編集: MainScene生成と状態設定をSceneManagerへ集約
//||  2026_07_13  v2.00  編集: 直接Object所有をSceneManager所有へ変更
//||                         ViewScene RenderTexture表示とResizeを追加
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include <Windows.h>
#include <cstdint>
#include <memory>

#include "GraphicBase.h"
#include "ImGuiLayer.h"
#include "SceneManager.h"

namespace Engine
{
    class GameApp final
    {
    public:
        // 未初期化のApplicationを作成する
        GameApp();

        // Sceneを先に終了してから描画基盤を破棄する
        ~GameApp();

        //Sceneと描画基盤の二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元Application
        GameApp(const GameApp&) = delete;

        //Sceneと描画基盤の二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元Application
        //戻り値: 代入先Applicationへの参照
        GameApp& operator=(const GameApp&) = delete;

        //内部参照の不整合を防ぐためMove構築を禁止する
        //引数: 移動元Application
        GameApp(GameApp&&) = delete;

        //内部参照の不整合を防ぐためMove代入を禁止する
        //引数: 移動元Application
        //戻り値: 代入先Applicationへの参照
        GameApp& operator=(GameApp&&) = delete;

        // 描画基盤を初期化しMainSceneの生成と状態設定をSceneManagerへ依頼する
        // hwnd: DirectX 12 SwapChainの表示対象Window Handle
        // width: 初期描画領域の横幅
        // height: 初期描画領域の縦幅
        // 戻り値: 全初期化に成功した場合はtrue
        bool Initialize(
            HWND hwnd,
            uint32_t width,
            uint32_t height
        );

        // Scene GPU Resourceを解放してからDirectX 12描画基盤を終了する
        void Finalize();

        // 全Active Sceneを更新する
        // deltaTime: 前Frameからの経過秒数
        void Update(float deltaTime);

        // 全Active SceneをRenderTextureへ描画しViewSceneをBackBufferへ転送する
        void Draw();

        // BackBufferと全SceneのCamera RenderTextureを同寸法へ変更する
        // width: 新しい描画領域の横幅
        // height: 新しい描画領域の縦幅
        // 戻り値: 全ResourceのResizeに成功した場合はtrue
        bool Resize(uint32_t width, uint32_t height);

        //再生開始時点のScene定義をStop復元用に一度だけ保存する
        bool CapturePlaybackState();

        //保存済みScene定義を初期化し、再生開始前の状態へ戻す
        bool RestorePlaybackState();

        // Applicationが所有するSceneManagerを取得する
        // 戻り値: SceneManagerへの参照
        SceneManager& GetSceneManager();
        const SceneManager& GetSceneManager() const;

        // Applicationが所有するDirectX 12描画基盤を取得する
        // 戻り値: Component初期化と描画に使用するDirectX12への参照
        DirectX12& GetDirectX12();
        const DirectX12& GetDirectX12() const;

        bool BeginImGuiFrame(float deltaTime);
        bool BeginImGuiWindow(const char* name);
        void EndImGuiWindow();
        void ImGuiText(const char* text);
        bool ImGuiButton(const char* label);
        bool BeginImGuiTabBar(const char* identifier);
        void EndImGuiTabBar();
        bool BeginImGuiTabItem(const char* label);
        void EndImGuiTabItem();
        bool ImGuiCollapsingHeader(const char* label, bool defaultOpen);
        void ImGuiSeparator();
        void ImGuiProgressBar(float fraction, const char* overlay);
        void ImGuiPlotLines(
            const char* label,
            const float* values,
            std::uint32_t valueCount,
            float minimum,
            float maximum
        );

    private:
        GraphicBase Graphics; // DirectX 12描画基盤の所有者
        ImGuiLayer ImmediateUI; //外部Mainから構築するDear ImGui描画層
        SceneManager Scenes; // 全SceneとViewScene状態の所有者
        std::unique_ptr<SceneManager> PlaybackSnapshot; //再生開始直前のCPU Scene定義
        HWND RenderWindow; //Game入力Focusを受け取る描画子Window
        bool Initialized; // Application全体の初期化完了状態
    };
}
