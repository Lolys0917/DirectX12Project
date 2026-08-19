#include "EngineExtensionAPI.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

namespace
{
    struct ProgramState final
    {
        const EngineHostAPI* Host = nullptr;
        std::uint32_t SceneID = 0;
        std::uint32_t CapsuleID = 0;
        float ElapsedTime = 0.0f;
    };

    struct SavedProgramState final
    {
        float ElapsedTime = 0.0f;
    };

    //概要：Main Sceneへ前後往復する色付きCapsuleを作成してMain Program状態を初期化する
    //引数：host=版番号付き外部Engine API関数表
    //戻り値：作成したMain Program状態、API不整合又は確保失敗時はnullptr
    void* ENGINE_EXTENSION_CALL CreateProgram(const EngineHostAPI* host)
    {
        if (host == nullptr || host->Size < sizeof(EngineHostAPI) ||
            host->AbiVersion != EngineExtensionAbiVersion)
        {
            return nullptr;
        }

        ProgramState* State = new (std::nothrow) ProgramState{}; //Main DLLが所有する状態

        if (State == nullptr)
        {
            return nullptr;
        }

        State->Host = host;
        State->SceneID = host->GetMainSceneID(host->Context);

        if (State->SceneID == 0 && host->GetSceneCount(host->Context) > 0)
        {
            EngineExternalSceneInfo Scene{}; //先頭Sceneの情報
            Scene.Size = sizeof(EngineExternalSceneInfo);

            if (host->GetSceneInfo(host->Context, 0, &Scene))
            {
                State->SceneID = Scene.SceneID;
            }
        }

        State->CapsuleID = host->FindObjectByName(
            host->Context,
            State->SceneID,
            static_cast<std::uint32_t>(EngineExternalObjectType::Capsule),
            "MainOscillatingCapsule"
        );

        if (State->CapsuleID == 0)
        {
            State->CapsuleID = host->CreateObject(
                host->Context,
                State->SceneID,
                static_cast<std::uint32_t>(EngineExternalObjectType::Capsule),
                "MainOscillatingCapsule",
                0
            );
        }

        if (State->CapsuleID == 0)
        {
            delete State;
            return nullptr;
        }

        EngineExternalTransform Transform{}; //Capsuleの基準Transform
        Transform.Position = { 3.0f, 1.0f, 2.0f };
        Transform.Rotation = { 0.0f, 0.0f, 0.0f };
        Transform.Scale = { 1.2f, 1.2f, 1.2f };
        host->SetObjectTransform(
            host->Context,
            State->SceneID,
            State->CapsuleID,
            &Transform
        );
        const EngineExternalColor BaseColor{ 0.95f, 0.4f, 0.18f, 1.0f }; //RGB直接指定色
        host->SetObjectColor(
            host->Context,
            State->SceneID,
            State->CapsuleID,
            &BaseColor
        );
        const EngineExternalColor ColorMultiplier{ 1.0f, 0.7f, 0.8f, 1.0f }; //現在色への乗算係数
        host->MultiplyObjectColor(
            host->Context,
            State->SceneID,
            State->CapsuleID,
            &ColorMultiplier
        );
        host->SetProgramSuggestion(host->Context, "MainOscillatingCapsule");
        host->SetProgramSuggestion(host->Context, "SetObjectTransform");
        host->SetProgramSuggestion(host->Context, "MultiplyObjectColor");
        host->AddLog(
            host->Context,
            "[Info] MainProgram | MainOscillatingCapsule created by external API."
        );
        return State;
    }

    //概要：Main DLLが所有するProgram状態を破棄する
    //引数：instance=CreateProgramが返した状態
    //戻り値：なし
    void ENGINE_EXTENSION_CALL DestroyProgram(void* instance)
    {
        delete static_cast<ProgramState*>(instance);
    }

    //概要：SceneとAttach済みSub Scriptより先にCapsuleをZ軸の前後へ往復させる
    //引数：instance=Program状態、deltaTime=前回更新からの秒数
    //戻り値：なし
    void ENGINE_EXTENSION_CALL UpdateProgram(void* instance, float deltaTime)
    {
        ProgramState* State = static_cast<ProgramState*>(instance); //更新するMain Program状態

        if (State == nullptr || State->Host == nullptr || State->CapsuleID == 0)
        {
            return;
        }

        State->ElapsedTime += deltaTime;
        EngineExternalTransform Transform{}; //今回Capsuleへ設定するLocal Transform
        Transform.Position =
        {
            3.0f,
            1.0f,
            2.0f + std::sin(State->ElapsedTime * 1.5f) * 2.0f
        };
        Transform.Rotation = { 0.0f, 0.0f, 0.0f };
        Transform.Scale = { 1.2f, 1.2f, 1.2f };
        State->Host->SetObjectTransform(
            State->Host->Context,
            State->SceneID,
            State->CapsuleID,
            &Transform
        );
    }

    //概要：Hot Reloadで引き継ぐMain Program状態Byte数を取得する
    //引数：instance=Program状態
    //戻り値：保存状態のByte数、状態不在時は0
    std::uint32_t ENGINE_EXTENSION_CALL GetProgramStateSize(void* instance)
    {
        return instance == nullptr
            ? 0u
            : static_cast<std::uint32_t>(sizeof(SavedProgramState));
    }

    //概要：Hot Reload前の往復運動位相をEngine所有Bufferへ保存する
    //引数：instance=Program状態、destination=保存先、destinationSize=保存先Byte数
    //戻り値：状態を保存できた場合true
    bool ENGINE_EXTENSION_CALL SaveProgramState(
        void* instance,
        void* destination,
        std::uint32_t destinationSize
    )
    {
        if (instance == nullptr || destination == nullptr ||
            destinationSize < sizeof(SavedProgramState))
        {
            return false;
        }

        const SavedProgramState State
        {
            static_cast<ProgramState*>(instance)->ElapsedTime
        }; //DLL Pointerを含めない保存状態
        std::memcpy(destination, &State, sizeof(State));
        return true;
    }

    //概要：Hot Reload後のMain Programへ旧往復運動位相を復元する
    //引数：instance=新Program状態、source=旧状態Buffer、sourceSize=旧状態Byte数
    //戻り値：状態を復元できた場合true
    bool ENGINE_EXTENSION_CALL LoadProgramState(
        void* instance,
        const void* source,
        std::uint32_t sourceSize
    )
    {
        if (instance == nullptr || source == nullptr ||
            sourceSize < sizeof(SavedProgramState))
        {
            return false;
        }

        SavedProgramState State{}; //DLL境界Bufferから復元する値
        std::memcpy(&State, source, sizeof(State));
        static_cast<ProgramState*>(instance)->ElapsedTime = State.ElapsedTime;
        return true;
    }
}

//概要：Engineが共通名から取得する版番号付きMain Module定義を返す
//引数：requestedAbiVersion=Engineが要求するC ABI版番号
//戻り値：互換Module定義、版不一致時はnullptr
ENGINE_EXTENSION_EXPORT const EngineExtensionModuleDescriptor*
ENGINE_EXTENSION_CALL EngineGetExtensionModule(std::uint32_t requestedAbiVersion)
{
    static const EngineExtensionModuleDescriptor Descriptor
    {
        sizeof(EngineExtensionModuleDescriptor),
        EngineExtensionAbiVersion,
        "ProgramTabTestAPI",
        2,
        CreateProgram,
        DestroyProgram,
        UpdateProgram,
        GetProgramStateSize,
        SaveProgramState,
        LoadProgramState
    };
    return requestedAbiVersion == EngineExtensionAbiVersion ? &Descriptor : nullptr;
}
