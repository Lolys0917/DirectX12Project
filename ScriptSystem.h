//|| ScriptSystem.h ||:::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Native及びDLL ScriptのFactory登録、生成、Module寿命管理を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "EditorTypes.h"
#include "Script.h"

namespace Engine
{
    class ScriptRegistry final
    {
    public:
        using Factory = std::function<std::unique_ptr<Script>()>;

        ScriptRegistry();
        ~ScriptRegistry();

        ScriptRegistry(const ScriptRegistry&) = delete;
        ScriptRegistry& operator=(const ScriptRegistry&) = delete;

        //概要：Native Script派生型のFactoryをRegistryへ登録する
        //引数：key=一意識別子、displayName=Editor表示名、arguments=Script構築引数
        //戻り値：Factoryを登録できた場合はtrue
        template<class ScriptClass, class... ArgumentTypes>
        bool RegisterNativeScript(
            const std::string& key,
            const std::string& displayName,
            ArgumentTypes&&... arguments
        )
        {
            static_assert(std::is_base_of_v<Script, ScriptClass>);

            auto Arguments = std::make_tuple(
                std::forward<ArgumentTypes>(arguments)...
            ); //Factoryごとに保持するNative Script構築引数

            return RegisterFactory(
                key,
                displayName,
                "Native",
                [Arguments = std::move(Arguments)]() mutable
                {
                    return std::apply(
                        [](auto&&... values)
                        {
                            return std::make_unique<ScriptClass>(values...);
                        },
                        Arguments
                    );
                }
            );
        }

        bool RegisterFactory(
            const std::string& key,
            const std::string& displayName,
            const std::string& moduleName,
            Factory factory
        );

        bool UnregisterScript(const std::string& key);
        std::unique_ptr<Script> CreateScript(const std::string& key) const;
        std::vector<EditorScriptInfo> GetCatalog() const;
        bool Contains(const std::string& key) const;

    private:
        struct Entry final
        {
            std::string Key; //Registry内の一意識別子
            std::string DisplayName; //Editor表示名
            std::string ModuleName; //Native又はDLL Module名
            Factory Create; //未登録Scriptを生成するFactory
        };

        std::unordered_map<std::string, Entry> Entries; //KeyからFactoryへの索引
        std::vector<std::string> RegistrationOrder; //UI表示用の安定登録順
    };

    class ScriptModuleManager final
    {
    public:
        struct ModuleRecord;

        explicit ScriptModuleManager(ScriptRegistry& registry);
        ~ScriptModuleManager();

        ScriptModuleManager(const ScriptModuleManager&) = delete;
        ScriptModuleManager& operator=(const ScriptModuleManager&) = delete;

        bool LoadModule(const std::wstring& path);
        bool UnloadModule(const std::wstring& path);
        bool IsLoaded(const std::wstring& path) const;
        std::vector<std::wstring> GetLoadedModulePaths() const;

    private:
        std::wstring NormalizePath(const std::wstring& path) const;

        ScriptRegistry& Registry; //DLL Script Factoryの登録先
        std::vector<std::shared_ptr<ModuleRecord>> Modules; //読み込み中Module一覧
    };
}
