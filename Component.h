//|| Component.h ||:::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  オブジェクトへ付属する全コンポーネントの基底クラスを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  所有者、強いID、描画ライフサイクル、複製定義を追加
//||

#pragma once

#include <memory>
#include <string>

#include "EntityTypes.h"

namespace Engine
{
    class DirectX12;
    class Object;
    class ObjectManager;
    struct RenderContext;

    class Component
    {
    public:
        //コンポーネントを破棄する
        virtual ~Component();

        //所有関係とIDの重複を防ぐためCopy構築を禁止する
        //引数: コピー元Component
        Component(const Component&) = delete;

        //所有関係とIDの重複を防ぐためCopy代入を禁止する
        //引数: コピー元Component
        //戻り値: 代入先Componentへの参照
        Component& operator=(const Component&) = delete;

        //Manager内Pointerの不変性を保つためMove構築を禁止する
        //引数: 移動元Component
        Component(Component&&) = delete;

        //Manager内Pointerの不変性を保つためMove代入を禁止する
        //引数: 移動元Component
        //戻り値: 代入先Componentへの参照
        Component& operator=(Component&&) = delete;

        //GPU資源を含むコンポーネントを初期化する
        //dx12 : DirectX12基盤
        //戻り値 : 初期化に成功した場合true
        virtual bool Initialize(DirectX12& dx12);

        //コンポーネントを更新する
        //deltaTime : 前フレームからの経過秒
        virtual void Update(float deltaTime);

        //指定カメラの描画パスへ描画する
        //renderContext : 描画対象とカメラを含む描画情報
        virtual void Draw(const RenderContext& renderContext);

        //GPU資源を含むコンポーネントを終了する
        virtual void Finalize();

        //未登録状態の複製定義を作成する
        //戻り値 : 所有者とIDを持たない複製コンポーネント
        virtual std::unique_ptr<Component> Clone() const;

        ComponentID GetID() const { return ID; }
        ComponentType GetType() const { return Type; }
        const std::string& GetName() const { return Name; }
        Object* GetOwner() { return Owner; }
        const Object* GetOwner() const { return Owner; }

        //Componentが更新と描画の対象か確認する
        //戻り値: Componentが有効な場合はtrue
        bool IsActive() const { return Active; }

        //Componentの初期化が完了しているか確認する
        //戻り値: Initialize成功後かつFinalize前の場合はtrue
        bool IsInitialized() const { return Initialized; }
        void SetActive(bool active) { Active = active; }

    protected:
        //指定種別の未登録コンポーネントを作成する
        //componentType : コンポーネント種別
        explicit Component(ComponentType componentType = ComponentType::Component);

        //複製先へ基底設定をコピーする
        //destination : 未登録の複製先
        void CopyDefinitionTo(Component& destination) const;

    private:
        friend class Object;
        friend class ObjectManager;

        //オブジェクトへ所有された直後のコンポーネントを登録する
        //owner : 所有オブジェクト
        //componentID : 割り当てるコンポーネントID
        //resolvedName : 同型内で一意に解決済みの名前
        void AssignRegistration(
            Object& owner,
            ComponentID componentID,
            const std::string& resolvedName
        );

        //登録情報を無効化する
        void ClearRegistration();

        ComponentID ID; //シーン内で一意なコンポーネントID
        ComponentType Type; //コンポーネント種別
        std::string Name; //所有オブジェクトかつ同型内で一意な名前
        Object* Owner; //所有オブジェクト
        bool Active; //更新と描画を行う場合true
        bool Initialized; //Initialize成功後かつFinalize前の場合true
    };
}
