#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "Camera.h"
class Model;
class Grid;
class Camera;

class Core
{
private:
    Camera camera_;
public:
    static const int WIDTH = 1280;
    static const int HEIGHT = 720;
    static const int FRAME_COUNT = 2;
    static const int SRV_MAX = 256;

    Core() = default;
    ~Core();

    bool Initialize(HINSTANCE hInstance);
    void Run(Model& model);
    void Run(Model& model, Grid& grid);

    void Render(Model& model);
    void Render(Model& model, Grid& grid);

    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }

    D3D12_GPU_DESCRIPTOR_HANDLE CreateTextureSrv(ID3D12Resource* resource);
    void ExecuteCommandListAndWait();
    void WaitForGPU();

    Camera& GetCamera() { return camera_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    bool CreateWindowApp(HINSTANCE hInstance);
    bool InitD3D();
    bool InitGraphics();

private:
    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;

    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;

    UINT frameIndex = 0;
    UINT rtvDescriptorSize = 0;
    UINT srvDescriptorSize = 0;
    UINT srvIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    bool comInitialized = false;
};