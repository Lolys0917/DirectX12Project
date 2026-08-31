//|| RotationScript.cpp ||:::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  所有Objectを毎フレーム指定角速度で回転するNative Scriptを実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#include "RotationScript.h"

#include <cstdio>

#include "Object.h"

namespace Engine
{
    //概要：毎秒のXYZ回転量を持つ未登録Native Scriptを作成する
    //引数：radiansPerSecond=毎秒加算するラジアン単位のXYZ回転量
    //戻り値：なし
    RotationScript::RotationScript(const DirectX::XMFLOAT3& radiansPerSecond)
        : Script("native.rotation", "Rotation Script")
        , RadiansPerSecond(radiansPerSecond)
    {
    }

    //概要：Finalize済みNative回転Scriptを破棄する
    //引数：なし
    //戻り値：なし
    RotationScript::~RotationScript() = default;

    //概要：毎秒のXYZ回転量を変更する
    //引数：value=ラジアン単位のXYZ回転量
    //戻り値：なし
    void RotationScript::SetRadiansPerSecond(const DirectX::XMFLOAT3& value)
    {
        RadiansPerSecond = value;
    }

    //概要：毎秒のXYZ回転量を取得する
    //引数：なし
    //戻り値：ラジアン単位のXYZ回転量
    const DirectX::XMFLOAT3& RotationScript::GetRadiansPerSecond() const
    {
        return RadiansPerSecond;
    }

    std::vector<ScriptExposedMember> RotationScript::GetExposedMembers() const
    {
        char Buffer[128]{};
        std::snprintf(Buffer, std::size(Buffer), "%g,%g,%g",
            RadiansPerSecond.x, RadiansPerSecond.y, RadiansPerSecond.z);
        return {
            { "RadiansPerSecond", "Vector3", Buffer, false, false },
            { "ResetRotation", "Action", "", true, false }
        };
    }

    bool RotationScript::SetExposedMember(const std::string& name, const std::string& value)
    {
        DirectX::XMFLOAT3 Parsed{};
        if (name != "RadiansPerSecond" || ::sscanf_s(
            value.c_str(), "%f,%f,%f", &Parsed.x, &Parsed.y, &Parsed.z) != 3)
        {
            return false;
        }
        RadiansPerSecond = Parsed;
        return true;
    }

    bool RotationScript::InvokeExposedFunction(const std::string& name)
    {
        Object* Owner = GetOwner();
        if (name != "ResetRotation" || Owner == nullptr)
        {
            return false;
        }
        Owner->SetRotation({ 0.0f, 0.0f, 0.0f });
        return true;
    }

    //概要：同じ角速度を持つ未登録Rotation Scriptを複製する
    //引数：なし
    //戻り値：新しいRotation Script
    std::unique_ptr<Script> RotationScript::CloneScript() const
    {
        return std::make_unique<RotationScript>(RadiansPerSecond);
    }

    //概要：所有Objectへ経過時間に比例したXYZ回転量を加算する
    //引数：deltaTime=前フレームからの経過秒数
    //戻り値：なし
    void RotationScript::OnUpdate(float deltaTime)
    {
        Object* Owner = GetOwner(); //回転させる所有Object

        if (Owner == nullptr)
        {
            return;
        }

        const DirectX::XMFLOAT3& Rotation = Owner->GetRotation(); //加算前のXYZ回転角
        Owner->SetRotation(DirectX::XMFLOAT3(
            Rotation.x + RadiansPerSecond.x * deltaTime,
            Rotation.y + RadiansPerSecond.y * deltaTime,
            Rotation.z + RadiansPerSecond.z * deltaTime
        ));
    }
}
