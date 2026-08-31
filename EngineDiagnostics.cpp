//|| EngineDiagnostics.cpp ||::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Scene、Object、Component、Script、DLL及び描画Lifecycleの実行時診断を実装する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.00  新規作成
//||

#include "EngineDiagnostics.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "Camera.h"
#include "Capsule.h"
#include "Component.h"
#include "DirectX12.h"
#include "EngineAPI.h"
#include "ExtensionSystem.h"
#include "GameApp.h"
#include "GameObjectTemplate.h"
#include "IRenderable.h"
#include "MessageLog.h"
#include "Object.h"
#include "ObjectManager.h"
#include "PrimitiveObject.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Script.h"
#include "ScriptSystem.h"
#include "Vertex.h"
#include "VertexMesh.h"
#include "NativeScripts.h"
#include "OBJModel.h"
#include "Texture2D.h"
#include "Box.h"
#include "PlaybackSettings.h"

namespace Engine
{
    namespace
    {
        constexpr wchar_t DiagnosticModeVariable[] = L"DX12_ENGINE_DIAGNOSTICS";
        constexpr wchar_t DiagnosticReportVariable[] = L"DX12_ENGINE_DIAGNOSTIC_REPORT";
        constexpr wchar_t DiagnosticScriptDLLVariable[] = L"DX12_ENGINE_DIAGNOSTIC_SCRIPT_DLL";
        constexpr wchar_t DiagnosticExtensionDLLVariable[] = L"DX12_ENGINE_DIAGNOSTIC_EXTENSION_DLL";

        struct DiagnosticContext final
        {
            EngineDiagnosticResult Result; //構築中の診断結果

            //条件を検証結果へ追加する
            //condition: 成功条件、name: Reportへ表示する検証名
            void Check(bool condition, const std::string& name)
            {
                if (condition)
                {
                    ++Result.PassedCheckCount;
                    return;
                }

                Result.Failures.emplace_back(name);
            }
        };

        struct MeshBounds final
        {
            DirectX::XMFLOAT3 Minimum{}; //CPU Meshの各軸最小値
            DirectX::XMFLOAT3 Maximum{}; //CPU Meshの各軸最大値
            bool Valid = false; //頂点を一つ以上取得できた場合true
        };

        //Windows環境変数を取得する
        //name: 取得する変数名
        //戻り値: 環境変数値、未定義又は読取失敗時は空
        std::wstring GetEnvironmentValue(const wchar_t* name)
        {
            const DWORD RequiredLength = GetEnvironmentVariableW(name, nullptr, 0); //終端を含む必要文字数

            if (RequiredLength == 0)
            {
                return std::wstring();
            }

            std::wstring Value(RequiredLength, L'\0'); //環境変数読取Buffer
            const DWORD WrittenLength = GetEnvironmentVariableW(
                name,
                Value.data(),
                RequiredLength
            ); //終端を除く書込文字数

            if (WrittenLength == 0 || WrittenLength >= RequiredLength)
            {
                return std::wstring();
            }

            Value.resize(WrittenLength);
            return Value;
        }

        //浮動小数値を許容誤差付きで比較する
        bool NearlyEqual(float left, float right, float tolerance = 0.001f)
        {
            return std::fabs(left - right) <= tolerance;
        }

        //PrimitiveのCPU Mesh境界を取得する
        MeshBounds GetMeshBounds(const PrimitiveObject& primitive)
        {
            const std::vector<Vertex>& Vertices = primitive.GetMesh().GetVertices(); //境界を計算する頂点
            MeshBounds Bounds; //計算結果

            if (Vertices.empty())
            {
                return Bounds;
            }

            const float Maximum = (std::numeric_limits<float>::max)(); //初期最小値用の上限
            Bounds.Minimum = { Maximum, Maximum, Maximum };
            Bounds.Maximum = { -Maximum, -Maximum, -Maximum };

            for (const Vertex& Value : Vertices)
            {
                Bounds.Minimum.x = (std::min)(Bounds.Minimum.x, Value.Position.x);
                Bounds.Minimum.y = (std::min)(Bounds.Minimum.y, Value.Position.y);
                Bounds.Minimum.z = (std::min)(Bounds.Minimum.z, Value.Position.z);
                Bounds.Maximum.x = (std::max)(Bounds.Maximum.x, Value.Position.x);
                Bounds.Maximum.y = (std::max)(Bounds.Maximum.y, Value.Position.y);
                Bounds.Maximum.z = (std::max)(Bounds.Maximum.z, Value.Position.z);
            }

            Bounds.Valid = true;
            return Bounds;
        }

        //PrimitiveのCPU Mesh外形寸法を検証する
        bool HasMeshExtent(
            const Object* object,
            float expectedX,
            float expectedY,
            float expectedZ,
            float tolerance = 0.03f
        )
        {
            const auto* Primitive = dynamic_cast<const PrimitiveObject*>(object); //Meshを持つ検証対象

            if (Primitive == nullptr)
            {
                return false;
            }

            const MeshBounds Bounds = GetMeshBounds(*Primitive); //CPU Mesh境界
            return Bounds.Valid &&
                NearlyEqual(Bounds.Maximum.x - Bounds.Minimum.x, expectedX, tolerance) &&
                NearlyEqual(Bounds.Maximum.y - Bounds.Minimum.y, expectedY, tolerance) &&
                NearlyEqual(Bounds.Maximum.z - Bounds.Minimum.z, expectedZ, tolerance);
        }

        //Scene内の登録、必須Object及びComponent初期化を検証する
        void ValidateScene(
            DiagnosticContext& context,
            const Scene& scene,
            const std::string& label
        )
        {
            const ObjectManager& Objects = scene.GetObjectManager(); //検証対象Object Manager
            const std::vector<ObjectID> ObjectIDs = Objects.GetObjectIDs(); //登録中Object一覧
            const std::vector<ComponentID> ComponentIDs = Objects.GetComponentIDs(); //登録中Component一覧
            std::unordered_set<std::string> Names; //Scene内Object名重複検出用集合

            context.Check(scene.IsInitialized(), label + " | Scene initialized");
            context.Check(scene.GetPrimaryCamera() != nullptr, label + " | Primary Camera available");
            context.Check(scene.GetRenderTexture() != nullptr, label + " | Primary RenderTexture available");
            context.Check(Objects.FindObject("MainCamera") != nullptr, label + " | MainCamera object available");
            context.Check(Objects.FindObject("DebugGrid") != nullptr, label + " | DebugGrid object available");
            context.Check(ObjectIDs.size() == Objects.GetObjectCount(), label + " | Object count consistent");
            context.Check(ComponentIDs.size() == Objects.GetComponentCount(), label + " | Component count consistent");

            for (ObjectID ID : ObjectIDs)
            {
                const Object* Value = Objects.FindObject(ID); //IDから検索したObject
                const bool Registered = Value != nullptr && Value->GetID() == ID &&
                    !Value->GetName().empty() && Objects.FindObject(Value->GetName()) == Value;
                context.Check(Registered, label + " | Object registration ID=" + std::to_string(ID.GetValue()));

                if (Value == nullptr)
                {
                    continue;
                }

                context.Check(Names.emplace(Value->GetName()).second,
                    label + " | Unique object name=" + Value->GetName());

                if (dynamic_cast<const IRenderable*>(Value) != nullptr)
                {
                    context.Check(Objects.IsRenderableObjectInitialized(ID),
                        label + " | Renderable initialized=" + Value->GetName());
                }
            }

            for (ComponentID ID : ComponentIDs)
            {
                const Component* Value = Objects.FindComponent(ID); //IDから検索したComponent
                const bool Registered = Value != nullptr && Value->GetID() == ID &&
                    Value->GetOwner() != nullptr && !Value->GetName().empty();
                context.Check(Registered,
                    label + " | Component registration ID=" + std::to_string(ID.GetValue()));
                context.Check(Value != nullptr && Value->IsInitialized(),
                    label + " | Component initialized ID=" + std::to_string(ID.GetValue()));
            }
        }

        //指定Objectが存在する場合だけAPIで削除する
        void RemoveIfPresent(EngineAPI& engine, SceneID sceneID, ObjectID objectID)
        {
            Scene* TargetScene = engine.GetSceneManager().FindScene(sceneID); //削除先Scene

            if (TargetScene != nullptr &&
                TargetScene->GetObjectManager().FindObject(objectID) != nullptr)
            {
                engine.RemoveObject(sceneID, objectID);
            }
        }
    }

    bool EngineDiagnosticResult::Passed() const
    {
        return Failures.empty();
    }

    std::string EngineDiagnosticResult::ToText() const
    {
        std::ostringstream Stream; //UTF-8診断Report構築先
        Stream << "ENGINE_DIAGNOSTICS " << (Passed() ? "PASS" : "FAIL") << '\n';
        Stream << "Passed checks: " << PassedCheckCount << '\n';
        Stream << "Failed checks: " << Failures.size() << '\n';

        for (const std::string& Failure : Failures)
        {
            Stream << "[FAIL] " << Failure << '\n';
        }

        Stream << "Engine logs: " << EngineLogs.size() << '\n';

        for (const std::string& Log : EngineLogs)
        {
            Stream << Log << '\n';
        }

        return Stream.str();
    }

    bool IsEngineDiagnosticModeEnabled()
    {
        const std::wstring Value = GetEnvironmentValue(DiagnosticModeVariable); //診断起動指定
        return !Value.empty() && Value != L"0";
    }

    EngineDiagnosticResult RunEngineDiagnostics(EngineAPI& engine)
    {
        DiagnosticContext Context; //全検証結果の集約先
        GameApp& Application = engine.GetApplication(); //更新、描画、Resize検証対象
        DirectX12& Graphics = engine.GetDirectX12(); //描画基盤状態の検証対象
        SceneManager& Scenes = engine.GetSceneManager(); //Scene Lifecycle検証対象
        const SceneID OriginalMainSceneID = engine.GetMainSceneID(); //診断後に戻すMain Scene
        const SceneID OriginalViewSceneID = engine.GetViewSceneID(); //診断後に戻すView Scene
        Scene* MainScene = Scenes.FindScene(OriginalMainSceneID); //組込みAPI検証先Main Scene

        Context.Check(OriginalMainSceneID.IsValid(), "Startup | Main Scene ID valid");
        Context.Check(OriginalViewSceneID == OriginalMainSceneID, "Startup | Main Scene is View Scene");
        Context.Check(MainScene != nullptr, "Startup | Main Scene resolved");
        Context.Check(Graphics.GetDevice() != nullptr, "Startup | DirectX 12 device available");
        Context.Check(!Graphics.IsFrameOpen(), "Startup | No frame left open");
        Camera UnregisteredCamera(64, 64); //基底登録検証を通してはならない未登録Component
        Context.Check(!UnregisteredCamera.Initialize(Graphics),
            "Lifecycle | Unregistered Component initialization rejected");

        if (MainScene == nullptr)
        {
            Context.Result.EngineLogs = MessageLog::GetInstance().GetSnapshot().Logs;
            return std::move(Context.Result);
        }

        ValidateScene(Context, *MainScene, "MainScene");
        ObjectManager& MainObjects = MainScene->GetObjectManager(); //Main Scene内部状態確認用Manager
        const std::size_t InitialObjectCount = MainObjects.GetObjectCount(); //Cleanup完全性検証用Object数
        const std::size_t InitialComponentCount = MainObjects.GetComponentCount(); //Cleanup完全性検証用Component数
        std::vector<ObjectID> CreatedObjectIDs; //診断終了時に逆順削除するObject一覧

        const auto Track = [&CreatedObjectIDs](Object* object)
        {
            if (object != nullptr)
            {
                CreatedObjectIDs.emplace_back(object->GetID());
            }
            return object;
        }; //作成成功ObjectをCleanup対象へ追加する処理

        Object* ParentObject = Track(engine.CreateObject(
            OriginalMainSceneID,
            ObjectType::Object,
            "__DiagnosticsParent"
        )); //親子と汎用Object API検証対象
        Object* BoxObject = Track(engine.CreateObject(
            OriginalMainSceneID,
            ObjectType::Box,
            "__DiagnosticsShape"
        )); //名前衝突とBox検証対象
        Object* SphereObject = Track(engine.CreateObject(
            OriginalMainSceneID,
            ObjectType::Sphere,
            "__DiagnosticsSphere"
        )); //Sphere寸法検証対象
        Object* PlaneObject = Track(engine.CreateObject(
            OriginalMainSceneID,
            ObjectType::Plane,
            "__DiagnosticsPlane"
        )); //Plane寸法検証対象
        Object* CylinderObject = Track(engine.CreateObject(
            OriginalMainSceneID,
            ObjectType::Cylinder,
            "__DiagnosticsCylinder"
        )); //Cylinder寸法検証対象
        Object* HalfSphereObject = Track(engine.CreateObject(
            OriginalMainSceneID,
            ObjectType::HalfSphere,
            "__DiagnosticsHalfSphere"
        )); //HalfSphere生成検証対象
        Object* CapsuleObject = Track(engine.CreateCapsuleModel(
            OriginalMainSceneID,
            "__DiagnosticsShape"
        )); //名前指定Capsule APIとScene全体一意名検証対象

        Context.Check(ParentObject != nullptr, "Object API | Generic object created");
        Context.Check(BoxObject != nullptr, "Object API | Box created");
        Context.Check(SphereObject != nullptr, "Object API | Sphere created");
        Context.Check(PlaneObject != nullptr, "Object API | Plane created");
        Context.Check(CylinderObject != nullptr, "Object API | Cylinder created");
        Context.Check(HalfSphereObject != nullptr, "Object API | HalfSphere created");
        Context.Check(CapsuleObject != nullptr, "Object API | Capsule created by name API");

        if (BoxObject != nullptr && CapsuleObject != nullptr)
        {
            Context.Check(BoxObject->GetName() != CapsuleObject->GetName(),
                "Naming | Same requested name resolved globally");
            Context.Check(engine.FindObjectID(OriginalMainSceneID, BoxObject->GetName()) == BoxObject->GetID(),
                "Naming | Box resolved without type");
            Context.Check(engine.FindObjectID(OriginalMainSceneID, CapsuleObject->GetName()) == CapsuleObject->GetID(),
                "Naming | Capsule resolved without type");
            Context.Check(!engine.FindObjectID(
                OriginalMainSceneID,
                ObjectType::Capsule,
                BoxObject->GetName()
            ).IsValid(), "Naming | Typed lookup rejects mismatched type");
        }

        if (BoxObject != nullptr)
        {
            Context.Check(engine.SetObjectSize(
                OriginalMainSceneID,
                BoxObject->GetID(),
                { 2.0f, 3.0f, 4.0f }
            ), "Size API | Box size by ID");
            Context.Check(HasMeshExtent(BoxObject, 2.0f, 3.0f, 4.0f), "Size API | Box CPU mesh extent");
            Context.Check(!engine.SetObjectSize(
                OriginalMainSceneID,
                BoxObject->GetID(),
                { 0.0f, 1.0f, 1.0f }
            ), "Size API | Invalid dimensions rejected");
        }

        if (SphereObject != nullptr)
        {
            Context.Check(engine.SetObjectSize(
                OriginalMainSceneID,
                SphereObject->GetID(),
                { 2.0f, 2.0f, 2.0f }
            ), "Size API | Sphere size by ID");
            Context.Check(HasMeshExtent(SphereObject, 2.0f, 2.0f, 2.0f), "Size API | Sphere CPU mesh extent");
        }

        if (PlaneObject != nullptr)
        {
            Context.Check(engine.SetObjectSize(
                OriginalMainSceneID,
                PlaneObject->GetID(),
                { 4.0f, 1.0f, 6.0f }
            ), "Size API | Plane size by ID");
            Context.Check(HasMeshExtent(PlaneObject, 4.0f, 0.0f, 6.0f), "Size API | Plane CPU mesh extent");
        }

        if (CylinderObject != nullptr)
        {
            Context.Check(engine.SetObjectSize(
                OriginalMainSceneID,
                CylinderObject->GetID(),
                { 2.0f, 4.0f, 2.0f }
            ), "Size API | Cylinder size by ID");
            Context.Check(HasMeshExtent(CylinderObject, 2.0f, 4.0f, 2.0f), "Size API | Cylinder CPU mesh extent");
        }

        if (HalfSphereObject != nullptr)
        {
            Context.Check(engine.SetObjectSize(
                OriginalMainSceneID,
                HalfSphereObject->GetID(),
                { 2.0f, 2.0f, 2.0f }
            ), "Size API | HalfSphere size by ID");
            const auto* Primitive = dynamic_cast<const PrimitiveObject*>(HalfSphereObject); //Mesh生成確認対象
            Context.Check(Primitive != nullptr && Primitive->GetMesh().GetVertexCount() > 0,
                "Size API | HalfSphere CPU mesh available");
        }

        if (CapsuleObject != nullptr)
        {
            Context.Check(engine.SetObjectSize(
                OriginalMainSceneID,
                CapsuleObject->GetName(),
                { 2.0f, 4.0f, 2.0f }
            ), "Size API | Capsule size by name");
            Context.Check(HasMeshExtent(CapsuleObject, 2.0f, 4.0f, 2.0f), "Size API | Capsule CPU mesh extent");
        }

        if (ParentObject != nullptr && BoxObject != nullptr)
        {
            Context.Check(engine.SetObjectParent(
                OriginalMainSceneID,
                BoxObject->GetID(),
                ParentObject->GetID(),
                true
            ), "Hierarchy | Parent assigned");
            EditorObjectInfo BoxInformation; //親とTransformの読取結果
            Context.Check(engine.TryGetObjectInfo(
                OriginalMainSceneID,
                BoxObject->GetID(),
                BoxInformation
            ) && BoxInformation.ParentID == ParentObject->GetID(), "Hierarchy | Parent read back");
            const std::vector<ObjectID> Children = engine.GetChildObjectIDs(
                OriginalMainSceneID,
                ParentObject->GetID()
            ); //親から取得した直接Child一覧
            Context.Check(std::find(Children.begin(), Children.end(), BoxObject->GetID()) != Children.end(),
                "Hierarchy | Child enumeration");
            Context.Check(!engine.SetObjectParent(
                OriginalMainSceneID,
                ParentObject->GetID(),
                BoxObject->GetID(),
                true
            ), "Hierarchy | Cycle rejected");
            Context.Check(engine.SetObjectParent(
                OriginalMainSceneID,
                BoxObject->GetID(),
                ObjectID(),
                true
            ), "Hierarchy | Parent removed");
        }

        if (CapsuleObject != nullptr)
        {
            const EditorTransformInfo Transform
            {
                { 1.0f, 2.0f, 3.0f },
                { 0.1f, 0.2f, 0.3f },
                { 1.1f, 1.2f, 1.3f }
            }; //書込と読取を確認するLocal Transform
            Context.Check(engine.SetObjectTransform(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                Transform
            ), "Transform API | Transform set");
            EditorObjectInfo Information; //Transform読取結果
            Context.Check(engine.TryGetObjectInfo(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                Information
            ) && NearlyEqual(Information.LocalTransform.Position.X, 1.0f) &&
                NearlyEqual(Information.LocalTransform.Rotation.Y, 0.2f) &&
                NearlyEqual(Information.LocalTransform.Scale.Z, 1.3f),
                "Transform API | Transform read back");
            const EditorColor Color{ 0.5f, 0.4f, 0.3f, 1.0f }; //直接設定するRGBA
            const EditorColor Multiplier{ 0.5f, 0.5f, 0.5f, 1.0f }; //乗算するRGBA
            EditorColor ReadColor; //色API読取結果
            Context.Check(engine.SetObjectColor(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                Color
            ), "Color API | Color set");
            Context.Check(engine.MultiplyObjectColor(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                Multiplier
            ), "Color API | Color multiplied");
            Context.Check(engine.TryGetObjectColor(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                ReadColor
            ) && NearlyEqual(ReadColor.Red, 0.25f) && NearlyEqual(ReadColor.Blue, 0.15f),
                "Color API | Color read back");

            Context.Check(engine.AttachScript(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                "native.rotation"
            ), "Script | Native Script attached");
            const std::vector<ComponentID> ComponentsBeforeRename = engine.GetComponentIDs(
                OriginalMainSceneID,
                CapsuleObject->GetID()
            ); //Attach後Component一覧
            Script* NativeScript = nullptr; //AttachしたNative Script

            for (ComponentID ID : ComponentsBeforeRename)
            {
                NativeScript = dynamic_cast<Script*>(MainObjects.FindComponent(ID));

                if (NativeScript != nullptr && NativeScript->GetScriptKey() == "native.rotation")
                {
                    break;
                }

                NativeScript = nullptr;
            }

            Context.Check(NativeScript != nullptr && NativeScript->IsInitialized() && NativeScript->IsAttached(),
                "Script | Base initialization and attach completed");

            if (NativeScript != nullptr)
            {
                const std::vector<ScriptExposedMember> PublicMembers =
                    NativeScript->GetExposedMembers();
                Context.Check(!PublicMembers.empty() &&
                    PublicMembers.front().Name == "RadiansPerSecond",
                    "Script reflection | Native public variable enumerated");
                Context.Check(engine.SetScriptMember(
                    OriginalMainSceneID,
                    NativeScript->GetID(),
                    "RadiansPerSecond",
                    "0,2,0"
                ), "Script reflection | Native public variable changed");
                Context.Check(engine.SetComponentActive(
                    OriginalMainSceneID,
                    NativeScript->GetID(),
                    false
                ) && !NativeScript->IsActive(), "Component API | Active disabled");
                Context.Check(engine.SetComponentActive(
                    OriginalMainSceneID,
                    NativeScript->GetID(),
                    true
                ) && NativeScript->IsActive(), "Component API | Active enabled");
                Context.Check(engine.RenameComponent(
                    OriginalMainSceneID,
                    NativeScript->GetID(),
                    "RenamedRotation"
                ), "Component API | Component renamed");
                Context.Check(engine.AttachScript(
                    OriginalMainSceneID,
                    CapsuleObject->GetID(),
                    "native.rotation"
                ) && engine.GetComponentIDs(
                    OriginalMainSceneID,
                    CapsuleObject->GetID()
                ).size() == ComponentsBeforeRename.size(),
                    "Script | Registry key prevents duplicate after rename");

                const float PreviousRotation = CapsuleObject->GetRotation().y; //Script更新前Y回転
                Application.Update(0.25f);
                Context.Check(CapsuleObject->GetRotation().y > PreviousRotation,
                    "Script | OnStart and OnUpdate executed");
                Context.Check(engine.InvokeScriptFunction(
                    OriginalMainSceneID,
                    NativeScript->GetID(),
                    "ResetRotation"
                ) && NearlyEqual(CapsuleObject->GetRotation().y, 0.0f),
                    "Script reflection | Native public function invoked");
            }

            Context.Check(engine.SetObjectActive(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                false
            ), "Object API | Active disabled");
            Object* InactiveDuplicate = Track(engine.DuplicateObject(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                "__DiagnosticsInactiveDuplicate"
            )); //非Active状態でもGPUとComponentを初期化する複製
            Context.Check(InactiveDuplicate != nullptr && !InactiveDuplicate->IsActive(),
                "Object API | Inactive definition duplicated");

            if (InactiveDuplicate != nullptr)
            {
                Context.Check(MainObjects.IsRenderableObjectInitialized(InactiveDuplicate->GetID()),
                    "Lifecycle | Inactive Renderable initialized");

                for (ComponentID ID : engine.GetComponentIDs(
                    OriginalMainSceneID,
                    InactiveDuplicate->GetID()
                ))
                {
                    const Component* Value = MainObjects.FindComponent(ID); //複製Component初期化確認対象
                    Context.Check(Value != nullptr && Value->IsInitialized(),
                        "Lifecycle | Duplicated Component initialized ID=" + std::to_string(ID.GetValue()));
                }
            }

            Context.Check(engine.SetObjectActive(
                OriginalMainSceneID,
                CapsuleObject->GetID(),
                true
            ), "Object API | Active restored");

            if (NativeScript != nullptr)
            {
                const ComponentID NativeScriptID = NativeScript->GetID(); //削除後検索に使用するID
                Context.Check(engine.RemoveComponent(OriginalMainSceneID, NativeScriptID),
                    "Component API | Component removed");
                Context.Check(MainObjects.FindComponent(NativeScriptID) == nullptr,
                    "Component API | Removed Component became tombstone");
            }
        }

        GameObjectTemplate* TemplateObject = dynamic_cast<GameObjectTemplate*>(Track(
            engine.CreateGameObjectTemplate(
                OriginalMainSceneID,
                "__DiagnosticsGameObject"
            )
        )); //ゲーム固有設定と複製検証対象
        Context.Check(TemplateObject != nullptr, "Game Object API | Template created");

        if (TemplateObject != nullptr)
        {
            Context.Check(engine.SetGameObjectTemplateInfo(
                OriginalMainSceneID,
                TemplateObject->GetID(),
                "Diagnostics",
                7.5f,
                250.0f
            ), "Game Object API | Template values set");
            auto* TemplateDuplicate = dynamic_cast<GameObjectTemplate*>(Track(engine.DuplicateObject(
                OriginalMainSceneID,
                TemplateObject->GetID(),
                "__DiagnosticsGameObjectCopy"
            ))); //型と固有値を維持する複製
            Context.Check(TemplateDuplicate != nullptr &&
                TemplateDuplicate->GetGameplayTag() == "Diagnostics" &&
                NearlyEqual(TemplateDuplicate->GetMoveSpeed(), 7.5f) &&
                NearlyEqual(TemplateDuplicate->GetMaximumHealth(), 250.0f),
                "Game Object API | Template definition duplicated");
        }

        const std::wstring ScriptDLLPath = GetEnvironmentValue(DiagnosticScriptDLLVariable); //任意DLL Script診断Path

        if (!ScriptDLLPath.empty())
        {
            ScriptModuleManager& Modules = engine.GetScriptModuleManager(); //DLL Script Module管理器
            Context.Check(Modules.LoadModule(ScriptDLLPath), "DLL Script | Module loaded");
            Context.Check(Modules.IsLoaded(ScriptDLLPath), "DLL Script | Loaded state available");
            Context.Check(engine.GetScriptRegistry().Contains("dll:Sample.Rotation:rotation"),
                "DLL Script | Factory registered");
            Object* DLLScriptObject = Track(engine.CreateObject(
                OriginalMainSceneID,
                ObjectType::Box,
                "__DiagnosticsDLLScript"
            )); //DLL Script接続先
            Context.Check(DLLScriptObject != nullptr, "DLL Script | Owner created");

            if (DLLScriptObject != nullptr)
            {
                Context.Check(engine.AttachScript(
                    OriginalMainSceneID,
                    DLLScriptObject->GetID(),
                    "dll:Sample.Rotation:rotation"
                ), "DLL Script | Script attached");
                const float BeforeUpdate = DLLScriptObject->GetRotation().y; //DLL更新前Y回転
                Application.Update(0.25f);
                Context.Check(DLLScriptObject->GetRotation().y > BeforeUpdate,
                    "DLL Script | Host callbacks updated Object");
                Context.Check(Modules.LoadModule(ScriptDLLPath),
                    "DLL Script | Same-name module hot reloaded");
                Context.Check(engine.GetScriptRegistry().Contains("dll:Sample.Rotation:rotation"),
                    "DLL Script | Replacement factory registered");
                Context.Check(Modules.UnloadModule(ScriptDLLPath),
                    "DLL Script | Factory module unloaded");
                Context.Check(!engine.GetScriptRegistry().Contains("dll:Sample.Rotation:rotation"),
                    "DLL Script | Factory unregistered");
                const float BeforeRetainedUpdate = DLLScriptObject->GetRotation().y; //利用中Module寿命検証前Y回転
                Application.Update(0.25f);
                Context.Check(DLLScriptObject->GetRotation().y > BeforeRetainedUpdate,
                    "DLL Script | Attached instance retained module lifetime");
            }
            else if (Modules.IsLoaded(ScriptDLLPath))
            {
                Modules.UnloadModule(ScriptDLLPath);
            }
        }

        const std::wstring ExtensionDLLPath = GetEnvironmentValue(DiagnosticExtensionDLLVariable); //任意Main Program DLL診断Path

        if (!ExtensionDLLPath.empty())
        {
            ExtensionModuleManager& Extensions = engine.GetExtensionModuleManager(); //Main Program DLL管理器
            const std::filesystem::path ModulePath(ExtensionDLLPath); //LoadLibraryへ渡すDLL Path
            Context.Check(Extensions.LoadOrReload(ModulePath), "Extension DLL | Module loaded");
            Context.Check(Extensions.IsLoaded(), "Extension DLL | Loaded state available");
            Context.Check(Extensions.GetModuleName() == "SceneMainProgram",
                "Extension DLL | Scene descriptor read");
            ObjectID ExtensionCapsuleID = engine.FindObjectID(
                OriginalMainSceneID,
                "MainOscillatingCapsule"
            ); //外部APIが名前付き生成したCapsule
            Context.Check(ExtensionCapsuleID.IsValid(), "Extension DLL | Named Capsule created");
            EditorObjectInfo BeforeUpdateInformation{}; //Init後のCapsule情報
            const bool BeforeUpdateFound = engine.TryGetObjectInfo(
                OriginalMainSceneID,
                ExtensionCapsuleID,
                BeforeUpdateInformation
            ); //名前指定InitのTransform反映結果
            Context.Check(BeforeUpdateFound &&
                NearlyEqual(BeforeUpdateInformation.LocalTransform.Position.X, 3.0f) &&
                NearlyEqual(BeforeUpdateInformation.LocalTransform.Position.Y, 1.0f) &&
                NearlyEqual(BeforeUpdateInformation.LocalTransform.Position.Z, 2.0f),
                "Extension DLL | Scene Init applied named transform");
            const std::uint64_t PreviousFrame = Extensions.GetFrameNumber(); //Update前Frame番号
            engine.UpdateExtensions(0.25f);
            Context.Check(Extensions.GetFrameNumber() == PreviousFrame + 1,
                "Extension DLL | Update executed");
            EditorObjectInfo AfterUpdateInformation{}; //Update後のCapsule情報
            Context.Check(BeforeUpdateFound && engine.TryGetObjectInfo(
                OriginalMainSceneID,
                ExtensionCapsuleID,
                AfterUpdateInformation
            ) && !NearlyEqual(
                AfterUpdateInformation.LocalTransform.Position.Z,
                BeforeUpdateInformation.LocalTransform.Position.Z
            ), "Extension DLL | Scene Update changed named Object");
            const std::uint64_t PreviousGeneration = Extensions.GetGeneration(); //Hot Reload前世代
            Context.Check(Extensions.LoadOrReload(ModulePath), "Extension DLL | Hot reload succeeded");
            Context.Check(Extensions.GetGeneration() == PreviousGeneration + 1,
                "Extension DLL | Generation advanced");
            Extensions.Unload();
            Context.Check(!Extensions.IsLoaded(), "Extension DLL | Module unloaded");
            const MessageLogSnapshot EndSnapshot =
                MessageLog::GetInstance().GetSnapshot(); //End Callback後Log
            Context.Check(std::any_of(
                EndSnapshot.Logs.begin(),
                EndSnapshot.Logs.end(),
                [](const std::string& log)
                {
                    return log.find("MainProgram | MainScene End") != std::string::npos;
                }
            ), "Extension DLL | Scene End executed");
            RemoveIfPresent(engine, OriginalMainSceneID, ExtensionCapsuleID);
        }

        const SceneID SecondarySceneID = engine.CreateScene(
            "__DiagnosticsScene",
            320,
            240
        ); //標準Scene Lifecycle検証対象
        Context.Check(SecondarySceneID.IsValid(), "Scene API | Scene created");
        Scene* SecondaryScene = Scenes.FindScene(SecondarySceneID); //作成済み標準Scene

        if (SecondaryScene != nullptr)
        {
            ValidateScene(Context, *SecondaryScene, "SecondaryScene");
        }

        const SceneID DuplicateSceneID = SecondarySceneID.IsValid()
            ? engine.DuplicateScene(SecondarySceneID, "__DiagnosticsSceneCopy")
            : SceneID(); //Scene定義複製対象
        Context.Check(DuplicateSceneID.IsValid(), "Scene API | Scene duplicated");

        if (Scene* DuplicateScene = Scenes.FindScene(DuplicateSceneID))
        {
            ValidateScene(Context, *DuplicateScene, "DuplicatedScene");
        }

        if (SecondarySceneID.IsValid())
        {
            Context.Check(engine.SetSceneActive(SecondarySceneID, false) &&
                !Scenes.IsActive(SecondarySceneID), "Scene API | Scene deactivated");
            Context.Check(engine.SetViewScene(SecondarySceneID) &&
                engine.GetViewSceneID() == SecondarySceneID && Scenes.IsActive(SecondarySceneID),
                "Scene API | View Scene selected and activated");
            Context.Check(engine.SetViewScene(OriginalViewSceneID), "Scene API | View Scene restored");
            Context.Check(engine.SetMainScene(SecondarySceneID) &&
                engine.GetMainSceneID() == SecondarySceneID &&
                engine.GetViewSceneID() == SecondarySceneID,
                "Scene API | Main Scene switched");
            Context.Check(engine.SetMainScene(OriginalMainSceneID) &&
                engine.GetMainSceneID() == OriginalMainSceneID &&
                engine.GetViewSceneID() == OriginalMainSceneID,
                "Scene API | Main Scene restored");
        }

        Context.Check(!engine.RemoveScene(OriginalMainSceneID),
            "Protection | Main Scene removal rejected");
        const Object* PrimaryCameraOwner = MainScene->GetPrimaryCamera() == nullptr
            ? nullptr
            : MainScene->GetPrimaryCamera()->GetOwner(); //削除保護対象Camera Owner
        Context.Check(PrimaryCameraOwner != nullptr && !engine.RemoveObject(
            OriginalMainSceneID,
            PrimaryCameraOwner->GetID()
        ), "Protection | Primary Camera Object removal rejected");
        Context.Check(!engine.RemoveComponent(
            OriginalMainSceneID,
            MainScene->GetPrimaryCameraID()
        ), "Protection | Primary Camera Component removal rejected");

        Application.Draw();
        Context.Check(!Graphics.IsFrameOpen(), "Draw | Frame completed");

        for (ObjectID ID : CreatedObjectIDs)
        {
            const auto* Primitive = dynamic_cast<const PrimitiveObject*>(MainObjects.FindObject(ID)); //GPU再生成確認対象

            if (Primitive != nullptr && Primitive->IsActive())
            {
                Context.Check(Primitive->GetMesh().IsGPUResourceReady(),
                    "Draw | Dirty Primitive GPU resource rebuilt ID=" + std::to_string(ID.GetValue()));
            }
        }

        const std::uint32_t OriginalWidth = Graphics.GetWidth(); //Resize後に戻すBackBuffer幅
        const std::uint32_t OriginalHeight = Graphics.GetHeight(); //Resize後に戻すBackBuffer高さ
        const std::uint32_t TestWidth = OriginalWidth > 64 ? OriginalWidth - 1 : OriginalWidth + 1; //診断用幅
        const std::uint32_t TestHeight = OriginalHeight > 64 ? OriginalHeight - 1 : OriginalHeight + 1; //診断用高さ
        const bool Resized = Application.Resize(TestWidth, TestHeight); //全Sceneを含むResize結果
        Context.Check(Resized && Graphics.GetWidth() == TestWidth && Graphics.GetHeight() == TestHeight,
            "Resize | BackBuffer and Scene resources resized");
        Context.Check(!Resized || Application.Resize(OriginalWidth, OriginalHeight),
            "Resize | Original dimensions restored");

        if (DuplicateSceneID.IsValid())
        {
            Context.Check(engine.RemoveScene(DuplicateSceneID), "Scene API | Duplicated Scene removed");
        }

        if (SecondarySceneID.IsValid())
        {
            Context.Check(engine.RemoveScene(SecondarySceneID), "Scene API | Secondary Scene removed");
        }

        for (auto Iterator = CreatedObjectIDs.rbegin(); Iterator != CreatedObjectIDs.rend(); ++Iterator)
        {
            RemoveIfPresent(engine, OriginalMainSceneID, *Iterator);
        }

        const std::vector<ObjectID> RemainingObjectIDs = MainObjects.GetObjectIDs();
        const bool ProgramTombstonesInactive = std::all_of(
            RemainingObjectIDs.begin(),
            RemainingObjectIDs.end(),
            [&MainObjects](ObjectID id)
            {
                const Object* Value = MainObjects.FindObject(id);
                return Value == nullptr || Value->GetName().find("StressBox") != 0 ||
                    !Value->IsActive();
            });
        Context.Check(MainObjects.GetObjectCount() >= InitialObjectCount &&
            ProgramTombstonesInactive,
            "Cleanup | Main Program objects retained as inactive differential tombstones");
        Context.Check(MainObjects.GetComponentCount() == InitialComponentCount,
            "Cleanup | Main Scene component count restored");
        Context.Check(engine.GetMainSceneID() == OriginalMainSceneID &&
            engine.GetViewSceneID() == OriginalViewSceneID,
            "Cleanup | Main and View Scene restored");
        Context.Check(Scenes.GetSceneCount() == 1, "Cleanup | Temporary Scenes removed");
        // Asset/physics regressions use isolated objects and restore global settings.
        const std::wstring DDSPath = L"Assets/Textures/joran-quinten-CRmulUkILVg-unsplash.dds";
        Texture2D DDS;
        Context.Check(DDS.LoadFromFile(Graphics, DDSPath) && DDS.GetWidth() == 3000 && DDS.GetHeight() == 1998,
            "Assets | Provided DDS uploads without padding distortion");
        Box TexturedBox;
        Context.Check(TexturedBox.CreateGPUResource(Graphics) && TexturedBox.SetTexture(Graphics, DDSPath),
            "Assets | DDS applies to primitive meshes");
        auto BoxCopy = TexturedBox.Clone();
        auto* PrimitiveCopy = dynamic_cast<PrimitiveObject*>(BoxCopy.get());
        Context.Check(PrimitiveCopy && PrimitiveCopy->GetMesh().GetTexturePath() == DDSPath &&
            PrimitiveCopy->CreateGPUResource(Graphics), "Assets | Texture survives primitive cloning and GPU recreation");
        OBJModel TexturedModel;
        Context.Check(TexturedModel.Load(Graphics, L"12222_Cat_v1_l3.obj", DirectX::XMFLOAT4{1,1,1,1}) &&
            TexturedModel.SetTexture(Graphics, DDSPath), "Assets | DDS applies to OBJ models");
        DirectX::XMFLOAT3 PreviewCenter{}; float PreviewRadius = 0;
        TexturedModel.GetBounds(PreviewCenter, PreviewRadius);
        Context.Check(std::isfinite(PreviewRadius) && PreviewRadius > 0,
            "Assets | Model preview auto-fit bounds are finite");
        Context.Check(Graphics.SetSkyTexture(DDSPath), "Assets | DDS sky loads");
        Application.Draw();
        Context.Check(!Graphics.IsFrameOpen(), "Assets | Background and scene render together");
        Context.Check(Graphics.SetSkyTexture(L""), "Assets | Background can be removed");

        const PlaybackSettings SavedPlaybackSettings = ActivePlaybackSettings;
        ActivePlaybackSettings = {};
        ActivePlaybackSettings.LinearDrag = 0;
        ActivePlaybackSettings.GroundEnabled = false;
        ObjectManager PhysicsProbe;
        Object* FallingObject = PhysicsProbe.CreateObject<Object>("Gravity Probe");
        GravityScript* Gravity = FallingObject
            ? PhysicsProbe.AddComponent<GravityScript>(FallingObject->GetID(), "Gravity") : nullptr;
        Context.Check(Gravity && PhysicsProbe.InitializeComponents(Graphics), "Physics | Gravity script initializes");
        if (Gravity)
        {
            FallingObject->SetPosition({0,10,0});
            PhysicsProbe.UpdateComponents(0.1f);
            Context.Check(FallingObject->GetPosition().y < 10, "Physics | Default gravity moves an opted-in object");
            Gravity->InvokeExposedFunction("ResetVelocity");
            ActivePlaybackSettings.GravityY = 9.81f;
            const float PreviousHeight = FallingObject->GetPosition().y;
            PhysicsProbe.UpdateComponents(0.1f);
            Context.Check(FallingObject->GetPosition().y > PreviousHeight, "Physics | Editing base gravity reverses acceleration");
            Gravity->InvokeExposedFunction("ResetVelocity");
            ActivePlaybackSettings.GravityY = -9.81f;
            ActivePlaybackSettings.GroundEnabled = true;
            ActivePlaybackSettings.GroundHeight = 2;
            ActivePlaybackSettings.Restitution = 0;
            FallingObject->SetPosition({0,3,0});
            PhysicsProbe.UpdateComponents(2.0f);
            Context.Check(NearlyEqual(FallingObject->GetPosition().y, 2.5f), "Physics | Ground height and object offset prevent penetration");
        }
        ActivePlaybackSettings = SavedPlaybackSettings;
        Application.Draw();
        Context.Check(!Graphics.IsFrameOpen(), "Cleanup | Final frame completed");

        Context.Result.EngineLogs = MessageLog::GetInstance().GetSnapshot().Logs;
        return std::move(Context.Result);
    }

    bool WriteEngineDiagnosticReport(const EngineDiagnosticResult& result)
    {
        std::wstring ReportPath = GetEnvironmentValue(DiagnosticReportVariable); //呼出側指定のReport Path

        if (ReportPath.empty())
        {
            ReportPath = L"EngineDiagnostics.log";
        }

        std::ofstream Report(
            std::filesystem::path(ReportPath),
            std::ios::binary | std::ios::trunc
        ); //既存結果を置換する診断Report

        if (!Report)
        {
            return false;
        }

        Report << result.ToText();
        return static_cast<bool>(Report);
    }
}
