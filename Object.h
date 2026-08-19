//|| Object.h ||::::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  姿勢とコンポーネントを所有するゲームオブジェクト基底を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Transformを統合し強いIDとComponent所有を追加
//||

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "Component.h"
#include "EntityTypes.h"
#include "Transform.h"

namespace Engine
{
    class ObjectManager;

    class Object
    {
    public:
        //汎用オブジェクトを作成する
        Object();

        //オブジェクトを破棄する
        virtual ~Object();

        //IDとComponent所有権の重複を防ぐためCopy構築を禁止する
        //引数: コピー元Object
        Object(const Object&) = delete;

        //IDとComponent所有権の重複を防ぐためCopy代入を禁止する
        //引数: コピー元Object
        //戻り値: 代入先Objectへの参照
        Object& operator=(const Object&) = delete;

        //Manager内Pointerの不変性を保つためMove構築を禁止する
        //引数: 移動元Object
        Object(Object&&) = delete;

        //Manager内Pointerの不変性を保つためMove代入を禁止する
        //引数: 移動元Object
        //戻り値: 代入先Objectへの参照
        Object& operator=(Object&&) = delete;

        //未登録状態の複製定義を作成する
        //戻り値 : コンポーネントを含まない複製オブジェクト
        virtual std::unique_ptr<Object> Clone() const;

        //概要：Scene内のObject IDを取得する
        //引数：なし
        //戻り値：登録済みID、未登録時は無効ID
        ObjectID GetID() const { return ID; }

        //概要：Objectの具象種別を取得する
        //引数：なし
        //戻り値：Object種別
        ObjectType GetType() const { return Type; }

        //概要：同じObject型内で一意な名前を取得する
        //引数：なし
        //戻り値：解決済みObject名
        const std::string& GetName() const { return Name; }

        //概要：Objectが更新と描画の対象か確認する
        //引数：なし
        //戻り値：Objectが有効な場合はtrue
        bool IsActive() const { return Active; }
        //概要：Objectと所有Componentの有効状態を変更する
        //引数：active=有効にする場合はtrue
        //戻り値：なし
        void SetActive(bool active) { Active = active; }

        //概要：ObjectのLocal座標を変更する
        //引数：position=設定するXYZ座標
        //戻り値：なし
        void SetPosition(const DirectX::XMFLOAT3& position);

        //概要：ObjectのLocal XYZ回転角を変更する
        //引数：rotation=ラジアン単位のXYZ回転角
        //戻り値：なし
        void SetRotation(const DirectX::XMFLOAT3& rotation);

        //概要：ObjectのLocal XYZ拡縮率を変更する
        //引数：scale=設定するXYZ拡縮率
        //戻り値：なし
        void SetScale(const DirectX::XMFLOAT3& scale);

        //概要：ObjectのLocal座標を取得する
        //引数：なし
        //戻り値：XYZ Local座標
        const DirectX::XMFLOAT3& GetPosition() const;

        //概要：ObjectのLocal XYZ回転角を取得する
        //引数：なし
        //戻り値：ラジアン単位のXYZ回転角
        const DirectX::XMFLOAT3& GetRotation() const;

        //概要：ObjectのLocal XYZ拡縮率を取得する
        //引数：なし
        //戻り値：XYZ拡縮率
        const DirectX::XMFLOAT3& GetScale() const;

        Transform& GetTransform();
        const Transform& GetTransform() const;
        ObjectID GetParentID() const;
        const std::vector<ObjectID>& GetChildIDs() const;

        //オブジェクトのワールド行列を取得する
        //戻り値 : 拡縮、回転、平行移動を合成したワールド行列
        DirectX::XMMATRIX GetWorldMatrix() const;

        //現在所有している有効なコンポーネント数を取得する
        //戻り値 : 有効なコンポーネント数
        std::size_t GetComponentCount() const;

        //現在所有しているコンポーネントIDの安全なスナップショットを取得する
        //戻り値 : 有効なコンポーネントID一覧
        std::vector<ComponentID> GetComponentIDs() const;

    protected:
        //指定種別のオブジェクトを作成する
        //objectType : オブジェクト種別
        explicit Object(ObjectType objectType);

        //複製先へ姿勢と有効状態をコピーする
        //destination : 未登録の複製先
        void CopyDefinitionTo(Object& destination) const;

    private:
        friend class ObjectManager;

        //未登録コンポーネントの所有権を受け取る
        //component : 所有するコンポーネント
        //戻り値 : オブジェクト内の安定スロット番号
        std::size_t AttachComponent(std::unique_ptr<Component> component);

        //指定スロットのコンポーネントを破棄してtombstone化する
        //componentSlot : オブジェクト内スロット番号
        void DetachComponent(std::size_t componentSlot);

        //安定Slot番号から所有Componentを直接取得する
        //引数: componentSlot Object内の安定Slot番号
        //戻り値: 有効な所有Component、無効Slotの場合はnullptr
        Component* GetComponentAt(std::size_t componentSlot);

        //安定Slot番号から読み取り専用の所有Componentを直接取得する
        //引数: componentSlot Object内の安定Slot番号
        //戻り値: 有効な所有Component、無効Slotの場合はnullptr
        const Component* GetComponentAt(std::size_t componentSlot) const;

        //所有者かつ同型内で一意なコンポーネント名を解決する
        //componentType : コンポーネント種別
        //requestedName : 希望名
        //戻り値 : 同名時に数値接尾辞を追加した一意名
        std::string ResolveComponentName(
            ComponentType componentType,
            const std::string& requestedName
        );

        //解決済みコンポーネント名とIDを登録する
        //componentType : コンポーネント種別
        //resolvedName : 解決済み名
        //componentID : 登録ID
        //戻り値 : 登録に成功した場合true
        bool RegisterComponentName(
            ComponentType componentType,
            const std::string& resolvedName,
            ComponentID componentID
        );

        //登録済みコンポーネント名を解除する
        //componentType : コンポーネント種別
        //resolvedName : 解決済み名
        void UnregisterComponentName(
            ComponentType componentType,
            const std::string& resolvedName
        );

        //所有者かつ同型内の名前からコンポーネントIDを検索する
        //componentType : コンポーネント種別
        //resolvedName : 解決済み名
        //戻り値 : 見つからない場合は無効ID
        ComponentID FindComponentID(
            ComponentType componentType,
            const std::string& resolvedName
        ) const;

        //ObjectManagerから登録情報を設定する
        //objectID : 割り当てるオブジェクトID
        //resolvedName : 同型内で一意に解決済みの名前
        void AssignRegistration(
            ObjectID objectID,
            const std::string& resolvedName
        );

        ObjectID ID; //シーン内で一意なオブジェクトID
        ObjectType Type; //オブジェクト種別
        std::string Name; //同型内で一意な解決済み名
        bool Active; //更新と描画の対象にする場合true
        Transform ObjectTransform; //必ず存在し削除できないLocal姿勢
        Object* Parent; //親Objectへの非所有参照、Rootの場合はnullptr
        std::vector<ObjectID> Children; //登録順の直接Child Object ID
        std::vector<std::unique_ptr<Component>> Components; //安定スロットを持つ所有コンポーネント
        std::vector<std::unordered_map<std::string, ComponentID>> ComponentIDByNameByType; //型と名前からIDを引く索引
        std::vector<std::unordered_map<std::string, std::uint32_t>> ComponentSuffixByNameByType; //同名接尾辞の次回候補
        std::size_t ComponentCount; //tombstoneを除く所有コンポーネント数
    };
}
