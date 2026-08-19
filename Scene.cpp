//|| Scene.cpp ||:::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  独立ObjectManager、必須Camera及びDebugGridの生成と、全Cameraの
//||  RenderTextureへComponentを描画する処理を実装する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.20  編集: 派生Scene固有Object構築Hookと動的型維持Cloneを追加
//||  2026_07_13  v1.10  編集: 必須要素、複製、Resize及びRollback失敗をログへ記録
//||  2026_07_13  v1.00  新規作成: Scene所有権と複数Camera描画を追加
//||

#include "Scene.h"

#include <DirectXMath.h>

#include <utility>
#include <vector>

#include "Camera.h"
#include "DirectX12.h"
#include "Grid.h"
#include "MessageLog.h"
#include "Object.h"
#include "RenderContext.h"
#include "RenderTexture.h"

namespace Engine
{
    // 未登録かつ未初期化のSceneを作成する
    Scene::Scene()
        : ID()
        , Name()
        , Objects(std::make_unique<ObjectManager>())
        , PrimaryCameraID()
        , MainCameraObjectID()
        , DebugGridObjectID()
        , Width(0)
        , Height(0)
        , Initialized(false)
    {
    }

    // 所有Componentを終了してSceneを破棄する
    Scene::~Scene()
    {
        Finalize();
    }

    // MainCamera ObjectとCamera Component及びDebugGrid ObjectとGridを生成する
    // dx12: 必須ComponentのGPU初期化に使用する描画基盤
    // width: Camera RenderTextureの初期横幅
    // height: Camera RenderTextureの初期縦幅
    // 戻り値: 必須Object生成とComponent初期化に成功した場合はtrue
    bool Scene::Initialize(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height
    )
    {
        if (Initialized)
        {
            return true;
        }

        if (!Objects || width == 0 || height == 0 || dx12.GetDevice() == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Initialize received an invalid ObjectManager, size, or device."
            );
            return false;
        }

        if (Objects->GetObjectCount() != 0 || Objects->GetComponentCount() != 0)
        {
            Objects->FinalizeComponents();
            Objects = std::make_unique<ObjectManager>();
        }

        Object* MainCameraObject =
            Objects->CreateObject<Object>("MainCamera"); // 必須Cameraを所有するObject

        if (MainCameraObject == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Required MainCamera Object creation failed."
            );
            Finalize();
            return false;
        }

        MainCameraObject->SetPosition(DirectX::XMFLOAT3(0.0f, 6.0f, -10.0f));

        Camera* MainCamera = Objects->AddComponent<Camera>(
            MainCameraObject->GetID(),
            "Camera",
            width,
            height
        ); // Scene生成時に必ず存在するPrimary Camera

        Object* DebugGridObject =
            Objects->CreateObject<Object>("DebugGrid"); // 必須Debug Gridを所有するObject

        if (MainCamera == nullptr || DebugGridObject == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Required Camera Component or DebugGrid Object creation failed."
            );
            Finalize();
            return false;
        }

        Grid* DebugGrid = Objects->AddComponent<Grid>(
            DebugGridObject->GetID(),
            "Grid"
        ); // Scene生成時に必ず存在するDebug Grid

        if (DebugGrid == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Required DebugGrid Component creation failed."
            );
            Finalize();
            return false;
        }

        if (!OnCreateSceneObjects(dx12))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Derived Scene Object construction failed."
            );
            Finalize();
            return false;
        }

        if (!Objects->InitializeComponents(dx12))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Scene Component initialization failed."
            );
            Finalize();
            return false;
        }

        MainCameraObjectID = MainCameraObject->GetID();
        DebugGridObjectID = DebugGridObject->GetID();
        PrimaryCameraID = MainCamera->GetID();
        Width = width;
        Height = height;
        Initialized = true;

        return true;
    }

    // GPU Resourceを持たない独立ObjectManagerへScene定義を複製して初期化する
    // dx12: 複製ComponentのGPU初期化に使用する描画基盤
    // 戻り値: 初期化済み複製Scene、失敗した場合はnullptr
    std::unique_ptr<Scene> Scene::Clone(DirectX12& dx12) const
    {
        return CloneInternal(&dx12);
    }

    //GPU ResourceとLifecycleを開始せず、現在のScene定義だけを複製する
    std::unique_ptr<Scene> Scene::CloneDefinition() const
    {
        return CloneInternal(nullptr);
    }

    //共通のScene定義複製を行い、描画基盤指定時だけComponentを初期化する
    std::unique_ptr<Scene> Scene::CloneInternal(DirectX12* dx12) const
    {
        if (!Objects || !Initialized)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Clone was requested from an uninitialized Scene."
            );
            return nullptr;
        }

        const Camera* SourcePrimaryCamera =
            GetPrimaryCamera(); // 複製元のPrimary Camera
        const Object* SourcePrimaryOwner = SourcePrimaryCamera == nullptr
            ? nullptr
            : SourcePrimaryCamera->GetOwner(); // Primary Cameraを所有する複製元Object

        if (SourcePrimaryCamera == nullptr || SourcePrimaryOwner == nullptr)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] Scene | Primary Camera ownership was inconsistent during Clone."
            );
            return nullptr;
        }

        const ObjectType PrimaryOwnerType =
            SourcePrimaryOwner->GetType(); // Primary Camera OwnerのObject種別
        const std::string PrimaryOwnerName =
            SourcePrimaryOwner->GetName(); // Primary Camera Ownerの解決済み名
        const std::string PrimaryCameraName =
            SourcePrimaryCamera->GetName(); // Primary Cameraの解決済み名

        std::unique_ptr<ObjectManager> ClonedObjects =
            Objects->CloneDefinition(); // IDとOwner関係を再構築した独立Manager

        if (!ClonedObjects)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | ObjectManager definition cloning failed."
            );
            return nullptr;
        }

        std::unique_ptr<Scene> ClonedScene =
            CreateCloneInstance(); // 動的Scene型を維持する未登録の複製Scene

        if (!ClonedScene)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Clone instance creation failed."
            );
            return nullptr;
        }

        ClonedScene->Objects = std::move(ClonedObjects);
        ClonedScene->Width = Width;
        ClonedScene->Height = Height;

        Object* ClonedMainCameraObject = ClonedScene->Objects->FindObject(
            ObjectType::Object,
            "MainCamera"
        ); // 新IDを持つ必須MainCamera Object
        Object* ClonedDebugGridObject = ClonedScene->Objects->FindObject(
            ObjectType::Object,
            "DebugGrid"
        ); // 新IDを持つ必須DebugGrid Object
        Object* ClonedPrimaryOwner = ClonedScene->Objects->FindObject(
            PrimaryOwnerType,
            PrimaryOwnerName
        ); // 新IDを持つPrimary Camera Owner
        Component* ClonedPrimaryComponent = ClonedPrimaryOwner == nullptr
            ? nullptr
            : ClonedScene->Objects->FindComponent(
                ClonedPrimaryOwner->GetID(),
                ComponentType::Camera,
                PrimaryCameraName
            ); // 新IDを持つPrimary Camera Component
        Camera* ClonedPrimaryCamera =
            dynamic_cast<Camera*>(ClonedPrimaryComponent); // 型を確認済みの複製Camera

        if (ClonedMainCameraObject == nullptr || ClonedDebugGridObject == nullptr ||
            ClonedPrimaryCamera == nullptr)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] Scene | Required Object or Primary Camera remapping failed during Clone."
            );
            return nullptr;
        }

        ClonedScene->MainCameraObjectID = ClonedMainCameraObject->GetID();
        ClonedScene->DebugGridObjectID = ClonedDebugGridObject->GetID();
        ClonedScene->PrimaryCameraID = ClonedPrimaryCamera->GetID();

        if (dx12 != nullptr && (!ClonedScene->Objects->InitializeComponents(*dx12) ||
            ClonedScene->GetPrimaryCamera() == nullptr))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Cloned Component GPU initialization failed."
            );
            return nullptr;
        }

        ClonedScene->Initialized = dx12 != nullptr;
        return ClonedScene;
    }

    //定義複製済みSceneのComponent LifecycleとGPU Resourceを開始する
    bool Scene::ActivateClonedDefinition(DirectX12& dx12)
    {
        if (Initialized)
        {
            return true;
        }

        if (!Objects || Width == 0 || Height == 0 ||
            !Objects->InitializeComponents(dx12) || GetPrimaryCamera() == nullptr)
        {
            Objects->FinalizeComponents();
            return false;
        }

        Initialized = true;
        return true;
    }

    // 派生Scene固有のObjectとComponentを必須Camera、Gridへ追加する
    // dx12: Scene固有ComponentのResource作成に使用する描画基盤
    // 戻り値: 基底Sceneには追加要素がないためtrue
    bool Scene::OnCreateSceneObjects(DirectX12& dx12)
    {
        (void)dx12;
        return true;
    }

    // Clone先として基底Scene型の未初期化Instanceを作成する
    // 戻り値: 新しい基底Scene
    std::unique_ptr<Scene> Scene::CreateCloneInstance() const
    {
        return std::make_unique<Scene>();
    }

    // 所有する全Componentを終了する
    void Scene::Finalize()
    {
        if (Objects)
        {
            Objects->FinalizeComponents();
        }

        PrimaryCameraID = ComponentID();
        MainCameraObjectID = ObjectID();
        DebugGridObjectID = ObjectID();
        Width = 0;
        Height = 0;
        Initialized = false;
    }

    // Sceneへ追加済みの未初期化Componentを初期化する
    // dx12: Component初期化に使用する描画基盤
    // 戻り値: 全Componentの初期化に成功した場合はtrue
    bool Scene::InitializePendingComponents(DirectX12& dx12)
    {
        return Objects != nullptr && Objects->InitializeComponents(dx12);
    }

    // Scene内の有効Object及びComponentを更新する
    // deltaTime: 前Frameからの経過秒数
    void Scene::Update(float deltaTime)
    {
        if (Initialized && Objects)
        {
            Objects->UpdateComponents(deltaTime);
        }
    }

    // 全Camera専用RenderTextureへScene内Componentを描画する
    // dx12: 描画命令を記録中のDirectX 12描画基盤
    // clearColor: 各Camera出力を消去するRGBA色
    void Scene::Render(
        DirectX12& dx12,
        const float clearColor[4]
    )
    {
        if (!Initialized || !Objects || !dx12.IsFrameOpen())
        {
            return;
        }

        const std::vector<Component*> Cameras =
            Objects->FindComponentsByType(ComponentType::Camera); // Scene内Camera一覧

        for (Component* CameraComponent : Cameras) // 各Cameraを独立RenderTextureへ描画する
        {
            Camera* ViewCamera =
                dynamic_cast<Camera*>(CameraComponent); // 今回の描画に使用するCamera
            Object* CameraOwner = ViewCamera == nullptr
                ? nullptr
                : ViewCamera->GetOwner(); // Cameraを所有するObject

            if (ViewCamera == nullptr || !ViewCamera->IsActive() ||
                !ViewCamera->IsInitialized() || CameraOwner == nullptr ||
                !CameraOwner->IsActive() || ViewCamera->GetRenderTexture() == nullptr)
            {
                continue;
            }

            ViewCamera->BeginRender(dx12, clearColor);

            RenderContext Context
            {
                dx12,
                *ViewCamera
            }; // Camera pass固有の描画Context

            Objects->DrawComponents(Context);
            ViewCamera->EndRender(dx12);
        }
    }

    // Scene内の全Camera RenderTextureを同じ寸法へ変更する
    // dx12: GPU待機とResource再生成に使用する描画基盤
    // width: 新しいRenderTexture横幅
    // height: 新しいRenderTexture縦幅
    // 戻り値: 全Cameraの変更に成功した場合はtrue
    bool Scene::Resize(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height
    )
    {
        if (!Initialized || !Objects || width == 0 || height == 0 ||
            dx12.IsFrameOpen())
        {
            MessageLog::GetInstance().AddLog(
                "[Error] Scene | Resize was rejected because the Scene, size, or frame state was invalid."
            );
            return false;
        }

        const uint32_t PreviousWidth = Width; //失敗時に戻す従来のCamera幅
        const uint32_t PreviousHeight = Height; //失敗時に戻す従来のCamera高さ
        std::vector<Camera*> ResizedCameras; //新しい寸法へ変更済みのCamera一覧
        const std::vector<Component*> Cameras =
            Objects->FindComponentsByType(ComponentType::Camera); // Resize対象Camera一覧

        ResizedCameras.reserve(Cameras.size());

        for (Component* CameraComponent : Cameras) // 全Camera出力を同じ寸法へ変更する
        {
            Camera* TargetCamera =
                dynamic_cast<Camera*>(CameraComponent); // 今回ResizeするCamera

            if (TargetCamera == nullptr || !TargetCamera->Resize(dx12, width, height))
            {
                for (Camera* ResizedCamera : ResizedCameras) //変更済み出力を従来寸法へ戻すCamera
                {
                    if (!ResizedCamera->Resize(dx12, PreviousWidth, PreviousHeight))
                    {
                        MessageLog::GetInstance().AddPermanentLog(
                            "[Critical] Scene | Camera RenderTexture Resize rollback failed."
                        );
                    }
                }

                MessageLog::GetInstance().AddLog(
                    "[Error] Scene | One Camera RenderTexture failed to Resize."
                );
                return false;
            }

            ResizedCameras.emplace_back(TargetCamera);
        }

        Width = width;
        Height = height;
        return true;
    }

    // Scene出力に使用するPrimary Cameraを変更する
    // componentID: このScene内のCamera Component ID
    // 戻り値: 有効なCameraへ変更できた場合はtrue
    bool Scene::SetPrimaryCamera(ComponentID componentID)
    {
        if (!Objects)
        {
            return false;
        }

        Component* TargetComponent =
            Objects->FindComponent(componentID); // Primary候補Component

        if (dynamic_cast<Camera*>(TargetComponent) == nullptr)
        {
            return false;
        }

        PrimaryCameraID = componentID;
        return true;
    }

    // SceneローカルのObjectManagerを取得する
    // 戻り値: このSceneだけが所有するObjectManagerへの参照
    ObjectManager& Scene::GetObjectManager()
    {
        return *Objects;
    }

    // SceneローカルのObjectManagerを取得する
    // 戻り値: このSceneだけが所有するObjectManagerへの参照
    const ObjectManager& Scene::GetObjectManager() const
    {
        return *Objects;
    }

    // SceneManagerから割り当てられたSceneIDを取得する
    // 戻り値: 登録済みSceneID、未登録の場合は無効SceneID
    SceneID Scene::GetID() const
    {
        return ID;
    }

    // SceneManager内で一意なScene名を取得する
    // 戻り値: 解決済みScene名
    const std::string& Scene::GetName() const
    {
        return Name;
    }

    // Primary Camera Component IDを取得する
    // 戻り値: Primary Camera ID、未設定の場合は無効ComponentID
    ComponentID Scene::GetPrimaryCameraID() const
    {
        return PrimaryCameraID;
    }

    // Primary Cameraを取得する
    // 戻り値: Primary CameraへのPointer、未設定の場合はnullptr
    Camera* Scene::GetPrimaryCamera()
    {
        return Objects == nullptr
            ? nullptr
            : dynamic_cast<Camera*>(Objects->FindComponent(PrimaryCameraID));
    }

    // Primary Cameraを取得する
    // 戻り値: Primary CameraへのPointer、未設定の場合はnullptr
    const Camera* Scene::GetPrimaryCamera() const
    {
        return Objects == nullptr
            ? nullptr
            : dynamic_cast<const Camera*>(Objects->FindComponent(PrimaryCameraID));
    }

    // Scene出力となるPrimary Camera RenderTextureを取得する
    // 戻り値: Scene出力RenderTexture、未初期化の場合はnullptr
    RenderTexture* Scene::GetRenderTexture()
    {
        Camera* PrimaryCamera = GetPrimaryCamera(); // Scene出力に使用するCamera
        return PrimaryCamera == nullptr ? nullptr : PrimaryCamera->GetRenderTexture();
    }

    // Scene出力となるPrimary Camera RenderTextureを取得する
    // 戻り値: Scene出力RenderTexture、未初期化の場合はnullptr
    const RenderTexture* Scene::GetRenderTexture() const
    {
        const Camera* PrimaryCamera = GetPrimaryCamera(); // Scene出力に使用するCamera
        return PrimaryCamera == nullptr ? nullptr : PrimaryCamera->GetRenderTexture();
    }

    // Sceneの必須Component初期化状態を取得する
    // 戻り値: Initialize成功後かつFinalize前の場合はtrue
    bool Scene::IsInitialized() const
    {
        return Initialized;
    }

    // SceneManagerから強いSceneIDと解決済み名を設定する
    // sceneID: 割り当てるSceneID
    // resolvedName: Manager内で一意に解決済みのScene名
    void Scene::AssignRegistration(
        SceneID sceneID,
        const std::string& resolvedName
    )
    {
        ID = sceneID;
        Name = resolvedName;
    }
}
