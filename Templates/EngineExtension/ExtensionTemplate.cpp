#include "EngineExtensionAPI.h"

#include <cstdint>
#include <new>

namespace
{
    struct ExtensionState final
    {
        const EngineHostAPI* Host = nullptr;
        std::uint64_t UpdateCount = 0;
    };

    //概要：追加機能のInstanceを作成する
    //引数：host=版番号付きEngine読取編集API
    //戻り値：作成したInstance、互換性不一致又は確保失敗時はnullptr
    void* ENGINE_EXTENSION_CALL CreateExtension(const EngineHostAPI* host)
    {
        if (host == nullptr || host->Size < sizeof(EngineHostAPI) ||
            host->AbiVersion != EngineExtensionAbiVersion)
        {
            return nullptr;
        }

        ExtensionState* State = new (std::nothrow) ExtensionState{};

        if (State != nullptr)
        {
            State->Host = host;
            host->AddLog(host->Context, "[Info] ExtensionTemplate | Attached.");
        }

        return State;
    }

    //概要：追加機能のInstanceを破棄する
    //引数：instance=CreateExtensionが返した状態
    //戻り値：なし
    void ENGINE_EXTENSION_CALL DestroyExtension(void* instance)
    {
        delete static_cast<ExtensionState*>(instance);
    }

    //概要：追加機能をEngine固定更新ごとに実行する
    //引数：instance=追加機能状態、deltaTime=前回更新からの秒数
    //戻り値：なし
    void ENGINE_EXTENSION_CALL UpdateExtension(void* instance, float deltaTime)
    {
        ExtensionState* State = static_cast<ExtensionState*>(instance);
        (void)deltaTime;

        if (State != nullptr)
        {
            ++State->UpdateCount;
        }
    }
}

//概要：Engineが共通名から取得する追加機能の関数表を返す
//引数：requestedAbiVersion=Engineが要求するABI版番号
//戻り値：互換Descriptor、版不一致時はnullptr
ENGINE_EXTENSION_EXPORT const EngineExtensionModuleDescriptor*
ENGINE_EXTENSION_CALL EngineGetExtensionModule(std::uint32_t requestedAbiVersion)
{
    static const EngineExtensionModuleDescriptor Descriptor
    {
        sizeof(EngineExtensionModuleDescriptor),
        EngineExtensionAbiVersion,
        "EngineExtensionTemplate",
        1,
        CreateExtension,
        DestroyExtension,
        UpdateExtension,
        nullptr,
        nullptr,
        nullptr
    };
    return requestedAbiVersion == EngineExtensionAbiVersion ? &Descriptor : nullptr;
}
