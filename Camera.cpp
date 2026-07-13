//|| Camera.cpp ||::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Objectへ付加するCamera Componentと専用RenderTextureを管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Component化、専用Depth、複数Camera描画へ対応
//||  2026_06_01  v1.00  新規作成
//||

#include "Camera.h"

#include <Windows.h>

#include <algorithm>
#include <utility>

#include "DirectX12.h"
#include "Object.h"
#include "RenderContext.h"

namespace Engine
{
    //指定した初期描画解像度でCamera Componentを作成する
    //引数: width 初期描画幅、height 初期描画高さ
    Camera::Camera(uint32_t width, uint32_t height)
        : Component(StaticType)
        , Target(0.0f, 0.0f, 0.0f)
        , Up(0.0f, 1.0f, 0.0f)
        , DetachedPosition(0.0f, 6.0f, -10.0f)
        , Width(std::max<uint32_t>(1, width))
        , Height(std::max<uint32_t>(1, height))
        , Aspect(static_cast<float>(Width) / static_cast<float>(Height))
        , MoveSpeed(5.0f)
    {
    }

    //Camera専用RenderTextureを解放してComponentを破棄する
    Camera::~Camera()
    {
        Finalize();
    }

    //専用Color/Depth RenderTextureを作成する
    //引数: dx12 GPU Resource作成に使用する描画基盤
    //戻り値: Resource作成に成功した場合はtrue
    bool Camera::Initialize(DirectX12& dx12)
    {
        if (OutputTexture)
        {
            return true;
        }

        OutputTexture = std::make_unique<RenderTexture>();

        if (!OutputTexture->Initialize(
            dx12,
            Width,
            Height,
            dx12.GetBackBufferFormat()))
        {
            OutputTexture.reset();
            return false;
        }

        return true;
    }

    //入力に応じて所有ObjectのCamera位置を更新する
    //引数: deltaTime 前回更新からの秒数
    void Camera::Update(float deltaTime)
    {
        DirectX::XMFLOAT3 Position = GetPosition(); //更新前のCamera位置
        const float Move = MoveSpeed * deltaTime; //今回の更新で移動する距離

        if ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0)
        {
            Position.x -= Move;
        }

        if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0)
        {
            Position.x += Move;
        }

        if ((GetAsyncKeyState(VK_UP) & 0x8000) != 0)
        {
            Position.z += Move;
        }

        if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0)
        {
            Position.z -= Move;
        }

        Object* Owner = GetOwner(); //Camera Componentを所有するObject

        if (Owner != nullptr)
        {
            Owner->SetPosition(Position);
        }
        else
        {
            DetachedPosition = Position;
        }
    }

    //Camera自体は描画対象ではないため何も描画しない
    //引数: renderContext 現在の描画Context
    void Camera::Draw(const RenderContext& renderContext)
    {
        (void)renderContext;
    }

    //Camera専用RenderTextureを解放する
    void Camera::Finalize()
    {
        OutputTexture.reset();
    }

    //未登録状態のCamera定義を複製する
    //戻り値: GPU Resourceを持たない複製Component
    std::unique_ptr<Component> Camera::Clone() const
    {
        auto Duplicate = std::make_unique<Camera>(Width, Height); //未登録の複製Camera
        Duplicate->Target = Target;
        Duplicate->Up = Up;
        Duplicate->DetachedPosition = DetachedPosition;
        Duplicate->MoveSpeed = MoveSpeed;
        CopyDefinitionTo(*Duplicate);
        return Duplicate;
    }

    //専用RenderTextureを安全に再作成する
    //引数: dx12 描画基盤、width 新しい幅、height 新しい高さ
    //戻り値: 再作成に成功した場合はtrue
    bool Camera::Resize(DirectX12& dx12, uint32_t width, uint32_t height)
    {
        const uint32_t NewWidth = (std::max<uint32_t>)(1, width); //検証済みの新しいRenderTexture幅
        const uint32_t NewHeight = (std::max<uint32_t>)(1, height); //検証済みの新しいRenderTexture高さ

        if (!OutputTexture)
        {
            auto NewTexture = std::make_unique<RenderTexture>(); //成功時だけ採用する新しいCamera出力

            if (!NewTexture->Initialize(
                dx12,
                NewWidth,
                NewHeight,
                dx12.GetBackBufferFormat()))
            {
                return false;
            }

            OutputTexture = std::move(NewTexture);
        }
        else if (!OutputTexture->Resize(dx12, NewWidth, NewHeight))
        {
            return false;
        }

        Width = NewWidth;
        Height = NewHeight;
        Aspect = static_cast<float>(Width) / static_cast<float>(Height);
        return true;
    }

    void Camera::SetTarget(const DirectX::XMFLOAT3& target)
    {
        Target = target;
    }

    void Camera::SetUp(const DirectX::XMFLOAT3& up)
    {
        Up = up;
    }

    void Camera::SetMoveSpeed(float moveSpeed)
    {
        MoveSpeed = std::max(0.0f, moveSpeed);
    }

    DirectX::XMMATRIX Camera::GetViewMatrix() const
    {
        const DirectX::XMFLOAT3 Position = GetPosition(); //所有Objectから得たCamera位置

        return DirectX::XMMatrixLookAtLH(
            DirectX::XMLoadFloat3(&Position),
            DirectX::XMLoadFloat3(&Target),
            DirectX::XMLoadFloat3(&Up)
        );
    }

    DirectX::XMMATRIX Camera::GetProjectionMatrix() const
    {
        return DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(45.0f),
            Aspect,
            0.1f,
            1000.0f
        );
    }

    DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const
    {
        return GetViewMatrix() * GetProjectionMatrix();
    }

    DirectX::XMFLOAT3 Camera::GetPosition() const
    {
        const Object* Owner = GetOwner(); //Camera Componentを所有するObject
        return Owner != nullptr ? Owner->GetPosition() : DetachedPosition;
    }

    //このCamera専用RenderTextureへの描画を開始する
    //引数: dx12 描画基盤、clearColor RGBA消去色
    void Camera::BeginRender(DirectX12& dx12, const float clearColor[4])
    {
        if (OutputTexture)
        {
            OutputTexture->Begin(dx12, clearColor);
        }
    }

    //このCamera専用RenderTextureへの描画を終了する
    //引数: dx12 描画基盤
    void Camera::EndRender(DirectX12& dx12)
    {
        if (OutputTexture)
        {
            OutputTexture->End(dx12);
        }
    }

    RenderTexture* Camera::GetRenderTexture()
    {
        return OutputTexture.get();
    }

    const RenderTexture* Camera::GetRenderTexture() const
    {
        return OutputTexture.get();
    }
}
