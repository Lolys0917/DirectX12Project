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

using namespace Microsoft::WRL;

class ClassDirectXManager
{
public:
    void InitGraphics(ID3D12Device *device);
    void WaitForGPU();
private:
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
};