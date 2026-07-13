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
        , Position(0.0f, 0.0f, 0.0f)
        , Rotation(0.0f, 0.0f, 0.0f)
        , Scale(1.0f, 1.0f, 1.0f)
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
        destination.Position = Position;
        destination.Rotation = Rotation;
        destination.Scale = Scale;
    }

    //オブジェクトのワールド行列を取得する
    //戻り値 : 拡縮、回転、平行移動を合成したワールド行列
    DirectX::XMMATRIX Object::GetWorldMatrix() const
    {
        DirectX::XMMATRIX ScaleMatrix = DirectX::XMMatrixScaling(
            Scale.x,
            Scale.y,
            Scale.z
        ); //拡縮行列

        DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
            Rotation.x,
            Rotation.y,
            Rotation.z
        ); //回転行列

        DirectX::XMMATRIX TranslationMatrix = DirectX::XMMatrixTranslation(
            Position.x,
            Position.y,
            Position.z
        ); //平行移動行列

        return ScaleMatrix * RotationMatrix * TranslationMatrix;
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
