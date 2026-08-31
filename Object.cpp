//|| Object.cpp ||::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  オブジェクトの姿勢、コンポーネント所有、名前索引を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成
//||

#include "Object.h"

#include <utility>

namespace Engine
{
    namespace
    {
        //コンポーネント種別の既定名を取得する
        //componentType : コンポーネント種別
        //戻り値 : 空名登録時に使用する名前
        const char* GetDefaultComponentName(ComponentType componentType)
        {
            switch (componentType)
            {
            case ComponentType::Mesh: return "Mesh";
            case ComponentType::Polygon: return "Polygon";
            case ComponentType::Model: return "Model";
            case ComponentType::Camera: return "Camera";
            case ComponentType::Grid: return "Grid";
            case ComponentType::Collider: return "Collider";
            case ComponentType::BoxCollider: return "BoxCollider";
            case ComponentType::SphereCollider: return "SphereCollider";
            case ComponentType::CapsuleCollider: return "CapsuleCollider";
            case ComponentType::CylinderCollider: return "CylinderCollider";
            case ComponentType::PlaneCollider: return "PlaneCollider";
            default: return "Component";
            }
        }
    }

    //汎用オブジェクトを作成する
    Object::Object()
        : Object(ObjectType::Object)
    {
    }

    //指定種別のオブジェクトを作成する
    //objectType : オブジェクト種別
    Object::Object(ObjectType objectType)
        : ID()
        , Type(objectType)
        , Name()
        , Active(true)
        , Tag("Untagged")
        , Layer(0)
        , ProcessingGroup()
        , GroupOrder(0)
        , ExecutionOrder(0)
        , ObjectTransform()
        , Parent(nullptr)
        , Children()
        , Components()
        , ComponentIDByNameByType(static_cast<std::size_t>(ComponentType::Count))
        , ComponentSuffixByNameByType(static_cast<std::size_t>(ComponentType::Count))
        , ComponentCount(0)
    {
    }

    //オブジェクトを破棄する
    Object::~Object() = default;

    //未登録状態の複製定義を作成する
    //戻り値 : コンポーネントを含まない複製オブジェクト
    std::unique_ptr<Object> Object::Clone() const
    {
        std::unique_ptr<Object> ClonedObject =
            std::unique_ptr<Object>(new Object(Type)); //複製オブジェクト

        CopyDefinitionTo(*ClonedObject);
        return ClonedObject;
    }

    //複製先へ姿勢と有効状態をコピーする
    //destination : 未登録の複製先
    void Object::CopyDefinitionTo(Object& destination) const
    {
        destination.Active = Active;
        destination.Tag = Tag;
        destination.Layer = Layer;
        destination.ProcessingGroup = ProcessingGroup;
        destination.GroupOrder = GroupOrder;
        destination.ExecutionOrder = ExecutionOrder;
        destination.ObjectTransform = ObjectTransform;
    }

    //概要：ObjectのLocal座標を変更する
    //引数：position=設定するXYZ座標
    //戻り値：なし
    void Object::SetPosition(const DirectX::XMFLOAT3& position)
    {
        ObjectTransform.SetLocalPosition(position);
    }

    //概要：ObjectのLocal回転角を変更する
    //引数：rotation=ラジアン単位のXYZ回転角
    //戻り値：なし
    void Object::SetRotation(const DirectX::XMFLOAT3& rotation)
    {
        ObjectTransform.SetLocalRotation(rotation);
    }

    //概要：ObjectのLocal拡縮率を変更する
    //引数：scale=設定するXYZ拡縮率
    //戻り値：なし
    void Object::SetScale(const DirectX::XMFLOAT3& scale)
    {
        ObjectTransform.SetLocalScale(scale);
    }

    //概要：ObjectのLocal座標を取得する
    //引数：なし
    //戻り値：XYZ Local座標
    const DirectX::XMFLOAT3& Object::GetPosition() const
    {
        return ObjectTransform.GetLocalPosition();
    }

    //概要：ObjectのLocal回転角を取得する
    //引数：なし
    //戻り値：ラジアン単位のXYZ回転角
    const DirectX::XMFLOAT3& Object::GetRotation() const
    {
        return ObjectTransform.GetLocalRotation();
    }

    //概要：ObjectのLocal拡縮率を取得する
    //引数：なし
    //戻り値：XYZ Local拡縮率
    const DirectX::XMFLOAT3& Object::GetScale() const
    {
        return ObjectTransform.GetLocalScale();
    }

    //概要：削除不能な必須Transformを取得する
    //引数：なし
    //戻り値：Objectが所有するTransformへの参照
    Transform& Object::GetTransform()
    {
        return ObjectTransform;
    }

    //概要：削除不能な必須Transformを読み取り専用で取得する
    //引数：なし
    //戻り値：Objectが所有するTransformへの読み取り専用参照
    const Transform& Object::GetTransform() const
    {
        return ObjectTransform;
    }

    //概要：親ObjectのIDを取得する
    //引数：なし
    //戻り値：親Object ID、Rootの場合は無効ID
    ObjectID Object::GetParentID() const
    {
        return Parent == nullptr ? ObjectID() : Parent->GetID();
    }

    //概要：直接Child ObjectのID一覧を取得する
    //引数：なし
    //戻り値：登録順のChild Object ID一覧
    const std::vector<ObjectID>& Object::GetChildIDs() const
    {
        return Children;
    }

    //オブジェクトのワールド行列を取得する
    //戻り値 : 拡縮、回転、平行移動を合成したワールド行列
    DirectX::XMMATRIX Object::GetWorldMatrix() const
    {
        const DirectX::XMMATRIX LocalMatrix = ObjectTransform.GetLocalMatrix(); //自身のLocal姿勢
        return Parent == nullptr
            ? LocalMatrix
            : LocalMatrix * Parent->GetWorldMatrix();
    }

    //現在所有している有効なコンポーネント数を取得する
    //戻り値 : 有効なコンポーネント数
    std::size_t Object::GetComponentCount() const
    {
        return ComponentCount;
    }

    //現在所有しているコンポーネントIDの安全なスナップショットを取得する
    //戻り値 : 有効なコンポーネントID一覧
    std::vector<ComponentID> Object::GetComponentIDs() const
    {
        std::vector<ComponentID> ComponentIDs; //返却するコンポーネントID一覧
        ComponentIDs.reserve(ComponentCount);

        for (const std::unique_ptr<Component>& OwnedComponent : Components) //所有スロットを確認する
        {
            if (OwnedComponent && OwnedComponent->GetID().IsValid())
            {
                ComponentIDs.push_back(OwnedComponent->GetID());
            }
        }

        return ComponentIDs;
    }

    //未登録コンポーネントの所有権を受け取る
    //component : 所有するコンポーネント
    //戻り値 : オブジェクト内の安定スロット番号
    std::size_t Object::AttachComponent(std::unique_ptr<Component> component)
    {
        if (!component)
        {
            return Components.size();
        }

        component->Owner = this;
        Components.push_back(std::move(component));
        ++ComponentCount;
        return Components.size() - 1;
    }

    //指定スロットのコンポーネントを破棄してtombstone化する
    //componentSlot : オブジェクト内スロット番号
    void Object::DetachComponent(std::size_t componentSlot)
    {
        if (componentSlot >= Components.size() || !Components[componentSlot])
        {
            return;
        }

        Components[componentSlot]->ClearRegistration();
        Components[componentSlot].reset();
        --ComponentCount;
    }

    //安定Slot番号から所有Componentを直接取得する
    //引数: componentSlot Object内の安定Slot番号
    //戻り値: 有効な所有Component、無効Slotの場合はnullptr
    Component* Object::GetComponentAt(std::size_t componentSlot)
    {
        if (componentSlot >= Components.size())
        {
            return nullptr;
        }

        return Components[componentSlot].get();
    }

    //安定Slot番号から読み取り専用の所有Componentを直接取得する
    //引数: componentSlot Object内の安定Slot番号
    //戻り値: 有効な所有Component、無効Slotの場合はnullptr
    const Component* Object::GetComponentAt(std::size_t componentSlot) const
    {
        if (componentSlot >= Components.size())
        {
            return nullptr;
        }

        return Components[componentSlot].get();
    }

    //所有者かつ同型内で一意なコンポーネント名を解決する
    //componentType : コンポーネント種別
    //requestedName : 希望名
    //戻り値 : 同名時に数値接尾辞を追加した一意名
    std::string Object::ResolveComponentName(
        ComponentType componentType,
        const std::string& requestedName
    )
    {
        const std::size_t TypeIndex = static_cast<std::size_t>(componentType); //型別索引
        std::string BaseName = requestedName.empty()
            ? GetDefaultComponentName(componentType)
            : requestedName; //空名を補完した基底名
        const auto& NameMap = ComponentIDByNameByType.at(TypeIndex); //現在の名前索引

        if (!NameMap.contains(BaseName))
        {
            return BaseName;
        }

        std::uint32_t& NextSuffix =
            ComponentSuffixByNameByType.at(TypeIndex)[BaseName]; //次に試す数値接尾辞

        if (NextSuffix == 0)
        {
            NextSuffix = 1;
        }

        std::string CandidateName; //接尾辞を加えた候補名
        do
        {
            CandidateName = BaseName + "_" + std::to_string(NextSuffix);
            ++NextSuffix;
        } while (NameMap.contains(CandidateName));

        return CandidateName;
    }

    //解決済みコンポーネント名とIDを登録する
    //componentType : コンポーネント種別
    //resolvedName : 解決済み名
    //componentID : 登録ID
    //戻り値 : 登録に成功した場合true
    bool Object::RegisterComponentName(
        ComponentType componentType,
        const std::string& resolvedName,
        ComponentID componentID
    )
    {
        const std::size_t TypeIndex = static_cast<std::size_t>(componentType); //型別索引
        return ComponentIDByNameByType.at(TypeIndex)
            .emplace(resolvedName, componentID).second;
    }

    //登録済みコンポーネント名を解除する
    //componentType : コンポーネント種別
    //resolvedName : 解決済み名
    void Object::UnregisterComponentName(
        ComponentType componentType,
        const std::string& resolvedName
    )
    {
        const std::size_t TypeIndex = static_cast<std::size_t>(componentType); //型別索引
        ComponentIDByNameByType.at(TypeIndex).erase(resolvedName);
    }

    //所有者かつ同型内の名前からコンポーネントIDを検索する
    //componentType : コンポーネント種別
    //resolvedName : 解決済み名
    //戻り値 : 見つからない場合は無効ID
    ComponentID Object::FindComponentID(
        ComponentType componentType,
        const std::string& resolvedName
    ) const
    {
        const std::size_t TypeIndex = static_cast<std::size_t>(componentType); //型別索引
        const auto& NameMap = ComponentIDByNameByType.at(TypeIndex); //検索対象の名前索引
        const auto Found = NameMap.find(resolvedName); //検索結果
        return Found == NameMap.end() ? ComponentID() : Found->second;
    }

    //ObjectManagerから登録情報を設定する
    //objectID : 割り当てるオブジェクトID
    //resolvedName : 同型内で一意に解決済みの名前
    void Object::AssignRegistration(
        ObjectID objectID,
        const std::string& resolvedName
    )
    {
        ID = objectID;
        Name = resolvedName;
    }
}
