//|| Scene.h ||:::::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  独立ObjectManager、必須Camera及びDebugGridを所有し、全Cameraの
//||  RenderTextureへScene内Componentを描画するSceneを定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.20  編集: 派生Scene用のObject構築Hookと派生型Cloneを追加
//||  2026_07_13  v1.00  新規作成: Scene所有権と複数Camera描画を追加
//||

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "EntityTypes.h"
#include "ObjectManager.h"

namespace Engine
{
    class Camera;
    class DirectX12;
    class RenderTexture;
    class SceneManager;

    class Scene
    {
    public:
        // 未登録かつ未初期化のSceneを作成する
        Scene();

        // 所有Componentを終了してSceneを破棄する
        virtual ~Scene();

        //Scene固有IDと所有Managerの重複を防ぐためCopy構築を禁止する
        //引数: コピー元Scene
        Scene(const Scene&) = delete;

        //Scene固有IDと所有Managerの重複を防ぐためCopy代入を禁止する
        //引数: コピー元Scene
        //戻り値: 代入先Sceneへの参照
        Scene& operator=(const Scene&) = delete;

        //ComponentからOwnerへの参照を保つためMove構築を禁止する
        //引数: 移動元Scene
        Scene(Scene&&) = delete;

        //ComponentからOwnerへの参照を保つためMove代入を禁止する
        //引数: 移動元Scene
        //戻り値: 代入先Sceneへの参照
        Scene& operator=(Scene&&) = delete;

        // MainCamera ObjectとCamera Component及びDebugGrid ObjectとGridを生成する
        // dx12: 必須ComponentのGPU初期化に使用する描画基盤
        // width: Camera RenderTextureの初期横幅
        // height: Camera RenderTextureの初期縦幅
        // 戻り値: 必須Object生成とComponent初期化に成功した場合はtrue
        bool Initialize(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height
        );

        // GPU Resourceを持たない独立ObjectManagerへScene定義を複製して初期化する
        // dx12: 複製ComponentのGPU初期化に使用する描画基盤
        // 戻り値: 初期化済み複製Scene、失敗した場合はnullptr
        std::unique_ptr<Scene> Clone(DirectX12& dx12) const;

        // 所有する全Componentを終了する
        void Finalize();

        // Sceneへ追加済みの未初期化Componentを初期化する
        // dx12: Component初期化に使用する描画基盤
        // 戻り値: 全Componentの初期化に成功した場合はtrue
        bool InitializePendingComponents(DirectX12& dx12);

        // Scene内の有効Object及びComponentを更新する
        // deltaTime: 前Frameからの経過秒数
        virtual void Update(float deltaTime);

        // 全Camera専用RenderTextureへScene内Componentを描画する
        // dx12: 描画命令を記録中のDirectX 12描画基盤
        // clearColor: 各Camera出力を消去するRGBA色
        void Render(
            DirectX12& dx12,
            const float clearColor[4]
        );

        // Scene内の全Camera RenderTextureを同じ寸法へ変更する
        // dx12: GPU待機とResource再生成に使用する描画基盤
        // width: 新しいRenderTexture横幅
        // height: 新しいRenderTexture縦幅
        // 戻り値: 全Cameraの変更に成功した場合はtrue
        bool Resize(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height
        );

        // Scene出力に使用するPrimary Cameraを変更する
        // componentID: このScene内のCamera Component ID
        // 戻り値: 有効なCameraへ変更できた場合はtrue
        bool SetPrimaryCamera(ComponentID componentID);

        // SceneローカルのObjectManagerを取得する
        // 戻り値: このSceneだけが所有するObjectManagerへの参照
        ObjectManager& GetObjectManager();
        const ObjectManager& GetObjectManager() const;

        // SceneManagerから割り当てられたSceneIDを取得する
        // 戻り値: 登録済みSceneID、未登録の場合は無効SceneID
        SceneID GetID() const;

        // SceneManager内で一意なScene名を取得する
        // 戻り値: 解決済みScene名
        const std::string& GetName() const;

        // Primary Camera Component IDを取得する
        // 戻り値: Primary Camera ID、未設定の場合は無効ComponentID
        ComponentID GetPrimaryCameraID() const;

        // Primary Cameraを取得する
        // 戻り値: Primary CameraへのPointer、未設定の場合はnullptr
        Camera* GetPrimaryCamera();
        const Camera* GetPrimaryCamera() const;

        // Scene出力となるPrimary Camera RenderTextureを取得する
        // 戻り値: Scene出力RenderTexture、未初期化の場合はnullptr
        RenderTexture* GetRenderTexture();
        const RenderTexture* GetRenderTexture() const;

        // Sceneの必須Component初期化状態を取得する
        // 戻り値: Initialize成功後かつFinalize前の場合はtrue
        bool IsInitialized() const;

    protected:
        // 派生Scene固有のObjectとComponentを必須Camera、Gridへ追加する
        // dx12: Scene固有ComponentのResource作成に使用する描画基盤
        // 戻り値: Scene固有要素の構築に成功した場合はtrue
        virtual bool OnCreateSceneObjects(DirectX12& dx12);

        // Clone先として同じ動的Scene型の未初期化Instanceを作成する
        // 戻り値: 派生型を維持した空Scene、作成失敗時はnullptr
        virtual std::unique_ptr<Scene> CreateCloneInstance() const;

    private:
        friend class SceneManager;

        // SceneManagerから強いSceneIDと解決済み名を設定する
        // sceneID: 割り当てるSceneID
        // resolvedName: Manager内で一意に解決済みのScene名
        void AssignRegistration(
            SceneID sceneID,
            const std::string& resolvedName
        );

    private:
        SceneID ID; // SceneManager内で一意かつ再利用しないSceneID
        std::string Name; // SceneManager内で一意な解決済みScene名
        std::unique_ptr<ObjectManager> Objects; // Sceneごとに独立して所有するObjectManager
        ComponentID PrimaryCameraID; // Scene出力に使用するCamera Component ID
        ObjectID MainCameraObjectID; // 必須MainCamera Object ID
        ObjectID DebugGridObjectID; // 必須DebugGrid Object ID
        uint32_t Width; // Camera RenderTextureの現在横幅
        uint32_t Height; // Camera RenderTextureの現在縦幅
        bool Initialized; // 必須ObjectとComponentの初期化完了状態
    };
}
