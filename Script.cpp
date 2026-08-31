//|| Script.cpp ||:::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Sub ProgramのAttach、Start、Update、Stop、Detach順序を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.10  Component基底の初期化と終了契約を適用
//||  2026_08_17  v1.00  新規作成
//||

#include "Script.h"

#include <utility>

namespace Engine
{
    //概要：識別子と表示名を持つ未登録Scriptを作成する
    //引数：scriptKey=Registry内の一意識別子、displayName=Editor表示名
    //戻り値：なし
    Script::Script(
        std::string scriptKey,
        std::string displayName
    )
        : Component(ComponentType::Script)
        , ScriptKey(std::move(scriptKey))
        , DisplayName(std::move(displayName))
        , Attached(false)
        , Started(false)
    {
    }

    //概要：実行中Scriptを安全に停止して破棄する
    //引数：なし
    //戻り値：なし
    Script::~Script() = default;

    //概要：Scriptを所有Objectへ接続してSub Programを実行可能にする
    //引数：dx12=Component共通初期化契約の描画基盤
    //戻り値：OnAttachに成功した場合はtrue
    bool Script::Initialize(DirectX12& dx12)
    {
        if (!Component::Initialize(dx12))
        {
            return false;
        }

        if (Attached)
        {
            return true;
        }

        Attached = OnAttach();
        return Attached;
    }

    //概要：初回だけOnStartを呼び、その後は毎フレームOnUpdateを呼ぶ
    //引数：deltaTime=前フレームからの経過秒数
    //戻り値：なし
    void Script::Update(float deltaTime)
    {
        if (!Attached)
        {
            return;
        }

        if (!Started)
        {
            OnStart();
            Started = true;
        }

        OnUpdate(deltaTime);
    }

    //概要：Scriptは描画命令を持たないため描画Contextを受け流す
    //引数：renderContext=現在Cameraの描画Context
    //戻り値：なし
    void Script::Draw(const RenderContext& renderContext)
    {
        (void)renderContext;
    }

    //概要：開始済みScriptをStopしてから所有Objectとの接続を解除する
    //引数：なし
    //戻り値：なし
    void Script::Finalize()
    {
        if (Started)
        {
            OnStop();
            Started = false;
        }

        if (Attached)
        {
            OnDetach();
            Attached = false;
        }

        Component::Finalize();
    }

    //概要：派生型を維持した未登録Script定義を複製する
    //引数：なし
    //戻り値：所有者と実行状態を持たない複製Component
    std::unique_ptr<Component> Script::Clone() const
    {
        std::unique_ptr<Script> Duplicate = CloneScript(); //派生型が生成した複製Script

        if (!Duplicate)
        {
            return nullptr;
        }

        CopyDefinitionTo(*Duplicate);
        return Duplicate;
    }

    //概要：RegistryとModuleを含むScript識別子を取得する
    //引数：なし
    //戻り値：Scriptの一意識別子
    const std::string& Script::GetScriptKey() const
    {
        return ScriptKey;
    }

    //概要：エディターへ表示するScript名を取得する
    //引数：なし
    //戻り値：Script表示名
    const std::string& Script::GetDisplayName() const
    {
        return DisplayName;
    }

    //概要：Scriptが所有ObjectへのAttachを完了しているか確認する
    //引数：なし
    //戻り値：OnAttach成功後かつOnDetach前の場合はtrue
    bool Script::IsAttached() const
    {
        return Attached;
    }

    //概要：ScriptのStart処理が完了しているか確認する
    //引数：なし
    //戻り値：OnStart後かつOnStop前の場合はtrue
    bool Script::HasStarted() const
    {
        return Started;
    }

    std::vector<ScriptExposedMember> Script::GetExposedMembers() const
    {
        return {};
    }

    bool Script::SetExposedMember(const std::string& name, const std::string& value)
    {
        (void)name;
        (void)value;
        return false;
    }

    bool Script::InvokeExposedFunction(const std::string& name)
    {
        (void)name;
        return false;
    }

    //概要：基底Scriptの接続処理を成功として扱う
    //引数：なし
    //戻り値：常にtrue
    bool Script::OnAttach()
    {
        return true;
    }

    //概要：基底Scriptの開始時処理を行う
    //引数：なし
    //戻り値：なし
    void Script::OnStart()
    {
    }

    //概要：基底Scriptの毎フレーム処理を行う
    //引数：deltaTime=前フレームからの経過秒数
    //戻り値：なし
    void Script::OnUpdate(float deltaTime)
    {
        (void)deltaTime;
    }

    //概要：基底Scriptの停止時処理を行う
    //引数：なし
    //戻り値：なし
    void Script::OnStop()
    {
    }

    //概要：基底Scriptの切断時処理を行う
    //引数：なし
    //戻り値：なし
    void Script::OnDetach()
    {
    }
}
