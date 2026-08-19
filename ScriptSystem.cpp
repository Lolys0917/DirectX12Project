//|| ScriptSystem.cpp ||:::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  DLLごとのHMODULEと関数表を保持しSub Programとして実行する処理を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.10  DLL LoaderのWindows Error診断を追加
//||  2026_08_17  v1.00  新規作成
//||

#include "ScriptSystem.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <limits>
#include <utility>

#include <DirectXMath.h>

#include "MessageLog.h"
#include "GameInput.h"
#include "Object.h"
#include "PrimitiveObject.h"
#include "ScriptModuleAPI.h"

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

        struct DynamicScriptDescriptor final
        {
            std::string TypeKey; //Module内Script識別子
            std::string DisplayName; //Editor表示名
            decltype(EngineScriptDescriptor::Create) Create = nullptr; //DLL Instance生成関数
            decltype(EngineScriptDescriptor::Destroy) Destroy = nullptr; //DLL Instance破棄関数
            decltype(EngineScriptDescriptor::OnAttach) Attach = nullptr; //接続通知関数
            decltype(EngineScriptDescriptor::OnStart) Start = nullptr; //開始通知関数
            decltype(EngineScriptDescriptor::OnUpdate) Update = nullptr; //毎Frame更新関数
            decltype(EngineScriptDescriptor::OnStop) Stop = nullptr; //停止通知関数
            decltype(EngineScriptDescriptor::OnDetach) Detach = nullptr; //切断通知関数
        };

        struct DynamicScriptHostContext final
        {
            Object* Owner = nullptr; //Callbackが操作する所有Object
        };

        //概要：DLL Scriptが所有Object IDを取得する
        //引数：context=DynamicScriptが所有するHost Context
        //戻り値：所有Object ID、利用できない場合は0
        std::uint32_t ENGINE_SCRIPT_CALL GetHostObjectID(void* context)
        {
            const auto* Host = static_cast<const DynamicScriptHostContext*>(context); //DLLごとのHost Context
            return Host != nullptr && Host->Owner != nullptr
                ? Host->Owner->GetID().GetValue()
                : ObjectID::InvalidValue;
        }

        //概要：DLL Scriptから受け取ったUTF-8文字列を共通Logへ追加する
        //引数：context=未使用Host Context、message=追加するUTF-8文字列
        //戻り値：なし
        void ENGINE_SCRIPT_CALL AddHostLog(void* context, const char* message)
        {
            (void)context;

            if (message != nullptr)
            {
                MessageLog::GetInstance().AddLog(message);
            }
        }

        //概要：DLL Scriptが所有Objectの有効状態を取得する
        //引数：context=DynamicScriptが所有するHost Context
        //戻り値：有効な場合は1、無効又は利用できない場合は0
        std::uint32_t ENGINE_SCRIPT_CALL GetHostActive(void* context)
        {
            const auto* Host = static_cast<const DynamicScriptHostContext*>(context); //DLLごとのHost Context
            return Host != nullptr && Host->Owner != nullptr && Host->Owner->IsActive()
                ? 1u
                : 0u;
        }

        //概要：DLL Scriptから所有Objectの有効状態を変更する
        //引数：context=DynamicScriptが所有するHost Context、active=有効にする場合は0以外
        //戻り値：なし
        void ENGINE_SCRIPT_CALL SetHostActive(void* context, std::uint32_t active)
        {
            auto* Host = static_cast<DynamicScriptHostContext*>(context); //DLLごとのHost Context

            if (Host != nullptr && Host->Owner != nullptr)
            {
                Host->Owner->SetActive(active != 0);
            }
        }

        //概要：所有ObjectのX、Y、Z値をDLL出力配列へコピーする
        //引数：context=DynamicScriptが所有するHost Context、output=3要素の出力配列、selector=取得値選択関数
        //戻り値：なし
        void CopyVectorToScript(
            void* context,
            float output[3],
            const DirectX::XMFLOAT3& (*selector)(const Object&)
        )
        {
            if (output == nullptr)
            {
                return;
            }

            const auto* Host = static_cast<const DynamicScriptHostContext*>(context); //DLLごとのHost Context

            if (Host == nullptr || Host->Owner == nullptr)
            {
                output[0] = 0.0f;
                output[1] = 0.0f;
                output[2] = 0.0f;
                return;
            }

            const DirectX::XMFLOAT3& Value = selector(*Host->Owner); //取得対象のObject値
            output[0] = Value.x;
            output[1] = Value.y;
            output[2] = Value.z;
        }

        //概要：DLL入力配列から所有ObjectのX、Y、Z値を設定する
        //引数：context=DynamicScriptが所有するHost Context、value=3要素の入力配列、setter=設定関数
        //戻り値：なし
        void CopyVectorFromScript(
            void* context,
            const float value[3],
            void (*setter)(Object&, const DirectX::XMFLOAT3&)
        )
        {
            auto* Host = static_cast<DynamicScriptHostContext*>(context); //DLLごとのHost Context

            if (Host == nullptr || Host->Owner == nullptr || value == nullptr)
            {
                return;
            }

            setter(*Host->Owner, DirectX::XMFLOAT3(value[0], value[1], value[2]));
        }

        //概要：DLL Scriptへ所有Objectの座標を返す
        //引数：context=DynamicScriptが所有するHost Context、output=XYZ出力配列
        //戻り値：なし
        void ENGINE_SCRIPT_CALL GetHostPosition(void* context, float output[3])
        {
            CopyVectorToScript(context, output, [](const Object& owner) -> const DirectX::XMFLOAT3&
            {
                return owner.GetPosition();
            });
        }

        //概要：DLL Scriptから所有Objectの座標を設定する
        //引数：context=DynamicScriptが所有するHost Context、value=XYZ入力配列
        //戻り値：なし
        void ENGINE_SCRIPT_CALL SetHostPosition(void* context, const float value[3])
        {
            CopyVectorFromScript(context, value, [](Object& owner, const DirectX::XMFLOAT3& position)
            {
                owner.SetPosition(position);
            });
        }

        //概要：DLL Scriptへ所有Objectの回転を返す
        //引数：context=DynamicScriptが所有するHost Context、output=XYZ出力配列
        //戻り値：なし
        void ENGINE_SCRIPT_CALL GetHostRotation(void* context, float output[3])
        {
            CopyVectorToScript(context, output, [](const Object& owner) -> const DirectX::XMFLOAT3&
            {
                return owner.GetRotation();
            });
        }

        //概要：DLL Scriptから所有Objectの回転を設定する
        //引数：context=DynamicScriptが所有するHost Context、value=XYZ入力配列
        //戻り値：なし
        void ENGINE_SCRIPT_CALL SetHostRotation(void* context, const float value[3])
        {
            CopyVectorFromScript(context, value, [](Object& owner, const DirectX::XMFLOAT3& rotation)
            {
                owner.SetRotation(rotation);
            });
        }

        //概要：DLL Scriptへ所有Objectの拡縮率を返す
        //引数：context=DynamicScriptが所有するHost Context、output=XYZ出力配列
        //戻り値：なし
        void ENGINE_SCRIPT_CALL GetHostScale(void* context, float output[3])
        {
            CopyVectorToScript(context, output, [](const Object& owner) -> const DirectX::XMFLOAT3&
            {
                return owner.GetScale();
            });
        }

        //概要：DLL Scriptから所有Objectの拡縮率を設定する
        //引数：context=DynamicScriptが所有するHost Context、value=XYZ入力配列
        //戻り値：なし
        void ENGINE_SCRIPT_CALL SetHostScale(void* context, const float value[3])
        {
            CopyVectorFromScript(context, value, [](Object& owner, const DirectX::XMFLOAT3& scale)
            {
                owner.SetScale(scale);
            });
        }

        //概要：DLL Scriptへ所有ObjectのObjectType数値を返す
        //引数：context=DynamicScriptが所有するHost Context
        //戻り値：ObjectType数値、利用できない場合は無効値
        std::uint32_t ENGINE_SCRIPT_CALL GetHostObjectType(void* context)
        {
            const auto* Host = static_cast<const DynamicScriptHostContext*>(context); //DLLごとのHost Context
            return Host != nullptr && Host->Owner != nullptr
                ? static_cast<std::uint32_t>(Host->Owner->GetType())
                : (std::numeric_limits<std::uint32_t>::max)();
        }

        //概要：DLL Scriptへ指定Virtual Keyの現在押下状態を返す
        //引数：context=未使用Host Context、virtualKey=Windows Virtual-Key Code
        //戻り値：Keyが押されている場合は1、それ以外は0
        std::uint32_t ENGINE_SCRIPT_CALL IsHostKeyDown(
            void* context,
            std::uint32_t virtualKey
        )
        {
            (void)context;
            return GameInput::IsKeyDown(virtualKey) ? 1u : 0u;
        }

        //概要：DLL Scriptへ所有Primitive ObjectのRGBA色をコピーする
        //引数：context=DynamicScriptが所有するHost Context、output=RGBA出力配列
        //戻り値：所有ObjectがPrimitiveの場合は1、それ以外は0
        std::uint32_t ENGINE_SCRIPT_CALL GetHostColor(void* context, float output[4])
        {
            const auto* Host = static_cast<const DynamicScriptHostContext*>(context); //DLLごとのHost Context
            const auto* Primitive = Host == nullptr
                ? nullptr
                : dynamic_cast<const PrimitiveObject*>(Host->Owner); //色を所有するPrimitive

            if (Primitive == nullptr || output == nullptr)
            {
                return 0;
            }

            const DirectX::XMFLOAT4& Color = Primitive->GetColor(); //現在のRGBA色
            output[0] = Color.x;
            output[1] = Color.y;
            output[2] = Color.z;
            output[3] = Color.w;
            return 1;
        }

        //概要：DLL Scriptから所有Primitive ObjectのRGBA色を変更する
        //引数：context=DynamicScriptが所有するHost Context、value=RGBA入力配列
        //戻り値：所有ObjectがPrimitiveで設定できた場合は1、それ以外は0
        std::uint32_t ENGINE_SCRIPT_CALL SetHostColor(void* context, const float value[4])
        {
            auto* Host = static_cast<DynamicScriptHostContext*>(context); //DLLごとのHost Context
            auto* Primitive = Host == nullptr
                ? nullptr
                : dynamic_cast<PrimitiveObject*>(Host->Owner); //色を変更するPrimitive

            if (Primitive == nullptr || value == nullptr)
            {
                return 0;
            }

            Primitive->SetColor(DirectX::XMFLOAT4(value[0], value[1], value[2], value[3]));
            return 1;
        }

        //概要：DLL Scriptから所有Primitiveの現在RGBA色へ係数を乗算する
        //引数：context=DynamicScriptが所有するHost Context、multiplier=RGBA乗算係数
        //戻り値：所有ObjectがPrimitiveで乗算できた場合1、それ以外は0
        std::uint32_t ENGINE_SCRIPT_CALL MultiplyHostColor(
            void* context,
            const float multiplier[4]
        )
        {
            auto* Host = static_cast<DynamicScriptHostContext*>(context); //DLLごとのHost Context
            auto* Primitive = Host == nullptr
                ? nullptr
                : dynamic_cast<PrimitiveObject*>(Host->Owner); //色を変更するPrimitive

            if (Primitive == nullptr || multiplier == nullptr)
            {
                return 0;
            }

            const DirectX::XMFLOAT4 Current = Primitive->GetColor(); //乗算前のRGBA色
            Primitive->SetColor(DirectX::XMFLOAT4(
                Current.x * multiplier[0],
                Current.y * multiplier[1],
                Current.z * multiplier[2],
                Current.w * multiplier[3]
            ));
            return 1;
        }
    }

    struct ScriptModuleManager::ModuleRecord final
    {
        HMODULE Handle = nullptr; //関数Pointer利用中保持するDLL Handle
        std::wstring Path; //正規化済みDLL Path
        std::string Name; //Module表示名
        std::vector<DynamicScriptDescriptor> Scripts; //DLLからCopyした関数表
        std::vector<std::string> RegistryKeys; //Unload時に解除する登録Key

        //概要：保持中のDLLを関数表破棄後に解放する
        //引数：なし
        //戻り値：なし
        ~ModuleRecord()
        {
            if (Handle != nullptr)
            {
                FreeLibrary(Handle);
                Handle = nullptr;
            }
        }
    };

    namespace
    {
        class DynamicScript final : public Script
        {
        public:
            DynamicScript(
                std::string registryKey,
                std::shared_ptr<ScriptModuleManager::ModuleRecord> module,
                std::size_t descriptorIndex
            );
            ~DynamicScript() override;

        protected:
            std::unique_ptr<Script> CloneScript() const override;
            bool OnAttach() override;
            void OnStart() override;
            void OnUpdate(float deltaTime) override;
            void OnStop() override;
            void OnDetach() override;

        private:
            const DynamicScriptDescriptor* GetDescriptor() const;

            std::string RegistryKey; //Module名を含むRegistry Key
            std::shared_ptr<ScriptModuleManager::ModuleRecord> Module; //関数Pointerを有効に保つModule
            std::size_t DescriptorIndex; //Module内Script関数表位置
            DynamicScriptHostContext HostContext; //所有ObjectへのHost Callback Context
            EngineScriptHostAPI HostAPI; //DLL Instanceへ渡すHost関数表
            void* Instance; //DLL側Createが生成した不透明Instance
        };

        //概要：DLL関数表を保持するDynamic Script Componentを作成する
        //引数：registryKey=一意なScript Key、module=DLL寿命所有者、descriptorIndex=Module内関数表位置
        //戻り値：なし
        DynamicScript::DynamicScript(
            std::string registryKey,
            std::shared_ptr<ScriptModuleManager::ModuleRecord> module,
            std::size_t descriptorIndex
        )
            : Script(
                registryKey,
                module != nullptr && descriptorIndex < module->Scripts.size()
                    ? module->Scripts[descriptorIndex].DisplayName
                    : registryKey
            )
            , RegistryKey(std::move(registryKey))
            , Module(std::move(module))
            , DescriptorIndex(descriptorIndex)
            , HostContext()
            , HostAPI
            {
                sizeof(EngineScriptHostAPI),
                EngineScriptAbiVersion,
                &HostContext,
                GetHostObjectID,
                AddHostLog,
                GetHostActive,
                SetHostActive,
                GetHostPosition,
                SetHostPosition,
                GetHostRotation,
                SetHostRotation,
                GetHostScale,
                SetHostScale,
                GetHostObjectType,
                IsHostKeyDown,
                GetHostColor,
                SetHostColor,
                MultiplyHostColor
            }
            , Instance(nullptr)
        {
        }

        //概要：Finalize済みDynamic Script Componentを破棄する
        //引数：なし
        //戻り値：なし
        DynamicScript::~DynamicScript() = default;

        //概要：同じModule関数表を参照する未登録Script定義を複製する
        //引数：なし
        //戻り値：新しいDynamic Script Component
        std::unique_ptr<Script> DynamicScript::CloneScript() const
        {
            return std::make_unique<DynamicScript>(
                RegistryKey,
                Module,
                DescriptorIndex
            );
        }

        //概要：DLL内Instanceを生成して所有Objectへ接続する
        //引数：なし
        //戻り値：Instance生成と接続に成功した場合はtrue
        bool DynamicScript::OnAttach()
        {
            const DynamicScriptDescriptor* Descriptor = GetDescriptor(); //実行するDLL関数表
            HostContext.Owner = GetOwner();

            if (Descriptor == nullptr || HostContext.Owner == nullptr ||
                Descriptor->Create == nullptr || Descriptor->Destroy == nullptr)
            {
                return false;
            }

            Instance = Descriptor->Create(&HostAPI);

            if (Instance == nullptr)
            {
                return false;
            }

            if (Descriptor->Attach != nullptr && Descriptor->Attach(Instance) == 0)
            {
                Descriptor->Destroy(Instance);
                Instance = nullptr;
                HostContext.Owner = nullptr;
                return false;
            }

            return true;
        }

        //概要：DLL Scriptの初回更新前処理を呼び出す
        //引数：なし
        //戻り値：なし
        void DynamicScript::OnStart()
        {
            const DynamicScriptDescriptor* Descriptor = GetDescriptor(); //実行するDLL関数表

            if (Descriptor != nullptr && Descriptor->Start != nullptr)
            {
                Descriptor->Start(Instance);
            }
        }

        //概要：DLL Scriptの毎フレーム処理を呼び出す
        //引数：deltaTime=前フレームからの経過秒数
        //戻り値：なし
        void DynamicScript::OnUpdate(float deltaTime)
        {
            const DynamicScriptDescriptor* Descriptor = GetDescriptor(); //実行するDLL関数表

            if (Descriptor != nullptr && Descriptor->Update != nullptr)
            {
                Descriptor->Update(Instance, deltaTime);
            }
        }

        //概要：DLL Scriptの停止処理を呼び出す
        //引数：なし
        //戻り値：なし
        void DynamicScript::OnStop()
        {
            const DynamicScriptDescriptor* Descriptor = GetDescriptor(); //実行するDLL関数表

            if (Descriptor != nullptr && Descriptor->Stop != nullptr)
            {
                Descriptor->Stop(Instance);
            }
        }

        //概要：DLL Scriptを所有Objectから切断しInstanceをDLL側で破棄する
        //引数：なし
        //戻り値：なし
        void DynamicScript::OnDetach()
        {
            const DynamicScriptDescriptor* Descriptor = GetDescriptor(); //実行するDLL関数表

            if (Descriptor != nullptr && Descriptor->Detach != nullptr)
            {
                Descriptor->Detach(Instance);
            }

            if (Descriptor != nullptr && Descriptor->Destroy != nullptr && Instance != nullptr)
            {
                Descriptor->Destroy(Instance);
            }

            Instance = nullptr;
            HostContext.Owner = nullptr;
        }

        //概要：保持中Moduleから安全にScript関数表を取得する
        //引数：なし
        //戻り値：有効な関数表、Module不整合時はnullptr
        const DynamicScriptDescriptor* DynamicScript::GetDescriptor() const
        {
            return Module != nullptr && DescriptorIndex < Module->Scripts.size()
                ? &Module->Scripts[DescriptorIndex]
                : nullptr;
        }
    }

    //概要：空のScript Factory Registryを作成する
    //引数：なし
    //戻り値：なし
    ScriptRegistry::ScriptRegistry() = default;

    //概要：登録済みFactoryを破棄する
    //引数：なし
    //戻り値：なし
    ScriptRegistry::~ScriptRegistry() = default;

    //概要：Native又はDLL Script Factoryを一意Keyで登録する
    //引数：key=一意識別子、displayName=表示名、moduleName=提供元名、factory=生成関数
    //戻り値：登録できた場合はtrue
    bool ScriptRegistry::RegisterFactory(
        const std::string& key,
        const std::string& displayName,
        const std::string& moduleName,
        Factory factory
    )
    {
        if (key.empty() || displayName.empty() || !factory || Entries.contains(key))
        {
            return false;
        }

        Entry NewEntry
        {
            key,
            displayName,
            moduleName.empty() ? "Unknown" : moduleName,
            std::move(factory)
        }; //登録するScript Factory情報

        if (!Entries.emplace(key, std::move(NewEntry)).second)
        {
            return false;
        }

        RegistrationOrder.emplace_back(key);
        return true;
    }

    //概要：指定KeyのScript Factoryを登録解除する
    //引数：key=解除するScript識別子
    //戻り値：登録が存在して解除した場合はtrue
    bool ScriptRegistry::UnregisterScript(const std::string& key)
    {
        if (Entries.erase(key) == 0)
        {
            return false;
        }

        RegistrationOrder.erase(
            std::remove(RegistrationOrder.begin(), RegistrationOrder.end(), key),
            RegistrationOrder.end()
        );
        return true;
    }

    //概要：指定KeyのFactoryから未登録Scriptを生成する
    //引数：key=生成するScript識別子
    //戻り値：未登録Script、未登録Key又は生成失敗時はnullptr
    std::unique_ptr<Script> ScriptRegistry::CreateScript(const std::string& key) const
    {
        const auto Iterator = Entries.find(key); //Script Factory検索結果
        return Iterator == Entries.end() ? nullptr : Iterator->second.Create();
    }

    //概要：エディターで選択可能なScript一覧を登録順で取得する
    //引数：なし
    //戻り値：Script Key、表示名、Module名の一覧
    std::vector<EditorScriptInfo> ScriptRegistry::GetCatalog() const
    {
        std::vector<EditorScriptInfo> Result; //返却するScript一覧
        Result.reserve(RegistrationOrder.size());

        for (const std::string& Key : RegistrationOrder)
        {
            const auto Iterator = Entries.find(Key); //現在Keyの登録情報

            if (Iterator != Entries.end())
            {
                Result.push_back(EditorScriptInfo
                {
                    Iterator->second.Key,
                    Iterator->second.DisplayName,
                    Iterator->second.ModuleName
                });
            }
        }

        return Result;
    }

    //概要：指定Script Keyが登録済みか確認する
    //引数：key=確認するScript識別子
    //戻り値：登録済みの場合はtrue
    bool ScriptRegistry::Contains(const std::string& key) const
    {
        return Entries.contains(key);
    }

    //概要：DLL Script Factoryを指定Registryへ登録するModule Managerを作成する
    //引数：registry=Factory登録先
    //戻り値：なし
    ScriptModuleManager::ScriptModuleManager(ScriptRegistry& registry)
        : Registry(registry)
        , Modules()
    {
    }

    //概要：Factory登録を解除して読み込み中DLLを解放する
    //引数：なし
    //戻り値：なし
    ScriptModuleManager::~ScriptModuleManager()
    {
        while (!Modules.empty())
        {
            const std::wstring Path = Modules.back()->Path; //最後に読み込んだModule Path
            UnloadModule(Path);
        }
    }

    //概要：DLLから共通Entry Pointを取得し全Script関数表をRegistryへ登録する
    //引数：path=読み込むDLL Path
    //戻り値：Module内の全Scriptを登録できた場合はtrue
    bool ScriptModuleManager::LoadModule(const std::wstring& path)
    {
        const std::wstring NormalizedPath = NormalizePath(path); //比較とLoadに使う絶対Path

        if (NormalizedPath.empty() || IsLoaded(NormalizedPath))
        {
            return false;
        }

        HMODULE Handle = LoadLibraryW(NormalizedPath.c_str()); //このDLL専用のModule Handle

        if (Handle == nullptr)
        {
            const DWORD ErrorCode = GetLastError(); //LoadLibrary失敗直後の診断Code
            MessageLog::GetInstance().AddLog(
                "[Error] ScriptModule | LoadLibraryW failed (" +
                FormatWindowsLoaderError(ErrorCode) + ")."
            );
            return false;
        }

        const auto EntryPoint = reinterpret_cast<EngineGetScriptModuleFunction>(
            GetProcAddress(Handle, EngineScriptModuleEntryPoint)
        ); //このHMODULEだけを対象に取得した共通名Export関数

        if (EntryPoint == nullptr)
        {
            const DWORD ErrorCode = GetLastError(); //GetProcAddress失敗直後の診断Code
            FreeLibrary(Handle);
            MessageLog::GetInstance().AddLog(
                "[Error] ScriptModule | EngineGetScriptModule export was not found (" +
                FormatWindowsLoaderError(ErrorCode) + ")."
            );
            return false;
        }

        const EngineScriptModuleDescriptor* Source = EntryPoint(
            EngineScriptAbiVersion
        ); //DLLが所有するModule関数表

        if (Source == nullptr || Source->Size < sizeof(EngineScriptModuleDescriptor) ||
            Source->AbiVersion != EngineScriptAbiVersion || Source->ModuleName == nullptr ||
            Source->Scripts == nullptr || Source->ScriptCount == 0)
        {
            FreeLibrary(Handle);
            MessageLog::GetInstance().AddLog(
                "[Error] ScriptModule | Module descriptor or ABI version was invalid."
            );
            return false;
        }

        auto Module = std::make_shared<ModuleRecord>(); //DLLとCopy済み関数表の寿命所有者
        Module->Handle = Handle;
        Module->Path = NormalizedPath;
        Module->Name = Source->ModuleName;
        Module->Scripts.reserve(Source->ScriptCount);

        for (std::uint32_t Index = 0; Index < Source->ScriptCount; ++Index)
        {
            const EngineScriptDescriptor& SourceScript = Source->Scripts[Index]; //検証するDLL Script関数表

            if (SourceScript.Size < sizeof(EngineScriptDescriptor) ||
                SourceScript.TypeKey == nullptr || SourceScript.DisplayName == nullptr ||
                SourceScript.Create == nullptr || SourceScript.Destroy == nullptr ||
                SourceScript.OnUpdate == nullptr)
            {
                MessageLog::GetInstance().AddLog(
                    "[Error] ScriptModule | A Script descriptor was invalid."
                );
                return false;
            }

            DynamicScriptDescriptor Descriptor; //Host側へCopyする関数表
            Descriptor.TypeKey = SourceScript.TypeKey;
            Descriptor.DisplayName = SourceScript.DisplayName;
            Descriptor.Create = SourceScript.Create;
            Descriptor.Destroy = SourceScript.Destroy;
            Descriptor.Attach = SourceScript.OnAttach;
            Descriptor.Start = SourceScript.OnStart;
            Descriptor.Update = SourceScript.OnUpdate;
            Descriptor.Stop = SourceScript.OnStop;
            Descriptor.Detach = SourceScript.OnDetach;
            Module->Scripts.emplace_back(std::move(Descriptor));
        }

        bool ReplacingExternalModule = false; //同名外部ModuleのFactoryを世代交代する場合true

        for (auto Iterator = Modules.begin(); Iterator != Modules.end();)
        {
            if (*Iterator == nullptr || (*Iterator)->Name != Module->Name)
            {
                ++Iterator;
                continue;
            }

            for (const std::string& Key : (*Iterator)->RegistryKeys)
            {
                Registry.UnregisterScript(Key);
            }

            Iterator = Modules.erase(Iterator);
            ReplacingExternalModule = true;
        }

        for (std::size_t Index = 0; Index < Module->Scripts.size(); ++Index)
        {
            const std::string RegistryKey = "dll:" + Module->Name + ":" +
                Module->Scripts[Index].TypeKey; //Module名で名前空間化したScript Key

            if (!Registry.RegisterFactory(
                RegistryKey,
                Module->Scripts[Index].DisplayName,
                Module->Name,
                [RegistryKey, Module, Index]()
                {
                    return std::make_unique<DynamicScript>(
                        RegistryKey,
                        Module,
                        Index
                    );
                }))
            {
                for (const std::string& RegisteredKey : Module->RegistryKeys)
                {
                    Registry.UnregisterScript(RegisteredKey);
                }

                MessageLog::GetInstance().AddLog(
                    "[Error] ScriptModule | Script key registration failed."
                );
                return false;
            }

            Module->RegistryKeys.emplace_back(RegistryKey);
        }

        Modules.emplace_back(std::move(Module));
        MessageLog::GetInstance().AddLog(
            ReplacingExternalModule
                ? "[Info] ScriptModule | External Script module hot reloaded."
                : "[Info] ScriptModule | DLL Script module loaded."
        );
        return true;
    }

    //概要：Moduleの新規Factory登録を解除し利用者がなくなった時点でDLLを解放する
    //引数：path=登録解除するDLL Path
    //戻り値：読み込み済みModuleを解除した場合はtrue
    bool ScriptModuleManager::UnloadModule(const std::wstring& path)
    {
        const std::wstring NormalizedPath = NormalizePath(path); //比較に使う絶対Path
        const auto Iterator = std::find_if(
            Modules.begin(),
            Modules.end(),
            [&NormalizedPath](const std::shared_ptr<ModuleRecord>& module)
            {
                return module != nullptr && module->Path == NormalizedPath;
            }
        ); //Pathに対応するModule検索結果

        if (Iterator == Modules.end())
        {
            return false;
        }

        for (const std::string& Key : (*Iterator)->RegistryKeys)
        {
            Registry.UnregisterScript(Key);
        }

        Modules.erase(Iterator);
        return true;
    }

    //概要：指定DLL Pathが読み込み済みか確認する
    //引数：path=確認するDLL Path
    //戻り値：読み込み済みの場合はtrue
    bool ScriptModuleManager::IsLoaded(const std::wstring& path) const
    {
        const std::wstring NormalizedPath = NormalizePath(path); //比較に使う絶対Path
        return std::any_of(
            Modules.begin(),
            Modules.end(),
            [&NormalizedPath](const std::shared_ptr<ModuleRecord>& module)
            {
                return module != nullptr && module->Path == NormalizedPath;
            }
        );
    }

    //概要：現在読み込み中のDLL Path一覧を取得する
    //引数：なし
    //戻り値：読み込み順の絶対Path一覧
    std::vector<std::wstring> ScriptModuleManager::GetLoadedModulePaths() const
    {
        std::vector<std::wstring> Result; //返却するModule Path一覧
        Result.reserve(Modules.size());

        for (const std::shared_ptr<ModuleRecord>& Module : Modules)
        {
            if (Module != nullptr)
            {
                Result.emplace_back(Module->Path);
            }
        }

        return Result;
    }

    //概要：DLL Pathを絶対Pathへ変換し大文字小文字差をなくす
    //引数：path=正規化するPath
    //戻り値：比較用の小文字絶対Path、失敗時は空文字列
    std::wstring ScriptModuleManager::NormalizePath(const std::wstring& path) const
    {
        if (path.empty())
        {
            return std::wstring();
        }

        const DWORD RequiredLength = GetFullPathNameW(
            path.c_str(),
            0,
            nullptr,
            nullptr
        ); //終端を含む絶対Path文字数

        if (RequiredLength == 0)
        {
            return std::wstring();
        }

        std::wstring Result(RequiredLength, L'\0'); //絶対Path出力Buffer
        const DWORD WrittenLength = GetFullPathNameW(
            path.c_str(),
            RequiredLength,
            Result.data(),
            nullptr
        ); //終端を除く書き込み文字数

        if (WrittenLength == 0 || WrittenLength >= RequiredLength)
        {
            return std::wstring();
        }

        Result.resize(WrittenLength);
        std::transform(
            Result.begin(),
            Result.end(),
            Result.begin(),
            [](wchar_t character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            }
        );
        return Result;
    }
}
