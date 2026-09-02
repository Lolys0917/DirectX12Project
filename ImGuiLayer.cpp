//|| ImGuiLayer.cpp ||:::::::::::::::::::::::::
//||
//||  Dear ImGui v1.92のDirectX 12描画とViewport入力をEngineへ接続する

#include "ImGuiLayer.h"

#include <algorithm>
#include <cfloat>

#include "DirectX12.h"
#include "MessageLog.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"

namespace Engine
{
    namespace
    {
        constexpr std::uint32_t ImGuiDescriptorCount = 64;
    }

    ImGuiLayer::ImGuiLayer()
        : DescriptorHeap()
        , FreeDescriptorIndices()
        , RenderWindow(nullptr)
        , DescriptorIncrement(0)
        , Initialized(false)
        , FrameActive(false)
    {
    }

    ImGuiLayer::~ImGuiLayer()
    {
        Finalize();
    }

    bool ImGuiLayer::Initialize(DirectX12& dx12, HWND renderWindow)
    {
        if (Initialized)
        {
            return true;
        }

        ID3D12Device* Device = dx12.GetDevice();
        if (Device == nullptr || dx12.GetCommandQueue() == nullptr || renderWindow == nullptr)
        {
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC HeapDescription{};
        HeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        HeapDescription.NumDescriptors = ImGuiDescriptorCount;
        HeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(Device->CreateDescriptorHeap(
            &HeapDescription,
            IID_PPV_ARGS(&DescriptorHeap))))
        {
            return false;
        }

        DescriptorIncrement = Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );
        FreeDescriptorIndices.reserve(ImGuiDescriptorCount);
        for (std::uint32_t Index = ImGuiDescriptorCount; Index > 0; --Index)
        {
            FreeDescriptorIndices.push_back(Index - 1);
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& InputOutput = ImGui::GetIO();
        InputOutput.IniFilename = nullptr;
        InputOutput.LogFilename = nullptr;
        InputOutput.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        ImGui_ImplDX12_InitInfo Initialization{};
        Initialization.Device = Device;
        Initialization.CommandQueue = dx12.GetCommandQueue();
        Initialization.NumFramesInFlight = DirectX12::BackBufferCount;
        Initialization.RTVFormat = dx12.GetBackBufferFormat();
        Initialization.DSVFormat = dx12.GetDepthStencilFormat();
        Initialization.UserData = this;
        Initialization.SrvDescriptorHeap = DescriptorHeap.Get();
        Initialization.SrvDescriptorAllocFn = AllocateDescriptor;
        Initialization.SrvDescriptorFreeFn = FreeDescriptor;
        if (!ImGui_ImplDX12_Init(&Initialization))
        {
            ImGui::DestroyContext();
            DescriptorHeap.Reset();
            FreeDescriptorIndices.clear();
            DescriptorIncrement = 0;
            return false;
        }

        RenderWindow = renderWindow;
        Initialized = true;
        return true;
    }

    void ImGuiLayer::Finalize()
    {
        if (!Initialized)
        {
            return;
        }

        CancelFrame();
        ImGui_ImplDX12_Shutdown();
        ImGui::DestroyContext();
        DescriptorHeap.Reset();
        FreeDescriptorIndices.clear();
        RenderWindow = nullptr;
        DescriptorIncrement = 0;
        Initialized = false;
    }

    bool ImGuiLayer::BeginFrame(float deltaTime)
    {
        if (!Initialized || FrameActive)
        {
            return false;
        }

        ImGuiIO& InputOutput = ImGui::GetIO();
        RECT ClientRectangle{};
        GetClientRect(RenderWindow, &ClientRectangle);
        InputOutput.DisplaySize = ImVec2(
            static_cast<float>(std::max(1L, ClientRectangle.right - ClientRectangle.left)),
            static_cast<float>(std::max(1L, ClientRectangle.bottom - ClientRectangle.top))
        );
        InputOutput.DeltaTime = std::clamp(deltaTime, 1.0f / 1000.0f, 0.1f);
        UpdateInput();
        ImGui_ImplDX12_NewFrame();
        ImGui::NewFrame();
        FrameActive = true;
        return true;
    }

    void ImGuiLayer::Render(DirectX12& dx12)
    {
        if (!FrameActive)
        {
            return;
        }

        ImGui::Render();
        FrameActive = false;
        if (!dx12.IsFrameOpen() || !dx12.BindBackBuffer())
        {
            return;
        }

        ID3D12DescriptorHeap* Heaps[] = { DescriptorHeap.Get() };
        dx12.GetCommandList()->SetDescriptorHeaps(1, Heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx12.GetCommandList());
    }

    void ImGuiLayer::CancelFrame()
    {
        if (FrameActive)
        {
            ImGui::EndFrame();
            FrameActive = false;
        }
    }

    bool ImGuiLayer::BeginWindow(const char* name)
    {
        if (!FrameActive || name == nullptr || name[0] == '\0')
        {
            return false;
        }

        ImGui::SetNextWindowSize(ImVec2(620.0f, 640.0f), ImGuiCond_FirstUseEver);
        return ImGui::Begin(name);
    }

    void ImGuiLayer::EndWindow()
    {
        if (FrameActive)
        {
            ImGui::End();
        }
    }

    void ImGuiLayer::Text(const char* text)
    {
        if (FrameActive && text != nullptr)
        {
            ImGui::TextUnformatted(text);
        }
    }

    bool ImGuiLayer::Button(const char* label)
    {
        return FrameActive && label != nullptr && label[0] != '\0' && ImGui::Button(label);
    }

    bool ImGuiLayer::BeginTabBar(const char* identifier)
    {
        return FrameActive && identifier != nullptr && identifier[0] != '\0' &&
            ImGui::BeginTabBar(identifier);
    }

    void ImGuiLayer::EndTabBar()
    {
        if (FrameActive)
        {
            ImGui::EndTabBar();
        }
    }

    bool ImGuiLayer::BeginTabItem(const char* label)
    {
        return FrameActive && label != nullptr && label[0] != '\0' &&
            ImGui::BeginTabItem(label);
    }

    void ImGuiLayer::EndTabItem()
    {
        if (FrameActive)
        {
            ImGui::EndTabItem();
        }
    }

    bool ImGuiLayer::CollapsingHeader(const char* label, bool defaultOpen)
    {
        return FrameActive && label != nullptr && label[0] != '\0' &&
            ImGui::CollapsingHeader(
                label,
                defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None
            );
    }

    void ImGuiLayer::Separator()
    {
        if (FrameActive)
        {
            ImGui::Separator();
        }
    }

    void ImGuiLayer::ProgressBar(float fraction, const char* overlay)
    {
        if (FrameActive)
        {
            ImGui::ProgressBar(
                std::clamp(fraction, 0.0f, 1.0f),
                ImVec2(-FLT_MIN, 0.0f),
                overlay
            );
        }
    }

    void ImGuiLayer::PlotLines(
        const char* label,
        const float* values,
        std::uint32_t valueCount,
        float minimum,
        float maximum
    )
    {
        if (!FrameActive || label == nullptr || values == nullptr || valueCount == 0)
        {
            return;
        }

        ImGui::PlotLines(
            label,
            values,
            static_cast<int>(valueCount),
            0,
            nullptr,
            minimum,
            maximum,
            ImVec2(0.0f, 80.0f)
        );
    }

    void ImGuiLayer::AllocateDescriptor(
        ImGui_ImplDX12_InitInfo* information,
        D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle
    )
    {
        auto* Layer = information == nullptr
            ? nullptr
            : static_cast<ImGuiLayer*>(information->UserData);
        if (Layer == nullptr || Layer->FreeDescriptorIndices.empty() ||
            cpuHandle == nullptr || gpuHandle == nullptr)
        {
            if (cpuHandle != nullptr) cpuHandle->ptr = 0;
            if (gpuHandle != nullptr) gpuHandle->ptr = 0;
            return;
        }

        const std::uint32_t Index = Layer->FreeDescriptorIndices.back();
        Layer->FreeDescriptorIndices.pop_back();
        *cpuHandle = Layer->DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        *gpuHandle = Layer->DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        cpuHandle->ptr += static_cast<SIZE_T>(Index) * Layer->DescriptorIncrement;
        gpuHandle->ptr += static_cast<UINT64>(Index) * Layer->DescriptorIncrement;
    }

    void ImGuiLayer::FreeDescriptor(
        ImGui_ImplDX12_InitInfo* information,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
    )
    {
        (void)gpuHandle;
        auto* Layer = information == nullptr
            ? nullptr
            : static_cast<ImGuiLayer*>(information->UserData);
        if (Layer == nullptr || Layer->DescriptorIncrement == 0 ||
            Layer->DescriptorHeap == nullptr)
        {
            return;
        }

        const SIZE_T Start = Layer->DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
        if (cpuHandle.ptr < Start)
        {
            return;
        }
        const std::uint32_t Index = static_cast<std::uint32_t>(
            (cpuHandle.ptr - Start) / Layer->DescriptorIncrement
        );
        if (Index < ImGuiDescriptorCount)
        {
            Layer->FreeDescriptorIndices.push_back(Index);
        }
    }

    void ImGuiLayer::UpdateInput()
    {
        ImGuiIO& InputOutput = ImGui::GetIO();
        POINT Cursor{};
        if (GetCursorPos(&Cursor) && ScreenToClient(RenderWindow, &Cursor))
        {
            RECT Client{};
            GetClientRect(RenderWindow, &Client);
            if (PtInRect(&Client, Cursor))
            {
                InputOutput.AddMousePosEvent(
                    static_cast<float>(Cursor.x),
                    static_cast<float>(Cursor.y)
                );
            }
            else
            {
                InputOutput.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            }
        }
        InputOutput.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        InputOutput.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        InputOutput.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
        InputOutput.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
    }
}
