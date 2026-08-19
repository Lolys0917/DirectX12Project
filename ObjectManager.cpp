//|| ObjectManager.cpp ||::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Scene内のObjectとComponentへ安定IDを割り当て走査なし検索を提供する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v2.50  非Active描画Objectの初期化と状態読取を追加
//||  2026_08_19  v2.40  Object名をScene内で一意にし名前単独検索を追加
//||  2026_08_17  v2.30  Editor用Object複製と名前索引更新処理を追加
//||  2026_07_13  v2.20  異常な登録、索引不整合及び初期化失敗をMessageLogへ記録
//||  2026_07_13  v2.10  IRenderable Objectの描画Lifecycleを統合
//||  2026_07_13  v2.00  強いID、Vector<Map>索引、所有者別Component索引を実装
//||

#include "ObjectManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "DirectX12.h"
#include "IRenderable.h"
#include "MessageLog.h"
#include "RenderContext.h"

namespace
{
    /**
     * ObjectManager処理失敗を対象ID付きでMessageLogへ追加する
     * @param operation 失敗した処理の説明
     * @param id 対象Object又はComponentのID
     * @param permanent 索引不整合として一括消去対象外にする場合はtrue
     */
    void AddObjectManagerFailureLog(
        const char* operation,
        std::uint32_t id,
        bool permanent
    )
    {
        char Message[320]{}; // 処理内容と対象IDを含む表示用メッセージ
        sprintf_s(
            Message,
            "[Error] ObjectManager | %s ID=%u.",
            operation,
            id
        );

        if (permanent)
        {
            Engine::MessageLog::GetInstance().AddPermanentLog(Message);
            return;
        }

        Engine::MessageLog::GetInstance().AddLog(Message);
    }
}

namespace Engine
{
    //空のObject、Component ID索引を持つManagerを作成する
    ObjectManager::ObjectManager()
        : ObjectCount(0)
        , ComponentCount(0)
    {
        ObjectsByID.emplace_back(nullptr);
        RenderableObjectInitializedByID.emplace_back(false);
        ComponentsByID.emplace_back();
    }

    //初期化済みRenderable ObjectとComponentを終了して全所有物を破棄する
    ObjectManager::~ObjectManager()
    {
        FinalizeComponents();
    }

    //未登録Objectの所有権を受け取り安定IDを割り当てる
    //引数: object 未登録Object、requestedName 希望名
    //戻り値: 登録済みObject、失敗時はnullptr
    Object* ObjectManager::AddObject(
        std::unique_ptr<Object> object,
        const std::string& requestedName
    )
    {
        if (!object || object->ID.IsValid() || !IsValidObjectType(object->Type))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] ObjectManager | AddObject rejected an invalid or already registered Object."
            );
            return nullptr;
        }

        if (ObjectsByID.size() >= (std::numeric_limits<std::uint32_t>::max)())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] ObjectManager | ObjectID space was exhausted."
            );
            return nullptr;
        }

        const std::string ResolvedName = ResolveObjectName(
            requestedName
        ); //Scene内で一意な登録名

        const ObjectID NewID(static_cast<std::uint32_t>(ObjectsByID.size())); //再利用しない新規Object ID
        const auto InsertResult = ObjectIDByName.emplace(
            ResolvedName,
            NewID
        ); //Scene内名前索引への登録結果

        if (!InsertResult.second)
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] ObjectManager | Resolved Object name collided with an existing index."
            );
            return nullptr;
        }

        Object* RegisteredObject = object.get(); //登録後も安定しているObject参照
        RegisteredObject->AssignRegistration(NewID, ResolvedName);
        ObjectsByID.emplace_back(std::move(object));
        RenderableObjectInitializedByID.emplace_back(false);
        ++ObjectCount;
        return RegisteredObject;
    }

    //未登録Componentを指定Objectへ所有させ安定IDを割り当てる
    //引数: ownerID 所有Object、component 未登録Component、requestedName 希望名
    //戻り値: 登録済みComponent、失敗時はnullptr
    Component* ObjectManager::AddComponent(
        ObjectID ownerID,
        std::unique_ptr<Component> component,
        const std::string& requestedName
    )
    {
        Object* Owner = FindObject(ownerID); //Componentを所有する登録済みObject

        if (!Owner || !component || component->ID.IsValid() || component->Owner != nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] ObjectManager | AddComponent rejected an invalid owner or registered Component."
            );
            return nullptr;
        }

        if (!IsValidComponentType(component->Type) ||
            ComponentsByID.size() >= (std::numeric_limits<std::uint32_t>::max)())
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] ObjectManager | Component type was invalid or ComponentID space was exhausted."
            );
            return nullptr;
        }

        const std::string ResolvedName = Owner->ResolveComponentName(
            component->Type,
            requestedName
        ); //OwnerかつComponent型内で一意な登録名

        const ComponentID NewID(
            static_cast<std::uint32_t>(ComponentsByID.size())
        ); //再利用しない新規Component ID

        const std::size_t OwnerSlot = Owner->AttachComponent(
            std::move(component)
        ); //Object内で削除後も詰めない安定slot

        Component* RegisteredComponent = Owner->GetComponentAt(OwnerSlot); //Objectへ所有移動したComponent

        if (!RegisteredComponent || !Owner->RegisterComponentName(
            RegisteredComponent->Type,
            ResolvedName,
            NewID))
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] ObjectManager | Component name registration failed after ownership transfer."
            );
            Owner->DetachComponent(OwnerSlot);
            return nullptr;
        }

        RegisteredComponent->AssignRegistration(*Owner, NewID, ResolvedName);

        ComponentLocation Location{}; //IDからOwner内実体を直接参照する位置情報
        Location.OwnerID = ownerID;
        Location.OwnerSlot = OwnerSlot;
        Location.Pointer = RegisteredComponent;
        ComponentsByID.emplace_back(Location);
        ++ComponentCount;
        return RegisteredComponent;
    }

    //Objectと所有Componentを削除してIDをtombstone化する
    //引数: objectID 削除するObject
    //戻り値: 削除した場合はtrue
    bool ObjectManager::RemoveObject(ObjectID objectID)
    {
        Object* Target = FindObject(objectID); //削除対象Object

        if (!Target)
        {
            return false;
        }

        const std::vector<ObjectID> ChildIDs = Target->Children; //再帰削除する直接Child一覧

        for (ObjectID ChildID : ChildIDs)
        {
            RemoveObject(ChildID);
        }

        if (Target->Parent != nullptr)
        {
            RemoveChildReference(*Target->Parent, objectID);
            Target->Parent = nullptr;
        }

        if (IRenderable* Renderable = dynamic_cast<IRenderable*>(Target)) //削除前にGPU Resourceを終了する
        {
            Renderable->Finalize();
            RenderableObjectInitializedByID[objectID.GetValue()] = false;
        }

        const std::vector<ComponentID> OwnedComponents =
            Target->GetComponentIDs(); //Object破棄前に無効化するComponent一覧

        for (ComponentID OwnedID : OwnedComponents) //削除対象Objectが所有するComponent ID
        {
            RemoveComponent(OwnedID);
        }

        ObjectIDByName.erase(Target->Name);
        ObjectsByID[objectID.GetValue()].reset();
        --ObjectCount;
        return true;
    }

    //概要：循環を防ぎながらObjectの親を変更する
    //引数：childID=変更するObject、parentID=新しい親又は無効ID、keepWorldTransform=見た目のWorld姿勢を維持する場合true
    //戻り値：親子関係を変更できた場合はtrue
    bool ObjectManager::SetParent(
        ObjectID childID,
        ObjectID parentID,
        bool keepWorldTransform
    )
    {
        Object* Child = FindObject(childID); //親を変更するObject
        Object* NewParent = parentID.IsValid() ? FindObject(parentID) : nullptr; //新しい親Object

        if (Child == nullptr || (parentID.IsValid() && NewParent == nullptr) ||
            Child == NewParent ||
            (NewParent != nullptr && IsDescendantOf(NewParent->GetID(), childID)))
        {
            return false;
        }

        if (Child->Parent == NewParent)
        {
            return true;
        }

        Transform NewLocalTransform = Child->ObjectTransform; //親変更後に設定するLocal姿勢

        if (keepWorldTransform)
        {
            const DirectX::XMMATRIX WorldMatrix = Child->GetWorldMatrix(); //変更前のWorld姿勢
            DirectX::XMMATRIX LocalMatrix = WorldMatrix; //新しい親から見たLocal姿勢

            if (NewParent != nullptr)
            {
                DirectX::XMVECTOR Determinant; //逆行列計算で使用する行列式
                const DirectX::XMMATRIX ParentInverse = DirectX::XMMatrixInverse(
                    &Determinant,
                    NewParent->GetWorldMatrix()
                ); //新しい親World行列の逆行列

                if (std::fabs(DirectX::XMVectorGetX(Determinant)) <= 0.000001f)
                {
                    return false;
                }

                LocalMatrix = WorldMatrix * ParentInverse;
            }

            if (!NewLocalTransform.SetLocalMatrix(LocalMatrix))
            {
                return false;
            }
        }

        if (Child->Parent != nullptr)
        {
            RemoveChildReference(*Child->Parent, childID);
        }

        Child->Parent = NewParent;
        Child->ObjectTransform = NewLocalTransform;

        if (NewParent != nullptr)
        {
            NewParent->Children.emplace_back(childID);
        }

        return true;
    }

    //概要：指定Objectが候補Ancestorの子孫か確認する
    //引数：objectID=確認するObject、possibleAncestorID=祖先候補Object
    //戻り値：親をたどって候補へ到達した場合はtrue
    bool ObjectManager::IsDescendantOf(
        ObjectID objectID,
        ObjectID possibleAncestorID
    ) const
    {
        const Object* Current = FindObject(objectID); //親を順に確認するObject

        while (Current != nullptr && Current->Parent != nullptr)
        {
            Current = Current->Parent;

            if (Current->GetID() == possibleAncestorID)
            {
                return true;
            }
        }

        return false;
    }

    //Componentを削除してIDとObject内slotをtombstone化する
    //引数: componentID 削除するComponent
    //戻り値: 削除した場合はtrue
    bool ObjectManager::RemoveComponent(ComponentID componentID)
    {
        Component* Target = FindComponent(componentID); //削除対象Component

        if (!Target)
        {
            return false;
        }

        ComponentLocation& Location = ComponentsByID[componentID.GetValue()]; //IDに対応するOwner位置
        Object* Owner = FindObject(Location.OwnerID); //Componentを所有するObject

        if (!Owner)
        {
            AddObjectManagerFailureLog(
                "Registered Component owner index is missing for Component",
                componentID.GetValue(),
                true
            );
            return false;
        }

        const ComponentType TargetType = Target->Type; //名前索引解除前のComponent型
        const std::string TargetName = Target->Name; //名前索引解除前の解決済み名

        if (Target->Initialized)
        {
            Target->Finalize();
            Target->Initialized = false;
        }

        Owner->UnregisterComponentName(TargetType, TargetName);
        Target->ClearRegistration();
        Owner->DetachComponent(Location.OwnerSlot);
        Location = {};
        --ComponentCount;
        return true;
    }

    //概要：指定Objectとその全Component定義を新しい安定IDで複製する
    //引数：sourceID=複製元Object ID、requestedName=複製先Objectの希望名
    //戻り値：登録済みの複製Object、失敗した場合はnullptr
    Object* ObjectManager::CloneObject(
        ObjectID sourceID,
        const std::string& requestedName
    )
    {
        const Object* Source = FindObject(sourceID); //複製元Object

        if (Source == nullptr)
        {
            return nullptr;
        }

        std::unique_ptr<Object> Definition = Source->Clone(); //IDを持たないObject定義

        if (!Definition)
        {
            AddObjectManagerFailureLog(
                "Object clone definition was unavailable for Object",
                sourceID.GetValue(),
                false
            );
            return nullptr;
        }

        Object* Duplicate = AddObject(
            std::move(Definition),
            requestedName.empty() ? Source->GetName() : requestedName
        ); //新しいIDを割り当てた複製Object

        if (Duplicate == nullptr)
        {
            return nullptr;
        }

        for (ComponentID SourceComponentID : Source->GetComponentIDs())
        {
            const Component* SourceComponent = FindComponent(SourceComponentID); //複製するComponent
            std::unique_ptr<Component> ComponentDefinition = SourceComponent == nullptr
                ? nullptr
                : SourceComponent->Clone(); //所有者とIDを持たないComponent定義

            if (SourceComponent == nullptr || !ComponentDefinition ||
                AddComponent(
                    Duplicate->GetID(),
                    std::move(ComponentDefinition),
                    SourceComponent->GetName()) == nullptr)
            {
                AddObjectManagerFailureLog(
                    "Component clone failed while cloning Object",
                    sourceID.GetValue(),
                    false
                );
                RemoveObject(Duplicate->GetID());
                return nullptr;
            }
        }

        return Duplicate;
    }

    //概要：Scene内名前索引を保ったまま一意な名前へ変更する
    //引数：objectID=変更対象Object ID、requestedName=新しい希望名
    //戻り値：名前を変更できた場合はtrue
    bool ObjectManager::RenameObject(
        ObjectID objectID,
        const std::string& requestedName
    )
    {
        Object* Target = FindObject(objectID); //名前を変更するObject

        if (Target == nullptr || requestedName.empty())
        {
            return false;
        }

        if (Target->Name == requestedName)
        {
            return true;
        }

        const std::string PreviousName = Target->Name; //失敗時に復元する従来名
        ObjectIDByName.erase(PreviousName);

        const std::string ResolvedName = ResolveObjectName(
            requestedName
        ); //Scene内で一意に解決した新しい名前

        if (!ObjectIDByName.emplace(ResolvedName, objectID).second)
        {
            ObjectIDByName.emplace(PreviousName, objectID);
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] ObjectManager | Object rename index registration failed."
            );
            return false;
        }

        Target->Name = ResolvedName;
        return true;
    }

    //概要：Componentの所有者別名前索引を保ったまま一意な名前へ変更する
    //引数：componentID=変更対象Component ID、requestedName=新しい希望名
    //戻り値：名前を変更できた場合はtrue
    bool ObjectManager::RenameComponent(
        ComponentID componentID,
        const std::string& requestedName
    )
    {
        Component* Target = FindComponent(componentID); //名前を変更するComponent

        if (Target == nullptr || Target->Owner == nullptr || requestedName.empty())
        {
            return false;
        }

        if (Target->Name == requestedName)
        {
            return true;
        }

        Object& Owner = *Target->Owner; //名前索引を所有するObject
        const ComponentType Type = Target->Type; //名前を解決するComponent型
        const std::string PreviousName = Target->Name; //失敗時に復元する従来名
        Owner.UnregisterComponentName(Type, PreviousName);

        const std::string ResolvedName = Owner.ResolveComponentName(
            Type,
            requestedName
        ); //所有者かつ型内で一意な新しい名前

        if (!Owner.RegisterComponentName(Type, ResolvedName, componentID))
        {
            Owner.RegisterComponentName(Type, PreviousName, componentID);
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] ObjectManager | Component rename index registration failed."
            );
            return false;
        }

        Target->Name = ResolvedName;
        return true;
    }

    //Object IDから走査せず登録Objectを取得する
    //引数: objectID 検索するObject ID
    //戻り値: 登録Object、無効または削除済みIDの場合はnullptr
    Object* ObjectManager::FindObject(ObjectID objectID)
    {
        if (!objectID.IsValid() || objectID.GetValue() >= ObjectsByID.size())
        {
            return nullptr;
        }

        return ObjectsByID[objectID.GetValue()].get();
    }

    //Object IDから走査せず読み取り専用の登録Objectを取得する
    //引数: objectID 検索するObject ID
    //戻り値: 登録Object、無効または削除済みIDの場合はnullptr
    const Object* ObjectManager::FindObject(ObjectID objectID) const
    {
        if (!objectID.IsValid() || objectID.GetValue() >= ObjectsByID.size())
        {
            return nullptr;
        }

        return ObjectsByID[objectID.GetValue()].get();
    }

    //Scene内で一意な解決済み名からObjectを平均O(1)で検索する
    //引数: resolvedName 解決済み名
    //戻り値: 見つかったObject、未登録時はnullptr
    Object* ObjectManager::FindObject(const std::string& resolvedName)
    {
        return const_cast<Object*>(
            static_cast<const ObjectManager&>(*this).FindObject(resolvedName)
        );
    }

    //Scene内で一意な解決済み名から読み取り専用Objectを平均O(1)で検索する
    //引数: resolvedName 解決済み名
    //戻り値: 見つかったObject、未登録時はnullptr
    const Object* ObjectManager::FindObject(const std::string& resolvedName) const
    {
        const auto Iterator = ObjectIDByName.find(resolvedName); //名前のhash検索結果
        return Iterator == ObjectIDByName.end() ? nullptr : FindObject(Iterator->second);
    }

    //型×解決済み名からObjectを平均O(1)で検索する
    //引数: objectType Object型、resolvedName 解決済み名
    //戻り値: 見つかったObject、未登録時はnullptr
    Object* ObjectManager::FindObject(
        ObjectType objectType,
        const std::string& resolvedName
    )
    {
        return const_cast<Object*>(
            static_cast<const ObjectManager&>(*this).FindObject(objectType, resolvedName)
        );
    }

    //型×解決済み名から読み取り専用Objectを平均O(1)で検索する
    //引数: objectType Object型、resolvedName 解決済み名
    //戻り値: 見つかったObject、未登録時はnullptr
    const Object* ObjectManager::FindObject(
        ObjectType objectType,
        const std::string& resolvedName
    ) const
    {
        if (!IsValidObjectType(objectType))
        {
            return nullptr;
        }

        const Object* Target = FindObject(resolvedName); //Scene内で一意な名前のObject
        return Target != nullptr && Target->GetType() == objectType ? Target : nullptr;
    }

    //Component IDから走査せず登録Componentを取得する
    //引数: componentID 検索するComponent ID
    //戻り値: 登録Component、無効または削除済みIDの場合はnullptr
    Component* ObjectManager::FindComponent(ComponentID componentID)
    {
        if (!componentID.IsValid() || componentID.GetValue() >= ComponentsByID.size())
        {
            return nullptr;
        }

        return ComponentsByID[componentID.GetValue()].Pointer;
    }

    //Component IDから走査せず読み取り専用の登録Componentを取得する
    //引数: componentID 検索するComponent ID
    //戻り値: 登録Component、無効または削除済みIDの場合はnullptr
    const Component* ObjectManager::FindComponent(ComponentID componentID) const
    {
        if (!componentID.IsValid() || componentID.GetValue() >= ComponentsByID.size())
        {
            return nullptr;
        }

        return ComponentsByID[componentID.GetValue()].Pointer;
    }

    //Owner×型×解決済み名からComponentを平均O(1)で検索する
    //引数: ownerID 所有Object、componentType Component型、resolvedName 解決済み名
    //戻り値: 見つかったComponent、未登録時はnullptr
    Component* ObjectManager::FindComponent(
        ObjectID ownerID,
        ComponentType componentType,
        const std::string& resolvedName
    )
    {
        return const_cast<Component*>(
            static_cast<const ObjectManager&>(*this).FindComponent(
                ownerID,
                componentType,
                resolvedName
            )
        );
    }

    //Owner×型×解決済み名から読み取り専用Componentを平均O(1)で検索する
    //引数: ownerID 所有Object、componentType Component型、resolvedName 解決済み名
    //戻り値: 見つかったComponent、未登録時はnullptr
    const Component* ObjectManager::FindComponent(
        ObjectID ownerID,
        ComponentType componentType,
        const std::string& resolvedName
    ) const
    {
        const Object* Owner = FindObject(ownerID); //検索対象の所有Object

        if (!Owner || !IsValidComponentType(componentType))
        {
            return nullptr;
        }

        return FindComponent(Owner->FindComponentID(componentType, resolvedName));
    }

    //有効Object IDのスナップショットを取得する
    //戻り値: 登録順の有効Object ID一覧
    std::vector<ObjectID> ObjectManager::GetObjectIDs() const
    {
        std::vector<ObjectID> Result; //返却する有効Object ID一覧
        Result.reserve(ObjectCount);

        for (std::size_t Index = 1; Index < ObjectsByID.size(); ++Index) //有効Objectを確認するID Slot
        {
            if (ObjectsByID[Index])
            {
                Result.emplace_back(static_cast<std::uint32_t>(Index));
            }
        }

        return Result;
    }

    //有効Component IDのスナップショットを取得する
    //戻り値: 登録順の有効Component ID一覧
    std::vector<ComponentID> ObjectManager::GetComponentIDs() const
    {
        std::vector<ComponentID> Result; //返却する有効Component ID一覧
        Result.reserve(ComponentCount);

        for (std::size_t Index = 1; Index < ComponentsByID.size(); ++Index) //有効Componentを確認するID Slot
        {
            if (ComponentsByID[Index].Pointer)
            {
                Result.emplace_back(static_cast<std::uint32_t>(Index));
            }
        }

        return Result;
    }

    //指定Objectが所有するComponent IDを取得する
    //引数: ownerID 所有Object
    //戻り値: Object内登録順の有効Component ID一覧
    std::vector<ComponentID> ObjectManager::GetComponentIDs(ObjectID ownerID) const
    {
        const Object* Owner = FindObject(ownerID); //Component一覧を取得するObject
        return Owner ? Owner->GetComponentIDs() : std::vector<ComponentID>{};
    }

    //指定型の有効Componentを描画等の一括処理用に取得する
    //引数: componentType 取得するComponent型
    //戻り値: 登録順の非所有Pointer一覧、無効型の場合は空
    std::vector<Component*> ObjectManager::FindComponentsByType(
        ComponentType componentType
    )
    {
        std::vector<Component*> Result; //指定型に一致したComponent一覧

        if (!IsValidComponentType(componentType))
        {
            return Result;
        }

        for (std::size_t Index = 1; Index < ComponentsByID.size(); ++Index) //指定型と比較するComponent ID Slot
        {
            Component* Target = ComponentsByID[Index].Pointer; //型を判定する登録Component

            if (Target && Target->Type == componentType)
            {
                Result.emplace_back(Target);
            }
        }

        return Result;
    }

    //指定型の有効Componentを読み取り用に取得する
    //引数: componentType 取得するComponent型
    //戻り値: 登録順の非所有Pointer一覧、無効型の場合は空
    std::vector<const Component*> ObjectManager::FindComponentsByType(
        ComponentType componentType
    ) const
    {
        std::vector<const Component*> Result; //指定型に一致したComponent一覧

        if (!IsValidComponentType(componentType))
        {
            return Result;
        }

        for (std::size_t Index = 1; Index < ComponentsByID.size(); ++Index) //指定型と比較するComponent ID Slot
        {
            const Component* Target = ComponentsByID[Index].Pointer; //型を判定する登録Component

            if (Target && Target->Type == componentType)
            {
                Result.emplace_back(Target);
            }
        }

        return Result;
    }

    //未初期化Renderable ObjectとComponentを初期化する
    //引数: dx12 GPU Resource作成に使用する描画基盤
    //戻り値: 全未初期化対象が成功した場合はtrue
    bool ObjectManager::InitializeComponents(DirectX12& dx12)
    {
        bool Succeeded = true; //全描画対象の初期化結果

        for (std::size_t Index = 1; Index < ObjectsByID.size(); ++Index) //Renderable Objectを確認するID Slot
        {
            Object* TargetObject = ObjectsByID[Index].get(); //現在初期化を試みるObject

            if (!TargetObject || RenderableObjectInitializedByID[Index])
            {
                continue;
            }

            IRenderable* Renderable = dynamic_cast<IRenderable*>(TargetObject); //Objectの描画契約

            if (Renderable == nullptr)
            {
                continue;
            }

            if (Renderable->CreateGPUResource(dx12))
            {
                RenderableObjectInitializedByID[Index] = true;
            }
            else
            {
                AddObjectManagerFailureLog(
                    "Renderable Object GPU initialization failed for Object",
                    static_cast<std::uint32_t>(Index),
                    false
                );
                Succeeded = false;
            }
        }

        for (std::size_t Index = 1; Index < ComponentsByID.size(); ++Index) //初期化状態を確認するComponent ID Slot
        {
            Component* Target = ComponentsByID[Index].Pointer; //現在初期化を試みるComponent

            if (!Target || Target->Initialized)
            {
                continue;
            }

            if (Target->Initialize(dx12))
            {
                Target->Initialized = true;
            }
            else
            {
                AddObjectManagerFailureLog(
                    "Component initialization failed for Component",
                    static_cast<std::uint32_t>(Index),
                    false
                );
                Succeeded = false;
            }
        }

        return Succeeded;
    }

    //指定Renderable ObjectのGPU初期化が完了しているか確認する
    //引数: objectID 確認対象Object
    //戻り値: 登録済みRenderable Objectの初期化が完了している場合はtrue
    bool ObjectManager::IsRenderableObjectInitialized(ObjectID objectID) const
    {
        const std::size_t Index = objectID.GetValue(); //状態配列へ使用するObject ID値
        return Index > 0 && Index < ObjectsByID.size() && ObjectsByID[Index] != nullptr &&
            dynamic_cast<const IRenderable*>(ObjectsByID[Index].get()) != nullptr &&
            RenderableObjectInitializedByID[Index];
    }

    //有効Renderable Objectと初期化済みComponentを更新する
    //引数: deltaTime 前回更新からの秒数
    void ObjectManager::UpdateComponents(float deltaTime)
    {
        for (std::size_t Index = 1; Index < ObjectsByID.size(); ++Index) //更新対象のRenderable Object Slot
        {
            Object* TargetObject = ObjectsByID[Index].get(); //現在更新するObject

            if (!TargetObject || !TargetObject->Active ||
                !RenderableObjectInitializedByID[Index])
            {
                continue;
            }

            if (IRenderable* Renderable = dynamic_cast<IRenderable*>(TargetObject)) //Object描画契約
            {
                Renderable->Update(deltaTime);
            }
        }

        for (std::size_t Index = 1; Index < ComponentsByID.size(); ++Index) //更新対象を確認するComponent ID Slot
        {
            Component* Target = ComponentsByID[Index].Pointer; //現在更新するComponent

            if (!Target || !Target->Initialized || !Target->Active ||
                !Target->Owner || !Target->Owner->Active)
            {
                continue;
            }

            Target->Update(deltaTime);
        }
    }

    //有効Renderable Objectと初期化済みComponentを現在のCamera passへ描画する
    //引数: renderContext 描画基盤とCameraを持つContext
    void ObjectManager::DrawComponents(const RenderContext& renderContext)
    {
        for (std::size_t Index = 1; Index < ObjectsByID.size(); ++Index) //描画対象のRenderable Object Slot
        {
            Object* TargetObject = ObjectsByID[Index].get(); //現在描画するObject

            if (!TargetObject || !TargetObject->Active ||
                !RenderableObjectInitializedByID[Index])
            {
                continue;
            }

            if (IRenderable* Renderable = dynamic_cast<IRenderable*>(TargetObject)) //Object描画契約
            {
                Renderable->Draw(renderContext);
            }
        }

        for (std::size_t Index = 1; Index < ComponentsByID.size(); ++Index) //描画対象を確認するComponent ID Slot
        {
            Component* Target = ComponentsByID[Index].Pointer; //現在描画するComponent

            if (!Target || !Target->Initialized || !Target->Active ||
                !Target->Owner || !Target->Owner->Active)
            {
                continue;
            }

            Target->Draw(renderContext);
        }
    }

    //初期化済みComponentとRenderable Objectを登録逆順に終了する
    void ObjectManager::FinalizeComponents()
    {
        for (std::size_t Index = ComponentsByID.size(); Index > 1; --Index) //逆順終了用のComponent ID Slot境界
        {
            Component* Target = ComponentsByID[Index - 1].Pointer; //現在終了するComponent

            if (Target && Target->Initialized)
            {
                Target->Finalize();
                Target->Initialized = false;
            }
        }

        for (std::size_t Index = ObjectsByID.size(); Index > 1; --Index) //逆順終了用のObject ID Slot境界
        {
            Object* TargetObject = ObjectsByID[Index - 1].get(); //現在終了するObject

            if (!TargetObject)
            {
                continue;
            }

            if (IRenderable* Renderable = dynamic_cast<IRenderable*>(TargetObject)) //Object描画契約
            {
                Renderable->Finalize();
                RenderableObjectInitializedByID[Index - 1] = false;
            }
        }
    }

    //ObjectとComponentのCPU定義を独立Managerへ複製する
    //戻り値: IDを新規発行した未初期化ObjectManager
    std::unique_ptr<ObjectManager> ObjectManager::CloneDefinition() const
    {
        auto Duplicate = std::make_unique<ObjectManager>(); //複製先ObjectManager
        std::unordered_map<ObjectID, ObjectID> ObjectIDRemap; //複製元から複製先へのOwner ID対応
        ObjectIDRemap.reserve(ObjectCount);

        for (ObjectID SourceID : GetObjectIDs()) //複製元の有効Object ID
        {
            const Object* Source = FindObject(SourceID); //複製元Object
            std::unique_ptr<Object> ObjectDefinition = Source ? Source->Clone() : nullptr; //GPU状態を持たないObject定義

            if (!Source || !ObjectDefinition)
            {
                AddObjectManagerFailureLog(
                    "Object clone definition was unavailable for Object",
                    SourceID.GetValue(),
                    false
                );
                return nullptr;
            }

            Object* Destination = Duplicate->AddObject(
                std::move(ObjectDefinition),
                Source->GetName()
            ); //複製先へ登録したObject

            if (!Destination)
            {
                AddObjectManagerFailureLog(
                    "Cloned Object registration failed for source Object",
                    SourceID.GetValue(),
                    false
                );
                return nullptr;
            }

            ObjectIDRemap.emplace(SourceID, Destination->GetID());
        }

        for (ObjectID SourceID : GetObjectIDs())
        {
            const Object* Source = FindObject(SourceID); //親子関係を複製する元Object

            if (Source == nullptr || !Source->GetParentID().IsValid())
            {
                continue;
            }

            const auto ChildIterator = ObjectIDRemap.find(SourceID); //複製先Child ID
            const auto ParentIterator = ObjectIDRemap.find(Source->GetParentID()); //複製先Parent ID

            if (ChildIterator == ObjectIDRemap.end() || ParentIterator == ObjectIDRemap.end() ||
                !Duplicate->SetParent(
                    ChildIterator->second,
                    ParentIterator->second,
                    false
                ))
            {
                AddObjectManagerFailureLog(
                    "Cloned Object hierarchy registration failed for source Object",
                    SourceID.GetValue(),
                    false
                );
                return nullptr;
            }
        }

        for (ComponentID SourceID : GetComponentIDs()) //複製元の有効Component ID
        {
            const Component* Source = FindComponent(SourceID); //複製元Component

            if (!Source || !Source->GetOwner())
            {
                AddObjectManagerFailureLog(
                    "Component clone source or owner was unavailable for Component",
                    SourceID.GetValue(),
                    true
                );
                return nullptr;
            }

            const auto OwnerIterator = ObjectIDRemap.find(Source->GetOwner()->GetID()); //複製先Owner検索結果
            std::unique_ptr<Component> ComponentDefinition = Source->Clone(); //GPU状態を持たないComponent定義

            if (OwnerIterator == ObjectIDRemap.end() || !ComponentDefinition ||
                !Duplicate->AddComponent(
                    OwnerIterator->second,
                    std::move(ComponentDefinition),
                    Source->GetName()))
            {
                AddObjectManagerFailureLog(
                    "Cloned Component registration failed for source Component",
                    SourceID.GetValue(),
                    false
                );
                return nullptr;
            }
        }

        return Duplicate;
    }

    //有効な登録Object数を取得する
    //戻り値: tombstoneを除くObject数
    std::size_t ObjectManager::GetObjectCount() const
    {
        return ObjectCount;
    }

    //有効な登録Component数を取得する
    //戻り値: tombstoneを除くComponent数
    std::size_t ObjectManager::GetComponentCount() const
    {
        return ComponentCount;
    }

    //希望名からScene内で一意なObject名を解決する
    //引数: requestedName 希望名
    //戻り値: 空名を補正し必要なら_数値を加えた名前
    std::string ObjectManager::ResolveObjectName(const std::string& requestedName)
    {
        const std::string BaseName = requestedName.empty() ? "Object" : requestedName; //接尾辞を除く基底名

        if (!ObjectIDByName.contains(BaseName))
        {
            ObjectSuffixByName.try_emplace(BaseName, 1);
            return BaseName;
        }

        std::uint32_t& NextSuffix = ObjectSuffixByName[BaseName]; //再走査を避ける次回接尾辞

        if (NextSuffix == 0)
        {
            NextSuffix = 1;
        }

        std::string Candidate; //Scene内で一意になるまで更新する候補名

        do
        {
            Candidate = BaseName + "_" + std::to_string(NextSuffix);
            ++NextSuffix;
        } while (ObjectIDByName.contains(Candidate));

        return Candidate;
    }

    //Object型が索引範囲内か判定する
    //引数: objectType 判定するObject型
    //戻り値: 型が有効な場合はtrue
    bool ObjectManager::IsValidObjectType(ObjectType objectType)
    {
        return static_cast<std::size_t>(objectType) <
            static_cast<std::size_t>(ObjectType::Count);
    }

    //Component型が索引範囲内か判定する
    //引数: componentType 判定するComponent型
    //戻り値: 型が有効な場合はtrue
    bool ObjectManager::IsValidComponentType(ComponentType componentType)
    {
        return static_cast<std::size_t>(componentType) <
            static_cast<std::size_t>(ComponentType::Count);
    }

    //概要：親Objectの直接Child一覧から指定IDを取り除く
    //引数：parent=参照を所有する親Object、childID=取り除くChild Object ID
    //戻り値：なし
    void ObjectManager::RemoveChildReference(Object& parent, ObjectID childID)
    {
        parent.Children.erase(
            std::remove(parent.Children.begin(), parent.Children.end(), childID),
            parent.Children.end()
        );
    }
}
