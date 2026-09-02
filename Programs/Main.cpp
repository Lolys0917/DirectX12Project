#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

#include "Main.h"
#include "GameEngineAPI.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "Psapi.lib")

using namespace EngineGame;

namespace Game::Main
{
    namespace
    {
        constexpr std::uint64_t MonitorRefreshMilliseconds = 500;
        constexpr std::size_t HistoryLength = 120;
        constexpr std::size_t ObjectPreviewLimit = 18;
        constexpr std::size_t ObjectTypeCount = 9;

        struct SceneStatus final
        {
            std::string Name;
            std::uint32_t ObjectCount = 0;
            std::uint32_t ActiveObjectCount = 0;
            std::uint32_t ComponentCount = 0;
            bool Active = false;
            bool ViewScene = false;
            bool MainScene = false;
        };

        struct GameStatus final
        {
            std::uint64_t SampleTick = 0;
            std::uint64_t EngineFrame = 0;
            float DeltaTime = 0.0f;
            std::uint32_t SceneCount = 0;
            std::uint32_t ActiveSceneCount = 0;
            std::uint32_t ObjectCount = 0;
            std::uint32_t ActiveObjectCount = 0;
            std::uint32_t ComponentCount = 0;
            std::uint32_t ScriptCount = 0;
            std::array<std::uint32_t, ObjectTypeCount> ObjectCountsByType{};
            std::vector<SceneStatus> Scenes;
            std::vector<EngineExternalObjectInfo> ObjectPreviews;
            bool Valid = false;
        };

        struct PCStatus final
        {
            std::uint64_t SampleTick = 0;
            std::uint64_t PreviousIdleTime = 0;
            std::uint64_t PreviousKernelTime = 0;
            std::uint64_t PreviousUserTime = 0;
            std::uint64_t PreviousProcessKernelTime = 0;
            std::uint64_t PreviousProcessUserTime = 0;
            float SystemCPUPercent = 0.0f;
            float ProcessCPUPercent = 0.0f;
            std::uint64_t TotalPhysicalMemory = 0;
            std::uint64_t AvailablePhysicalMemory = 0;
            std::uint64_t TotalPageFile = 0;
            std::uint64_t AvailablePageFile = 0;
            std::uint64_t WorkingSet = 0;
            std::uint64_t PeakWorkingSet = 0;
            std::uint64_t PrivateUsage = 0;
            std::uint64_t ProcessPageFileUsage = 0;
            std::uint64_t UpTimeMilliseconds = 0;
            std::uint32_t LogicalProcessorCount = 0;
            std::uint32_t HandleCount = 0;
            std::uint32_t ThreadCount = 0;
            std::vector<float> SystemCPUHistory;
            std::vector<float> ProcessCPUHistory;
            std::vector<float> WorkingSetHistoryMB;
            bool Valid = false;
        };

        GameStatus CurrentGameStatus;
        PCStatus CurrentPCStatus;
        std::vector<float> FrameTimeHistory;
        LARGE_INTEGER PerformanceFrequency{};
        LARGE_INTEGER PreviousFrameCounter{};

        std::uint64_t ToInteger(const FILETIME& value)
        {
            ULARGE_INTEGER Result{};
            Result.LowPart = value.dwLowDateTime;
            Result.HighPart = value.dwHighDateTime;
            return Result.QuadPart;
        }

        void AppendHistory(std::vector<float>& history, float value)
        {
            if (history.size() >= HistoryLength)
            {
                history.erase(history.begin());
            }
            history.push_back(value);
        }

        std::string Fixed(float value, int precision = 1)
        {
            std::ostringstream Stream;
            Stream << std::fixed << std::setprecision(precision) << value;
            return Stream.str();
        }

        std::string FormatBytes(std::uint64_t bytes)
        {
            constexpr double Kilobyte = 1024.0;
            constexpr double Megabyte = Kilobyte * 1024.0;
            constexpr double Gigabyte = Megabyte * 1024.0;
            const double Value = static_cast<double>(bytes);
            if (Value >= Gigabyte) return Fixed(static_cast<float>(Value / Gigabyte), 2) + " GB";
            if (Value >= Megabyte) return Fixed(static_cast<float>(Value / Megabyte), 1) + " MB";
            return Fixed(static_cast<float>(Value / Kilobyte), 1) + " KB";
        }

        std::string FormatDuration(std::uint64_t milliseconds)
        {
            const std::uint64_t TotalSeconds = milliseconds / 1000;
            const std::uint64_t Days = TotalSeconds / 86400;
            const std::uint64_t Hours = (TotalSeconds / 3600) % 24;
            const std::uint64_t Minutes = (TotalSeconds / 60) % 60;
            std::ostringstream Stream;
            if (Days > 0) Stream << Days << "d ";
            Stream << Hours << "h " << Minutes << "m";
            return Stream.str();
        }

        const char* GetObjectTypeName(std::uint32_t type)
        {
            constexpr const char* Names[ObjectTypeCount] =
            {
                "Object", "Box", "Sphere", "Plane", "Cylinder",
                "HalfSphere", "Capsule", "SkyBox", "Folder"
            };
            return type < ObjectTypeCount ? Names[type] : "Unknown";
        }

        void UpdateFrameTimeHistory()
        {
            LARGE_INTEGER CurrentCounter{};
            if (PerformanceFrequency.QuadPart == 0)
            {
                QueryPerformanceFrequency(&PerformanceFrequency);
                QueryPerformanceCounter(&PreviousFrameCounter);
                return;
            }
            QueryPerformanceCounter(&CurrentCounter);
            const double Seconds = static_cast<double>(
                CurrentCounter.QuadPart - PreviousFrameCounter.QuadPart
            ) / static_cast<double>(PerformanceFrequency.QuadPart);
            PreviousFrameCounter = CurrentCounter;
            AppendHistory(FrameTimeHistory, static_cast<float>(
                (std::min)(Seconds * 1000.0, 250.0)
            ));
        }

        void UpdateGameStatus(bool force = false)
        {
            const std::uint64_t Now = GetTickCount64();
            if (!force && CurrentGameStatus.Valid &&
                Now - CurrentGameStatus.SampleTick < MonitorRefreshMilliseconds)
            {
                return;
            }

            const EngineHostAPI* Host = Advanced.Host();
            if (Host == nullptr || Host->GetSceneCount == nullptr ||
                Host->GetSceneInfo == nullptr || Host->GetObjectCount == nullptr ||
                Host->GetObjectInfo == nullptr)
            {
                CurrentGameStatus.Valid = false;
                return;
            }

            GameStatus Fresh;
            Fresh.SampleTick = Now;
            Fresh.EngineFrame = Host->GetFrameNumber == nullptr
                ? FrameCount : Host->GetFrameNumber(Host->Context);
            Fresh.DeltaTime = Host->GetDeltaTime == nullptr
                ? 0.0f : Host->GetDeltaTime(Host->Context);
            Fresh.SceneCount = Host->GetSceneCount(Host->Context);
            Fresh.Scenes.reserve(Fresh.SceneCount);

            for (std::uint32_t SceneIndex = 0; SceneIndex < Fresh.SceneCount; ++SceneIndex)
            {
                EngineExternalSceneInfo SceneInformation{};
                SceneInformation.Size = sizeof(SceneInformation);
                if (!Host->GetSceneInfo(Host->Context, SceneIndex, &SceneInformation)) continue;

                SceneStatus SceneSnapshot;
                SceneSnapshot.Name = SceneInformation.Name;
                SceneSnapshot.Active = SceneInformation.Active;
                SceneSnapshot.ViewScene = SceneInformation.ViewScene;
                SceneSnapshot.MainScene = SceneInformation.MainScene;
                SceneSnapshot.ObjectCount = Host->GetObjectCount(
                    Host->Context, SceneInformation.SceneID
                );
                Fresh.ActiveSceneCount += SceneSnapshot.Active ? 1u : 0u;

                for (std::uint32_t ObjectIndex = 0;
                    ObjectIndex < SceneSnapshot.ObjectCount; ++ObjectIndex)
                {
                    EngineExternalObjectInfo ObjectInformation{};
                    ObjectInformation.Size = sizeof(ObjectInformation);
                    if (!Host->GetObjectInfo(
                        Host->Context, SceneInformation.SceneID, ObjectIndex, &ObjectInformation
                    )) continue;

                    ++Fresh.ObjectCount;
                    Fresh.ActiveObjectCount += ObjectInformation.Active ? 1u : 0u;
                    SceneSnapshot.ActiveObjectCount += ObjectInformation.Active ? 1u : 0u;
                    Fresh.ComponentCount += ObjectInformation.ComponentCount;
                    SceneSnapshot.ComponentCount += ObjectInformation.ComponentCount;
                    if (ObjectInformation.ObjectType < ObjectTypeCount)
                    {
                        ++Fresh.ObjectCountsByType[ObjectInformation.ObjectType];
                    }
                    if (Fresh.ObjectPreviews.size() < ObjectPreviewLimit)
                    {
                        Fresh.ObjectPreviews.push_back(ObjectInformation);
                    }

                    if (ObjectInformation.ComponentCount == 0 ||
                        Host->GetComponentInfo == nullptr) continue;

                    for (std::uint32_t ComponentIndex = 0;
                        ComponentIndex < ObjectInformation.ComponentCount; ++ComponentIndex)
                    {
                        EngineExternalComponentInfo ComponentInformation{};
                        ComponentInformation.Size = sizeof(ComponentInformation);
                        if (Host->GetComponentInfo(
                            Host->Context, SceneInformation.SceneID,
                            ObjectInformation.ObjectID, ComponentIndex, &ComponentInformation
                        ) && ComponentInformation.ComponentType ==
                            static_cast<std::uint32_t>(EngineExternalComponentType::Script))
                        {
                            ++Fresh.ScriptCount;
                        }
                    }
                }
                Fresh.Scenes.emplace_back(std::move(SceneSnapshot));
            }
            Fresh.Valid = true;
            CurrentGameStatus = std::move(Fresh);
        }

        std::uint32_t CountProcessThreads()
        {
            const DWORD ProcessID = GetCurrentProcessId();
            HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (Snapshot == INVALID_HANDLE_VALUE) return 0;
            std::uint32_t Count = 0;
            THREADENTRY32 Entry{};
            Entry.dwSize = sizeof(Entry);
            if (Thread32First(Snapshot, &Entry))
            {
                do
                {
                    Count += Entry.th32OwnerProcessID == ProcessID ? 1u : 0u;
                } while (Thread32Next(Snapshot, &Entry));
            }
            CloseHandle(Snapshot);
            return Count;
        }

        void UpdatePCStatus(bool force = false)
        {
            const std::uint64_t Now = GetTickCount64();
            if (!force && CurrentPCStatus.Valid &&
                Now - CurrentPCStatus.SampleTick < MonitorRefreshMilliseconds)
            {
                return;
            }

            FILETIME IdleTime{}, KernelTime{}, UserTime{};
            FILETIME CreationTime{}, ExitTime{}, ProcessKernelTime{}, ProcessUserTime{};
            const bool HasSystemTimes = GetSystemTimes(&IdleTime, &KernelTime, &UserTime) != FALSE;
            const bool HasProcessTimes = GetProcessTimes(
                GetCurrentProcess(), &CreationTime, &ExitTime,
                &ProcessKernelTime, &ProcessUserTime
            ) != FALSE;
            if (HasSystemTimes && HasProcessTimes)
            {
                const std::uint64_t Idle = ToInteger(IdleTime);
                const std::uint64_t Kernel = ToInteger(KernelTime);
                const std::uint64_t User = ToInteger(UserTime);
                const std::uint64_t ProcessKernel = ToInteger(ProcessKernelTime);
                const std::uint64_t ProcessUser = ToInteger(ProcessUserTime);
                const std::uint64_t TotalDelta =
                    Kernel - CurrentPCStatus.PreviousKernelTime +
                    User - CurrentPCStatus.PreviousUserTime;
                const std::uint64_t IdleDelta = Idle - CurrentPCStatus.PreviousIdleTime;
                const std::uint64_t ProcessDelta =
                    ProcessKernel - CurrentPCStatus.PreviousProcessKernelTime +
                    ProcessUser - CurrentPCStatus.PreviousProcessUserTime;
                if (CurrentPCStatus.PreviousKernelTime != 0 && TotalDelta > 0)
                {
                    CurrentPCStatus.SystemCPUPercent = (std::clamp)(
                        100.0f * static_cast<float>(TotalDelta - (std::min)(IdleDelta, TotalDelta)) /
                            static_cast<float>(TotalDelta), 0.0f, 100.0f
                    );
                    CurrentPCStatus.ProcessCPUPercent = (std::clamp)(
                        100.0f * static_cast<float>(ProcessDelta) /
                            static_cast<float>(TotalDelta), 0.0f, 100.0f
                    );
                }
                CurrentPCStatus.PreviousIdleTime = Idle;
                CurrentPCStatus.PreviousKernelTime = Kernel;
                CurrentPCStatus.PreviousUserTime = User;
                CurrentPCStatus.PreviousProcessKernelTime = ProcessKernel;
                CurrentPCStatus.PreviousProcessUserTime = ProcessUser;
            }

            MEMORYSTATUSEX MemoryStatus{};
            MemoryStatus.dwLength = sizeof(MemoryStatus);
            if (GlobalMemoryStatusEx(&MemoryStatus))
            {
                CurrentPCStatus.TotalPhysicalMemory = MemoryStatus.ullTotalPhys;
                CurrentPCStatus.AvailablePhysicalMemory = MemoryStatus.ullAvailPhys;
                CurrentPCStatus.TotalPageFile = MemoryStatus.ullTotalPageFile;
                CurrentPCStatus.AvailablePageFile = MemoryStatus.ullAvailPageFile;
            }

            PROCESS_MEMORY_COUNTERS_EX ProcessMemory{};
            ProcessMemory.cb = sizeof(ProcessMemory);
            if (GetProcessMemoryInfo(
                GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&ProcessMemory),
                sizeof(ProcessMemory)
            ))
            {
                CurrentPCStatus.WorkingSet = ProcessMemory.WorkingSetSize;
                CurrentPCStatus.PeakWorkingSet = ProcessMemory.PeakWorkingSetSize;
                CurrentPCStatus.PrivateUsage = ProcessMemory.PrivateUsage;
                CurrentPCStatus.ProcessPageFileUsage = ProcessMemory.PagefileUsage;
            }

            DWORD Handles = 0;
            GetProcessHandleCount(GetCurrentProcess(), &Handles);
            CurrentPCStatus.HandleCount = Handles;
            CurrentPCStatus.ThreadCount = CountProcessThreads();
            CurrentPCStatus.LogicalProcessorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
            CurrentPCStatus.UpTimeMilliseconds = Now;
            CurrentPCStatus.SampleTick = Now;
            CurrentPCStatus.Valid = true;
            AppendHistory(CurrentPCStatus.SystemCPUHistory, CurrentPCStatus.SystemCPUPercent);
            AppendHistory(CurrentPCStatus.ProcessCPUHistory, CurrentPCStatus.ProcessCPUPercent);
            AppendHistory(CurrentPCStatus.WorkingSetHistoryMB,
                static_cast<float>(CurrentPCStatus.WorkingSet / (1024.0 * 1024.0)));
        }

        void DrawGameStateTab()
        {
            if (ImGui.Button("Refresh game snapshot")) UpdateGameStatus(true);
            if (!CurrentGameStatus.Valid)
            {
                ImGui.Text("Game state is not available.");
                return;
            }

            const float SimulationFPS = CurrentGameStatus.DeltaTime > 0.0f
                ? 1.0f / CurrentGameStatus.DeltaTime : 0.0f;
            ImGui.Text("Simulation frame: " + std::to_string(CurrentGameStatus.EngineFrame) +
                " | Main elapsed: " + Fixed(ElapsedTime, 1) + " s");
            ImGui.Text("Last simulation dt: " + Fixed(CurrentGameStatus.DeltaTime * 1000.0f, 2) +
                " ms | " + Fixed(SimulationFPS, 1) + " FPS");

            if (ImGui.CollapsingHeader("World overview", true))
            {
                ImGui.Text("Scenes: " + std::to_string(CurrentGameStatus.SceneCount) +
                    " | Active: " + std::to_string(CurrentGameStatus.ActiveSceneCount));
                ImGui.Text("Objects: " + std::to_string(CurrentGameStatus.ObjectCount) +
                    " | Active: " + std::to_string(CurrentGameStatus.ActiveObjectCount) +
                    " | Inactive: " + std::to_string(
                        CurrentGameStatus.ObjectCount - CurrentGameStatus.ActiveObjectCount));
                ImGui.Text("Components: " + std::to_string(CurrentGameStatus.ComponentCount) +
                    " | Scripts: " + std::to_string(CurrentGameStatus.ScriptCount));
            }

            if (ImGui.CollapsingHeader("Frame timing", true))
            {
                const float AverageFrameTime = FrameTimeHistory.empty() ? 0.0f :
                    std::accumulate(FrameTimeHistory.begin(), FrameTimeHistory.end(), 0.0f) /
                        static_cast<float>(FrameTimeHistory.size());
                ImGui.Text("UI/render loop average: " + Fixed(AverageFrameTime, 2) + " ms");
                const float Maximum = FrameTimeHistory.empty() ? 33.0f : (std::max)(
                    33.0f, *std::max_element(FrameTimeHistory.begin(), FrameTimeHistory.end()));
                ImGui.PlotLines("Frame time (ms)", FrameTimeHistory, 0.0f, Maximum);
            }

            if (ImGui.CollapsingHeader("Scene breakdown", true))
            {
                for (const SceneStatus& Scene : CurrentGameStatus.Scenes)
                {
                    std::string Flags;
                    if (Scene.MainScene) Flags += " Main";
                    if (Scene.ViewScene) Flags += " View";
                    if (!Scene.Active) Flags += " Inactive";
                    ImGui.Text(Scene.Name + " [" + Flags + " ] | Objects " +
                        std::to_string(Scene.ObjectCount) + " (active " +
                        std::to_string(Scene.ActiveObjectCount) + ") | Components " +
                        std::to_string(Scene.ComponentCount));
                }
            }

            if (ImGui.CollapsingHeader("Object types", false))
            {
                for (std::uint32_t Type = 0; Type < ObjectTypeCount; ++Type)
                {
                    if (CurrentGameStatus.ObjectCountsByType[Type] > 0)
                    {
                        ImGui.Text(std::string(GetObjectTypeName(Type)) + ": " +
                            std::to_string(CurrentGameStatus.ObjectCountsByType[Type]));
                    }
                }
            }

            if (ImGui.CollapsingHeader("Object state sample", false))
            {
                ImGui.Text("Showing " + std::to_string(CurrentGameStatus.ObjectPreviews.size()) +
                    " / " + std::to_string(CurrentGameStatus.ObjectCount) + " objects");
                ImGui.Separator();
                for (const EngineExternalObjectInfo& Object : CurrentGameStatus.ObjectPreviews)
                {
                    const EngineExternalVector3& Position = Object.LocalTransform.Position;
                    const EngineExternalVector3& Rotation = Object.LocalTransform.Rotation;
                    const EngineExternalVector3& Scale = Object.LocalTransform.Scale;
                    ImGui.Text(std::string(Object.Active ? "[A] " : "[-] ") + Object.Name +
                        " <" + GetObjectTypeName(Object.ObjectType) + "> | Components " +
                        std::to_string(Object.ComponentCount) +
                        (Object.ParentObjectID == 0 ? std::string() :
                            " | Parent #" + std::to_string(Object.ParentObjectID)));
                    ImGui.Text("  Pos (" +
                        Fixed(Position.X, 2) + ", " + Fixed(Position.Y, 2) + ", " +
                        Fixed(Position.Z, 2) + ") | Rot (" +
                        Fixed(Rotation.X, 2) + ", " + Fixed(Rotation.Y, 2) + ", " +
                        Fixed(Rotation.Z, 2) + ") | Scale (" +
                        Fixed(Scale.X, 2) + ", " + Fixed(Scale.Y, 2) + ", " +
                        Fixed(Scale.Z, 2) + ")");
                    ImGui.Text("  Group: " + std::string(Object.Group) + " | Tag: " +
                        Object.Tag + " | Layer: " + std::to_string(Object.Layer));
                }
            }
        }

        void DrawPCStateTab()
        {
            if (ImGui.Button("Refresh PC snapshot")) UpdatePCStatus(true);
            if (!CurrentPCStatus.Valid)
            {
                ImGui.Text("PC state is not available.");
                return;
            }

            if (ImGui.CollapsingHeader("System load", true))
            {
                ImGui.Text("Logical processors: " +
                    std::to_string(CurrentPCStatus.LogicalProcessorCount) +
                    " | PC uptime: " + FormatDuration(CurrentPCStatus.UpTimeMilliseconds));
                ImGui.ProgressBar(CurrentPCStatus.SystemCPUPercent / 100.0f,
                    "CPU " + Fixed(CurrentPCStatus.SystemCPUPercent, 1) + "%");
                ImGui.PlotLines("System CPU history (%)",
                    CurrentPCStatus.SystemCPUHistory, 0.0f, 100.0f);

                const std::uint64_t UsedPhysical = CurrentPCStatus.TotalPhysicalMemory -
                    (std::min)(CurrentPCStatus.AvailablePhysicalMemory,
                        CurrentPCStatus.TotalPhysicalMemory);
                const float MemoryFraction = CurrentPCStatus.TotalPhysicalMemory == 0 ? 0.0f :
                    static_cast<float>(UsedPhysical) /
                        static_cast<float>(CurrentPCStatus.TotalPhysicalMemory);
                ImGui.Text("Physical memory: " + FormatBytes(UsedPhysical) + " / " +
                    FormatBytes(CurrentPCStatus.TotalPhysicalMemory) + " | Available " +
                    FormatBytes(CurrentPCStatus.AvailablePhysicalMemory));
                ImGui.ProgressBar(MemoryFraction,
                    "RAM " + Fixed(MemoryFraction * 100.0f, 1) + "%");
                ImGui.Text("Commit/page file: " + FormatBytes(
                    CurrentPCStatus.TotalPageFile - (std::min)(
                        CurrentPCStatus.AvailablePageFile, CurrentPCStatus.TotalPageFile)) +
                    " / " + FormatBytes(CurrentPCStatus.TotalPageFile));
            }

            if (ImGui.CollapsingHeader("Engine process", true))
            {
                ImGui.ProgressBar(CurrentPCStatus.ProcessCPUPercent / 100.0f,
                    "Process CPU " + Fixed(CurrentPCStatus.ProcessCPUPercent, 1) + "%");
                ImGui.PlotLines("Process CPU history (%)",
                    CurrentPCStatus.ProcessCPUHistory, 0.0f, 100.0f);
                ImGui.Text("Working set: " + FormatBytes(CurrentPCStatus.WorkingSet));
                ImGui.Text("Private memory: " + FormatBytes(CurrentPCStatus.PrivateUsage));
                ImGui.Text("Peak working set: " + FormatBytes(CurrentPCStatus.PeakWorkingSet));
                ImGui.Text("Process page file: " +
                    FormatBytes(CurrentPCStatus.ProcessPageFileUsage));
                const float WorkingSetMaximum = CurrentPCStatus.WorkingSetHistoryMB.empty()
                    ? 64.0f : (std::max)(64.0f, *std::max_element(
                        CurrentPCStatus.WorkingSetHistoryMB.begin(),
                        CurrentPCStatus.WorkingSetHistoryMB.end()) * 1.15f);
                ImGui.PlotLines("Working set history (MB)",
                    CurrentPCStatus.WorkingSetHistoryMB, 0.0f, WorkingSetMaximum);
                ImGui.Text("Threads: " + std::to_string(CurrentPCStatus.ThreadCount) +
                    " | Handles: " + std::to_string(CurrentPCStatus.HandleCount) +
                    " | PID: " + std::to_string(GetCurrentProcessId()));
            }

            if (ImGui.CollapsingHeader("Monitor behavior", false))
            {
                ImGui.Text("World and PC snapshots refresh every 0.5 seconds.");
                ImGui.Text("The monitor remains active while gameplay is stopped or paused.");
                ImGui.Text("CPU values are machine-wide and this process's total machine share.");
            }
        }
    }

    std::uint64_t FrameCount = 0;
    float ElapsedTime = 0.0f;

    void Init()
    {
        FrameCount = 0;
        ElapsedTime = 0.0f;
        CurrentGameStatus = {};
        CurrentPCStatus = {};
        FrameTimeHistory.clear();
        PerformanceFrequency = {};
        PreviousFrameCounter = {};
        Log("[Info] MainProgram | Common Main initialized.");
        InitializeScenes();
    }

    void Update(float deltaTime)
    {
        ++FrameCount;
        ElapsedTime += deltaTime;
        RunScenes(deltaTime);
    }

    void End()
    {
        CurrentGameStatus = {};
        CurrentPCStatus = {};
        FrameTimeHistory.clear();
        Log("[Info] MainProgram | Common Main ended.");
    }

    void UserInterface()
    {
        UpdateFrameTimeHistory();
        UpdateGameStatus();
        UpdatePCStatus();

        const bool Visible = ImGui.Begin("Runtime Status Monitor");
        if (Visible && ImGui.BeginTabBar("RuntimeStatusTabs"))
        {
            if (ImGui.BeginTabItem("Game State"))
            {
                DrawGameStateTab();
                ImGui.EndTabItem();
            }
            if (ImGui.BeginTabItem("PC State"))
            {
                DrawPCStateTab();
                ImGui.EndTabItem();
            }
            ImGui.EndTabBar();
        }
        ImGui.End();
    }
}

ENGINE_REGISTER_MAIN(Main)
