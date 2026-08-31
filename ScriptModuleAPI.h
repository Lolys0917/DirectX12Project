//|| ScriptModuleAPI.h ||::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DLLとエンジン間でC++所有権を共有せずScript関数表を受け渡すABIを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <cstdint>

#if defined(_MSC_VER)
#define ENGINE_SCRIPT_CALL __cdecl
#define ENGINE_SCRIPT_EXPORT extern "C" __declspec(dllexport)
#else
#define ENGINE_SCRIPT_CALL
#define ENGINE_SCRIPT_EXPORT extern "C"
#endif

inline constexpr std::uint32_t EngineScriptAbiVersion = 1; //関数表ABIの現行Version
inline constexpr char EngineScriptModuleEntryPoint[] =
    "EngineGetScriptModule"; //全DLLで共通利用するExport関数名

struct EngineScriptHostAPI final
{
    std::uint32_t Size; //この構造体のByte数
    std::uint32_t AbiVersion; //Hostが提供するABI Version
    void* Context; //各Callbackへ渡すHost側Context
    std::uint32_t(ENGINE_SCRIPT_CALL* GetObjectID)(void* context); //所有Object ID取得関数
    void(ENGINE_SCRIPT_CALL* AddLog)(void* context, const char* message); //UTF-8 Log追加関数
    std::uint32_t(ENGINE_SCRIPT_CALL* GetActive)(void* context); //所有Object有効状態取得関数
    void(ENGINE_SCRIPT_CALL* SetActive)(void* context, std::uint32_t active); //所有Object有効状態設定関数
    void(ENGINE_SCRIPT_CALL* GetPosition)(void* context, float output[3]); //所有Object座標取得関数
    void(ENGINE_SCRIPT_CALL* SetPosition)(void* context, const float value[3]); //所有Object座標設定関数
    void(ENGINE_SCRIPT_CALL* GetRotation)(void* context, float output[3]); //所有Object回転取得関数
    void(ENGINE_SCRIPT_CALL* SetRotation)(void* context, const float value[3]); //所有Object回転設定関数
    void(ENGINE_SCRIPT_CALL* GetScale)(void* context, float output[3]); //所有Object拡縮取得関数
    void(ENGINE_SCRIPT_CALL* SetScale)(void* context, const float value[3]); //所有Object拡縮設定関数
    std::uint32_t(ENGINE_SCRIPT_CALL* GetObjectType)(void* context); //所有Object種別取得関数
    std::uint32_t(ENGINE_SCRIPT_CALL* IsKeyDown)(void* context, std::uint32_t virtualKey); //Keyboard押下状態取得関数
    std::uint32_t(ENGINE_SCRIPT_CALL* GetColor)(void* context, float output[4]); //Primitive色取得関数
    std::uint32_t(ENGINE_SCRIPT_CALL* SetColor)(void* context, const float value[4]); //Primitive色設定関数
    std::uint32_t(ENGINE_SCRIPT_CALL* MultiplyColor)(void* context, const float multiplier[4]); //Primitive現在色へのRGBA乗算関数
};

struct EngineScriptDescriptor final
{
    std::uint32_t Size; //この構造体のByte数
    const char* TypeKey; //Module内で一意なUTF-8識別子
    const char* DisplayName; //エディター表示用UTF-8名
    void*(ENGINE_SCRIPT_CALL* Create)(const EngineScriptHostAPI* host); //DLL内Instance生成関数
    void(ENGINE_SCRIPT_CALL* Destroy)(void* instance); //DLL内Instance破棄関数
    std::uint32_t(ENGINE_SCRIPT_CALL* OnAttach)(void* instance); //所有Objectへの接続関数
    void(ENGINE_SCRIPT_CALL* OnStart)(void* instance); //初回Update直前関数
    void(ENGINE_SCRIPT_CALL* OnUpdate)(void* instance, float deltaTime); //毎Frame更新関数
    void(ENGINE_SCRIPT_CALL* OnStop)(void* instance); //実行終了関数
    void(ENGINE_SCRIPT_CALL* OnDetach)(void* instance); //所有Objectからの切断関数

    //Sizeで存在確認する追記専用の公開変数／関数Reflection API。
    std::uint32_t(ENGINE_SCRIPT_CALL* GetExposedMemberCount)(void* instance);
    bool(ENGINE_SCRIPT_CALL* GetExposedMemberInfo)(void* instance, std::uint32_t index, struct EngineScriptExposedMemberInfo* information);
    bool(ENGINE_SCRIPT_CALL* SetExposedMember)(void* instance, const char* name, const char* value);
    bool(ENGINE_SCRIPT_CALL* InvokeExposedFunction)(void* instance, const char* name);
};

struct EngineScriptExposedMemberInfo final
{
    std::uint32_t Size;
    bool Function;
    bool ReadOnly;
    char Name[128];
    char Type[64];
    char Value[256];
};

struct EngineScriptModuleDescriptor final
{
    std::uint32_t Size; //この構造体のByte数
    std::uint32_t AbiVersion; //Moduleが要求するABI Version
    const char* ModuleName; //Module表示用UTF-8名
    std::uint32_t ScriptCount; //Scripts配列の要素数
    const EngineScriptDescriptor* Scripts; //Moduleが公開するScript関数表配列
};

using EngineGetScriptModuleFunction =
    const EngineScriptModuleDescriptor*(ENGINE_SCRIPT_CALL*)(
        std::uint32_t requestedAbiVersion
    );
