//|| Script.h ||:::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Objectへ差し込み毎フレーム実行できるSub Programの基底クラスを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.10  Attach状態の読取APIを追加
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Component.h"

namespace Engine
{
    struct ScriptExposedMember final
    {
        std::string Name;
        std::string Type;
        std::string Value;
        bool Function = false;
        bool ReadOnly = false;
    };

    class Script : public Component
    {
    public:
        virtual ~Script();

        bool Initialize(DirectX12& dx12) final;
        void Update(float deltaTime) final;
        void Draw(const RenderContext& renderContext) final;
        void Finalize() final;
        std::unique_ptr<Component> Clone() const final;

        const std::string& GetScriptKey() const;
        const std::string& GetDisplayName() const;
        bool IsAttached() const;
        bool HasStarted() const;
        virtual std::vector<ScriptExposedMember> GetExposedMembers() const;
        virtual bool SetExposedMember(const std::string& name, const std::string& value);
        virtual bool InvokeExposedFunction(const std::string& name);

    protected:
        explicit Script(
            std::string scriptKey,
            std::string displayName
        );

        virtual std::unique_ptr<Script> CloneScript() const = 0;
        virtual bool OnAttach();
        virtual void OnStart();
        virtual void OnUpdate(float deltaTime);
        virtual void OnStop();
        virtual void OnDetach();

    private:
        std::string ScriptKey; //RegistryとDLLを含めて一意なScript識別子
        std::string DisplayName; //Editorへ表示するScript名
        bool Attached; //OnAttach成功後かつOnDetach前の場合true
        bool Started; //OnStart実行後かつOnStop前の場合true
    };
}
