//|| GameObjectTemplate.cpp ||:::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  ゲーム用Object雛形の複製可能な設定値を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_18  v1.00  新規作成
//||

#include "GameObjectTemplate.h"

#include <algorithm>

namespace Engine
{
    //概要：一般的な移動速度と最大体力を持つ未登録Object雛形を作成する
    //引数：なし
    //戻り値：なし
    GameObjectTemplate::GameObjectTemplate()
        : Object(ObjectType::Object)
        , GameplayTag("Gameplay")
        , MoveSpeed(3.0f)
        , MaximumHealth(100.0f)
    {
    }

    //概要：Object雛形を破棄する
    //引数：なし
    //戻り値：なし
    GameObjectTemplate::~GameObjectTemplate() = default;

    //概要：Transform、有効状態、ゲーム設定を持つ未登録の複製を作成する
    //引数：なし
    //戻り値：Componentを含まないGameObjectTemplate複製
    std::unique_ptr<Object> GameObjectTemplate::Clone() const
    {
        auto Duplicate = std::make_unique<GameObjectTemplate>(); //設定を複写する雛形Object
        CopyDefinitionTo(*Duplicate);
        Duplicate->GameplayTag = GameplayTag;
        Duplicate->MoveSpeed = MoveSpeed;
        Duplicate->MaximumHealth = MaximumHealth;
        return Duplicate;
    }

    //概要：ゲーム側で検索や分岐に使うTagを変更する
    //引数：gameplayTag=新しいUTF-8 Tag、空の場合はGameplay
    //戻り値：なし
    void GameObjectTemplate::SetGameplayTag(const std::string& gameplayTag)
    {
        GameplayTag = gameplayTag.empty() ? "Gameplay" : gameplayTag;
    }

    //概要：ゲーム側で検索や分岐に使うTagを取得する
    //引数：なし
    //戻り値：現在のUTF-8 Gameplay Tag
    const std::string& GameObjectTemplate::GetGameplayTag() const
    {
        return GameplayTag;
    }

    //概要：移動Scriptが利用できる既定速度を変更する
    //引数：moveSpeed=0以上の毎秒移動量
    //戻り値：なし
    void GameObjectTemplate::SetMoveSpeed(float moveSpeed)
    {
        MoveSpeed = std::max(0.0f, moveSpeed);
    }

    //概要：移動Scriptが利用できる既定速度を取得する
    //引数：なし
    //戻り値：0以上の毎秒移動量
    float GameObjectTemplate::GetMoveSpeed() const
    {
        return MoveSpeed;
    }

    //概要：体力Scriptが利用できる最大体力を変更する
    //引数：maximumHealth=0以上の最大体力
    //戻り値：なし
    void GameObjectTemplate::SetMaximumHealth(float maximumHealth)
    {
        MaximumHealth = std::max(0.0f, maximumHealth);
    }

    //概要：体力Scriptが利用できる最大体力を取得する
    //引数：なし
    //戻り値：0以上の最大体力
    float GameObjectTemplate::GetMaximumHealth() const
    {
        return MaximumHealth;
    }
}
