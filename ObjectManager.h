//|| ObjectManager.h ||::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Scene内のObjectとComponentへ安定IDを割り当て走査なし検索を提供する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v2.40  Renderable ObjectのGPU初期化状態読取APIを追加
//||  2026_08_19  v2.30  Scene内で一意なObject名索引と名前単独検索APIを追加
//||  2026_08_17  v2.20  Object複製とObject／Component名前変更APIを追加
//||  2026_07_13  v2.10  IRenderable Objectの描画Lifecycleを統合
//||  2026_07_13  v2.00  強いID、Vector<Map>索引、所有者別Component索引を実装
//||

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Component.h"
#include "EntityTypes.h"
#include "Object.h"

namespace Engine
{
    class DirectX12;
    struct RenderContext;

    class ObjectManager final
    {
    public:
        //空のObject、Component ID索引を持つManagerを作成する
        ObjectManager();

        //初期化済みRenderable ObjectとComponentを終了して全所有物を破棄する
        ~ObjectManager();

        //安定IDと所有権の重複を防ぐためCopy構築を禁止する
        //引数: コピー元ObjectManager
        ObjectManager(const ObjectManager&) = delete;

        //安定IDと所有権の重複を防ぐためCopy代入を禁止する
        //引数: コピー元ObjectManager
        //戻り値: 代入先ObjectManagerへの参照
        ObjectManager& operator=(const ObjectManager&) = delete;

        //概要：指定型のObjectを生成して型×解決名索引へ登録する
        //引数：requestedName=希望名、arguments=Object構築引数
        //戻り値：登録済みObject、失敗時はnullptr
        template<class ObjectClass, class... ArgumentTypes>
        ObjectClass* CreateObject(
            const std::string& requestedName,
            ArgumentTypes&&... arguments
        )
        {
            static_assert(std::is_base_of_v<Object, ObjectClass>);

            auto NewObject = std::make_unique<ObjectClass>(
                std::forward<ArgumentTypes>(arguments)...
            ); //登録するObject

            return static_cast<ObjectClass*>(AddObject(
                std::move(NewObject),
                requestedName
            ));
        }

        //未登録Objectの所有権を受け取り安定IDを割り当てる
        //引数: object 未登録Object、requestedName 希望名
        //戻り値: 登録済みObject、失敗時はnullptr
        Object* AddObject(
            std::unique_ptr<Object> object,
            const std::string& requestedName
        );

        //概要：指定ObjectへComponentを生成してOwner×型×解決名索引へ登録する
        //引数：ownerID=所有Object、requestedName=希望名、arguments=Component構築引数
        //戻り値：登録済みComponent、失敗時はnullptr
        template<class ComponentClass, class... ArgumentTypes>
        ComponentClass* AddComponent(
            ObjectID ownerID,
            const std::string& requestedName,
            ArgumentTypes&&... arguments
        )
        {
            static_assert(std::is_base_of_v<Component, ComponentClass>);

            auto NewComponent = std::make_unique<ComponentClass>(
                std::forward<ArgumentTypes>(arguments)...
            ); //登録するComponent

            return static_cast<ComponentClass*>(AddComponent(
                ownerID,
                std::move(NewComponent),
                requestedName
            ));
        }

        //未登録Componentを指定Objectへ所有させ安定IDを割り当てる
        //引数: ownerID 所有Object、component 未登録Component、requestedName 希望名
        //戻り値: 登録済みComponent、失敗時はnullptr
        Component* AddComponent(
            ObjectID ownerID,
            std::unique_ptr<Component> component,
            const std::string& requestedName
        );

        //Objectと所有Componentを削除してIDをtombstone化する
        //引数: objectID 削除するObject
        //戻り値: 削除した場合はtrue
        bool RemoveObject(ObjectID objectID);

        bool SetParent(
            ObjectID childID,
            ObjectID parentID,
            bool keepWorldTransform
        );

        bool IsDescendantOf(ObjectID objectID, ObjectID possibleAncestorID) const;

        //Componentを削除してIDとObject内slotをtombstone化する
        //引数: componentID 削除するComponent
        //戻り値: 削除した場合はtrue
        bool RemoveComponent(ComponentID componentID);

        //Objectと所有Componentを新しいIDで複製する
        //引数: sourceID 複製元Object、requestedName 複製先の希望名
        //戻り値: 登録済みの複製Object、失敗時はnullptr
        Object* CloneObject(
            ObjectID sourceID,
            const std::string& requestedName
        );

        //Object名を型内で一意な名前へ変更する
        //引数: objectID 変更対象Object、requestedName 新しい希望名
        //戻り値: 名前を変更できた場合はtrue
        bool RenameObject(
            ObjectID objectID,
            const std::string& requestedName
        );

        //Component名を所有Objectかつ型内で一意な名前へ変更する
        //引数: componentID 変更対象Component、requestedName 新しい希望名
        //戻り値: 名前を変更できた場合はtrue
        bool RenameComponent(
            ComponentID componentID,
            const std::string& requestedName
        );

        //ObjectのGroup、Tag、Layer、処理順を設定する。同名GroupのGroup順は一括変更する。
        bool SetObjectOrganization(
            ObjectID objectID,
            const std::string& group,
            const std::string& tag,
            std::uint32_t layer,
            std::int32_t groupOrder,
            std::int32_t executionOrder
        );
        std::size_t SetGroupActive(const std::string& group, bool active);

        //Object IDから走査せず登録Objectを取得する
        //引数: objectID 検索するObject ID
        //戻り値: 登録Object、無効または削除済みIDの場合はnullptr
        Object* FindObject(ObjectID objectID);

        //Object IDから走査せず読み取り専用の登録Objectを取得する
        //引数: objectID 検索するObject ID
        //戻り値: 登録Object、無効または削除済みIDの場合はnullptr
        const Object* FindObject(ObjectID objectID) const;

        //Scene内で一意な解決済み名からObjectを平均O(1)で検索する
        //引数: resolvedName 解決済み名
        //戻り値: 見つかったObject、未登録時はnullptr
        Object* FindObject(const std::string& resolvedName);

        //Scene内で一意な解決済み名から読み取り専用Objectを平均O(1)で検索する
        //引数: resolvedName 解決済み名
        //戻り値: 見つかったObject、未登録時はnullptr
        const Object* FindObject(const std::string& resolvedName) const;

        //型×解決済み名からObjectを平均O(1)で検索する
        //引数: objectType Object型、resolvedName 解決済み名
        //戻り値: 見つかったObject、未登録時はnullptr
        Object* FindObject(ObjectType objectType, const std::string& resolvedName);

        //型×解決済み名から読み取り専用Objectを平均O(1)で検索する
        //引数: objectType Object型、resolvedName 解決済み名
        //戻り値: 見つかったObject、未登録時はnullptr
        const Object* FindObject(ObjectType objectType, const std::string& resolvedName) const;

        //Component IDから走査せず登録Componentを取得する
        //引数: componentID 検索するComponent ID
        //戻り値: 登録Component、無効または削除済みIDの場合はnullptr
        Component* FindComponent(ComponentID componentID);

        //Component IDから走査せず読み取り専用の登録Componentを取得する
        //引数: componentID 検索するComponent ID
        //戻り値: 登録Component、無効または削除済みIDの場合はnullptr
        const Component* FindComponent(ComponentID componentID) const;

        //Owner×型×解決済み名からComponentを平均O(1)で検索する
        //引数: ownerID 所有Object、componentType Component型、resolvedName 解決済み名
        //戻り値: 見つかったComponent、未登録時はnullptr
        Component* FindComponent(
            ObjectID ownerID,
            ComponentType componentType,
            const std::string& resolvedName
        );

        //Owner×型×解決済み名から読み取り専用Componentを平均O(1)で検索する
        //引数: ownerID 所有Object、componentType Component型、resolvedName 解決済み名
        //戻り値: 見つかったComponent、未登録時はnullptr
        const Component* FindComponent(
            ObjectID ownerID,
            ComponentType componentType,
            const std::string& resolvedName
        ) const;

        //有効Object IDのスナップショットを取得する
        //戻り値: 登録順の有効Object ID一覧
        std::vector<ObjectID> GetObjectIDs() const;

        //有効Component IDのスナップショットを取得する
        //戻り値: 登録順の有効Component ID一覧
        std::vector<ComponentID> GetComponentIDs() const;

        //指定Objectが所有するComponent IDを取得する
        //引数: ownerID 所有Object
        //戻り値: Object内登録順の有効Component ID一覧
        std::vector<ComponentID> GetComponentIDs(ObjectID ownerID) const;

        //指定型の有効Componentを描画等の一括処理用に取得する
        //引数: componentType 取得するComponent型
        //戻り値: 登録順の非所有Pointer一覧、無効型の場合は空
        std::vector<Component*> FindComponentsByType(ComponentType componentType);

        //指定型の有効Componentを読み取り用に取得する
        //引数: componentType 取得するComponent型
        //戻り値: 登録順の非所有Pointer一覧、無効型の場合は空
        std::vector<const Component*> FindComponentsByType(ComponentType componentType) const;

        //未初期化Renderable ObjectとComponentを初期化する
        //引数: dx12 GPU Resource作成に使用する描画基盤
        //戻り値: 全未初期化対象が成功した場合はtrue
        bool InitializeComponents(DirectX12& dx12);

        //有効Renderable Objectと初期化済みComponentを更新する
        //引数: deltaTime 前回更新からの秒数
        void UpdateComponents(float deltaTime);

        //有効Renderable Objectと初期化済みComponentを現在のCamera passへ描画する
        //引数: renderContext 描画基盤とCameraを持つContext
        void DrawComponents(const RenderContext& renderContext);

        //初期化済みComponentとRenderable Objectを登録逆順に終了する
        void FinalizeComponents();

        //ObjectとComponentのCPU定義を独立Managerへ複製する
        //戻り値: IDを新規発行した未初期化ObjectManager
        std::unique_ptr<ObjectManager> CloneDefinition() const;

        //有効な登録Object数を取得する
        //戻り値: tombstoneを除くObject数
        std::size_t GetObjectCount() const;

        //有効な登録Component数を取得する
        //戻り値: tombstoneを除くComponent数
        std::size_t GetComponentCount() const;

        //指定Renderable ObjectのGPU初期化が完了しているか確認する
        //引数: objectID 確認対象Object
        //戻り値: 登録済みRenderable Objectの初期化が完了している場合はtrue
        bool IsRenderableObjectInitialized(ObjectID objectID) const;

    private:
        struct ComponentLocation
        {
            ObjectID OwnerID; //所有Object ID
            std::size_t OwnerSlot = 0; //Object内の安定Component slot
            Component* Pointer = nullptr; //Objectが所有するComponentへの非所有参照
        };

        //希望名からScene内で一意なObject名を解決する
        //引数: requestedName 希望名
        //戻り値: 空名を補正し必要なら_数値を加えた名前
        std::string ResolveObjectName(const std::string& requestedName);

        //Object型が索引範囲内か判定する
        //引数: objectType 判定するObject型
        //戻り値: 型が有効な場合はtrue
        static bool IsValidObjectType(ObjectType objectType);

        //Component型が索引範囲内か判定する
        //引数: componentType 判定するComponent型
        //戻り値: 型が有効な場合はtrue
        static bool IsValidComponentType(ComponentType componentType);

        void RemoveChildReference(Object& parent, ObjectID childID);
        static bool ComesBefore(const Object& left, const Object& right);

        std::vector<std::unique_ptr<Object>> ObjectsByID; //ObjectIDを直接slotに使う所有配列
        std::vector<bool> RenderableObjectInitializedByID; //ObjectIDごとの描画Resource初期化状態
        std::vector<ComponentLocation> ComponentsByID; //ComponentIDを直接slotに使う位置配列
        std::unordered_map<std::string, ObjectID> ObjectIDByName; //Scene内で一意な名前のObject索引
        std::unordered_map<std::string, std::uint32_t> ObjectSuffixByName; //基底名ごとの次回接尾辞
        std::size_t ObjectCount; //tombstoneを除くObject数
        std::size_t ComponentCount; //tombstoneを除くComponent数
    };
}
