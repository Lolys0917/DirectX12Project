//|| EngineExtensionAPI.h ||::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Engine本体と外部拡張DLLの間で使用する追記専用C ABIを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_09_02  v1.50  状態Monitor用Dear ImGui Widget APIを末尾追加
//||  2026_08_20  v1.20  Object／ComponentのID直接情報取得APIを末尾追加
//||  2026_08_19  v1.10  名前指定Capsule生成とID／名前指定寸法APIを末尾追加
//||  2026_08_18  v1.00  新規作成
//||

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define ENGINE_EXTENSION_CALL __cdecl
#define ENGINE_EXTENSION_EXPORT extern "C" __declspec(dllexport)
#else
#define ENGINE_EXTENSION_CALL
#define ENGINE_EXTENSION_EXPORT extern "C"
#endif

constexpr std::uint32_t EngineExtensionAbiVersion = 1;
constexpr const char* EngineExtensionEntryPoint = "EngineGetExtensionModule";

enum class EngineExternalObjectType : std::uint32_t
{
    Object,
    Box,
    Sphere,
    Plane,
    Cylinder,
    HalfSphere,
    Capsule,
    SkyBox,
    Folder
};

enum class EngineExternalComponentType : std::uint32_t
{
    Component,
    Mesh,
    Polygon,
    Model,
    Camera,
    Grid,
    Collider,
    BoxCollider,
    SphereCollider,
    CapsuleCollider,
    CylinderCollider,
    PlaneCollider,
    Script
};

struct EngineExternalVector3 final
{
    float X;
    float Y;
    float Z;
};

struct EngineExternalTransform final
{
    EngineExternalVector3 Position;
    EngineExternalVector3 Rotation;
    EngineExternalVector3 Scale;
};

struct EngineExternalColor final
{
    float Red;
    float Green;
    float Blue;
    float Alpha;
};

struct EngineExternalSceneInfo final
{
    std::uint32_t Size;
    std::uint32_t SceneID;
    bool Active;
    bool ViewScene;
    bool MainScene;
    char Name[128];
};

struct EngineExternalObjectInfo final
{
    std::uint32_t Size;
    std::uint32_t SceneID;
    std::uint32_t ObjectID;
    std::uint32_t ParentObjectID;
    std::uint32_t ObjectType;
    std::uint32_t ComponentCount;
    bool Active;
    char Name[128];
    EngineExternalTransform LocalTransform;
    char Group[128];
    char Tag[128];
    std::uint32_t Layer;
    std::int32_t GroupOrder;
    std::int32_t ExecutionOrder;
};

struct EngineExternalComponentInfo final
{
    std::uint32_t Size;
    std::uint32_t SceneID;
    std::uint32_t ObjectID;
    std::uint32_t ComponentID;
    std::uint32_t ComponentType;
    bool Active;
    char Name[128];
};

struct EngineExternalScriptInfo final
{
    std::uint32_t Size;
    char Key[128];
    char DisplayName[128];
    char ModuleName[128];
};

struct EngineExternalGameObjectTemplateInfo final
{
    std::uint32_t Size;
    std::uint32_t SceneID;
    std::uint32_t ObjectID;
    float MoveSpeed;
    float MaximumHealth;
    char GameplayTag[128];
};

struct EngineHostAPI final
{
    std::uint32_t Size;
    std::uint32_t AbiVersion;
    void* Context;

    void (ENGINE_EXTENSION_CALL* AddLog)(void* context, const char* message);
    std::uint64_t (ENGINE_EXTENSION_CALL* GetFrameNumber)(void* context);
    float (ENGINE_EXTENSION_CALL* GetDeltaTime)(void* context);

    std::uint32_t (ENGINE_EXTENSION_CALL* GetSceneCount)(void* context);
    bool (ENGINE_EXTENSION_CALL* GetSceneInfo)(void* context, std::uint32_t index, EngineExternalSceneInfo* information);
    bool (ENGINE_EXTENSION_CALL* SetSceneActive)(void* context, std::uint32_t sceneID, bool active);
    bool (ENGINE_EXTENSION_CALL* SetViewScene)(void* context, std::uint32_t sceneID);

    std::uint32_t (ENGINE_EXTENSION_CALL* GetObjectCount)(void* context, std::uint32_t sceneID);
    bool (ENGINE_EXTENSION_CALL* GetObjectInfo)(void* context, std::uint32_t sceneID, std::uint32_t index, EngineExternalObjectInfo* information);
    std::uint32_t (ENGINE_EXTENSION_CALL* CreateObject)(void* context, std::uint32_t sceneID, std::uint32_t objectType, const char* name, std::uint32_t parentObjectID);
    bool (ENGINE_EXTENSION_CALL* RemoveObject)(void* context, std::uint32_t sceneID, std::uint32_t objectID);
    bool (ENGINE_EXTENSION_CALL* RenameObject)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const char* name);
    bool (ENGINE_EXTENSION_CALL* SetObjectActive)(void* context, std::uint32_t sceneID, std::uint32_t objectID, bool active);
    bool (ENGINE_EXTENSION_CALL* SetObjectTransform)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const EngineExternalTransform* transform);
    bool (ENGINE_EXTENSION_CALL* SetObjectParent)(void* context, std::uint32_t sceneID, std::uint32_t objectID, std::uint32_t parentObjectID, bool keepWorldTransform);

    std::uint32_t (ENGINE_EXTENSION_CALL* GetComponentCount)(void* context, std::uint32_t sceneID, std::uint32_t objectID);
    bool (ENGINE_EXTENSION_CALL* GetComponentInfo)(void* context, std::uint32_t sceneID, std::uint32_t objectID, std::uint32_t index, EngineExternalComponentInfo* information);
    bool (ENGINE_EXTENSION_CALL* RemoveComponent)(void* context, std::uint32_t sceneID, std::uint32_t componentID);
    bool (ENGINE_EXTENSION_CALL* RenameComponent)(void* context, std::uint32_t sceneID, std::uint32_t componentID, const char* name);
    bool (ENGINE_EXTENSION_CALL* SetComponentActive)(void* context, std::uint32_t sceneID, std::uint32_t componentID, bool active);

    std::uint64_t (ENGINE_EXTENSION_CALL* GetEngineRevision)(void* context);
    std::uint32_t (ENGINE_EXTENSION_CALL* GetMainSceneID)(void* context);
    std::uint32_t (ENGINE_EXTENSION_CALL* GetViewSceneID)(void* context);
    std::uint32_t (ENGINE_EXTENSION_CALL* FindSceneByName)(void* context, const char* name);
    std::uint32_t (ENGINE_EXTENSION_CALL* CreateScene)(void* context, const char* name, std::uint32_t width, std::uint32_t height);
    std::uint32_t (ENGINE_EXTENSION_CALL* DuplicateScene)(void* context, std::uint32_t sourceSceneID, const char* name);
    bool (ENGINE_EXTENSION_CALL* RemoveScene)(void* context, std::uint32_t sceneID);
    bool (ENGINE_EXTENSION_CALL* SetMainScene)(void* context, std::uint32_t sceneID);

    std::uint32_t (ENGINE_EXTENSION_CALL* FindObjectByName)(void* context, std::uint32_t sceneID, std::uint32_t objectType, const char* name);
    std::uint32_t (ENGINE_EXTENSION_CALL* GetChildCount)(void* context, std::uint32_t sceneID, std::uint32_t objectID);
    std::uint32_t (ENGINE_EXTENSION_CALL* GetChildObjectID)(void* context, std::uint32_t sceneID, std::uint32_t objectID, std::uint32_t index);
    std::uint32_t (ENGINE_EXTENSION_CALL* DuplicateObject)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const char* name);

    std::uint32_t (ENGINE_EXTENSION_CALL* CreateGameObjectTemplate)(void* context, std::uint32_t sceneID, const char* name, std::uint32_t parentObjectID);
    bool (ENGINE_EXTENSION_CALL* GetGameObjectTemplateInfo)(void* context, std::uint32_t sceneID, std::uint32_t objectID, EngineExternalGameObjectTemplateInfo* information);
    bool (ENGINE_EXTENSION_CALL* SetGameObjectTemplateInfo)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const EngineExternalGameObjectTemplateInfo* information);

    std::uint32_t (ENGINE_EXTENSION_CALL* GetScriptCount)(void* context);
    bool (ENGINE_EXTENSION_CALL* GetScriptInfo)(void* context, std::uint32_t index, EngineExternalScriptInfo* information);
    bool (ENGINE_EXTENSION_CALL* AttachScript)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const char* scriptKey);

    bool (ENGINE_EXTENSION_CALL* GetObjectColor)(void* context, std::uint32_t sceneID, std::uint32_t objectID, EngineExternalColor* color);
    bool (ENGINE_EXTENSION_CALL* SetObjectColor)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const EngineExternalColor* color);
    bool (ENGINE_EXTENSION_CALL* IsKeyDown)(void* context, std::uint32_t virtualKey);
    bool (ENGINE_EXTENSION_CALL* MultiplyObjectColor)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const EngineExternalColor* multiplier);
    bool (ENGINE_EXTENSION_CALL* SetProgramSuggestion)(void* context, const char* suggestion);

    //v1.10以降の追記専用API。Sizeで存在確認してから古いHostとの互換性を保って使用する。
    std::uint32_t (ENGINE_EXTENSION_CALL* FindObjectByNameOnly)(void* context, std::uint32_t sceneID, const char* name);
    std::uint32_t (ENGINE_EXTENSION_CALL* CreateCapsuleModel)(void* context, std::uint32_t sceneID, const char* name, std::uint32_t parentObjectID);
    bool (ENGINE_EXTENSION_CALL* SetObjectSize)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const EngineExternalVector3* size);
    bool (ENGINE_EXTENSION_CALL* SetObjectSizeByName)(void* context, std::uint32_t sceneID, const char* name, const EngineExternalVector3* size);

    //v1.20以降の追記専用API。安定ID Handleから一覧走査なしで情報を取得する。
    bool (ENGINE_EXTENSION_CALL* GetObjectInfoByID)(void* context, std::uint32_t sceneID, std::uint32_t objectID, EngineExternalObjectInfo* information);
    bool (ENGINE_EXTENSION_CALL* GetComponentInfoByID)(void* context, std::uint32_t sceneID, std::uint32_t componentID, EngineExternalComponentInfo* information);

    //v1.30以降。Objectの分類とGroup Scheduler順をまとめて設定する。
    bool (ENGINE_EXTENSION_CALL* SetObjectOrganization)(void* context, std::uint32_t sceneID, std::uint32_t objectID, const char* group, const char* tag, std::uint32_t layer, std::int32_t groupOrder, std::int32_t executionOrder);

    //v1.40以降。Engine所有Dear ImGui Contextへ安全なWidget操作だけを転送する。
    bool (ENGINE_EXTENSION_CALL* BeginImGuiWindow)(void* context, const char* name);
    void (ENGINE_EXTENSION_CALL* EndImGuiWindow)(void* context);
    void (ENGINE_EXTENSION_CALL* ImGuiText)(void* context, const char* text);
    bool (ENGINE_EXTENSION_CALL* ImGuiButton)(void* context, const char* label);

    //v1.50以降。状態Monitor用のTab、分類、負荷可視化Widget。
    bool (ENGINE_EXTENSION_CALL* BeginImGuiTabBar)(void* context, const char* identifier);
    void (ENGINE_EXTENSION_CALL* EndImGuiTabBar)(void* context);
    bool (ENGINE_EXTENSION_CALL* BeginImGuiTabItem)(void* context, const char* label);
    void (ENGINE_EXTENSION_CALL* EndImGuiTabItem)(void* context);
    bool (ENGINE_EXTENSION_CALL* ImGuiCollapsingHeader)(void* context, const char* label, bool defaultOpen);
    void (ENGINE_EXTENSION_CALL* ImGuiSeparator)(void* context);
    void (ENGINE_EXTENSION_CALL* ImGuiProgressBar)(void* context, float fraction, const char* overlay);
    void (ENGINE_EXTENSION_CALL* ImGuiPlotLines)(void* context, const char* label, const float* values, std::uint32_t valueCount, float minimum, float maximum);
};

struct EngineExtensionModuleDescriptor final
{
    std::uint32_t Size;
    std::uint32_t AbiVersion;
    const char* ModuleName;
    std::uint32_t ModuleVersion;

    void* (ENGINE_EXTENSION_CALL* Create)(const EngineHostAPI* host);
    void (ENGINE_EXTENSION_CALL* Destroy)(void* instance);
    void (ENGINE_EXTENSION_CALL* Update)(void* instance, float deltaTime);
    std::uint32_t (ENGINE_EXTENSION_CALL* GetStateSize)(void* instance);
    bool (ENGINE_EXTENSION_CALL* SaveState)(void* instance, void* destination, std::uint32_t destinationSize);
    bool (ENGINE_EXTENSION_CALL* LoadState)(void* instance, const void* source, std::uint32_t sourceSize);

    //Descriptor Sizeで存在確認する任意の停止中対応UI構築Callback。
    void (ENGINE_EXTENSION_CALL* BuildUserInterface)(void* instance);
};

using EngineGetExtensionModuleFunction = const EngineExtensionModuleDescriptor* (
    ENGINE_EXTENSION_CALL*)(std::uint32_t requestedAbiVersion);
