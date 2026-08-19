#include "EngineExtensionAPI.h"

#include <cstdint>
#include <cstring>
#include <new>

namespace
{
    struct GameObjectProgramState final
    {
        const EngineHostAPI* Host = nullptr;
        std::uint32_t SceneID = 0;
        std::uint32_t ObjectID = 0;
    };

    //概要：既存View Sceneへゲーム用Object雛形を一度だけ作成してScriptを差し込む
    //引数：host=版番号付きEngine API関数表
    //戻り値：作成したProgram状態、API不整合又は確保失敗時はnullptr
    void* ENGINE_EXTENSION_CALL CreateGameObjectProgram(const EngineHostAPI* host)
    {
        if (host == nullptr || host->Size < sizeof(EngineHostAPI) ||
            host->AbiVersion != EngineExtensionAbiVersion)
        {
            return nullptr;
        }

        auto* State = new (std::nothrow) GameObjectProgramState{}; //Hot Reload間で移行する状態

        if (State == nullptr)
        {
            return nullptr;
        }

        State->Host = host;
        State->SceneID = host->GetViewSceneID(host->Context);
        State->ObjectID = host->FindObjectByName(
            host->Context,
            State->SceneID,
            static_cast<std::uint32_t>(EngineExternalObjectType::Object),
            "GameObjectTemplate"
        );

        const bool NeedsScript = State->ObjectID == 0; //新規ObjectだけへScriptを差し込む場合true

        if (NeedsScript)
        {
            State->ObjectID = host->CreateGameObjectTemplate(
                host->Context,
                State->SceneID,
                "GameObjectTemplate",
                0
            );
        }

        if (State->ObjectID != 0)
        {
            EngineExternalGameObjectTemplateInfo Information{}; //雛形へ設定するゲーム固有値
            Information.Size = sizeof(Information);
            Information.SceneID = State->SceneID;
            Information.ObjectID = State->ObjectID;
            Information.MoveSpeed = 5.0f;
            Information.MaximumHealth = 100.0f;
            strcpy_s(Information.GameplayTag, "Player");
            host->SetGameObjectTemplateInfo(
                host->Context,
                State->SceneID,
                State->ObjectID,
                &Information
            );

            if (NeedsScript)
            {
                host->AttachScript(
                    host->Context,
                    State->SceneID,
                    State->ObjectID,
                    "native.rotation"
                );
            }
        }

        return State;
    }

    //概要：ゲーム用Object Program状態を破棄する
    //引数：instance=CreateGameObjectProgramが返した状態
    //戻り値：なし
    void ENGINE_EXTENSION_CALL DestroyGameObjectProgram(void* instance)
    {
        delete static_cast<GameObjectProgramState*>(instance);
    }

    //概要：ゲーム固有の毎Frame処理を追加するための空の更新雛形を実行する
    //引数：instance=Program状態、deltaTime=前回更新からの秒数
    //戻り値：なし
    void ENGINE_EXTENSION_CALL UpdateGameObjectProgram(void* instance, float deltaTime)
    {
        (void)instance;
        (void)deltaTime;
    }

    //概要：Hot Reloadで引き継ぐObject IDのByte数を取得する
    //引数：instance=Program状態
    //戻り値：有効状態の場合はObject IDのByte数、無効時は0
    std::uint32_t ENGINE_EXTENSION_CALL GetGameObjectProgramStateSize(void* instance)
    {
        return instance == nullptr ? 0u : sizeof(std::uint32_t);
    }

    //概要：Hot Reload前のObject IDをEngine所有Bufferへ保存する
    //引数：instance=Program状態、destination=保存先、destinationSize=保存先Byte数
    //戻り値：状態を保存できた場合はtrue
    bool ENGINE_EXTENSION_CALL SaveGameObjectProgramState(
        void* instance,
        void* destination,
        std::uint32_t destinationSize
    )
    {
        if (instance == nullptr || destination == nullptr ||
            destinationSize < sizeof(std::uint32_t))
        {
            return false;
        }

        std::memcpy(
            destination,
            &static_cast<GameObjectProgramState*>(instance)->ObjectID,
            sizeof(std::uint32_t)
        );
        return true;
    }

    //概要：Hot Reload後のProgram状態へ旧Object IDを復元する
    //引数：instance=新Program状態、source=旧状態Buffer、sourceSize=旧状態Byte数
    //戻り値：状態を復元できた場合はtrue
    bool ENGINE_EXTENSION_CALL LoadGameObjectProgramState(
        void* instance,
        const void* source,
        std::uint32_t sourceSize
    )
    {
        if (instance == nullptr || source == nullptr ||
            sourceSize < sizeof(std::uint32_t))
        {
            return false;
        }

        std::memcpy(
            &static_cast<GameObjectProgramState*>(instance)->ObjectID,
            source,
            sizeof(std::uint32_t)
        );
        return true;
    }
}

//概要：Engineが共通名から取得するゲーム用Object Program定義を返す
//引数：requestedAbiVersion=Engineが要求するC ABI版番号
//戻り値：互換Module定義、版不一致時はnullptr
ENGINE_EXTENSION_EXPORT const EngineExtensionModuleDescriptor*
ENGINE_EXTENSION_CALL EngineGetExtensionModule(std::uint32_t requestedAbiVersion)
{
    static const EngineExtensionModuleDescriptor Descriptor
    {
        sizeof(EngineExtensionModuleDescriptor),
        EngineExtensionAbiVersion,
        "GameObjectProgramTemplate",
        1,
        CreateGameObjectProgram,
        DestroyGameObjectProgram,
        UpdateGameObjectProgram,
        GetGameObjectProgramStateSize,
        SaveGameObjectProgramState,
        LoadGameObjectProgramState
    };
    return requestedAbiVersion == EngineExtensionAbiVersion ? &Descriptor : nullptr;
}
