//|| RotationScriptModule.cpp ||:::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  共通名EngineGetScriptModuleをExportする外部DLL Scriptの最小例を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#include "../../ScriptModuleAPI.h"

#include <iterator>
#include <new>

namespace
{
    struct RotationScriptState final
    {
        const EngineScriptHostAPI* Host = nullptr; //Engine Object操作用Host関数表
        float RadiansPerSecond = 1.0f; //Y軸へ毎秒加算する回転量
    };

    //概要：DLL内で回転Script Instanceを生成する
    //引数：host=所有Objectを操作するEngine Host関数表
    //戻り値：DLL内Instance、生成失敗時はnullptr
    void* ENGINE_SCRIPT_CALL CreateRotationScript(const EngineScriptHostAPI* host)
    {
        if (host == nullptr || host->Size < sizeof(EngineScriptHostAPI) ||
            host->AbiVersion != EngineScriptAbiVersion)
        {
            return nullptr;
        }

        auto* State = new (std::nothrow) RotationScriptState(); //DLL側が所有するScript状態

        if (State != nullptr)
        {
            State->Host = host;
        }

        return State;
    }

    //概要：DLL内で生成した回転Script Instanceを同じDLL内で破棄する
    //引数：instance=CreateRotationScriptが返したInstance
    //戻り値：なし
    void ENGINE_SCRIPT_CALL DestroyRotationScript(void* instance)
    {
        delete static_cast<RotationScriptState*>(instance);
    }

    //概要：ScriptがObjectへ差し込まれたことを確認する
    //引数：instance=接続する回転Script Instance
    //戻り値：Host関数表と所有Objectが利用可能な場合は1
    std::uint32_t ENGINE_SCRIPT_CALL AttachRotationScript(void* instance)
    {
        const auto* State = static_cast<const RotationScriptState*>(instance); //接続を確認するScript状態

        if (State == nullptr || State->Host == nullptr ||
            State->Host->GetObjectID == nullptr)
        {
            return 0;
        }

        return State->Host->GetObjectID(State->Host->Context) != 0 ? 1u : 0u;
    }

    //概要：Host関数表で所有ObjectのY回転を毎フレーム更新する
    //引数：instance=実行する回転Script Instance、deltaTime=前フレームからの経過秒数
    //戻り値：なし
    void ENGINE_SCRIPT_CALL UpdateRotationScript(void* instance, float deltaTime)
    {
        const auto* State = static_cast<const RotationScriptState*>(instance); //実行するScript状態

        if (State == nullptr || State->Host == nullptr ||
            State->Host->GetRotation == nullptr || State->Host->SetRotation == nullptr)
        {
            return;
        }

        float Rotation[3]{}; //所有Objectの現在XYZ回転角
        State->Host->GetRotation(State->Host->Context, Rotation);
        Rotation[1] += State->RadiansPerSecond * deltaTime;
        State->Host->SetRotation(State->Host->Context, Rotation);
    }
}

//概要：このDLLが提供する全Script関数表をEngineへ返す
//引数：requestedAbiVersion=Engineが要求するABI Version
//戻り値：互換性のあるModule関数表、Version不一致時はnullptr
ENGINE_SCRIPT_EXPORT const EngineScriptModuleDescriptor* ENGINE_SCRIPT_CALL
EngineGetScriptModule(std::uint32_t requestedAbiVersion)
{
    static const EngineScriptDescriptor Scripts[] =
    {
        {
            sizeof(EngineScriptDescriptor),
            "rotation",
            "DLL Rotation Script",
            CreateRotationScript,
            DestroyRotationScript,
            AttachRotationScript,
            nullptr,
            UpdateRotationScript,
            nullptr,
            nullptr
        }
    }; //このModuleが公開するScript関数表

    static const EngineScriptModuleDescriptor Module
    {
        sizeof(EngineScriptModuleDescriptor),
        EngineScriptAbiVersion,
        "Sample.Rotation",
        static_cast<std::uint32_t>(std::size(Scripts)),
        Scripts
    }; //Engineへ返す静的Module記述子

    return requestedAbiVersion == EngineScriptAbiVersion ? &Module : nullptr;
}
