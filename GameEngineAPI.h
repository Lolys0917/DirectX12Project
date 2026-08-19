//|| GameEngineAPI.h ||:::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  SceneのInit、Update、Endから名前指定で使える組込みGame APIを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_20  v2.10  Object／Component Handle、集合検索、終了順序APIを追加
//||  2026_08_19  v2.00  Scene登録と名前指定High Level APIを追加
//||  2026_08_19  v1.00  名前付きCapsule生成と3軸外形寸法APIを追加
//||

#pragma once

#include <cstddef>
#include <cstdint>
#include <cctype>
#include <new>
#include <string>
#include <vector>

#include "EngineExtensionAPI.h"

namespace EngineGame
{
    namespace Detail
    {
        inline thread_local const EngineHostAPI* ActiveHost = nullptr; //現在Callback用Host
        inline thread_local std::uint32_t ActiveSceneID = 0; //現在Callback用Scene

        //概要：追記式Host APIの指定Memberが利用可能か確認する
        //引数：host=Host関数表、memberEnd=対象Member終端Offset
        //戻り値：Memberを安全に参照できる場合はtrue
        inline bool HasHostMember(const EngineHostAPI* host, std::size_t memberEnd)
        {
            return host != nullptr && host->Size >= memberEnd;
        }

        //概要：現在SceneのID指定Object情報を取得する
        //引数：objectID=安定ID、information=情報格納先
        //戻り値：Objectが存在する場合はtrue
        inline bool FindObjectInformation(
            const EngineHostAPI* host,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            EngineExternalObjectInfo& information
        )
        {
            if (host == nullptr || sceneID == 0 || objectID == 0)
            {
                return false;
            }

            constexpr std::size_t DirectMemberEnd =
                offsetof(EngineHostAPI, GetObjectInfoByID) +
                sizeof(decltype(EngineHostAPI::GetObjectInfoByID));

            if (HasHostMember(host, DirectMemberEnd) &&
                host->GetObjectInfoByID != nullptr)
            {
                information = EngineExternalObjectInfo{};
                information.Size = sizeof(EngineExternalObjectInfo);
                return host->GetObjectInfoByID(
                    host->Context,
                    sceneID,
                    objectID,
                    &information
                );
            }

            if (host->GetObjectCount == nullptr || host->GetObjectInfo == nullptr)
            {
                return false;
            }

            const std::uint32_t Count = host->GetObjectCount(
                host->Context,
                sceneID
            ); //現在SceneのObject数

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalObjectInfo Candidate{}; //照合中のObject
                Candidate.Size = sizeof(EngineExternalObjectInfo);

                if (host->GetObjectInfo(
                    host->Context,
                    sceneID,
                    Index,
                    &Candidate
                ) && Candidate.ObjectID == objectID)
                {
                    information = Candidate;
                    return true;
                }
            }

            return false;
        }

        inline bool FindObjectInformation(
            std::uint32_t objectID,
            EngineExternalObjectInfo& information
        )
        {
            return FindObjectInformation(
                ActiveHost,
                ActiveSceneID,
                objectID,
                information
            );
        }

        //概要：現在Sceneの一意名を安定Object IDへ解決する
        //引数：name=Object名
        //戻り値：一致ID、未登録時は0
        inline std::uint32_t FindObjectID(const std::string& name)
        {
            if (ActiveHost == nullptr || ActiveSceneID == 0 || name.empty())
            {
                return 0;
            }

            constexpr std::size_t NamedMemberEnd =
                offsetof(EngineHostAPI, FindObjectByNameOnly) +
                sizeof(decltype(EngineHostAPI::FindObjectByNameOnly)); //名前API終端

            if (HasHostMember(ActiveHost, NamedMemberEnd) &&
                ActiveHost->FindObjectByNameOnly != nullptr)
            {
                return ActiveHost->FindObjectByNameOnly(
                    ActiveHost->Context,
                    ActiveSceneID,
                    name.c_str()
                );
            }

            const std::uint32_t Count = ActiveHost->GetObjectCount == nullptr
                ? 0
                : ActiveHost->GetObjectCount(
                    ActiveHost->Context,
                    ActiveSceneID
                ); //Fallback検索数

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalObjectInfo Information{}; //名前照合用情報
                Information.Size = sizeof(EngineExternalObjectInfo);

                if (ActiveHost->GetObjectInfo != nullptr &&
                    ActiveHost->GetObjectInfo(
                        ActiveHost->Context,
                        ActiveSceneID,
                        Index,
                        &Information
                    ) && name == Information.Name)
                {
                    return Information.ObjectID;
                }
            }

            return 0;
        }
    }

    class ComponentHandle final
    {
    public:
        ComponentHandle() = default;

        ComponentHandle(
            const EngineHostAPI* host,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            std::uint32_t componentID
        )
            : Host(host)
            , SceneID(sceneID)
            , ObjectID(objectID)
            , ComponentID(componentID)
        {
        }

        std::uint32_t GetID() const { return ComponentID; }
        std::uint32_t GetObjectID() const { return ObjectID; }
        std::uint32_t GetSceneID() const { return SceneID; }

        bool TryGetInfo(EngineExternalComponentInfo& information) const
        {
            if (Host == nullptr || SceneID == 0 || ObjectID == 0 || ComponentID == 0)
            {
                return false;
            }

            constexpr std::size_t DirectMemberEnd =
                offsetof(EngineHostAPI, GetComponentInfoByID) +
                sizeof(decltype(EngineHostAPI::GetComponentInfoByID));

            if (Detail::HasHostMember(Host, DirectMemberEnd) &&
                Host->GetComponentInfoByID != nullptr)
            {
                information = EngineExternalComponentInfo{};
                information.Size = sizeof(EngineExternalComponentInfo);
                return Host->GetComponentInfoByID(
                    Host->Context,
                    SceneID,
                    ComponentID,
                    &information
                );
            }

            if (Host->GetComponentCount == nullptr || Host->GetComponentInfo == nullptr)
            {
                return false;
            }

            const std::uint32_t Count = Host->GetComponentCount(
                Host->Context,
                SceneID,
                ObjectID
            );

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalComponentInfo Candidate{};
                Candidate.Size = sizeof(EngineExternalComponentInfo);

                if (Host->GetComponentInfo(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    Index,
                    &Candidate
                ) && Candidate.ComponentID == ComponentID)
                {
                    information = Candidate;
                    return true;
                }
            }

            return false;
        }

        bool IsValid() const
        {
            EngineExternalComponentInfo Information{};
            return TryGetInfo(Information);
        }

        std::string GetName() const
        {
            EngineExternalComponentInfo Information{};
            return TryGetInfo(Information) ? Information.Name : std::string();
        }

        EngineExternalComponentType GetType() const
        {
            EngineExternalComponentInfo Information{};
            return TryGetInfo(Information)
                ? static_cast<EngineExternalComponentType>(Information.ComponentType)
                : EngineExternalComponentType::Component;
        }

        bool SetActive(bool active) const
        {
            return Host != nullptr && ComponentID != 0 &&
                Host->SetComponentActive != nullptr &&
                Host->SetComponentActive(
                    Host->Context,
                    SceneID,
                    ComponentID,
                    active
                );
        }

        bool Remove() const
        {
            return Host != nullptr && ComponentID != 0 &&
                Host->RemoveComponent != nullptr &&
                Host->RemoveComponent(Host->Context, SceneID, ComponentID);
        }

        explicit operator bool() const { return IsValid(); }
        operator std::uint32_t() const { return ComponentID; }

    private:
        const EngineHostAPI* Host = nullptr;
        std::uint32_t SceneID = 0;
        std::uint32_t ObjectID = 0;
        std::uint32_t ComponentID = 0;
    };

    class ObjectHandle final
    {
    public:
        ObjectHandle() = default;

        ObjectHandle(
            const EngineHostAPI* host,
            std::uint32_t sceneID,
            std::uint32_t objectID
        )
            : Host(host)
            , SceneID(sceneID)
            , ObjectID(objectID)
        {
        }

        std::uint32_t GetID() const { return ObjectID; }
        std::uint32_t GetSceneID() const { return SceneID; }

        bool TryGetInfo(EngineExternalObjectInfo& information) const
        {
            return Detail::FindObjectInformation(
                Host,
                SceneID,
                ObjectID,
                information
            );
        }

        bool IsValid() const
        {
            EngineExternalObjectInfo Information{};
            return TryGetInfo(Information);
        }

        std::string GetName() const
        {
            EngineExternalObjectInfo Information{};
            return TryGetInfo(Information) ? Information.Name : std::string();
        }

        EngineExternalObjectType GetType() const
        {
            EngineExternalObjectInfo Information{};
            return TryGetInfo(Information)
                ? static_cast<EngineExternalObjectType>(Information.ObjectType)
                : EngineExternalObjectType::Object;
        }

        std::vector<ComponentHandle> GetComponents() const
        {
            std::vector<ComponentHandle> Result;

            if (Host == nullptr || Host->GetComponentCount == nullptr ||
                Host->GetComponentInfo == nullptr || ObjectID == 0)
            {
                return Result;
            }

            const std::uint32_t Count = Host->GetComponentCount(
                Host->Context,
                SceneID,
                ObjectID
            );
            Result.reserve(Count);

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalComponentInfo Information{};
                Information.Size = sizeof(EngineExternalComponentInfo);

                if (Host->GetComponentInfo(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    Index,
                    &Information
                ))
                {
                    Result.emplace_back(
                        Host,
                        SceneID,
                        ObjectID,
                        Information.ComponentID
                    );
                }
            }

            return Result;
        }

        std::vector<ComponentHandle> GetComponents(
            EngineExternalComponentType type
        ) const
        {
            std::vector<ComponentHandle> Result;

            if (Host == nullptr || Host->GetComponentCount == nullptr ||
                Host->GetComponentInfo == nullptr || ObjectID == 0)
            {
                return Result;
            }

            const std::uint32_t Count = Host->GetComponentCount(
                Host->Context,
                SceneID,
                ObjectID
            );
            Result.reserve(Count);

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalComponentInfo Information{};
                Information.Size = sizeof(EngineExternalComponentInfo);

                if (Host->GetComponentInfo(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    Index,
                    &Information
                ) && Information.ComponentType == static_cast<std::uint32_t>(type))
                {
                    Result.emplace_back(
                        Host,
                        SceneID,
                        ObjectID,
                        Information.ComponentID
                    );
                }
            }

            return Result;
        }

        ComponentHandle GetComponent(EngineExternalComponentType type) const
        {
            const std::vector<ComponentHandle> Components = GetComponents(type);
            return Components.empty() ? ComponentHandle() : Components.front();
        }

        bool HasComponent(EngineExternalComponentType type) const
        {
            return static_cast<bool>(GetComponent(type));
        }

        bool SetSize(float width, float height, float depth) const
        {
            constexpr std::size_t MemberEnd = offsetof(EngineHostAPI, SetObjectSize) +
                sizeof(decltype(EngineHostAPI::SetObjectSize));
            const EngineExternalVector3 Size{ width, height, depth };
            return Detail::HasHostMember(Host, MemberEnd) && ObjectID != 0 &&
                Host->SetObjectSize != nullptr && Host->SetObjectSize(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    &Size
                );
        }

        bool SetTransform(
            float x,
            float y,
            float z,
            float rotationX = 0.0f,
            float rotationY = 0.0f,
            float rotationZ = 0.0f,
            float scaleX = 1.0f,
            float scaleY = 1.0f,
            float scaleZ = 1.0f
        ) const
        {
            const EngineExternalTransform Transform
            {
                { x, y, z },
                { rotationX, rotationY, rotationZ },
                { scaleX, scaleY, scaleZ }
            };
            return Host != nullptr && ObjectID != 0 &&
                Host->SetObjectTransform != nullptr && Host->SetObjectTransform(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    &Transform
                );
        }

        bool SetPosition(float x, float y, float z) const
        {
            EngineExternalObjectInfo Information{};

            if (!TryGetInfo(Information))
            {
                return false;
            }

            Information.LocalTransform.Position = { x, y, z };
            return Host->SetObjectTransform != nullptr && Host->SetObjectTransform(
                Host->Context,
                SceneID,
                ObjectID,
                &Information.LocalTransform
            );
        }

        bool Move(float x, float y, float z) const
        {
            EngineExternalObjectInfo Information{};

            if (!TryGetInfo(Information))
            {
                return false;
            }

            return SetPosition(
                Information.LocalTransform.Position.X + x,
                Information.LocalTransform.Position.Y + y,
                Information.LocalTransform.Position.Z + z
            );
        }

        bool SetColor(
            float red,
            float green,
            float blue,
            float alpha = 1.0f
        ) const
        {
            const EngineExternalColor Color{ red, green, blue, alpha };
            return Host != nullptr && ObjectID != 0 &&
                Host->SetObjectColor != nullptr && Host->SetObjectColor(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    &Color
                );
        }

        bool MultiplyColor(
            float red,
            float green,
            float blue,
            float alpha = 1.0f
        ) const
        {
            const EngineExternalColor Color{ red, green, blue, alpha };
            return Host != nullptr && ObjectID != 0 &&
                Host->MultiplyObjectColor != nullptr && Host->MultiplyObjectColor(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    &Color
                );
        }

        bool SetActive(bool active) const
        {
            return Host != nullptr && ObjectID != 0 &&
                Host->SetObjectActive != nullptr && Host->SetObjectActive(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    active
                );
        }

        bool AttachScript(const std::string& scriptKey) const
        {
            return Host != nullptr && ObjectID != 0 && !scriptKey.empty() &&
                Host->AttachScript != nullptr && Host->AttachScript(
                    Host->Context,
                    SceneID,
                    ObjectID,
                    scriptKey.c_str()
                );
        }

        bool Remove() const
        {
            return Host != nullptr && ObjectID != 0 && Host->RemoveObject != nullptr &&
                Host->RemoveObject(Host->Context, SceneID, ObjectID);
        }

        explicit operator bool() const { return IsValid(); }
        operator std::uint32_t() const { return ObjectID; }

    private:
        const EngineHostAPI* Host = nullptr;
        std::uint32_t SceneID = 0;
        std::uint32_t ObjectID = 0;
    };

    class AddObjectAPI final
    {
    public:
        //概要：指定SceneへObjectを追加するAPIを作成する
        //引数：host=Engine関数表、sceneID=対象Scene
        //戻り値：なし
        AddObjectAPI(const EngineHostAPI* host, std::uint32_t sceneID)
            : Host(host)
            , SceneID(sceneID)
        {
        }

        ObjectHandle Create(
            EngineExternalObjectType type,
            const std::string& name,
            std::uint32_t parentObjectID = 0
        ) const
        {
            if (Host == nullptr || SceneID == 0 || name.empty() ||
                Host->CreateObject == nullptr)
            {
                return ObjectHandle();
            }

            return ObjectHandle(Host, SceneID, Host->CreateObject(
                Host->Context,
                SceneID,
                static_cast<std::uint32_t>(type),
                name.c_str(),
                parentObjectID
            ));
        }

        ObjectHandle CreateCapsuleModel(
            const std::string& name,
            std::uint32_t parentObjectID = 0
        ) const
        {
            constexpr std::size_t CapsuleMemberEnd =
                offsetof(EngineHostAPI, CreateCapsuleModel) +
                sizeof(decltype(EngineHostAPI::CreateCapsuleModel)); //専用API終端

            if (Detail::HasHostMember(Host, CapsuleMemberEnd) &&
                Host->CreateCapsuleModel != nullptr && SceneID != 0 &&
                !name.empty())
            {
                return ObjectHandle(Host, SceneID, Host->CreateCapsuleModel(
                    Host->Context,
                    SceneID,
                    name.c_str(),
                    parentObjectID
                ));
            }

            return Create(EngineExternalObjectType::Capsule, name, parentObjectID);
        }

        ObjectHandle CreateBox(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Box, name);
        }

        ObjectHandle CreateSphere(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Sphere, name);
        }

        ObjectHandle CreatePlane(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Plane, name);
        }

        ObjectHandle CreateCylinder(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Cylinder, name);
        }

        std::vector<ObjectHandle> CreateMany(
            EngineExternalObjectType type,
            const std::string& baseName,
            std::uint32_t count
        ) const
        {
            std::vector<ObjectHandle> Result;
            Result.reserve(count);

            for (std::uint32_t Index = 0; Index < count; ++Index)
            {
                ObjectHandle Created = Create(
                    type,
                    baseName + std::to_string(Index)
                );

                if (Created.GetID() != 0)
                {
                    Result.emplace_back(Created);
                }
            }

            return Result;
        }

    private:
        const EngineHostAPI* Host; //Engine所有関数表
        std::uint32_t SceneID; //生成先Scene
    };

    class EngineProgramAPI final
    {
    public:
        //概要：互換用の明示Host APIをMain又は指定Sceneへ接続する
        //引数：host=Engine関数表、sceneID=対象Scene又は自動選択用0
        //戻り値：なし
        explicit EngineProgramAPI(
            const EngineHostAPI* host,
            std::uint32_t sceneID = 0
        )
            : Host(host)
            , SceneID(ResolveSceneID(host, sceneID))
            , AddObject(host, SceneID)
        {
        }

        std::uint32_t GetSceneID() const
        {
            return SceneID;
        }

        std::uint32_t FindObjectID(const std::string& name) const
        {
            const EngineHostAPI* PreviousHost = Detail::ActiveHost; //入れ子前Host
            const std::uint32_t PreviousScene = Detail::ActiveSceneID; //入れ子前Scene
            Detail::ActiveHost = Host;
            Detail::ActiveSceneID = SceneID;
            const std::uint32_t Result = Detail::FindObjectID(name); //名前解決結果
            Detail::ActiveHost = PreviousHost;
            Detail::ActiveSceneID = PreviousScene;
            return Result;
        }

        bool ObjectSetSize(
            std::uint32_t objectID,
            float width,
            float height,
            float depth
        ) const
        {
            constexpr std::size_t SizeMemberEnd =
                offsetof(EngineHostAPI, SetObjectSize) +
                sizeof(decltype(EngineHostAPI::SetObjectSize)); //寸法API終端

            if (!Detail::HasHostMember(Host, SizeMemberEnd) ||
                Host->SetObjectSize == nullptr || SceneID == 0 || objectID == 0)
            {
                return false;
            }

            const EngineExternalVector3 Size{ width, height, depth }; //外形寸法
            return Host->SetObjectSize(
                Host->Context,
                SceneID,
                objectID,
                &Size
            );
        }

        bool ObjectSetSize(
            const std::string& name,
            float width,
            float height,
            float depth
        ) const
        {
            return ObjectSetSize(FindObjectID(name), width, height, depth);
        }

        AddObjectAPI AddObject; //互換用Object生成入口

    private:
        static std::uint32_t ResolveSceneID(
            const EngineHostAPI* host,
            std::uint32_t requestedSceneID
        )
        {
            if (requestedSceneID != 0 || host == nullptr)
            {
                return requestedSceneID;
            }

            std::uint32_t Result = host->GetMainSceneID == nullptr
                ? 0
                : host->GetMainSceneID(host->Context); //優先Main Scene

            if (Result != 0 || host->GetSceneCount == nullptr ||
                host->GetSceneInfo == nullptr || host->GetSceneCount(host->Context) == 0)
            {
                return Result;
            }

            EngineExternalSceneInfo Information{}; //Fallback先頭Scene
            Information.Size = sizeof(EngineExternalSceneInfo);
            return host->GetSceneInfo(host->Context, 0, &Information)
                ? Information.SceneID
                : 0;
        }

        const EngineHostAPI* Host; //Engine所有関数表
        std::uint32_t SceneID; //操作対象Scene
    };

    class ActiveAddObjectAPI final
    {
    public:
        ObjectHandle Create(
            EngineExternalObjectType type,
            const std::string& name
        ) const
        {
            return AddObjectAPI(Detail::ActiveHost, Detail::ActiveSceneID).Create(
                type,
                name
            );
        }

        ObjectHandle CreateCapsuleModel(const std::string& name) const
        {
            return AddObjectAPI(
                Detail::ActiveHost,
                Detail::ActiveSceneID
            ).CreateCapsuleModel(name);
        }

        ObjectHandle CreateBox(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Box, name);
        }

        ObjectHandle CreateSphere(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Sphere, name);
        }

        ObjectHandle CreatePlane(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Plane, name);
        }

        ObjectHandle CreateCylinder(const std::string& name) const
        {
            return Create(EngineExternalObjectType::Cylinder, name);
        }

        std::vector<ObjectHandle> CreateMany(
            EngineExternalObjectType type,
            const std::string& baseName,
            std::uint32_t count
        ) const
        {
            return AddObjectAPI(
                Detail::ActiveHost,
                Detail::ActiveSceneID
            ).CreateMany(type, baseName, count);
        }

        std::vector<ObjectHandle> CreateBoxes(
            const std::string& baseName,
            std::uint32_t count
        ) const
        {
            return CreateMany(EngineExternalObjectType::Box, baseName, count);
        }

        std::vector<ObjectHandle> CreateCapsules(
            const std::string& baseName,
            std::uint32_t count
        ) const
        {
            std::vector<ObjectHandle> Result;
            Result.reserve(count);

            for (std::uint32_t Index = 0; Index < count; ++Index)
            {
                ObjectHandle Created = CreateCapsuleModel(
                    baseName + std::to_string(Index)
                );

                if (Created.GetID() != 0)
                {
                    Result.emplace_back(Created);
                }
            }

            return Result;
        }
    };

    class ObjectAPI final
    {
    public:
        ObjectHandle Find(const std::string& name) const
        {
            return ObjectHandle(
                Detail::ActiveHost,
                Detail::ActiveSceneID,
                Detail::FindObjectID(name)
            );
        }

        bool Exists(const std::string& name) const
        {
            return Find(name).GetID() != 0;
        }

        std::vector<ObjectHandle> FindAll(
            const std::string& namePart,
            bool caseSensitive = false
        ) const
        {
            std::vector<ObjectHandle> Result;

            if (Detail::ActiveHost == nullptr || namePart.empty() ||
                Detail::ActiveHost->GetObjectCount == nullptr ||
                Detail::ActiveHost->GetObjectInfo == nullptr)
            {
                return Result;
            }

            std::string Needle = namePart;

            if (!caseSensitive)
            {
                for (char& Character : Needle)
                {
                    Character = static_cast<char>(std::tolower(
                        static_cast<unsigned char>(Character)
                    ));
                }
            }

            const std::uint32_t Count = Detail::ActiveHost->GetObjectCount(
                Detail::ActiveHost->Context,
                Detail::ActiveSceneID
            );

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalObjectInfo Information{};
                Information.Size = sizeof(EngineExternalObjectInfo);

                if (!Detail::ActiveHost->GetObjectInfo(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    Index,
                    &Information
                ))
                {
                    continue;
                }

                std::string Candidate = Information.Name;

                if (!caseSensitive)
                {
                    for (char& Character : Candidate)
                    {
                        Character = static_cast<char>(std::tolower(
                            static_cast<unsigned char>(Character)
                        ));
                    }
                }

                if (Candidate.find(Needle) != std::string::npos)
                {
                    Result.emplace_back(
                        Detail::ActiveHost,
                        Detail::ActiveSceneID,
                        Information.ObjectID
                    );
                }
            }

            return Result;
        }

        std::vector<ObjectHandle> FindByType(EngineExternalObjectType type) const
        {
            return FindMatching([type](
                const EngineExternalObjectInfo& information,
                const ObjectHandle&)
                {
                    return information.ObjectType == static_cast<std::uint32_t>(type);
                });
        }

        std::vector<ObjectHandle> FindByComponent(
            EngineExternalComponentType type
        ) const
        {
            return FindMatching([type](
                const EngineExternalObjectInfo&,
                const ObjectHandle& object)
                {
                    return object.HasComponent(type);
                });
        }

        std::vector<ObjectHandle> FindByScript(const std::string& scriptKey) const
        {
            return FindMatching([&scriptKey](
                const EngineExternalObjectInfo&,
                const ObjectHandle& object)
                {
                    for (const ComponentHandle& Component : object.GetComponents(
                        EngineExternalComponentType::Script
                    ))
                    {
                        if (Component.GetName() == scriptKey)
                        {
                            return true;
                        }
                    }

                    return false;
                });
        }

        bool SetSize(
            const std::string& name,
            float width,
            float height,
            float depth
        ) const
        {
            return EngineProgramAPI(
                Detail::ActiveHost,
                Detail::ActiveSceneID
            ).ObjectSetSize(name, width, height, depth);
        }

        bool SetTransform(
            const std::string& name,
            float x,
            float y,
            float z,
            float rotationX = 0.0f,
            float rotationY = 0.0f,
            float rotationZ = 0.0f,
            float scaleX = 1.0f,
            float scaleY = 1.0f,
            float scaleZ = 1.0f
        ) const
        {
            const std::uint32_t ObjectID = Find(name).GetID(); //Transform対象ID
            const EngineExternalTransform Transform
            {
                { x, y, z },
                { rotationX, rotationY, rotationZ },
                { scaleX, scaleY, scaleZ }
            }; //一括設定値
            return Detail::ActiveHost != nullptr && ObjectID != 0 &&
                Detail::ActiveHost->SetObjectTransform != nullptr &&
                Detail::ActiveHost->SetObjectTransform(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    ObjectID,
                    &Transform
                );
        }

        bool SetPosition(
            const std::string& name,
            float x,
            float y,
            float z
        ) const
        {
            const std::uint32_t ObjectID = Find(name).GetID(); //位置変更対象ID
            EngineExternalObjectInfo Information{}; //回転とScale保持用情報

            if (!Detail::FindObjectInformation(ObjectID, Information))
            {
                return false;
            }

            Information.LocalTransform.Position = { x, y, z };
            return Detail::ActiveHost->SetObjectTransform(
                Detail::ActiveHost->Context,
                Detail::ActiveSceneID,
                ObjectID,
                &Information.LocalTransform
            );
        }

        bool Move(const std::string& name, float x, float y, float z) const
        {
            const std::uint32_t ObjectID = Find(name).GetID(); //移動対象ID
            EngineExternalObjectInfo Information{}; //現在Transform

            if (!Detail::FindObjectInformation(ObjectID, Information))
            {
                return false;
            }

            Information.LocalTransform.Position.X += x;
            Information.LocalTransform.Position.Y += y;
            Information.LocalTransform.Position.Z += z;
            return Detail::ActiveHost->SetObjectTransform(
                Detail::ActiveHost->Context,
                Detail::ActiveSceneID,
                ObjectID,
                &Information.LocalTransform
            );
        }

        bool SetColor(
            const std::string& name,
            float red,
            float green,
            float blue,
            float alpha = 1.0f
        ) const
        {
            const std::uint32_t ObjectID = Find(name).GetID(); //色設定対象ID
            const EngineExternalColor Color{ red, green, blue, alpha }; //設定色
            return Detail::ActiveHost != nullptr && ObjectID != 0 &&
                Detail::ActiveHost->SetObjectColor != nullptr &&
                Detail::ActiveHost->SetObjectColor(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    ObjectID,
                    &Color
                );
        }

        bool MultiplyColor(
            const std::string& name,
            float red,
            float green,
            float blue,
            float alpha = 1.0f
        ) const
        {
            const std::uint32_t ObjectID = Find(name).GetID(); //色乗算対象ID
            const EngineExternalColor Color{ red, green, blue, alpha }; //乗算係数
            return Detail::ActiveHost != nullptr && ObjectID != 0 &&
                Detail::ActiveHost->MultiplyObjectColor != nullptr &&
                Detail::ActiveHost->MultiplyObjectColor(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    ObjectID,
                    &Color
                );
        }

        bool Remove(const std::string& name) const
        {
            const std::uint32_t ObjectID = Find(name).GetID(); //削除対象ID
            return Detail::ActiveHost != nullptr && ObjectID != 0 &&
                Detail::ActiveHost->RemoveObject != nullptr &&
                Detail::ActiveHost->RemoveObject(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    ObjectID
                );
        }

        bool AttachScript(
            const std::string& name,
            const std::string& scriptKey
        ) const
        {
            const std::uint32_t ObjectID = Find(name).GetID(); //接続対象ID
            return Detail::ActiveHost != nullptr && ObjectID != 0 &&
                !scriptKey.empty() && Detail::ActiveHost->AttachScript != nullptr &&
                Detail::ActiveHost->AttachScript(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    ObjectID,
                    scriptKey.c_str()
                );
        }

    private:
        template<typename Predicate>
        std::vector<ObjectHandle> FindMatching(Predicate predicate) const
        {
            std::vector<ObjectHandle> Result;

            if (Detail::ActiveHost == nullptr ||
                Detail::ActiveHost->GetObjectCount == nullptr ||
                Detail::ActiveHost->GetObjectInfo == nullptr)
            {
                return Result;
            }

            const std::uint32_t Count = Detail::ActiveHost->GetObjectCount(
                Detail::ActiveHost->Context,
                Detail::ActiveSceneID
            );

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalObjectInfo Information{};
                Information.Size = sizeof(EngineExternalObjectInfo);

                if (!Detail::ActiveHost->GetObjectInfo(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    Index,
                    &Information
                ))
                {
                    continue;
                }

                const ObjectHandle Handle(
                    Detail::ActiveHost,
                    Detail::ActiveSceneID,
                    Information.ObjectID
                );

                if (predicate(Information, Handle))
                {
                    Result.emplace_back(Handle);
                }
            }

            return Result;
        }
    };

    class SceneAPI final
    {
    public:
        std::uint32_t GetID() const
        {
            return Detail::ActiveSceneID;
        }

        bool SetActive(bool active) const
        {
            return Detail::ActiveHost != nullptr && Detail::ActiveSceneID != 0 &&
                Detail::ActiveHost->SetSceneActive != nullptr &&
                Detail::ActiveHost->SetSceneActive(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID,
                    active
                );
        }

        bool SetView() const
        {
            return Detail::ActiveHost != nullptr && Detail::ActiveSceneID != 0 &&
                Detail::ActiveHost->SetViewScene != nullptr &&
                Detail::ActiveHost->SetViewScene(
                    Detail::ActiveHost->Context,
                    Detail::ActiveSceneID
                );
        }
    };

    class InputAPI final
    {
    public:
        bool IsKeyDown(std::uint32_t virtualKey) const
        {
            return Detail::ActiveHost != nullptr &&
                Detail::ActiveHost->IsKeyDown != nullptr &&
                Detail::ActiveHost->IsKeyDown(
                    Detail::ActiveHost->Context,
                    virtualKey
                );
        }
    };

    class AdvancedAPI final
    {
    public:
        const EngineHostAPI* Host() const
        {
            return Detail::ActiveHost;
        }

        EngineProgramAPI Program() const
        {
            return EngineProgramAPI(Detail::ActiveHost, Detail::ActiveSceneID);
        }
    };

    inline const ActiveAddObjectAPI AddObject; //名前指定Object生成API
    inline const ObjectAPI Object; //名前指定Object設定API
    inline const SceneAPI Scene; //現在Scene操作API
    inline const InputAPI Input; //入力取得API
    inline const AdvancedAPI Advanced; //低Level Engine API入口

    inline void Log(const std::string& message)
    {
        if (Detail::ActiveHost != nullptr && Detail::ActiveHost->AddLog != nullptr)
        {
            Detail::ActiveHost->AddLog(
                Detail::ActiveHost->Context,
                message.c_str()
            );
        }
    }

    struct SceneProgramDefinition final
    {
        const char* SceneName;
        void (*Init)();
        void (*Update)(float deltaTime);
        void (*End)();
        void (*StartDestroy)() = nullptr; //全Scene終了前の参照解除準備
        void (*EndDestroy)() = nullptr; //全Scene End後の最終解放通知
    };

    namespace Detail
    {
        inline std::vector<const SceneProgramDefinition*>& SceneDefinitions()
        {
            static std::vector<const SceneProgramDefinition*> Definitions; //登録Scene定義
            return Definitions;
        }
    }

    class SceneProgramRegistration final
    {
    public:
        explicit SceneProgramRegistration(const SceneProgramDefinition* definition)
        {
            if (definition != nullptr && definition->SceneName != nullptr &&
                definition->SceneName[0] != '\0')
            {
                Detail::SceneDefinitions().push_back(definition);
            }
        }
    };

    namespace Detail
    {
        class ScopedSceneContext final
        {
        public:
            ScopedSceneContext(const EngineHostAPI* host, std::uint32_t sceneID)
                : PreviousHost(ActiveHost)
                , PreviousSceneID(ActiveSceneID)
            {
                ActiveHost = host;
                ActiveSceneID = sceneID;
            }

            ~ScopedSceneContext()
            {
                ActiveHost = PreviousHost;
                ActiveSceneID = PreviousSceneID;
            }

        private:
            const EngineHostAPI* PreviousHost; //入れ子前Host
            std::uint32_t PreviousSceneID; //入れ子前Scene
        };

        struct SceneProgramEntry final
        {
            const SceneProgramDefinition* Definition = nullptr;
            std::uint32_t SceneID = 0;
            bool Initialized = false;
        };

        struct SceneProgramState final
        {
            const EngineHostAPI* Host = nullptr;
            std::vector<SceneProgramEntry> Entries;
        };

        inline bool FindSceneInformation(
            const EngineHostAPI* host,
            const char* sceneName,
            std::uint32_t sceneID,
            EngineExternalSceneInfo& information
        )
        {
            if (host == nullptr || host->GetSceneCount == nullptr ||
                host->GetSceneInfo == nullptr)
            {
                return false;
            }

            const std::uint32_t Count = host->GetSceneCount(host->Context); //Scene数

            for (std::uint32_t Index = 0; Index < Count; ++Index)
            {
                EngineExternalSceneInfo Candidate{}; //照合中Scene
                Candidate.Size = sizeof(EngineExternalSceneInfo);

                if (!host->GetSceneInfo(host->Context, Index, &Candidate))
                {
                    continue;
                }

                if ((sceneID != 0 && Candidate.SceneID == sceneID) ||
                    (sceneID == 0 && sceneName != nullptr &&
                        std::string(Candidate.Name) == sceneName))
                {
                    information = Candidate;
                    return true;
                }
            }

            return false;
        }

        inline void SynchronizeScenes(SceneProgramState& state)
        {
            for (SceneProgramEntry& Entry : state.Entries)
            {
                EngineExternalSceneInfo Information{}; //対応Scene情報

                if (FindSceneInformation(
                    state.Host,
                    Entry.Definition->SceneName,
                    Entry.SceneID,
                    Information
                ))
                {
                    Entry.SceneID = Information.SceneID;

                    if (!Entry.Initialized)
                    {
                        const ScopedSceneContext Context(state.Host, Entry.SceneID); //Init Context
                        Entry.Definition->Init();
                        Entry.Initialized = true;
                    }
                }
                else if (Entry.Initialized)
                {
                    const ScopedSceneContext Context(state.Host, Entry.SceneID); //End Context
                    if (Entry.Definition->StartDestroy != nullptr)
                    {
                        Entry.Definition->StartDestroy();
                    }
                    Entry.Definition->End();
                    if (Entry.Definition->EndDestroy != nullptr)
                    {
                        Entry.Definition->EndDestroy();
                    }
                    Entry.SceneID = 0;
                    Entry.Initialized = false;
                }
            }
        }

        inline void* CreateSceneProgram(const EngineHostAPI* host)
        {
            if (host == nullptr || host->AbiVersion != EngineExtensionAbiVersion ||
                host->GetSceneCount == nullptr || host->GetSceneInfo == nullptr ||
                SceneDefinitions().empty())
            {
                return nullptr;
            }

            SceneProgramState* State = new (std::nothrow) SceneProgramState{}; //DLL状態

            if (State == nullptr)
            {
                return nullptr;
            }

            try
            {
                State->Host = host;

                for (const SceneProgramDefinition* Definition : SceneDefinitions())
                {
                    State->Entries.push_back({ Definition, 0, false });
                }

                SynchronizeScenes(*State);
                return State;
            }
            catch (...)
            {
                delete State;
                return nullptr;
            }
        }

        inline void DestroySceneProgram(void* instance)
        {
            SceneProgramState* State = static_cast<SceneProgramState*>(instance); //破棄対象

            if (State == nullptr)
            {
                return;
            }

            for (SceneProgramEntry& Entry : State->Entries)
            {
                if (Entry.Initialized && Entry.Definition->StartDestroy != nullptr)
                {
                    const ScopedSceneContext Context(State->Host, Entry.SceneID); //破棄準備Context
                    Entry.Definition->StartDestroy();
                }
            }

            for (auto Iterator = State->Entries.rbegin();
                Iterator != State->Entries.rend(); ++Iterator)
            {
                if (Iterator->Initialized)
                {
                    const ScopedSceneContext Context(State->Host, Iterator->SceneID); //End Context
                    Iterator->Definition->End();
                }
            }

            for (auto Iterator = State->Entries.rbegin();
                Iterator != State->Entries.rend(); ++Iterator)
            {
                if (Iterator->Initialized && Iterator->Definition->EndDestroy != nullptr)
                {
                    const ScopedSceneContext Context(State->Host, Iterator->SceneID); //最終破棄Context
                    Iterator->Definition->EndDestroy();
                }
            }

            delete State;
        }

        inline void UpdateSceneProgram(void* instance, float deltaTime)
        {
            SceneProgramState* State = static_cast<SceneProgramState*>(instance); //更新対象

            if (State == nullptr)
            {
                return;
            }

            SynchronizeScenes(*State);

            for (SceneProgramEntry& Entry : State->Entries)
            {
                EngineExternalSceneInfo Information{}; //Active確認用情報

                if (Entry.Initialized && FindSceneInformation(
                    State->Host,
                    nullptr,
                    Entry.SceneID,
                    Information
                ) && Information.Active)
                {
                    const ScopedSceneContext Context(State->Host, Entry.SceneID); //Update Context
                    Entry.Definition->Update(deltaTime);
                }
            }
        }
    }
}

#define ENGINE_REGISTER_NAMED_SCENE(SceneIdentifier, EngineSceneName) \
    namespace \
    { \
        const EngineGame::SceneProgramDefinition SceneIdentifier##ProgramDefinition \
        { \
            EngineSceneName, \
            Game::SceneIdentifier::Init, \
            Game::SceneIdentifier::Update, \
            Game::SceneIdentifier::End \
        }; \
        const EngineGame::SceneProgramRegistration SceneIdentifier##ProgramRegistration \
        { \
            &SceneIdentifier##ProgramDefinition \
        }; \
    }

#define ENGINE_REGISTER_SCENE(SceneIdentifier) \
    ENGINE_REGISTER_NAMED_SCENE(SceneIdentifier, #SceneIdentifier)

#define ENGINE_REGISTER_NAMED_SCENE_LIFECYCLE(SceneIdentifier, EngineSceneName) \
    namespace \
    { \
        const EngineGame::SceneProgramDefinition SceneIdentifier##ProgramDefinition \
        { \
            EngineSceneName, \
            Game::SceneIdentifier::Init, \
            Game::SceneIdentifier::Update, \
            Game::SceneIdentifier::End, \
            Game::SceneIdentifier::StartDestroy, \
            Game::SceneIdentifier::EndDestroy \
        }; \
        const EngineGame::SceneProgramRegistration SceneIdentifier##ProgramRegistration \
        { \
            &SceneIdentifier##ProgramDefinition \
        }; \
    }

#define ENGINE_REGISTER_SCENE_LIFECYCLE(SceneIdentifier) \
    ENGINE_REGISTER_NAMED_SCENE_LIFECYCLE(SceneIdentifier, #SceneIdentifier)
