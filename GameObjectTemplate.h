//|| GameObjectTemplate.h ||:::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Native Mainと外部Programから複製して使えるゲーム用Object雛形を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_18  v1.00  新規作成
//||

#pragma once

#include <memory>
#include <string>

#include "Object.h"

namespace Engine
{
    class GameObjectTemplate final : public Object
    {
    public:
        GameObjectTemplate();
        ~GameObjectTemplate() override;

        GameObjectTemplate(const GameObjectTemplate&) = delete;
        GameObjectTemplate& operator=(const GameObjectTemplate&) = delete;

        std::unique_ptr<Object> Clone() const override;

        void SetGameplayTag(const std::string& gameplayTag);
        const std::string& GetGameplayTag() const;
        void SetMoveSpeed(float moveSpeed);
        float GetMoveSpeed() const;
        void SetMaximumHealth(float maximumHealth);
        float GetMaximumHealth() const;

    private:
        std::string GameplayTag; //ゲーム側が用途を識別する任意Tag
        float MoveSpeed; //移動Scriptが既定値として利用できる毎秒速度
        float MaximumHealth; //体力Scriptが既定値として利用できる最大値
    };
}
