//|| Component.cpp ||:::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  コンポーネント基底の登録情報と標準ライフサイクルを実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Component.h"

namespace Engine
{
    //指定種別の未登録コンポーネントを作成する
    //componentType : コンポーネント種別
    Component::Component(ComponentType componentType)
        : ID()
        , Type(componentType)
        , Name()
        , Owner(nullptr)
        , Active(true)
        , Initialized(false)
    {
    }

    //コンポーネントを破棄する
    Component::~Component() = default;

    //標準コンポーネントを初期化済みにする
    //dx12 : DirectX12基盤
    //戻り値 : 常にtrue
    bool Component::Initialize(DirectX12& dx12)
    {
        (void)dx12;
        return true;
    }

    //標準コンポーネントを更新する
    //deltaTime : 前フレームからの経過秒
    void Component::Update(float deltaTime)
    {
        (void)deltaTime;
    }

    //標準コンポーネントの描画処理を行う
    //renderContext : 描画対象とカメラを含む描画情報
    void Component::Draw(const RenderContext& renderContext)
    {
        (void)renderContext;
    }

    //標準コンポーネントを終了する
    void Component::Finalize()
    {
    }

    //未登録状態の複製定義を作成する
    //戻り値 : 所有者とIDを持たない複製コンポーネント
    std::unique_ptr<Component> Component::Clone() const
    {
        std::unique_ptr<Component> ClonedComponent =
            std::unique_ptr<Component>(new Component(Type)); //複製コンポーネント

        CopyDefinitionTo(*ClonedComponent);
        return ClonedComponent;
    }

    //複製先へ基底設定をコピーする
    //destination : 未登録の複製先
    void Component::CopyDefinitionTo(Component& destination) const
    {
        destination.Active = Active;
    }

    //オブジェクトへ所有された直後のコンポーネントを登録する
    //owner : 所有オブジェクト
    //componentID : 割り当てるコンポーネントID
    //resolvedName : 同型内で一意に解決済みの名前
    void Component::AssignRegistration(
        Object& owner,
        ComponentID componentID,
        const std::string& resolvedName
    )
    {
        Owner = &owner;
        ID = componentID;
        Name = resolvedName;
    }

    //登録情報を無効化する
    void Component::ClearRegistration()
    {
        ID = ComponentID();
        Name.clear();
        Owner = nullptr;
        Initialized = false;
    }
}
