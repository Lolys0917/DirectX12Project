//|| RotationScript.h ||:::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  エディターから差し込み可能なNative回転Script例を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <DirectXMath.h>

#include "Script.h"

namespace Engine
{
    class RotationScript final : public Script
    {
    public:
        explicit RotationScript(
            const DirectX::XMFLOAT3& radiansPerSecond = DirectX::XMFLOAT3(
                0.0f,
                1.0f,
                0.0f
            )
        );
        ~RotationScript() override;

        void SetRadiansPerSecond(const DirectX::XMFLOAT3& value);
        const DirectX::XMFLOAT3& GetRadiansPerSecond() const;

    protected:
        std::unique_ptr<Script> CloneScript() const override;
        void OnUpdate(float deltaTime) override;

    private:
        DirectX::XMFLOAT3 RadiansPerSecond; //毎秒加算するXYZ回転量
    };
}
