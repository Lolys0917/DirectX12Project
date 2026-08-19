//|| ProgramWorkspace.h ||:::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Editor内Program、自動保存、Background Compile、世代別DLLを管理する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace Engine
{
    enum class ProgramWorkspaceKind : std::uint8_t
    {
        MainProgram,
        ObjectScript
    };

    struct ProgramCompileResult final
    {
        bool Started = false;
        bool Succeeded = false;
        bool PreflightSucceeded = false;
        bool Automatic = false;
        unsigned long ExitCode = 0;
        std::uint64_t Revision = 0;
        std::wstring Output;
        std::filesystem::path ModulePath;
    };

    struct ProgramPreflightResult final
    {
        bool Ready = false;
        std::size_t CharacterIndex = 0;
        int Line = 1;
        std::wstring Message;
    };

    struct ProgramSaveResult final
    {
        bool Succeeded = false;
        std::uint64_t Revision = 0;
        std::filesystem::path SourcePath;
    };

    class ProgramWorkspace final
    {
    public:
        explicit ProgramWorkspace(
            ProgramWorkspaceKind kind = ProgramWorkspaceKind::MainProgram
        );
        ~ProgramWorkspace();

        ProgramWorkspace(const ProgramWorkspace&) = delete;
        ProgramWorkspace& operator=(const ProgramWorkspace&) = delete;

        bool Initialize();
        ProgramWorkspaceKind GetKind() const;
        const std::filesystem::path& GetDirectory() const;
        std::vector<std::filesystem::path> GetSourceFiles() const;
        bool CreateSourceFile(std::filesystem::path& createdPath);
        bool EnsureSceneSource(
            const std::string& sceneName,
            std::filesystem::path& sourcePath
        );
        bool RenameSourceFile(
            const std::filesystem::path& sourcePath,
            const std::wstring& requestedName,
            std::filesystem::path& renamedPath
        );
        bool DeleteSourceFile(const std::filesystem::path& sourcePath);
        bool LoadSourceFile(
            const std::filesystem::path& sourcePath,
            std::wstring& text
        ) const;
        bool SaveSourceFile(
            const std::filesystem::path& sourcePath,
            const std::wstring& text
        ) const;
        bool StartBackgroundSave(
            const std::filesystem::path& sourcePath,
            std::wstring text,
            std::uint64_t revision
        );
        bool PollBackgroundSave(ProgramSaveResult& result);
        bool IsSaving() const;
        void WaitForBackgroundSave();
        bool EnsureDefaultSource(std::filesystem::path& sourcePath);
        ProgramPreflightResult AnalyzeCompileReadiness(const std::wstring& text) const;
        ProgramCompileResult Compile(
            std::uint64_t revision = 0,
            bool automatic = false
        ) const;
        bool StartBackgroundCompile(std::uint64_t revision, bool automatic);
        bool PollBackgroundCompile(ProgramCompileResult& result);
        bool IsCompiling() const;
        bool HasLastSuccessfulSnapshot() const;
        bool RestoreLastSuccessfulSnapshot(
            std::filesystem::path& recoveryDirectory
        );
        void Shutdown();

    private:
        void RunCompileWorker(std::uint64_t revision, bool automatic);
        void RunSaveWorker(
            std::filesystem::path sourcePath,
            std::wstring text,
            std::uint64_t revision
        );
        bool DiscoverProjectRoot();
        bool WriteBuildProject() const;
        bool EnsureScriptTemplate(std::filesystem::path& sourcePath);
        bool CaptureLastSuccessfulSnapshot() const;
        bool IsManagedSourcePath(const std::filesystem::path& path) const;
        std::filesystem::path FindMSBuild() const;

        std::filesystem::path ProjectRoot;
        std::filesystem::path ProgramDirectory;
        std::filesystem::path BuildProjectPath;
        ProgramWorkspaceKind Kind;
        std::thread CompileWorker;
        mutable std::mutex CompileResultMutex;
        std::optional<ProgramCompileResult> CompletedCompileResult;
        std::atomic_bool Compiling;
        std::thread SaveWorker;
        mutable std::mutex SaveResultMutex;
        std::optional<ProgramSaveResult> CompletedSaveResult;
        std::atomic_bool Saving;
    };
}
