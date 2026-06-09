#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wincodec.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

#include "d3dx12.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")

using namespace Microsoft::WRL;

//ウィンドウサイズ
const int WIDTH = 1280;
const int HEIGHT = 720;
const int FRAME_COUNT = 2;

//構造体群
struct Vertex
{
    float x, y, z;
    float u, v;
};
struct ObjectState
{
    float PosX, PosY, PosZ;
    float SizeX, SizeY, SizeZ;
    float r, g, b, a;
};
struct TextureData
{
    ComPtr<ID3D12Resource> resource;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};

    UINT width = 0;
    UINT height = 0;
};

//グローバル保持変数
HWND g_hwnd = nullptr;

ComPtr<ID3D12Device> device;
ComPtr<IDXGISwapChain3> swapChain;
ComPtr<ID3D12CommandQueue> commandQueue;

ComPtr<ID3D12DescriptorHeap> rtvHeap;
ComPtr<ID3D12DescriptorHeap> srvHeap;

ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];

ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12GraphicsCommandList> commandList;

ComPtr<ID3D12Fence> fence;

HANDLE fenceEvent = nullptr;

UINT64 fenceValue = 0;

UINT frameIndex = 0;

UINT rtvDescriptorSize = 0;
UINT srvDescriptorSize = 0;

ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;

ComPtr<ID3D12Resource> vertexBuffer;

D3D12_VERTEX_BUFFER_VIEW vbView{};

UINT vertexCount = 0;

std::unordered_map<std::string, TextureData> g_textures;

//ウィンドウプロシージャー
LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    switch (msg)
    {
    case WM_DESTROY:

        PostQuitMessage(0);

        return 0;
    }

    return DefWindowProc(
        hwnd,
        msg,
        wparam,
        lparam
    );
}
//ウィンドウ作成
void CreateWindowApp(HINSTANCE hInstance)
{
    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "DX12WindowClass";

    RegisterClass(&wc);

    g_hwnd =
        CreateWindowEx(
            0,
            wc.lpszClassName,
            "DX12 Texture Sample",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WIDTH,
            HEIGHT,
            nullptr,
            nullptr,
            hInstance,
            nullptr
        );

    ShowWindow(
        g_hwnd,
        SW_SHOW
    );
}
//GPU同期処理
void WaitForGPU()
{
    UINT64 currentFence = fenceValue;

    commandQueue->Signal(
        fence.Get(),
        currentFence
    );

    fenceValue++;

    if (fence->GetCompletedValue() < currentFence)
    {
        fence->SetEventOnCompletion(
            currentFence,
            fenceEvent
        );

        WaitForSingleObject(
            fenceEvent,
            INFINITE
        );
    }

    frameIndex =
        swapChain->GetCurrentBackBufferIndex();
}