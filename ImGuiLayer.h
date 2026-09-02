//|| ImGuiLayer.h ||:::::::::::::::::::::::::::
//||
//||  Dear ImGuiのContext、入力、DirectX 12描画BackendをEngine側で所有する

#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <vector>

struct ImGui_ImplDX12_InitInfo;

namespace Engine
{
    class DirectX12;

    class ImGuiLayer final
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer();

        bool Initialize(DirectX12& dx12, HWND renderWindow);
        void Finalize();
        bool BeginFrame(float deltaTime);
        void Render(DirectX12& dx12);
        void CancelFrame();
        bool IsFrameActive() const { return FrameActive; }

        bool BeginWindow(const char* name);
        void EndWindow();
        void Text(const char* text);
        bool Button(const char* label);
        bool BeginTabBar(const char* identifier);
        void EndTabBar();
        bool BeginTabItem(const char* label);
        void EndTabItem();
        bool CollapsingHeader(const char* label, bool defaultOpen);
        void Separator();
        void ProgressBar(float fraction, const char* overlay);
        void PlotLines(
            const char* label,
            const float* values,
            std::uint32_t valueCount,
            float minimum,
            float maximum
        );

    private:
        static void AllocateDescriptor(
            ::ImGui_ImplDX12_InitInfo* information,
            D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle
        );
        static void FreeDescriptor(
            ::ImGui_ImplDX12_InitInfo* information,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
        );
        void UpdateInput();

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
        std::vector<std::uint32_t> FreeDescriptorIndices;
        HWND RenderWindow;
        std::uint32_t DescriptorIncrement;
        bool Initialized;
        bool FrameActive;
    };
}
