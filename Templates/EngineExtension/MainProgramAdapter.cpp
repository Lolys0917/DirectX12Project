//|| MainProgramAdapter.cpp ||::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Scene形式Main ProgramをEngine Extension C ABIへ接続する固定Adapter
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.00  新規作成
//||

#include "GameEngineAPI.h"

namespace
{
    //概要：登録済みScene ProgramのDLL状態を作成する
    //引数：host=版番号付きEngine関数表
    //戻り値：作成状態、初期化失敗時はnullptr
    void* ENGINE_EXTENSION_CALL CreateProgram(const EngineHostAPI* host)
    {
        return EngineGame::Detail::CreateSceneProgram(host);
    }

    //概要：登録済みSceneのEndを呼んでDLL状態を破棄する
    //引数：instance=Scene Program状態
    //戻り値：なし
    void ENGINE_EXTENSION_CALL DestroyProgram(void* instance)
    {
        EngineGame::Detail::DestroySceneProgram(instance);
    }

    //概要：Active SceneのUpdateを毎Frame呼び出す
    //引数：instance=Scene Program状態、deltaTime=前Frameからの秒数
    //戻り値：なし
    void ENGINE_EXTENSION_CALL UpdateProgram(void* instance, float deltaTime)
    {
        EngineGame::Detail::UpdateSceneProgram(instance, deltaTime);
    }
}

//概要：Engineが共通名から取得するScene Main Program定義を返す
//引数：requestedAbiVersion=Engineが要求するC ABI版番号
//戻り値：互換Module定義、版不一致時はnullptr
ENGINE_EXTENSION_EXPORT const EngineExtensionModuleDescriptor*
ENGINE_EXTENSION_CALL EngineGetExtensionModule(std::uint32_t requestedAbiVersion)
{
    static const EngineExtensionModuleDescriptor Descriptor
    {
        sizeof(EngineExtensionModuleDescriptor),
        EngineExtensionAbiVersion,
        "SceneMainProgram",
        1,
        CreateProgram,
        DestroyProgram,
        UpdateProgram,
        nullptr,
        nullptr,
        nullptr
    };
    return requestedAbiVersion == EngineExtensionAbiVersion ? &Descriptor : nullptr;
}
