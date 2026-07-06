#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>

class Core;

struct CameraConstantBuffer
{
    DirectX::XMFLOAT4X4 viewProj;
};

class Camera
{
public:
    bool Initialize(Core& core);
    void UpdateCamera(DirectX::XMVECTOR pos, DirectX::XMVECTOR focus);
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;

private:
    DirectX::XMFLOAT3 eye = { 0.0f, 0.0f, -5.0f };
    DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

    CameraConstantBuffer data = {};
    CameraConstantBuffer* mappedData = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
};