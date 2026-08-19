//|| EngineAPI.h ||::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Native Main Programと外部ToolからEngine全機能へ到達するFacadeを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EditorTypes.h"
#include "ExtensionSystem.h"
#include "ScriptSystem.h"

namespace Engine
{
    class DirectX12;
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
        bool TryGetObjectColor(SceneID sceneID, ObjectID objectID, EditorColor& color) const;
        bool SetObjectColor(SceneID sceneID, ObjectID objectID, const EditorColor& color);
        bool MultiplyObjectColor(SceneID sceneID, ObjectID objectID, const EditorColor& multiplier);
        bool SetProgramSuggestion(const std::string& suggestion);
        bool IsKeyDown(std::uint32_t virtualKey) const;
        bool SetObjectParent(SceneID sceneID, ObjectID objectID, ObjectID parentID, bool keepWorldTransform);
        bool RemoveComponent(SceneID sceneID, ComponentID componentID);
        bool RenameComponent(SceneID sceneID, ComponentID componentID, const std::string& name);
        bool SetComponentActive(SceneID sceneID, ComponentID componentID, bool active);
        bool AttachScript(
            SceneID sceneID,
            ObjectID objectID,
            const std::string& scriptKey
        );
        void UpdateExtensions(float deltaTime);
        bool ExecuteEditorCommand(const EditorCommand& command);
        EditorSnapshot CreateEditorSnapshot() const;
        std::uint64_t GetRevision() const;

    private:
        Scene* ResolveScene(SceneID sceneID);
        const Scene* ResolveScene(SceneID sceneID) const;
        bool DeleteObject(SceneID sceneID, ObjectID objectID);
        bool DeleteComponent(SceneID sceneID, ComponentID componentID);
        void IncrementRevision();

        GameApp& Application; //描画、Scene、Object APIの所有元
        ScriptRegistry Scripts; //NativeとDLL Script Factory Registry
        ScriptModuleManager Modules; //DLL Handleと関数表の寿命管理器
        ExtensionModuleManager Extensions; //Program DLL Hot Reloadと外部API管理器
        std::uint64_t Revision; //Editor構造変更番号
    };
}
