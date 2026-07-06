#include "Camera.h"

#include "Core.h"
#include "d3dx12.h"

bool Camera::Initialize(Core& core)
{
    const UINT size = (sizeof(CameraConstantBuffer) + 255) & ~255;

    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    HRESULT hr = core.GetDevice()->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)
    );
    if (FAILED(hr)) return false;

    hr = constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr)) return false;

    UpdateCamera(
        DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
        DirectX::XMVectorSet(target.x, target.y, target.z, 1.0f)
    );

    return true;
}

void Camera::UpdateCamera(DirectX::XMVECTOR pos, DirectX::XMVECTOR focus)
{
    DirectX::XMStoreFloat3(&eye, pos);
    DirectX::XMStoreFloat3(&target, focus);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(&eye),
        DirectX::XMLoadFloat3(&target),
        DirectX::XMLoadFloat3(&up)
    );

    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XM_PIDIV4,
        static_cast<float>(Core::WIDTH) / static_cast<float>(Core::HEIGHT),
        0.1f,
        100.0f
    );

    DirectX::XMStoreFloat4x4(&data.viewProj, view * proj);

    if (mappedData)
    {
        *mappedData = data;
    }
}

D3D12_GPU_VIRTUAL_ADDRESS Camera::GetGPUVirtualAddress() const
{
    return constantBuffer ? constantBuffer->GetGPUVirtualAddress() : 0;
}