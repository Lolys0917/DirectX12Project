//|| EngineAPI.h ||::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Native Main Programと外部ToolからEngine全機能へ到達するFacadeを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.10  名前指定とID指定を共有するPrimitive生成・寸法APIを追加
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "EditorTypes.h"
#include "ExtensionSystem.h"
#include "ScriptSystem.h"

namespace Engine
{
    class DirectX12;
    class Capsule;
    class GameObjectTemplate;
    class GameApp;
    class Object;
    class Scene;
    class SceneManager;

    class EngineAPI final
    {
    public:
        explicit EngineAPI(GameApp& application);
        ~EngineAPI();

        EngineAPI(const EngineAPI&) = delete;
        EngineAPI& operator=(const EngineAPI&) = delete;

        GameApp& GetApplication();
        const GameApp& GetApplication() const;
        DirectX12& GetDirectX12();
        const DirectX12& GetDirectX12() const;
        SceneManager& GetSceneManager();
        const SceneManager& GetSceneManager() const;
        ScriptRegistry& GetScriptRegistry();
        const ScriptRegistry& GetScriptRegistry() const;
        ScriptModuleManager& GetScriptModuleManager();
        const ScriptModuleManager& GetScriptModuleManager() const;
        ExtensionModuleManager& GetExtensionModuleManager();
        const ExtensionModuleManager& GetExtensionModuleManager() const;

        std::vector<SceneID> GetSceneIDs() const;
        std::vector<ObjectID> GetObjectIDs(SceneID sceneID) const;
        std::vector<ObjectID> GetChildObjectIDs(SceneID sceneID, ObjectID objectID) const;
        std::vector<ComponentID> GetComponentIDs(SceneID sceneID, ObjectID objectID) const;
        SceneID FindSceneID(const std::string& name) const;
        ObjectID FindObjectID(SceneID sceneID, const std::string& name) const;
        ObjectID FindObjectID(SceneID sceneID, ObjectType objectType, const std::string& name) const;
        bool TryGetSceneInfo(SceneID sceneID, EditorSceneInfo& information) const;
        bool TryGetObjectInfo(SceneID sceneID, ObjectID objectID, EditorObjectInfo& information) const;
        bool TryGetComponentInfo(SceneID sceneID, ComponentID componentID, EditorComponentInfo& information) const;

        Object* CreateObject(
            SceneID sceneID,
            ObjectType objectType,
            const std::string& requestedName,
            ObjectID parentID = ObjectID()
        );
        Capsule* CreateCapsuleModel(
            SceneID sceneID,
            const std::string& requestedName,
            ObjectID parentID = ObjectID()
        );
        GameObjectTemplate* CreateGameObjectTemplate(
            SceneID sceneID,
            const std::string& requestedName,
            ObjectID parentID = ObjectID()
        );
        GameObjectTemplate* FindGameObjectTemplate(SceneID sceneID, ObjectID objectID);
        const GameObjectTemplate* FindGameObjectTemplate(SceneID sceneID, ObjectID objectID) const;
        bool SetGameObjectTemplateInfo(
            SceneID sceneID,
            ObjectID objectID,
            const std::string& gameplayTag,
            float moveSpeed,
            float maximumHealth
        );
        SceneID CreateScene(const std::string& name, std::uint32_t width, std::uint32_t height);
        SceneID DuplicateScene(SceneID sourceSceneID, const std::string& name);
        bool RemoveScene(SceneID sceneID);
        bool SetMainScene(SceneID sceneID);
        SceneID GetMainSceneID() const;
        SceneID GetViewSceneID() const;
        bool SetSceneActive(SceneID sceneID, bool active);
        bool SetViewScene(SceneID sceneID);
        Object* DuplicateObject(
            SceneID sceneID,
            ObjectID objectID,
            const std::string& requestedName = std::string()
        );
        bool RemoveObject(SceneID sceneID, ObjectID objectID);
        bool RenameObject(SceneID sceneID, ObjectID objectID, const std::string& name);
        bool SetObjectActive(SceneID sceneID, ObjectID objectID, bool active);
        bool SetObjectTransform(SceneID sceneID, ObjectID objectID, const EditorTransformInfo& transform);
        bool SetObjectOrganization(
            SceneID sceneID,
            ObjectID objectID,
            const std::string& group,
            const std::string& tag,
            std::uint32_t layer,
            std::int32_t groupOrder,
            std::int32_t executionOrder
        );
        bool SetGroupActive(SceneID sceneID, const std::string& group, bool active);
        bool SetObjectSize(SceneID sceneID, ObjectID objectID, const EditorVector3& size);
        bool SetObjectSize(SceneID sceneID, const std::string& name, const EditorVector3& size);
        bool TryGetObjectColor(SceneID sceneID, ObjectID objectID, EditorColor& color) const;
        bool SetObjectColor(SceneID sceneID, ObjectID objectID, const EditorColor& color);
        bool MultiplyObjectColor(SceneID sceneID, ObjectID objectID, const EditorColor& multiplier);
        bool SetProgramSuggestion(const std::string& suggestion);
        bool IsKeyDown(std::uint32_t virtualKey) const;
        bool SetObjectParent(SceneID sceneID, ObjectID objectID, ObjectID parentID, bool keepWorldTransform);
        bool RemoveComponent(SceneID sceneID, ComponentID componentID);
        bool RenameComponent(SceneID sceneID, ComponentID componentID, const std::string& name);
        bool SetComponentActive(SceneID sceneID, ComponentID componentID, bool active);
        bool SetScriptMember(
            SceneID sceneID,
            ComponentID componentID,
            const std::string& member,
            const std::string& value
        );
        bool InvokeScriptFunction(
            SceneID sceneID,
            ComponentID componentID,
            const std::string& function
        );
        bool AttachScript(
            SceneID sceneID,
            ObjectID objectID,
            const std::string& scriptKey
        );
        void UpdateExtensions(float deltaTime);
        bool CapturePlaybackState();
        bool RestorePlaybackState();
        bool ExecuteEditorCommand(const EditorCommand& command);
        EditorSnapshot CreateEditorSnapshot() const;
        std::uint64_t GetRevision() const;

        //Main Programの新旧世代が宣言したObject集合を差分同期する
        void BeginProgramObjectSynchronization();
        void RecordProgramObjectDeclaration(
            SceneID sceneID,
            ObjectID objectID,
            bool created
        );
        void CommitProgramObjectSynchronization();
        void CancelProgramObjectSynchronization();

    private:
        Scene* ResolveScene(SceneID sceneID);
        const Scene* ResolveScene(SceneID sceneID) const;
        bool DeleteObject(SceneID sceneID, ObjectID objectID);
        bool DeleteComponent(SceneID sceneID, ComponentID componentID);
        void IncrementRevision();
        static std::uint64_t MakeProgramObjectKey(SceneID sceneID, ObjectID objectID);

        GameApp& Application; //描画、Scene、Object APIの所有元
        ScriptRegistry Scripts; //NativeとDLL Script Factory Registry
        ScriptModuleManager Modules; //DLL Handleと関数表の寿命管理器
        ExtensionModuleManager Extensions; //Program DLL Hot Reloadと外部API管理器
        std::uint64_t Revision; //Editor構造変更番号
        std::unordered_set<std::uint64_t> KnownProgramObjects; //過去にProgramが作成した再利用可能Object
        std::unordered_set<std::uint64_t> ManagedProgramObjects; //前世代が宣言したObject
        std::unordered_set<std::uint64_t> PendingProgramObjects; //新世代が宣言したObject
        std::unordered_set<std::uint64_t> PendingCreatedProgramObjects; //同期中に新規作成したObject
        bool ProgramObjectSynchronizationActive = false; //宣言収集中の場合true
    };
}
