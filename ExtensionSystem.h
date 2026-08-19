//|| ExtensionSystem.h ||:::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Program DLLの検証、状態移行、Hot Reload、毎Frame実行を管理する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_18  v1.00  新規作成
//||

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

#include "EngineExtensionAPI.h"

namespace Engine
{
    class EngineAPI;

    class ExtensionModuleManager final
    {
    public:
        explicit ExtensionModuleManager(EngineAPI& engine);
        ~ExtensionModuleManager();

        ExtensionModuleManager(const ExtensionModuleManager&) = delete;
        ExtensionModuleManager& operator=(const ExtensionModuleManager&) = delete;

        bool LoadOrReload(const std::filesystem::path& modulePath);
        void Unload();
        void DestroyInstance();
        bool CreateInstance();
        void Update(float deltaTime);
        bool IsLoaded() const;
        std::uint64_t GetGeneration() const;
        std::uint64_t GetFrameNumber() const;
        float GetDeltaTime() const;
        const std::string& GetModuleName() const;
        const std::filesystem::path& GetModulePath() const;

    private:
        struct LoadedModule final
        {
            HMODULE Handle = nullptr;
            const EngineExtensionModuleDescriptor* Descriptor = nullptr;
            void* Instance = nullptr;
            std::filesystem::path Path;
            std::string Name;
        };

        void ReleaseModule(LoadedModule& module);

        EngineAPI& Engine; //外部C APIを接続するNative Facade
        EngineHostAPI Host; //DLLへ渡す版番号付き関数表
        LoadedModule ActiveModule; //現在毎Frame実行するModule
        std::uint64_t Generation; //Hot Reload成功ごとに増える世代
        std::uint64_t FrameNumber; //Extension Updateの通算Frame番号
        float DeltaTime; //現在Extension Updateへ渡している秒数
    };
}
