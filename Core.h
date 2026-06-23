#pragma once

#define NOMINMAX
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <windows.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <DirectXMath.h>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")

const int WIDTH = 1280;
const int HEIGHT = 720;
const int FRAME_COUNT = 2;

using namespace Microsoft::WRL;

class ClassWindow
{
public:
    bool Initialize(
        HINSTANCE instance,
        WNDCLASS wc,
        int width,
        int height,
        const char* title
    );

    bool InitD3D();

    HWND GetHWND() const;

private:
    HWND hwnd;

    HANDLE fenceEvent = nullptr;

    ComPtr<ID3D12Device> device;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12CommandAllocator>
        commandAllocator;
    ComPtr<ID3D12GraphicsCommandList>
        commandList;
    ComPtr<ID3D12Fence>
        fence;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];

    UINT64 fenceValue = 0;

    UINT frameIndex;
    UINT rtvDescriptorSize = 0;
    UINT srvDescriptorSize = 0;
};
