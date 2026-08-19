//|| EngineAPI.cpp ||::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Native Main層、Script層、Editor操作を既存Engine APIへ接続する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#include "EngineAPI.h"

#include <limits>

#include "Box.h"
#include "Camera.h"
#include "Capsule.h"
#include "Component.h"
#include "Cylinder.h"
#include "DirectX12.h"
#include "GameApp.h"
#include "GameObjectTemplate.h"
#include "HalfSphere.h"
#include "MessageLog.h"
#include "Object.h"
#include "ObjectManager.h"
#include "Plane.h"
#include "PrimitiveObject.h"
#include "ProgramSuggestionRegistry.h"
#include "RotationScript.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Script.h"
#include "Sphere.h"

namespace Engine
{
    namespace
    {
        //概要：Object種別をエディター表示用文字列へ変換する
        //引数：type=変換するObject種別
        //戻り値：Object種別のUTF-8表示名
        const char* GetObjectTypeName(ObjectType type)
        {
            switch (type)
            {
            case ObjectType::Object: return "Object";
            case ObjectType::Box: return "Box";
            case ObjectType::Sphere: return "Sphere";
            case ObjectType::Plane: return "Plane";
            case ObjectType::Cylinder: return "Cylinder";
            case ObjectType::HalfSphere: return "HalfSphere";
            case ObjectType::Capsule: return "Capsule";
            case ObjectType::SkyBox: return "SkyBox";
            default: return "Unknown";
            }
        }

        //概要：Component種別をエディター表示用文字列へ変換する
        //引数：type=変換するComponent種別
        //戻り値：Component種別のUTF-8表示名
        const char* GetComponentTypeName(ComponentType type)
        {
            switch (type)
            {
            case ComponentType::Component: return "Component";
            case ComponentType::Mesh: return "Mesh";
            case ComponentType::Polygon: return "Polygon";
            case ComponentType::Model: return "Model";
            case ComponentType::Camera: return "Camera";
            case ComponentType::Grid: return "Grid";
            case ComponentType::Collider: return "Collider";
            case ComponentType::BoxCollider: return "Box Collider";
            case ComponentType::SphereCollider: return "Sphere Collider";
            case ComponentType::CapsuleCollider: return "Capsule Collider";
            case ComponentType::CylinderCollider: return "Cylinder Collider";
            case ComponentType::PlaneCollider: return "Plane Collider";
            case ComponentType::Script: return "Script";
            default: return "Unknown";
            }
        }
    }

    //概要：GameApp全機能とScript Sub Systemを束ねるNative APIを作成する
    //引数：application=初期化済みEngine Application
    //戻り値：なし
    EngineAPI::EngineAPI(GameApp& application)
        : Application(application)
        , Scripts()
        , Modules(Scripts)
        , Extensions(*this)
        , Revision(1)
    {
        Scripts.RegisterNativeScript<RotationScript>(
            "native.rotation",
            "Rotation Script"
        );
    }

    //概要：Script Moduleを先に解放してNative API Facadeを破棄する
    //引数：なし
    //戻り値：なし
    EngineAPI::~EngineAPI() = default;

    //概要：Facadeが操作するApplicationを取得する
    //引数：なし
    //戻り値：GameAppへの参照
    GameApp& EngineAPI::GetApplication()
    {
        return Application;
    }

    //概要：Facadeが操作するApplicationを読み取り専用で取得する
    //引数：なし
    //戻り値：読み取り専用GameAppへの参照
    const GameApp& EngineAPI::GetApplication() const
    {
        return Application;
    }

    //概要：全DirectX 12 APIへ到達できる描画基盤を取得する
    //引数：なし
    //戻り値：DirectX12への参照
    DirectX12& EngineAPI::GetDirectX12()
    {
        return Application.GetDirectX12();
    }

    //概要：描画基盤を読み取り専用で取得する
    //引数：なし
    //戻り値：読み取り専用DirectX12への参照
    const DirectX12& EngineAPI::GetDirectX12() const
    {
        return Application.GetDirectX12();
    }

    //概要：全Scene APIへ到達できるManagerを取得する
    //引数：なし
    //戻り値：SceneManagerへの参照
    SceneManager& EngineAPI::GetSceneManager()
    {
        return Application.GetSceneManager();
    }

    //概要：Scene Managerを読み取り専用で取得する
    //引数：なし
    //戻り値：読み取り専用SceneManagerへの参照
    const SceneManager& EngineAPI::GetSceneManager() const
    {
        return Application.GetSceneManager();
    }

    //概要：Native及びDLL Script Factory Registryを取得する
    //引数：なし
    //戻り値：ScriptRegistryへの参照
    ScriptRegistry& EngineAPI::GetScriptRegistry()
    {
        return Scripts;
    }

    //概要：Script Factory Registryを読み取り専用で取得する
    //引数：なし
    //戻り値：読み取り専用ScriptRegistryへの参照
    const ScriptRegistry& EngineAPI::GetScriptRegistry() const
    {
        return Scripts;
    }

    //概要：DLL Script Module Managerを取得する
    //引数：なし
    //戻り値：ScriptModuleManagerへの参照
    ScriptModuleManager& EngineAPI::GetScriptModuleManager()
    {
        return Modules;
    }

    //概要：DLL Script Module Managerを読み取り専用で取得する
    //引数：なし
    //戻り値：読み取り専用ScriptModuleManagerへの参照
    const ScriptModuleManager& EngineAPI::GetScriptModuleManager() const
    {
        return Modules;
    }

    //概要：Program DLL Hot Reloadと外部APIを管理するModule Managerを取得する
    //引数：なし
    //戻り値：ExtensionModuleManagerへの参照
    ExtensionModuleManager& EngineAPI::GetExtensionModuleManager()
    {
        return Extensions;
    }

    //概要：Program DLL Module Managerを読み取り専用で取得する
    //引数：なし
    //戻り値：読み取り専用ExtensionModuleManagerへの参照
    const ExtensionModuleManager& EngineAPI::GetExtensionModuleManager() const
    {
        return Extensions;
    }

    //概要：Engineが現在所有する全Scene IDを登録順で取得する
    //引数：なし
    //戻り値：有効Scene ID一覧
    std::vector<SceneID> EngineAPI::GetSceneIDs() const
    {
        return GetSceneManager().GetSceneIDs();
    }

    //概要：指定Sceneが所有する全Object IDを登録順で取得する
    //引数：sceneID=読み取るScene
    //戻り値：有効Object ID一覧、Sceneが存在しない場合は空
    std::vector<ObjectID> EngineAPI::GetObjectIDs(SceneID sceneID) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //列挙対象Scene
        return TargetScene == nullptr
            ? std::vector<ObjectID>()
            : TargetScene->GetObjectManager().GetObjectIDs();
    }

    //概要：指定Objectが直接所有するChild Object IDを登録順で取得する
    //引数：sceneID=対象Scene、objectID=親Object
    //戻り値：直接Child ID一覧、対象が存在しない場合は空
    std::vector<ObjectID> EngineAPI::GetChildObjectIDs(
        SceneID sceneID,
        ObjectID objectID
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //親Objectを所有するScene
        const Object* Parent = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //列挙対象の親Object
        return Parent == nullptr ? std::vector<ObjectID>() : Parent->GetChildIDs();
    }

    //概要：指定Objectが所有する全Component IDを登録順で取得する
    //引数：sceneID=対象Scene、objectID=対象Object
    //戻り値：有効Component ID一覧、対象が存在しない場合は空
    std::vector<ComponentID> EngineAPI::GetComponentIDs(
        SceneID sceneID,
        ObjectID objectID
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //列挙対象Scene
        return TargetScene == nullptr
            ? std::vector<ComponentID>()
            : TargetScene->GetObjectManager().GetComponentIDs(objectID);
    }

    //概要：解決済みScene名から安定Scene IDを検索する
    //引数：name=検索するUTF-8 Scene名
    //戻り値：一致したScene ID、存在しない場合は無効ID
    SceneID EngineAPI::FindSceneID(const std::string& name) const
    {
        const Scene* TargetScene = GetSceneManager().FindScene(name); //名前が一致したScene
        return TargetScene == nullptr ? SceneID() : TargetScene->GetID();
    }

    //概要：Scene内のObject型と解決済み名から安定Object IDを検索する
    //引数：sceneID=対象Scene、objectType=Object具象型、name=検索するUTF-8名
    //戻り値：一致したObject ID、存在しない場合は無効ID
    ObjectID EngineAPI::FindObjectID(
        SceneID sceneID,
        ObjectType objectType,
        const std::string& name
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //検索対象Scene
        const Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectType, name); //型と名前が一致したObject
        return TargetObject == nullptr ? ObjectID() : TargetObject->GetID();
    }

    //概要：指定Sceneの名前と実行状態を内部Tool用情報へ読み取る
    //引数：sceneID=対象Scene、information=読み取り結果の格納先
    //戻り値：Sceneが存在して情報を取得できた場合はtrue
    bool EngineAPI::TryGetSceneInfo(
        SceneID sceneID,
        EditorSceneInfo& information
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //情報を読み取るScene

        if (TargetScene == nullptr)
        {
            return false;
        }

        information = EditorSceneInfo{};
        information.ID = sceneID;
        information.Name = TargetScene->GetName();
        information.Active = GetSceneManager().IsActive(sceneID);
        information.ViewScene = GetSceneManager().GetViewSceneID() == sceneID;
        return true;
    }

    //概要：指定Objectの名前、親、Transform、Component一覧を内部Tool用情報へ読み取る
    //引数：sceneID=対象Scene、objectID=対象Object、information=読み取り結果の格納先
    //戻り値：Objectが存在して情報を取得できた場合はtrue
    bool EngineAPI::TryGetObjectInfo(
        SceneID sceneID,
        ObjectID objectID,
        EditorObjectInfo& information
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //Objectを所有するScene
        const Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //読み取り対象Object

        if (TargetObject == nullptr)
        {
            return false;
        }

        information = EditorObjectInfo{};
        information.ID = objectID;
        information.Type = TargetObject->GetType();
        information.Name = TargetObject->GetName();
        information.Active = TargetObject->IsActive();
        information.ParentID = TargetObject->GetParentID();
        const DirectX::XMFLOAT3& Position = TargetObject->GetPosition(); //Local座標
        const DirectX::XMFLOAT3& Rotation = TargetObject->GetRotation(); //Local回転
        const DirectX::XMFLOAT3& Scale = TargetObject->GetScale(); //Local拡縮
        information.LocalTransform.Position = { Position.x, Position.y, Position.z };
        information.LocalTransform.Rotation = { Rotation.x, Rotation.y, Rotation.z };
        information.LocalTransform.Scale = { Scale.x, Scale.y, Scale.z };

        for (ComponentID ID : TargetObject->GetComponentIDs())
        {
            EditorComponentInfo ComponentInformation; //所有Componentの内部Tool情報

            if (TryGetComponentInfo(sceneID, ID, ComponentInformation))
            {
                information.Components.emplace_back(std::move(ComponentInformation));
            }
        }

        return true;
    }

    //概要：指定Componentの名前、型、有効状態を内部Tool用情報へ読み取る
    //引数：sceneID=対象Scene、componentID=対象Component、information=読み取り結果の格納先
    //戻り値：Componentが存在して情報を取得できた場合はtrue
    bool EngineAPI::TryGetComponentInfo(
        SceneID sceneID,
        ComponentID componentID,
        EditorComponentInfo& information
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //Componentを所有するScene
        const Component* TargetComponent = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindComponent(componentID); //読み取り対象Component

        if (TargetComponent == nullptr)
        {
            return false;
        }

        const Script* ScriptComponent = dynamic_cast<const Script*>(TargetComponent); //Script表示判定
        information = EditorComponentInfo{};
        information.ID = componentID;
        information.Name = TargetComponent->GetName();
        information.TypeName = ScriptComponent == nullptr
            ? GetComponentTypeName(TargetComponent->GetType())
            : ScriptComponent->GetDisplayName();
        information.Active = TargetComponent->IsActive();
        information.Script = ScriptComponent != nullptr;
        return true;
    }

    //概要：指定Sceneへ選択した具象型のObjectを作成し必要なGPU資源を初期化する
    //引数：sceneID=作成先Scene、objectType=具象Object型、requestedName=希望名、parentID=新しい親又は無効ID
    //戻り値：登録済みObject、作成又は初期化失敗時はnullptr
    Object* EngineAPI::CreateObject(
        SceneID sceneID,
        ObjectType objectType,
        const std::string& requestedName,
        ObjectID parentID
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Objectを追加するScene

        if (TargetScene == nullptr)
        {
            return nullptr;
        }

        ObjectManager& Objects = TargetScene->GetObjectManager(); //作成先Object Manager
        Object* CreatedObject = nullptr; //具象型を切り替えて作成したObject
        const std::string Name = requestedName.empty()
            ? GetObjectTypeName(objectType)
            : requestedName; //空名を補完した希望名

        switch (objectType)
        {
        case ObjectType::Object: CreatedObject = Objects.CreateObject<Object>(Name); break;
        case ObjectType::Box: CreatedObject = Objects.CreateObject<Box>(Name); break;
        case ObjectType::Sphere: CreatedObject = Objects.CreateObject<Sphere>(Name); break;
        case ObjectType::Plane: CreatedObject = Objects.CreateObject<Plane>(Name); break;
        case ObjectType::Cylinder: CreatedObject = Objects.CreateObject<Cylinder>(Name); break;
        case ObjectType::HalfSphere: CreatedObject = Objects.CreateObject<HalfSphere>(Name); break;
        case ObjectType::Capsule: CreatedObject = Objects.CreateObject<Capsule>(Name); break;
        default: return nullptr;
        }

        if (CreatedObject == nullptr ||
            (parentID.IsValid() && !Objects.SetParent(
                CreatedObject->GetID(),
                parentID,
                false
            )) ||
            !TargetScene->InitializePendingComponents(GetDirectX12()))
        {
            if (CreatedObject != nullptr)
            {
                Objects.RemoveObject(CreatedObject->GetID());
            }

            return nullptr;
        }

        IncrementRevision();
        return CreatedObject;
    }

    //概要：ゲーム固有値を持つ複製可能なObject雛形を指定Sceneへ作成する
    //引数：sceneID=作成先Scene、requestedName=希望名、parentID=親又は無効ID
    //戻り値：登録済みGameObjectTemplate、作成失敗時はnullptr
    GameObjectTemplate* EngineAPI::CreateGameObjectTemplate(
        SceneID sceneID,
        const std::string& requestedName,
        ObjectID parentID
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //雛形Objectを追加するScene

        if (TargetScene == nullptr)
        {
            return nullptr;
        }

        ObjectManager& Objects = TargetScene->GetObjectManager(); //作成先Object Manager
        GameObjectTemplate* Created = Objects.CreateObject<GameObjectTemplate>(
            requestedName.empty() ? "GameObjectTemplate" : requestedName
        ); //登録済み雛形Object

        if (Created == nullptr ||
            (parentID.IsValid() && !Objects.SetParent(Created->GetID(), parentID, false)) ||
            !TargetScene->InitializePendingComponents(GetDirectX12()))
        {
            if (Created != nullptr)
            {
                Objects.RemoveObject(Created->GetID());
            }

            return nullptr;
        }

        IncrementRevision();
        return Created;
    }

    //概要：指定IDがGameObjectTemplateの場合に編集可能Pointerを取得する
    //引数：sceneID=対象Scene、objectID=対象Object
    //戻り値：GameObjectTemplate、型不一致又は未登録時はnullptr
    GameObjectTemplate* EngineAPI::FindGameObjectTemplate(
        SceneID sceneID,
        ObjectID objectID
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //検索対象Scene
        Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //型判定するObject
        return dynamic_cast<GameObjectTemplate*>(TargetObject);
    }

    //概要：指定IDがGameObjectTemplateの場合に読み取り専用Pointerを取得する
    //引数：sceneID=対象Scene、objectID=対象Object
    //戻り値：読み取り専用GameObjectTemplate、型不一致又は未登録時はnullptr
    const GameObjectTemplate* EngineAPI::FindGameObjectTemplate(
        SceneID sceneID,
        ObjectID objectID
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //検索対象Scene
        const Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //型判定するObject
        return dynamic_cast<const GameObjectTemplate*>(TargetObject);
    }

    //概要：GameObjectTemplateのゲーム用Tag、移動速度、最大体力を一括変更する
    //引数：sceneID=対象Scene、objectID=対象Object、gameplayTag=用途Tag、moveSpeed=移動速度、maximumHealth=最大体力
    //戻り値：対象型へ設定できた場合はtrue
    bool EngineAPI::SetGameObjectTemplateInfo(
        SceneID sceneID,
        ObjectID objectID,
        const std::string& gameplayTag,
        float moveSpeed,
        float maximumHealth
    )
    {
        GameObjectTemplate* Target = FindGameObjectTemplate(sceneID, objectID); //設定対象雛形

        if (Target == nullptr)
        {
            return false;
        }

        Target->SetGameplayTag(gameplayTag);
        Target->SetMoveSpeed(moveSpeed);
        Target->SetMaximumHealth(maximumHealth);
        IncrementRevision();
        return true;
    }

    //概要：標準Sceneを作成して内部Toolと外部Programへ新しいIDを返す
    //引数：name=希望Scene名、width=Camera横幅、height=Camera縦幅
    //戻り値：作成したScene ID、失敗時は無効ID
    SceneID EngineAPI::CreateScene(
        const std::string& name,
        std::uint32_t width,
        std::uint32_t height
    )
    {
        if (width == 0 || height == 0)
        {
            return SceneID();
        }

        const SceneID Created = GetSceneManager().CreateScene(
            GetDirectX12(),
            name.empty() ? "Scene" : name,
            width,
            height
        ); //登録済みScene ID

        if (Created.IsValid())
        {
            IncrementRevision();
        }

        return Created;
    }

    //概要：既存SceneのObjectとComponent定義を独立Sceneへ複製する
    //引数：sourceSceneID=複製元Scene、name=複製先の希望名
    //戻り値：複製Scene ID、失敗時は無効ID
    SceneID EngineAPI::DuplicateScene(
        SceneID sourceSceneID,
        const std::string& name
    )
    {
        const Scene* Source = GetSceneManager().FindScene(sourceSceneID); //複製元Scene

        if (Source == nullptr)
        {
            return SceneID();
        }

        const SceneID Created = GetSceneManager().DuplicateScene(
            GetDirectX12(),
            sourceSceneID,
            name.empty() ? Source->GetName() + "_Copy" : name
        ); //登録済み複製Scene ID

        if (Created.IsValid())
        {
            IncrementRevision();
        }

        return Created;
    }

    //概要：Main又はViewではないSceneをEngineから安全に削除する
    //引数：sceneID=削除対象Scene
    //戻り値：Sceneを削除できた場合はtrue
    bool EngineAPI::RemoveScene(SceneID sceneID)
    {
        if (sceneID == GetMainSceneID() || sceneID == GetViewSceneID())
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] EngineAPI | Main or View Scene cannot be deleted."
            );
            return false;
        }

        const bool Succeeded = GetSceneManager().RemoveScene(sceneID); //Scene削除結果

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：指定SceneをMain、View、Activeへ一括設定する
    //引数：sceneID=新しいMain Scene
    //戻り値：Main Sceneを変更できた場合はtrue
    bool EngineAPI::SetMainScene(SceneID sceneID)
    {
        const bool Succeeded = GetSceneManager().SetMainScene(sceneID); //Main Scene変更結果

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：Engineの基準となるMain Scene IDを取得する
    //引数：なし
    //戻り値：Main Scene ID、未設定時は無効ID
    SceneID EngineAPI::GetMainSceneID() const
    {
        return GetSceneManager().GetMainSceneID();
    }

    //概要：現在画面へ表示しているView Scene IDを取得する
    //引数：なし
    //戻り値：View Scene ID、未設定時は無効ID
    SceneID EngineAPI::GetViewSceneID() const
    {
        return GetSceneManager().GetViewSceneID();
    }

    //概要：指定Sceneの更新と描画の有効状態を変更する
    //引数：sceneID=対象Scene、active=有効にする場合はtrue
    //戻り値：状態を設定できた場合はtrue
    bool EngineAPI::SetSceneActive(SceneID sceneID, bool active)
    {
        const bool Succeeded = GetSceneManager().SetActive(sceneID, active); //Scene状態変更結果

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：指定Sceneを画面表示対象へ変更する
    //引数：sceneID=新しいView Scene
    //戻り値：View Sceneを変更できた場合はtrue
    bool EngineAPI::SetViewScene(SceneID sceneID)
    {
        const bool Succeeded = GetSceneManager().SetViewScene(sceneID); //View Scene変更結果

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：指定Objectと所有ComponentをEngineから削除する
    //引数：sceneID=対象Scene、objectID=削除対象Object
    //戻り値：安全条件を満たして削除できた場合はtrue
    bool EngineAPI::RemoveObject(SceneID sceneID, ObjectID objectID)
    {
        return DeleteObject(sceneID, objectID);
    }

    //概要：指定Objectを同じ型内で一意な名前へ変更する
    //引数：sceneID=対象Scene、objectID=対象Object、name=希望名
    //戻り値：名前を変更できた場合はtrue
    bool EngineAPI::RenameObject(
        SceneID sceneID,
        ObjectID objectID,
        const std::string& name
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Objectを所有するScene
        const bool Succeeded = TargetScene != nullptr &&
            TargetScene->GetObjectManager().RenameObject(objectID, name); //名前変更結果

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：指定Objectの更新と描画の有効状態を変更する
    //引数：sceneID=対象Scene、objectID=対象Object、active=有効にする場合はtrue
    //戻り値：状態を変更できた場合はtrue
    bool EngineAPI::SetObjectActive(
        SceneID sceneID,
        ObjectID objectID,
        bool active
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Objectを所有するScene
        Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //状態変更対象Object

        if (TargetObject == nullptr)
        {
            return false;
        }

        TargetObject->SetActive(active);
        IncrementRevision();
        return true;
    }

    //概要：指定ObjectのLocal Transformを一括変更する
    //引数：sceneID=対象Scene、objectID=対象Object、transform=新しいLocal姿勢
    //戻り値：Transformを変更できた場合はtrue
    bool EngineAPI::SetObjectTransform(
        SceneID sceneID,
        ObjectID objectID,
        const EditorTransformInfo& transform
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Objectを所有するScene
        Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //姿勢変更対象Object

        if (TargetObject == nullptr)
        {
            return false;
        }

        TargetObject->SetPosition({
            transform.Position.X,
            transform.Position.Y,
            transform.Position.Z
        });
        TargetObject->SetRotation({
            transform.Rotation.X,
            transform.Rotation.Y,
            transform.Rotation.Z
        });
        TargetObject->SetScale({
            transform.Scale.X,
            transform.Scale.Y,
            transform.Scale.Z
        });
        return true;
    }

    //概要：Box等Primitive Objectの現在RGBA頂点色を読み取る
    //引数：sceneID=対象Scene、objectID=対象Object、color=読取結果の格納先
    //戻り値：対象がPrimitive Objectで色を取得できた場合はtrue
    bool EngineAPI::TryGetObjectColor(
        SceneID sceneID,
        ObjectID objectID,
        EditorColor& color
    ) const
    {
        const Scene* TargetScene = ResolveScene(sceneID); //対象Objectを所有するScene
        const Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //色取得対象Object
        const auto* Primitive = dynamic_cast<const PrimitiveObject*>(TargetObject); //色を所有するPrimitive

        if (Primitive == nullptr)
        {
            return false;
        }

        const DirectX::XMFLOAT4& NativeColor = Primitive->GetColor(); //Primitiveの現在RGBA色
        color.Red = NativeColor.x;
        color.Green = NativeColor.y;
        color.Blue = NativeColor.z;
        color.Alpha = NativeColor.w;
        return true;
    }

    //概要：Box等Primitive ObjectのRGBA頂点色を変更してMesh再生成を要求する
    //引数：sceneID=対象Scene、objectID=対象Object、color=設定するRGBA色
    //戻り値：対象がPrimitive Objectで色を変更できた場合はtrue
    bool EngineAPI::SetObjectColor(
        SceneID sceneID,
        ObjectID objectID,
        const EditorColor& color
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //対象Objectを所有するScene
        Object* TargetObject = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindObject(objectID); //色変更対象Object
        auto* Primitive = dynamic_cast<PrimitiveObject*>(TargetObject); //色を所有するPrimitive

        if (Primitive == nullptr)
        {
            return false;
        }

        Primitive->SetColor(DirectX::XMFLOAT4(
            color.Red,
            color.Green,
            color.Blue,
            color.Alpha
        ));
        return true;
    }

    //概要：Primitive Objectの現在RGBA色へ指定係数を乗算する
    //引数：sceneID=対象Scene、objectID=対象Object、multiplier=RGBA乗算係数
    //戻り値：対象がPrimitive Objectで乗算色を設定できた場合true
    bool EngineAPI::MultiplyObjectColor(
        SceneID sceneID,
        ObjectID objectID,
        const EditorColor& multiplier
    )
    {
        EditorColor Current; //乗算前の現在色

        if (!TryGetObjectColor(sceneID, objectID, Current))
        {
            return false;
        }

        return SetObjectColor(
            sceneID,
            objectID,
            EditorColor
            {
                Current.Red * multiplier.Red,
                Current.Green * multiplier.Green,
                Current.Blue * multiplier.Blue,
                Current.Alpha * multiplier.Alpha
            }
        );
    }

    //概要：外部又はNative Main ProgramからEditorコード補完候補を追加する
    //引数：suggestion=追加するC++識別子
    //戻り値：候補が有効で登録済み又は追加済みの場合true
    bool EngineAPI::SetProgramSuggestion(const std::string& suggestion)
    {
        return ProgramSuggestionRegistry::GetInstance().SetSuggestion(suggestion);
    }

    //概要：Main又はScriptから指定Virtual Keyの現在押下状態を取得する
    //引数：virtualKey=Windows Virtual-Key Code
    //戻り値：Keyが現在押されている場合はtrue
    bool EngineAPI::IsKeyDown(std::uint32_t virtualKey) const
    {
        return virtualKey <= 0xffu &&
            (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
    }

    //概要：循環を防止しながら指定Objectの親を変更する
    //引数：sceneID=対象Scene、objectID=子Object、parentID=新しい親又は無効ID、keepWorldTransform=World姿勢を維持する場合true
    //戻り値：親関係を変更できた場合はtrue
    bool EngineAPI::SetObjectParent(
        SceneID sceneID,
        ObjectID objectID,
        ObjectID parentID,
        bool keepWorldTransform
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Objectを所有するScene
        const bool Succeeded = TargetScene != nullptr &&
            TargetScene->GetObjectManager().SetParent(
                objectID,
                parentID,
                keepWorldTransform
            ); //循環検証を含む親変更結果

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：指定ComponentをPrimary Camera保護条件に従って削除する
    //引数：sceneID=対象Scene、componentID=削除対象Component
    //戻り値：Componentを削除できた場合はtrue
    bool EngineAPI::RemoveComponent(SceneID sceneID, ComponentID componentID)
    {
        return DeleteComponent(sceneID, componentID);
    }

    //概要：指定Componentを所有Object内で一意な名前へ変更する
    //引数：sceneID=対象Scene、componentID=対象Component、name=希望名
    //戻り値：名前を変更できた場合はtrue
    bool EngineAPI::RenameComponent(
        SceneID sceneID,
        ComponentID componentID,
        const std::string& name
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Componentを所有するScene
        const bool Succeeded = TargetScene != nullptr &&
            TargetScene->GetObjectManager().RenameComponent(componentID, name); //名前変更結果

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：指定Componentの更新と描画の有効状態を変更する
    //引数：sceneID=対象Scene、componentID=対象Component、active=有効にする場合はtrue
    //戻り値：状態を変更できた場合はtrue
    bool EngineAPI::SetComponentActive(
        SceneID sceneID,
        ComponentID componentID,
        bool active
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Componentを所有するScene
        Component* TargetComponent = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetObjectManager().FindComponent(componentID); //状態変更対象Component

        if (TargetComponent == nullptr)
        {
            return false;
        }

        TargetComponent->SetActive(active);
        IncrementRevision();
        return true;
    }

    //概要：現在読み込まれている外部Programを固定更新ごとに実行する
    //引数：deltaTime=前回固定更新からの秒数
    //戻り値：なし
    void EngineAPI::UpdateExtensions(float deltaTime)
    {
        Extensions.Update(deltaTime);
    }

    //概要：Windows Editorから受け取った汎用Object、Component、Script操作を実行する
    //引数：command=Sceneと対象IDを含むEditor操作要求
    //戻り値：操作を完了した場合はtrue
    bool EngineAPI::ExecuteEditorCommand(const EditorCommand& command)
    {
        Scene* TargetScene = ResolveScene(command.Scene); //操作対象Scene
        bool Succeeded = false; //各操作の実行結果

        switch (command.Type)
        {
        case EditorCommandType::CreateObject:
            return CreateObject(
                command.Scene,
                command.ObjectKind,
                command.Text,
                command.Parent
            ) != nullptr;

        case EditorCommandType::DuplicateObject:
            return DuplicateObject(command.Scene, command.Object) != nullptr;

        case EditorCommandType::DeleteObject:
            return DeleteObject(command.Scene, command.Object);

        case EditorCommandType::RenameObject:
            Succeeded = TargetScene != nullptr && TargetScene->GetObjectManager().RenameObject(
                command.Object,
                command.Text
            );
            break;

        case EditorCommandType::ToggleObjectActive:
            if (TargetScene != nullptr)
            {
                Object* Target = TargetScene->GetObjectManager().FindObject(command.Object); //状態変更対象Object

                if (Target != nullptr)
                {
                    Target->SetActive(!Target->IsActive());
                    Succeeded = true;
                }
            }
            break;

        case EditorCommandType::SetObjectTransform:
            if (TargetScene != nullptr)
            {
                Object* Target = TargetScene->GetObjectManager().FindObject(
                    command.Object
                ); //Local姿勢を変更するObject

                if (Target != nullptr)
                {
                    Target->SetPosition(DirectX::XMFLOAT3(
                        command.Transform.Position.X,
                        command.Transform.Position.Y,
                        command.Transform.Position.Z
                    ));
                    Target->SetRotation(DirectX::XMFLOAT3(
                        command.Transform.Rotation.X,
                        command.Transform.Rotation.Y,
                        command.Transform.Rotation.Z
                    ));
                    Target->SetScale(DirectX::XMFLOAT3(
                        command.Transform.Scale.X,
                        command.Transform.Scale.Y,
                        command.Transform.Scale.Z
                    ));
                    Succeeded = true;
                }
            }
            break;

        case EditorCommandType::SetObjectParent:
            Succeeded = TargetScene != nullptr && TargetScene->GetObjectManager().SetParent(
                command.Object,
                command.Parent,
                command.KeepWorldTransform
            );
            break;

        case EditorCommandType::SetViewScene:
            Succeeded = GetSceneManager().SetViewScene(command.Scene);
            break;

        case EditorCommandType::AttachScript:
            return AttachScript(command.Scene, command.Object, command.Text);

        case EditorCommandType::DeleteComponent:
            return DeleteComponent(command.Scene, command.Component);

        case EditorCommandType::RenameComponent:
            Succeeded = TargetScene != nullptr && TargetScene->GetObjectManager().RenameComponent(
                command.Component,
                command.Text
            );
            break;

        case EditorCommandType::ToggleComponentActive:
            if (TargetScene != nullptr)
            {
                Component* Target = TargetScene->GetObjectManager().FindComponent(
                    command.Component
                ); //状態変更対象Component

                if (Target != nullptr)
                {
                    Target->SetActive(!Target->IsActive());
                    Succeeded = true;
                }
            }
            break;

        case EditorCommandType::LoadScriptModule:
            Succeeded = Modules.LoadModule(command.Path);

            if (Succeeded)
            {
                const SceneID MainSceneID = GetMainSceneID(); //自動Attach対象を所有するMain Scene
                const ObjectID DemoBoxID = FindObjectID(
                    MainSceneID,
                    ObjectType::Box,
                    "OscillatingBox"
                ); //Sub Scriptを接続する標準Box

                if (DemoBoxID.IsValid())
                {
                    AttachScript(
                        MainSceneID,
                        DemoBoxID,
                        "dll:EditorScriptPrograms:box.horizontal_oscillation"
                    );
                }
            }
            break;

        case EditorCommandType::LoadExtensionModule:
            Succeeded = Extensions.LoadOrReload(command.Path);
            break;

        case EditorCommandType::UnloadExtensionModule:
            Extensions.Unload();
            Succeeded = true;
            break;

        case EditorCommandType::Refresh:
            Succeeded = true;
            break;

        default:
            return false;
        }

        if (Succeeded)
        {
            IncrementRevision();
        }

        return Succeeded;
    }

    //概要：全Scene、Object、Component、Script FactoryをUI用Snapshotへ変換する
    //引数：なし
    //戻り値：現在RevisionとEngine階層を持つEditor Snapshot
    EditorSnapshot EngineAPI::CreateEditorSnapshot() const
    {
        EditorSnapshot Snapshot; //UIへ返すEngine構造Snapshot
        Snapshot.Revision = Revision;
        Snapshot.ViewSceneID = GetSceneManager().GetViewSceneID();
        Snapshot.Scripts = Scripts.GetCatalog();

        const SceneManager& SceneCollection = GetSceneManager(); //列挙するScene Manager

        for (SceneID ID : SceneCollection.GetSceneIDs())
        {
            const Scene* CurrentScene = SceneCollection.FindScene(ID); //Snapshotへ追加するScene

            if (CurrentScene == nullptr)
            {
                continue;
            }

            EditorSceneInfo SceneInfo; //現在SceneのUI表示情報
            SceneInfo.ID = ID;
            SceneInfo.Name = CurrentScene->GetName();
            SceneInfo.Active = SceneCollection.IsActive(ID);
            SceneInfo.ViewScene = Snapshot.ViewSceneID == ID;

            const ObjectManager& Objects = CurrentScene->GetObjectManager(); //列挙するObject Manager

            for (ObjectID ObjectIDValue : Objects.GetObjectIDs())
            {
                const Object* CurrentObject = Objects.FindObject(ObjectIDValue); //Snapshotへ追加するObject

                if (CurrentObject == nullptr)
                {
                    continue;
                }

                EditorObjectInfo ObjectInfo; //現在ObjectのUI表示情報
                ObjectInfo.ID = ObjectIDValue;
                ObjectInfo.Type = CurrentObject->GetType();
                ObjectInfo.Name = CurrentObject->GetName();
                ObjectInfo.Active = CurrentObject->IsActive();
                ObjectInfo.ParentID = CurrentObject->GetParentID();
                const DirectX::XMFLOAT3& Position = CurrentObject->GetPosition(); //Local座標
                const DirectX::XMFLOAT3& Rotation = CurrentObject->GetRotation(); //Local回転角
                const DirectX::XMFLOAT3& Scale = CurrentObject->GetScale(); //Local拡縮率
                ObjectInfo.LocalTransform.Position = { Position.x, Position.y, Position.z };
                ObjectInfo.LocalTransform.Rotation = { Rotation.x, Rotation.y, Rotation.z };
                ObjectInfo.LocalTransform.Scale = { Scale.x, Scale.y, Scale.z };

                for (ComponentID ComponentIDValue : CurrentObject->GetComponentIDs())
                {
                    const Component* CurrentComponent = Objects.FindComponent(
                        ComponentIDValue
                    ); //Snapshotへ追加するComponent

                    if (CurrentComponent == nullptr)
                    {
                        continue;
                    }

                    const Script* ScriptComponent = dynamic_cast<const Script*>(
                        CurrentComponent
                    ); //Script固有表示名を持つ場合の参照
                    ObjectInfo.Components.push_back(EditorComponentInfo
                    {
                        ComponentIDValue,
                        CurrentComponent->GetName(),
                        ScriptComponent == nullptr
                            ? GetComponentTypeName(CurrentComponent->GetType())
                            : ScriptComponent->GetDisplayName(),
                        CurrentComponent->IsActive(),
                        ScriptComponent != nullptr
                    });
                }

                SceneInfo.Objects.emplace_back(std::move(ObjectInfo));
            }

            Snapshot.Scenes.emplace_back(std::move(SceneInfo));
        }

        return Snapshot;
    }

    //概要：Editor構造の現在更新番号を取得する
    //引数：なし
    //戻り値：構造変更ごとに増えるRevision
    std::uint64_t EngineAPI::GetRevision() const
    {
        return Revision;
    }

    //概要：指定ID又はView Sceneから操作対象Sceneを解決する
    //引数：sceneID=希望Scene ID、無効値の場合はView Scene
    //戻り値：操作対象Scene、存在しない場合はnullptr
    Scene* EngineAPI::ResolveScene(SceneID sceneID)
    {
        const SceneID ResolvedID = sceneID.IsValid()
            ? sceneID
            : GetSceneManager().GetViewSceneID(); //無効ID時に使用するView Scene ID
        return GetSceneManager().FindScene(ResolvedID);
    }

    //概要：指定ID又はView Sceneから読み取り専用Sceneを解決する
    //引数：sceneID=希望Scene ID、無効値の場合はView Scene
    //戻り値：読み取り専用Scene、存在しない場合はnullptr
    const Scene* EngineAPI::ResolveScene(SceneID sceneID) const
    {
        const SceneID ResolvedID = sceneID.IsValid()
            ? sceneID
            : GetSceneManager().GetViewSceneID(); //無効ID時に使用するView Scene ID
        return GetSceneManager().FindScene(ResolvedID);
    }

    //概要：指定Objectと全Componentを同じSceneへ複製して新しいObjectを返す
    //引数：sceneID=対象Scene、objectID=複製元Object ID、requestedName=複製先希望名又は空
    //戻り値：登録済み複製Object、複製又は初期化失敗時はnullptr
    Object* EngineAPI::DuplicateObject(
        SceneID sceneID,
        ObjectID objectID,
        const std::string& requestedName
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //複製先Scene

        if (TargetScene == nullptr)
        {
            return nullptr;
        }

        ObjectManager& Objects = TargetScene->GetObjectManager(); //複製を行うObject Manager
        const Object* Source = Objects.FindObject(objectID); //複製元Object

        if (Source == nullptr)
        {
            return nullptr;
        }

        Object* Duplicate = Objects.CloneObject(
            objectID,
            requestedName.empty() ? Source->GetName() + "_Copy" : requestedName
        ); //新しいIDを持つ複製Object

        if (Duplicate == nullptr ||
            (Source->GetParentID().IsValid() && !Objects.SetParent(
                Duplicate->GetID(),
                Source->GetParentID(),
                false
            )) ||
            !TargetScene->InitializePendingComponents(GetDirectX12()))
        {
            if (Duplicate != nullptr)
            {
                Objects.RemoveObject(Duplicate->GetID());
            }

            return nullptr;
        }

        IncrementRevision();
        return Duplicate;
    }

    //概要：Primary Cameraを保護しつつ指定Objectと所有Componentを削除する
    //引数：sceneID=対象Scene、objectID=削除対象Object ID
    //戻り値：削除した場合はtrue
    bool EngineAPI::DeleteObject(SceneID sceneID, ObjectID objectID)
    {
        Scene* TargetScene = ResolveScene(sceneID); //削除対象Scene
        const Camera* PrimaryCamera = TargetScene == nullptr
            ? nullptr
            : TargetScene->GetPrimaryCamera(); //削除不可のPrimary Camera

        const ObjectID PrimaryCameraObjectID = PrimaryCamera != nullptr &&
            PrimaryCamera->GetOwner() != nullptr
            ? PrimaryCamera->GetOwner()->GetID()
            : ObjectID(); //削除から保護するPrimary Camera Object

        if (TargetScene == nullptr ||
            (PrimaryCamera != nullptr && PrimaryCamera->GetOwner() != nullptr &&
                (PrimaryCameraObjectID == objectID ||
                    TargetScene->GetObjectManager().IsDescendantOf(
                        PrimaryCameraObjectID,
                        objectID
                    ))))
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] EngineAPI | Primary Camera Object cannot be deleted."
            );
            return false;
        }

        if (!TargetScene->GetObjectManager().RemoveObject(objectID))
        {
            return false;
        }

        IncrementRevision();
        return true;
    }

    //概要：RegistryからScriptを生成して指定Objectへ差し込み初期化する
    //引数：sceneID=対象Scene、objectID=所有Object、scriptKey=Script識別子
    //戻り値：差し込みと初期化に成功した場合はtrue
    bool EngineAPI::AttachScript(
        SceneID sceneID,
        ObjectID objectID,
        const std::string& scriptKey
    )
    {
        Scene* TargetScene = ResolveScene(sceneID); //Script追加先Scene

        if (TargetScene == nullptr)
        {
            return false;
        }

        ObjectManager& Objects = TargetScene->GetObjectManager(); //Script追加先Object Manager
        Object* TargetObject = Objects.FindObject(objectID); //重複確認するAttach先Object

        if (TargetObject == nullptr)
        {
            return false;
        }

        for (ComponentID Component : Objects.GetComponentIDs(objectID))
        {
            const auto* ExistingScript = dynamic_cast<const Script*>(
                Objects.FindComponent(Component)
            ); //同じRegistry KeyでAttach済みのScript

            if (ExistingScript != nullptr && ExistingScript->GetName() == scriptKey)
            {
                return true;
            }
        }

        std::unique_ptr<Script> Definition = Scripts.CreateScript(scriptKey); //未登録Script定義

        if (!Definition)
        {
            return false;
        }

        Component* Added = Objects.AddComponent(
            objectID,
            std::move(Definition),
            scriptKey
        ); //新しいIDを割り当てたScript Component

        if (Added == nullptr || !TargetScene->InitializePendingComponents(GetDirectX12()))
        {
            if (Added != nullptr)
            {
                Objects.RemoveComponent(Added->GetID());
            }

            return false;
        }

        IncrementRevision();
        return true;
    }

    //概要：Primary Cameraを保護しつつ指定Componentを削除する
    //引数：sceneID=対象Scene、componentID=削除対象Component ID
    //戻り値：削除した場合はtrue
    bool EngineAPI::DeleteComponent(SceneID sceneID, ComponentID componentID)
    {
        Scene* TargetScene = ResolveScene(sceneID); //削除対象Scene

        if (TargetScene == nullptr || TargetScene->GetPrimaryCameraID() == componentID)
        {
            MessageLog::GetInstance().AddLog(
                "[Warning] EngineAPI | Primary Camera Component cannot be deleted."
            );
            return false;
        }

        if (!TargetScene->GetObjectManager().RemoveComponent(componentID))
        {
            return false;
        }

        IncrementRevision();
        return true;
    }

    //概要：Editor Snapshot再構築を通知するRevisionを安全に増加する
    //引数：なし
    //戻り値：なし
    void EngineAPI::IncrementRevision()
    {
        if (Revision < (std::numeric_limits<std::uint64_t>::max)())
        {
            ++Revision;
        }
    }
}
