//|| ExtensionSystem.cpp ||:::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  外部Program DLLへEngine読取編集APIを公開し安全な世代交代を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.10  名前指定Capsule生成、寸法設定、DLL診断を追加
//||  2026_08_18  v1.00  新規作成
//||

#include "ExtensionSystem.h"

#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

#include "Component.h"
#include "Capsule.h"
#include "EngineAPI.h"
#include "GameObjectTemplate.h"
#include "MessageLog.h"
#include "Object.h"
#include "ObjectManager.h"
#include "Scene.h"
#include "SceneManager.h"

namespace Engine
{
    namespace
    {
        //概要：Windows LoaderのError Codeを診断可能な一行文字列へ変換する
        //引数：errorCode=GetLastErrorが返した値
        //戻り値：数値CodeとSystem Messageを含む文字列
        std::string FormatWindowsLoaderError(DWORD errorCode)
        {
            LPWSTR Buffer = nullptr; //FormatMessageが確保するUnicode System Message
            const DWORD Length = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                errorCode,
                0,
                reinterpret_cast<LPWSTR>(&Buffer),
                0,
                nullptr
            ); //System MessageのByte数
            std::string Message = "Windows error " + std::to_string(errorCode); //必ず残す数値情報

            if (Length != 0 && Buffer != nullptr)
            {
                std::wstring Detail(Buffer, Length); //改行を除去するSystem Message

                while (!Detail.empty() &&
                    (Detail.back() == L'\r' || Detail.back() == L'\n' || Detail.back() == L' '))
                {
                    Detail.pop_back();
                }

                const int ByteCount = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    Detail.data(),
                    static_cast<int>(Detail.size()),
                    nullptr,
                    0,
                    nullptr,
                    nullptr
                ); //UTF-8変換後Byte数

                if (ByteCount > 0)
                {
                    std::string Utf8Detail(ByteCount, '\0'); //MessageLogへ渡すUTF-8詳細
                    WideCharToMultiByte(
                        CP_UTF8,
                        0,
                        Detail.data(),
                        static_cast<int>(Detail.size()),
                        Utf8Detail.data(),
                        ByteCount,
                        nullptr,
                        nullptr
                    );
                    Message += ": " + Utf8Detail;
                }
            }

            if (Buffer != nullptr)
            {
                LocalFree(Buffer);
            }

            return Message;
        }

        //概要：外部API ContextをNative Engine Facadeへ復元する
        //引数：context=EngineHostAPIへ設定したContext
        //戻り値：EngineAPIへの参照
        EngineAPI& GetEngine(void* context)
        {
            return *static_cast<EngineAPI*>(context);
        }

        //概要：UTF-8名を外部ABIの固定長Bufferへ終端付きで複写する
        //引数：destination=出力Buffer、capacity=Buffer容量、source=複写元文字列
        //戻り値：なし
        void CopyExternalName(
            char* destination,
            std::size_t capacity,
            const std::string& source
        )
        {
            if (destination == nullptr || capacity == 0)
            {
                return;
            }

            strncpy_s(destination, capacity, source.c_str(), _TRUNCATE);
        }

        //概要：外部Programから受け取った文字列をEngineログへ追加する
        //引数：context=Engine Context、message=UTF-8ログ文字列
        //戻り値：なし
        void ENGINE_EXTENSION_CALL AddExternalLog(void* context, const char* message)
        {
            (void)context;

            if (message != nullptr)
            {
                MessageLog::GetInstance().AddLog(message);
            }
        }

        //概要：現在Extensionが実行済みの通算Frame番号を取得する
        //引数：context=Engine Context
        //戻り値：通算Frame番号
        std::uint64_t ENGINE_EXTENSION_CALL GetExternalFrameNumber(void* context)
        {
            return GetEngine(context).GetExtensionModuleManager().GetFrameNumber();
        }

        //概要：現在Extensionへ渡されている固定更新秒数を取得する
        //引数：context=Engine Context
        //戻り値：秒単位Delta Time
        float ENGINE_EXTENSION_CALL GetExternalDeltaTime(void* context)
        {
            return GetEngine(context).GetExtensionModuleManager().GetDeltaTime();
        }

        //概要：外部APIから列挙可能なScene数を取得する
        //引数：context=Engine Context
        //戻り値：有効Scene数
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalSceneCount(void* context)
        {
            return static_cast<std::uint32_t>(GetEngine(context).GetSceneIDs().size());
        }

        //概要：登録順IndexからScene情報を外部ABI構造体へ読み取る
        //引数：context=Engine Context、index=Scene Index、information=結果格納先
        //戻り値：情報を取得できた場合はtrue
        bool ENGINE_EXTENSION_CALL GetExternalSceneInfo(
            void* context,
            std::uint32_t index,
            EngineExternalSceneInfo* information
        )
        {
            if (information == nullptr)
            {
                return false;
            }

            EngineAPI& API = GetEngine(context); //読取元Engine API
            const std::vector<SceneID> IDs = API.GetSceneIDs(); //登録順Scene ID

            if (index >= IDs.size())
            {
                return false;
            }

            EditorSceneInfo NativeInformation; //Native側Scene情報

            if (!API.TryGetSceneInfo(IDs[index], NativeInformation))
            {
                return false;
            }

            *information = EngineExternalSceneInfo{};
            information->Size = sizeof(EngineExternalSceneInfo);
            information->SceneID = IDs[index].GetValue();
            information->Active = NativeInformation.Active;
            information->ViewScene = NativeInformation.ViewScene;
            information->MainScene = API.GetSceneManager().GetMainSceneID() == IDs[index];
            CopyExternalName(information->Name, std::size(information->Name), NativeInformation.Name);
            return true;
        }

        //概要：外部ProgramからSceneの有効状態を変更する
        //引数：context=Engine Context、sceneID=対象Scene、active=有効にする場合true
        //戻り値：変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalSceneActive(
            void* context,
            std::uint32_t sceneID,
            bool active
        )
        {
            return GetEngine(context).SetSceneActive(SceneID(sceneID), active);
        }

        //概要：外部ProgramからView Sceneを変更する
        //引数：context=Engine Context、sceneID=新しいView Scene
        //戻り値：変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalViewScene(void* context, std::uint32_t sceneID)
        {
            return GetEngine(context).SetViewScene(SceneID(sceneID));
        }

        //概要：指定Sceneの外部API列挙可能Object数を取得する
        //引数：context=Engine Context、sceneID=対象Scene
        //戻り値：有効Object数
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalObjectCount(
            void* context,
            std::uint32_t sceneID
        )
        {
            return static_cast<std::uint32_t>(
                GetEngine(context).GetObjectIDs(SceneID(sceneID)).size()
            );
        }

        //概要：登録順IndexからObject情報とLocal Transformを読み取る
        //引数：context=Engine Context、sceneID=対象Scene、index=Object Index、information=結果格納先
        //戻り値：情報を取得できた場合はtrue
        bool ENGINE_EXTENSION_CALL GetExternalObjectInfo(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t index,
            EngineExternalObjectInfo* information
        )
        {
            if (information == nullptr)
            {
                return false;
            }

            EngineAPI& API = GetEngine(context); //読取元Engine API
            const SceneID NativeSceneID(sceneID); //強いScene ID
            const std::vector<ObjectID> IDs = API.GetObjectIDs(NativeSceneID); //登録順Object ID

            if (index >= IDs.size())
            {
                return false;
            }

            EditorObjectInfo NativeInformation; //Native側Object情報

            if (!API.TryGetObjectInfo(NativeSceneID, IDs[index], NativeInformation))
            {
                return false;
            }

            *information = EngineExternalObjectInfo{};
            information->Size = sizeof(EngineExternalObjectInfo);
            information->SceneID = sceneID;
            information->ObjectID = IDs[index].GetValue();
            information->ParentObjectID = NativeInformation.ParentID.GetValue();
            information->ObjectType = static_cast<std::uint32_t>(NativeInformation.Type);
            information->ComponentCount = static_cast<std::uint32_t>(NativeInformation.Components.size());
            information->Active = NativeInformation.Active;
            CopyExternalName(information->Name, std::size(information->Name), NativeInformation.Name);
            information->LocalTransform.Position = {
                NativeInformation.LocalTransform.Position.X,
                NativeInformation.LocalTransform.Position.Y,
                NativeInformation.LocalTransform.Position.Z
            };
            information->LocalTransform.Rotation = {
                NativeInformation.LocalTransform.Rotation.X,
                NativeInformation.LocalTransform.Rotation.Y,
                NativeInformation.LocalTransform.Rotation.Z
            };
            information->LocalTransform.Scale = {
                NativeInformation.LocalTransform.Scale.X,
                NativeInformation.LocalTransform.Scale.Y,
                NativeInformation.LocalTransform.Scale.Z
            };
            return true;
        }

        //概要：外部Programから具象Objectを作成して新しいIDを取得する
        //引数：context=Engine Context、sceneID=作成先Scene、objectType=Object種別、name=希望名、parentObjectID=親又は無効ID
        //戻り値：作成したObject ID、失敗時は0
        std::uint32_t ENGINE_EXTENSION_CALL CreateExternalObject(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectType,
            const char* name,
            std::uint32_t parentObjectID
        )
        {
            if (objectType >= static_cast<std::uint32_t>(ObjectType::Count))
            {
                return 0;
            }

            Object* Created = GetEngine(context).CreateObject(
                SceneID(sceneID),
                static_cast<ObjectType>(objectType),
                name == nullptr ? std::string() : std::string(name),
                ObjectID(parentObjectID)
            ); //Native側で作成したObject
            return Created == nullptr ? 0 : Created->GetID().GetValue();
        }

        //概要：外部Programから指定Objectを削除する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=削除対象Object
        //戻り値：削除できた場合はtrue
        bool ENGINE_EXTENSION_CALL RemoveExternalObject(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID
        )
        {
            return GetEngine(context).RemoveObject(SceneID(sceneID), ObjectID(objectID));
        }

        //概要：外部Programから指定Objectの名前を変更する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、name=希望名
        //戻り値：名前を変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL RenameExternalObject(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const char* name
        )
        {
            return name != nullptr && GetEngine(context).RenameObject(
                SceneID(sceneID),
                ObjectID(objectID),
                name
            );
        }

        //概要：外部Programから指定Objectの有効状態を変更する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、active=有効にする場合true
        //戻り値：状態を変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalObjectActive(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            bool active
        )
        {
            return GetEngine(context).SetObjectActive(
                SceneID(sceneID),
                ObjectID(objectID),
                active
            );
        }

        //概要：外部Programから指定ObjectのLocal Transformを一括変更する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、transform=新しい姿勢
        //戻り値：Transformを変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalObjectTransform(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const EngineExternalTransform* transform
        )
        {
            if (transform == nullptr)
            {
                return false;
            }

            EditorTransformInfo NativeTransform; //Native APIへ渡すTransform
            NativeTransform.Position = {
                transform->Position.X,
                transform->Position.Y,
                transform->Position.Z
            };
            NativeTransform.Rotation = {
                transform->Rotation.X,
                transform->Rotation.Y,
                transform->Rotation.Z
            };
            NativeTransform.Scale = {
                transform->Scale.X,
                transform->Scale.Y,
                transform->Scale.Z
            };
            return GetEngine(context).SetObjectTransform(
                SceneID(sceneID),
                ObjectID(objectID),
                NativeTransform
            );
        }

        //概要：外部Programから循環検証付きでObjectの親を変更する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=子Object、parentObjectID=親又は無効ID、keepWorldTransform=World姿勢維持指定
        //戻り値：親を変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalObjectParent(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            std::uint32_t parentObjectID,
            bool keepWorldTransform
        )
        {
            return GetEngine(context).SetObjectParent(
                SceneID(sceneID),
                ObjectID(objectID),
                ObjectID(parentObjectID),
                keepWorldTransform
            );
        }

        //概要：指定Objectが所有する外部API列挙可能Component数を取得する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object
        //戻り値：有効Component数
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalComponentCount(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID
        )
        {
            return static_cast<std::uint32_t>(GetEngine(context).GetComponentIDs(
                SceneID(sceneID),
                ObjectID(objectID)
            ).size());
        }

        //概要：Object内登録順IndexからComponent情報を読み取る
        //引数：context=Engine Context、sceneID=対象Scene、objectID=所有Object、index=Component Index、information=結果格納先
        //戻り値：情報を取得できた場合はtrue
        bool ENGINE_EXTENSION_CALL GetExternalComponentInfo(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            std::uint32_t index,
            EngineExternalComponentInfo* information
        )
        {
            if (information == nullptr)
            {
                return false;
            }

            EngineAPI& API = GetEngine(context); //読取元Engine API
            const SceneID NativeSceneID(sceneID); //強いScene ID
            const std::vector<ComponentID> IDs = API.GetComponentIDs(
                NativeSceneID,
                ObjectID(objectID)
            ); //Object内登録順Component ID

            if (index >= IDs.size())
            {
                return false;
            }

            EditorComponentInfo NativeInformation; //Native側Component情報

            if (!API.TryGetComponentInfo(NativeSceneID, IDs[index], NativeInformation))
            {
                return false;
            }

            const Scene* TargetScene = API.GetSceneManager().FindScene(NativeSceneID); //所有Scene
            const Component* TargetComponent = TargetScene == nullptr
                ? nullptr
                : TargetScene->GetObjectManager().FindComponent(IDs[index]); //型を読むComponent

            if (TargetComponent == nullptr)
            {
                return false;
            }

            *information = EngineExternalComponentInfo{};
            information->Size = sizeof(EngineExternalComponentInfo);
            information->SceneID = sceneID;
            information->ObjectID = objectID;
            information->ComponentID = IDs[index].GetValue();
            information->ComponentType = static_cast<std::uint32_t>(TargetComponent->GetType());
            information->Active = NativeInformation.Active;
            CopyExternalName(information->Name, std::size(information->Name), NativeInformation.Name);
            return true;
        }

        //概要：外部Programから指定Componentを削除する
        //引数：context=Engine Context、sceneID=対象Scene、componentID=削除対象Component
        //戻り値：削除できた場合はtrue
        bool ENGINE_EXTENSION_CALL RemoveExternalComponent(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t componentID
        )
        {
            return GetEngine(context).RemoveComponent(
                SceneID(sceneID),
                ComponentID(componentID)
            );
        }

        //概要：外部Programから指定Componentの名前を変更する
        //引数：context=Engine Context、sceneID=対象Scene、componentID=対象Component、name=希望名
        //戻り値：名前を変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL RenameExternalComponent(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t componentID,
            const char* name
        )
        {
            return name != nullptr && GetEngine(context).RenameComponent(
                SceneID(sceneID),
                ComponentID(componentID),
                name
            );
        }

        //概要：外部Programから指定Componentの有効状態を変更する
        //引数：context=Engine Context、sceneID=対象Scene、componentID=対象Component、active=有効にする場合true
        //戻り値：状態を変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalComponentActive(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t componentID,
            bool active
        )
        {
            return GetEngine(context).SetComponentActive(
                SceneID(sceneID),
                ComponentID(componentID),
                active
            );
        }

        //概要：外部Toolが差分更新を判断するためEngine構造Revisionを取得する
        //引数：context=Engine Context
        //戻り値：構造変更ごとに増えるRevision
        std::uint64_t ENGINE_EXTENSION_CALL GetExternalEngineRevision(void* context)
        {
            return GetEngine(context).GetRevision();
        }

        //概要：外部Programへ現在のMain Scene IDを返す
        //引数：context=Engine Context
        //戻り値：Main Scene ID、未設定時は0
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalMainSceneID(void* context)
        {
            return GetEngine(context).GetMainSceneID().GetValue();
        }

        //概要：外部Programへ現在のView Scene IDを返す
        //引数：context=Engine Context
        //戻り値：View Scene ID、未設定時は0
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalViewSceneID(void* context)
        {
            return GetEngine(context).GetViewSceneID().GetValue();
        }

        //概要：外部Programから解決済みScene名でScene IDを検索する
        //引数：context=Engine Context、name=検索するUTF-8 Scene名
        //戻り値：一致したScene ID、未登録時は0
        std::uint32_t ENGINE_EXTENSION_CALL FindExternalSceneByName(
            void* context,
            const char* name
        )
        {
            return name == nullptr
                ? 0u
                : GetEngine(context).FindSceneID(name).GetValue();
        }

        //概要：外部Programから標準Sceneを作成する
        //引数：context=Engine Context、name=希望名、width=Camera横幅、height=Camera縦幅
        //戻り値：作成したScene ID、失敗時は0
        std::uint32_t ENGINE_EXTENSION_CALL CreateExternalScene(
            void* context,
            const char* name,
            std::uint32_t width,
            std::uint32_t height
        )
        {
            return GetEngine(context).CreateScene(
                name == nullptr ? std::string() : std::string(name),
                width,
                height
            ).GetValue();
        }

        //概要：外部Programから既存Sceneを独立Sceneへ複製する
        //引数：context=Engine Context、sourceSceneID=複製元Scene、name=複製先希望名
        //戻り値：複製したScene ID、失敗時は0
        std::uint32_t ENGINE_EXTENSION_CALL DuplicateExternalScene(
            void* context,
            std::uint32_t sourceSceneID,
            const char* name
        )
        {
            return GetEngine(context).DuplicateScene(
                SceneID(sourceSceneID),
                name == nullptr ? std::string() : std::string(name)
            ).GetValue();
        }

        //概要：外部ProgramからMain又はViewではないSceneを削除する
        //引数：context=Engine Context、sceneID=削除対象Scene
        //戻り値：削除できた場合はtrue
        bool ENGINE_EXTENSION_CALL RemoveExternalScene(
            void* context,
            std::uint32_t sceneID
        )
        {
            return GetEngine(context).RemoveScene(SceneID(sceneID));
        }

        //概要：外部Programから指定SceneをMain、View、Activeへ設定する
        //引数：context=Engine Context、sceneID=新しいMain Scene
        //戻り値：設定できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalMainScene(
            void* context,
            std::uint32_t sceneID
        )
        {
            return GetEngine(context).SetMainScene(SceneID(sceneID));
        }

        //概要：外部ProgramからObject型と解決済み名でObject IDを検索する
        //引数：context=Engine Context、sceneID=対象Scene、objectType=Object種別、name=検索名
        //戻り値：一致したObject ID、未登録又は不正型時は0
        std::uint32_t ENGINE_EXTENSION_CALL FindExternalObjectByName(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectType,
            const char* name
        )
        {
            if (name == nullptr || objectType >= static_cast<std::uint32_t>(ObjectType::Count))
            {
                return 0;
            }

            return GetEngine(context).FindObjectID(
                SceneID(sceneID),
                static_cast<ObjectType>(objectType),
                name
            ).GetValue();
        }

        //概要：外部ProgramからScene内で一意な解決済み名だけでObject IDを検索する
        //引数：context=Engine Context、sceneID=対象Scene、name=検索名
        //戻り値：一致したObject ID、未登録時は0
        std::uint32_t ENGINE_EXTENSION_CALL FindExternalObjectByNameOnly(
            void* context,
            std::uint32_t sceneID,
            const char* name
        )
        {
            return name == nullptr
                ? 0
                : GetEngine(context).FindObjectID(SceneID(sceneID), name).GetValue();
        }

        //概要：外部Programから名前付きCapsule Modelを生成する
        //引数：context=Engine Context、sceneID=作成先Scene、name=希望名、parentObjectID=親又は無効ID
        //戻り値：作成したCapsule ID、失敗時は0
        std::uint32_t ENGINE_EXTENSION_CALL CreateExternalCapsuleModel(
            void* context,
            std::uint32_t sceneID,
            const char* name,
            std::uint32_t parentObjectID
        )
        {
            Capsule* Created = GetEngine(context).CreateCapsuleModel(
                SceneID(sceneID),
                name == nullptr ? std::string() : std::string(name),
                ObjectID(parentObjectID)
            ); //Native APIで作成したCapsule
            return Created == nullptr ? 0 : Created->GetID().GetValue();
        }

        //概要：外部ProgramからID指定Objectの3軸外形寸法を設定する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、size=X幅・Y高さ・Z奥行き
        //戻り値：対応Primitiveへ寸法を設定できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalObjectSize(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const EngineExternalVector3* size
        )
        {
            return size != nullptr && GetEngine(context).SetObjectSize(
                SceneID(sceneID),
                ObjectID(objectID),
                EditorVector3{ size->X, size->Y, size->Z }
            );
        }

        //概要：外部Programから名前指定Objectの3軸外形寸法を設定する
        //引数：context=Engine Context、sceneID=対象Scene、name=対象名、size=X幅・Y高さ・Z奥行き
        //戻り値：対応Primitiveへ寸法を設定できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalObjectSizeByName(
            void* context,
            std::uint32_t sceneID,
            const char* name,
            const EngineExternalVector3* size
        )
        {
            return name != nullptr && size != nullptr && GetEngine(context).SetObjectSize(
                SceneID(sceneID),
                name,
                EditorVector3{ size->X, size->Y, size->Z }
            );
        }

        //概要：外部Programへ指定Objectの直接Child数を返す
        //引数：context=Engine Context、sceneID=対象Scene、objectID=親Object
        //戻り値：直接Child数
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalChildCount(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID
        )
        {
            return static_cast<std::uint32_t>(GetEngine(context).GetChildObjectIDs(
                SceneID(sceneID),
                ObjectID(objectID)
            ).size());
        }

        //概要：外部Programへ登録順Indexの直接Child Object IDを返す
        //引数：context=Engine Context、sceneID=対象Scene、objectID=親Object、index=Child Index
        //戻り値：Child Object ID、範囲外時は0
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalChildObjectID(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            std::uint32_t index
        )
        {
            const std::vector<ObjectID> IDs = GetEngine(context).GetChildObjectIDs(
                SceneID(sceneID),
                ObjectID(objectID)
            ); //直接Child ID一覧
            return index < IDs.size() ? IDs[index].GetValue() : 0u;
        }

        //概要：外部ProgramからObjectとComponentを同じSceneへ複製する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=複製元Object、name=複製先希望名
        //戻り値：複製Object ID、失敗時は0
        std::uint32_t ENGINE_EXTENSION_CALL DuplicateExternalObject(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const char* name
        )
        {
            Object* Duplicate = GetEngine(context).DuplicateObject(
                SceneID(sceneID),
                ObjectID(objectID),
                name == nullptr ? std::string() : std::string(name)
            ); //Native APIが作成した複製Object
            return Duplicate == nullptr ? 0u : Duplicate->GetID().GetValue();
        }

        //概要：外部Programからゲーム設定を持つObject雛形を作成する
        //引数：context=Engine Context、sceneID=作成先Scene、name=希望名、parentObjectID=親又は0
        //戻り値：作成したObject ID、失敗時は0
        std::uint32_t ENGINE_EXTENSION_CALL CreateExternalGameObjectTemplate(
            void* context,
            std::uint32_t sceneID,
            const char* name,
            std::uint32_t parentObjectID
        )
        {
            GameObjectTemplate* Created = GetEngine(context).CreateGameObjectTemplate(
                SceneID(sceneID),
                name == nullptr ? std::string() : std::string(name),
                ObjectID(parentObjectID)
            ); //Native APIが作成した雛形Object
            return Created == nullptr ? 0u : Created->GetID().GetValue();
        }

        //概要：外部ProgramへGameObjectTemplate固有設定を読み取る
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、information=結果格納先
        //戻り値：対象がGameObjectTemplateで情報を取得できた場合はtrue
        bool ENGINE_EXTENSION_CALL GetExternalGameObjectTemplateInfo(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            EngineExternalGameObjectTemplateInfo* information
        )
        {
            if (information == nullptr)
            {
                return false;
            }

            const GameObjectTemplate* Target = GetEngine(context).FindGameObjectTemplate(
                SceneID(sceneID),
                ObjectID(objectID)
            ); //固有値を読み取る雛形Object

            if (Target == nullptr)
            {
                return false;
            }

            *information = EngineExternalGameObjectTemplateInfo{};
            information->Size = sizeof(EngineExternalGameObjectTemplateInfo);
            information->SceneID = sceneID;
            information->ObjectID = objectID;
            information->MoveSpeed = Target->GetMoveSpeed();
            information->MaximumHealth = Target->GetMaximumHealth();
            CopyExternalName(
                information->GameplayTag,
                std::size(information->GameplayTag),
                Target->GetGameplayTag()
            );
            return true;
        }

        //概要：外部ProgramからGameObjectTemplate固有設定を変更する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、information=新しい設定
        //戻り値：対象がGameObjectTemplateで設定できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalGameObjectTemplateInfo(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const EngineExternalGameObjectTemplateInfo* information
        )
        {
            if (information == nullptr)
            {
                return false;
            }

            const std::size_t TagLength = strnlen_s(
                information->GameplayTag,
                std::size(information->GameplayTag)
            ); //固定長Buffer内の安全なTag長
            return GetEngine(context).SetGameObjectTemplateInfo(
                SceneID(sceneID),
                ObjectID(objectID),
                std::string(information->GameplayTag, TagLength),
                information->MoveSpeed,
                information->MaximumHealth
            );
        }

        //概要：外部ProgramからAttach可能なScript Factory数を取得する
        //引数：context=Engine Context
        //戻り値：登録済みScript数
        std::uint32_t ENGINE_EXTENSION_CALL GetExternalScriptCount(void* context)
        {
            return static_cast<std::uint32_t>(
                GetEngine(context).GetScriptRegistry().GetCatalog().size()
            );
        }

        //概要：登録順IndexからScript識別子、表示名、Module名を外部へ読み取る
        //引数：context=Engine Context、index=Script Index、information=結果格納先
        //戻り値：Script情報を取得できた場合はtrue
        bool ENGINE_EXTENSION_CALL GetExternalScriptInfo(
            void* context,
            std::uint32_t index,
            EngineExternalScriptInfo* information
        )
        {
            if (information == nullptr)
            {
                return false;
            }

            const std::vector<EditorScriptInfo> Catalog =
                GetEngine(context).GetScriptRegistry().GetCatalog(); //登録順Script情報

            if (index >= Catalog.size())
            {
                return false;
            }

            *information = EngineExternalScriptInfo{};
            information->Size = sizeof(EngineExternalScriptInfo);
            CopyExternalName(information->Key, std::size(information->Key), Catalog[index].Key);
            CopyExternalName(
                information->DisplayName,
                std::size(information->DisplayName),
                Catalog[index].DisplayName
            );
            CopyExternalName(
                information->ModuleName,
                std::size(information->ModuleName),
                Catalog[index].ModuleName
            );
            return true;
        }

        //概要：外部ProgramからRegistry Scriptを指定ObjectへAttachする
        //引数：context=Engine Context、sceneID=対象Scene、objectID=所有Object、scriptKey=Registry識別子
        //戻り値：Scriptを差し込んで初期化できた場合はtrue
        bool ENGINE_EXTENSION_CALL AttachExternalScript(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const char* scriptKey
        )
        {
            return scriptKey != nullptr && GetEngine(context).AttachScript(
                SceneID(sceneID),
                ObjectID(objectID),
                scriptKey
            );
        }

        //概要：外部Main ProgramへPrimitive Objectの現在RGBA色を返す
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、color=結果格納先
        //戻り値：対象がBox等Primitiveで色を取得できた場合はtrue
        bool ENGINE_EXTENSION_CALL GetExternalObjectColor(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            EngineExternalColor* color
        )
        {
            if (color == nullptr)
            {
                return false;
            }

            EditorColor NativeColor; //Native APIから読み取るRGBA色

            if (!GetEngine(context).TryGetObjectColor(
                SceneID(sceneID),
                ObjectID(objectID),
                NativeColor
            ))
            {
                return false;
            }

            *color = EngineExternalColor
            {
                NativeColor.Red,
                NativeColor.Green,
                NativeColor.Blue,
                NativeColor.Alpha
            };
            return true;
        }

        //概要：外部Main ProgramからPrimitive ObjectのRGBA色を変更する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、color=設定するRGBA色
        //戻り値：対象がBox等Primitiveで色を変更できた場合はtrue
        bool ENGINE_EXTENSION_CALL SetExternalObjectColor(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const EngineExternalColor* color
        )
        {
            if (color == nullptr)
            {
                return false;
            }

            return GetEngine(context).SetObjectColor(
                SceneID(sceneID),
                ObjectID(objectID),
                EditorColor{ color->Red, color->Green, color->Blue, color->Alpha }
            );
        }

        //概要：外部Main ProgramからPrimitiveの現在色へRGBA係数を乗算する
        //引数：context=Engine Context、sceneID=対象Scene、objectID=対象Object、multiplier=RGBA乗算係数
        //戻り値：対象Primitiveへ乗算色を設定できた場合true
        bool ENGINE_EXTENSION_CALL MultiplyExternalObjectColor(
            void* context,
            std::uint32_t sceneID,
            std::uint32_t objectID,
            const EngineExternalColor* multiplier
        )
        {
            if (multiplier == nullptr)
            {
                return false;
            }

            return GetEngine(context).MultiplyObjectColor(
                SceneID(sceneID),
                ObjectID(objectID),
                EditorColor
                {
                    multiplier->Red,
                    multiplier->Green,
                    multiplier->Blue,
                    multiplier->Alpha
                }
            );
        }

        //概要：外部Main ProgramからEditorコード補完候補を追加する
        //引数：context=Engine Context、suggestion=追加するC++識別子
        //戻り値：候補が有効で登録できた場合true
        bool ENGINE_EXTENSION_CALL SetExternalProgramSuggestion(
            void* context,
            const char* suggestion
        )
        {
            return suggestion != nullptr &&
                GetEngine(context).SetProgramSuggestion(suggestion);
        }

        //概要：外部Main Programへ指定Virtual Keyの現在押下状態を返す
        //引数：context=Engine Context、virtualKey=Windows Virtual-Key Code
        //戻り値：Keyが現在押されている場合はtrue
        bool ENGINE_EXTENSION_CALL IsExternalKeyDown(
            void* context,
            std::uint32_t virtualKey
        )
        {
            return GetEngine(context).IsKeyDown(virtualKey);
        }
    }

    //概要：Native Engine APIを外部C ABI関数表へ接続する
    //引数：engine=外部Programから操作するEngine Facade
    //戻り値：なし
    ExtensionModuleManager::ExtensionModuleManager(EngineAPI& engine)
        : Engine(engine)
        , Host{}
        , ActiveModule{}
        , Generation(0)
        , FrameNumber(0)
        , DeltaTime(0.0f)
    {
        Host.Size = sizeof(EngineHostAPI);
        Host.AbiVersion = EngineExtensionAbiVersion;
        Host.Context = &Engine;
        Host.AddLog = AddExternalLog;
        Host.GetFrameNumber = GetExternalFrameNumber;
        Host.GetDeltaTime = GetExternalDeltaTime;
        Host.GetSceneCount = GetExternalSceneCount;
        Host.GetSceneInfo = GetExternalSceneInfo;
        Host.SetSceneActive = SetExternalSceneActive;
        Host.SetViewScene = SetExternalViewScene;
        Host.GetObjectCount = GetExternalObjectCount;
        Host.GetObjectInfo = GetExternalObjectInfo;
        Host.CreateObject = CreateExternalObject;
        Host.RemoveObject = RemoveExternalObject;
        Host.RenameObject = RenameExternalObject;
        Host.SetObjectActive = SetExternalObjectActive;
        Host.SetObjectTransform = SetExternalObjectTransform;
        Host.SetObjectParent = SetExternalObjectParent;
        Host.GetComponentCount = GetExternalComponentCount;
        Host.GetComponentInfo = GetExternalComponentInfo;
        Host.RemoveComponent = RemoveExternalComponent;
        Host.RenameComponent = RenameExternalComponent;
        Host.SetComponentActive = SetExternalComponentActive;
        Host.GetEngineRevision = GetExternalEngineRevision;
        Host.GetMainSceneID = GetExternalMainSceneID;
        Host.GetViewSceneID = GetExternalViewSceneID;
        Host.FindSceneByName = FindExternalSceneByName;
        Host.CreateScene = CreateExternalScene;
        Host.DuplicateScene = DuplicateExternalScene;
        Host.RemoveScene = RemoveExternalScene;
        Host.SetMainScene = SetExternalMainScene;
        Host.FindObjectByName = FindExternalObjectByName;
        Host.GetChildCount = GetExternalChildCount;
        Host.GetChildObjectID = GetExternalChildObjectID;
        Host.DuplicateObject = DuplicateExternalObject;
        Host.CreateGameObjectTemplate = CreateExternalGameObjectTemplate;
        Host.GetGameObjectTemplateInfo = GetExternalGameObjectTemplateInfo;
        Host.SetGameObjectTemplateInfo = SetExternalGameObjectTemplateInfo;
        Host.GetScriptCount = GetExternalScriptCount;
        Host.GetScriptInfo = GetExternalScriptInfo;
        Host.AttachScript = AttachExternalScript;
        Host.GetObjectColor = GetExternalObjectColor;
        Host.SetObjectColor = SetExternalObjectColor;
        Host.IsKeyDown = IsExternalKeyDown;
        Host.MultiplyObjectColor = MultiplyExternalObjectColor;
        Host.SetProgramSuggestion = SetExternalProgramSuggestion;
        Host.FindObjectByNameOnly = FindExternalObjectByNameOnly;
        Host.CreateCapsuleModel = CreateExternalCapsuleModel;
        Host.SetObjectSize = SetExternalObjectSize;
        Host.SetObjectSizeByName = SetExternalObjectSizeByName;
    }

    //概要：稼働中の外部Program Instanceを破棄してDLLを解放する
    //引数：なし
    //戻り値：なし
    ExtensionModuleManager::~ExtensionModuleManager()
    {
        Unload();
    }

    //概要：候補DLLを検証し旧状態を移してから稼働Moduleを世代交代する
    //引数：modulePath=Background Compileが生成した読込専用DLL Path
    //戻り値：新Moduleへの安全な切替が完了した場合はtrue
    bool ExtensionModuleManager::LoadOrReload(const std::filesystem::path& modulePath)
    {
        LoadedModule Candidate; //検証中の新しいModule
        Candidate.Path = modulePath;
        Candidate.Handle = LoadLibraryW(modulePath.c_str());

        if (Candidate.Handle == nullptr)
        {
            const DWORD ErrorCode = GetLastError(); //LoadLibrary失敗直後の診断Code
            MessageLog::GetInstance().AddLog(
                "[Error] Extension | LoadLibraryW failed (" +
                FormatWindowsLoaderError(ErrorCode) +
                "); active module was preserved."
            );
            return false;
        }

        const auto Entry = reinterpret_cast<EngineGetExtensionModuleFunction>(
            GetProcAddress(Candidate.Handle, EngineExtensionEntryPoint)
        ); //共通名から解決したModule取得関数
        if (Entry == nullptr)
        {
            const DWORD ErrorCode = GetLastError(); //GetProcAddress失敗直後の診断Code
            ReleaseModule(Candidate);
            MessageLog::GetInstance().AddLog(
                "[Error] Extension | EngineGetExtensionModule export was not found (" +
                FormatWindowsLoaderError(ErrorCode) +
                "); active module was preserved."
            );
            return false;
        }

        Candidate.Descriptor = Entry(EngineExtensionAbiVersion);

        if (Candidate.Descriptor == nullptr ||
            Candidate.Descriptor->Size < sizeof(EngineExtensionModuleDescriptor) ||
            Candidate.Descriptor->AbiVersion != EngineExtensionAbiVersion ||
            Candidate.Descriptor->Create == nullptr ||
            Candidate.Descriptor->Destroy == nullptr ||
            Candidate.Descriptor->Update == nullptr)
        {
            ReleaseModule(Candidate);
            MessageLog::GetInstance().AddLog(
                "[Error] Extension | DLL ABI validation failed for requested ABI " +
                std::to_string(EngineExtensionAbiVersion) +
                "; active module was preserved."
            );
            return false;
        }

        Candidate.Name = Candidate.Descriptor->ModuleName == nullptr
            ? modulePath.filename().string()
            : Candidate.Descriptor->ModuleName;
        Candidate.Instance = Candidate.Descriptor->Create(&Host);

        if (Candidate.Instance == nullptr)
        {
            ReleaseModule(Candidate);
            MessageLog::GetInstance().AddLog("[Error] Extension | Module instance creation failed; active module was preserved.");
            return false;
        }

        std::vector<std::byte> State; //旧世代から新世代へ渡す任意状態

        if (ActiveModule.Instance != nullptr &&
            ActiveModule.Descriptor->GetStateSize != nullptr &&
            ActiveModule.Descriptor->SaveState != nullptr)
        {
            const std::uint32_t StateSize = ActiveModule.Descriptor->GetStateSize(
                ActiveModule.Instance
            ); //旧Moduleが公開する状態Byte数
            State.resize(StateSize);

            if (StateSize > 0 && !ActiveModule.Descriptor->SaveState(
                ActiveModule.Instance,
                State.data(),
                StateSize
            ))
            {
                State.clear();
            }
        }

        if (!State.empty() && Candidate.Descriptor->LoadState != nullptr)
        {
            Candidate.Descriptor->LoadState(
                Candidate.Instance,
                State.data(),
                static_cast<std::uint32_t>(State.size())
            );
        }

        LoadedModule Previous = std::move(ActiveModule); //切替後に解放する旧Module
        ActiveModule = std::move(Candidate);
        ReleaseModule(Previous);
        ++Generation;
        MessageLog::GetInstance().AddLog("[Info] Extension | Module hot reloaded.");
        return true;
    }

    //概要：現在稼働する外部Program InstanceとDLLを解放する
    //引数：なし
    //戻り値：なし
    void ExtensionModuleManager::Unload()
    {
        ReleaseModule(ActiveModule);
    }

    //概要：稼働中Moduleの毎Frame処理をNative固定更新上で実行する
    //引数：deltaTime=前回固定更新からの秒数
    //戻り値：なし
    void ExtensionModuleManager::Update(float deltaTime)
    {
        DeltaTime = deltaTime;

        if (ActiveModule.Instance != nullptr && ActiveModule.Descriptor != nullptr)
        {
            ActiveModule.Descriptor->Update(ActiveModule.Instance, deltaTime);
            ++FrameNumber;
        }
    }

    //概要：外部Program Moduleが現在稼働中か判定する
    //引数：なし
    //戻り値：Module Instanceが存在する場合はtrue
    bool ExtensionModuleManager::IsLoaded() const
    {
        return ActiveModule.Instance != nullptr;
    }

    //概要：Hot Reloadに成功した現在世代番号を取得する
    //引数：なし
    //戻り値：初回Loadを1とする世代番号
    std::uint64_t ExtensionModuleManager::GetGeneration() const
    {
        return Generation;
    }

    //概要：外部Programが実行済みの通算Frame番号を取得する
    //引数：なし
    //戻り値：通算Frame番号
    std::uint64_t ExtensionModuleManager::GetFrameNumber() const
    {
        return FrameNumber;
    }

    //概要：現在外部Programへ渡している固定更新秒数を取得する
    //引数：なし
    //戻り値：秒単位Delta Time
    float ExtensionModuleManager::GetDeltaTime() const
    {
        return DeltaTime;
    }

    //概要：現在稼働する外部Programの表示名を取得する
    //引数：なし
    //戻り値：Module DescriptorのUTF-8名
    const std::string& ExtensionModuleManager::GetModuleName() const
    {
        return ActiveModule.Name;
    }

    //概要：現在読み込んでいる世代別DLL Pathを取得する
    //引数：なし
    //戻り値：LoadLibraryへ渡したDLL Path
    const std::filesystem::path& ExtensionModuleManager::GetModulePath() const
    {
        return ActiveModule.Path;
    }

    //概要：Module Instanceを先に破棄してからDLL Handleを解放する
    //引数：module=解放対象Module所有情報
    //戻り値：なし
    void ExtensionModuleManager::ReleaseModule(LoadedModule& module)
    {
        if (module.Instance != nullptr && module.Descriptor != nullptr &&
            module.Descriptor->Destroy != nullptr)
        {
            module.Descriptor->Destroy(module.Instance);
        }

        module.Instance = nullptr;
        module.Descriptor = nullptr;

        if (module.Handle != nullptr)
        {
            FreeLibrary(module.Handle);
        }

        module.Handle = nullptr;
        module.Path.clear();
        module.Name.clear();
    }
}
