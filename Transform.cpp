//|| Transform.cpp ||::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Objectが必ず所有するLocal姿勢と行列分解を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#include "Transform.h"

#include <algorithm>
#include <cmath>

namespace Engine
{
    namespace
    {
        //概要：QuaternionをXMMatrixRotationRollPitchYaw互換のXYZ回転角へ変換する
        //引数：quaternion=変換する正規化Quaternion
        //戻り値：ラジアン単位のPitch、Yaw、Roll
        DirectX::XMFLOAT3 QuaternionToEuler(DirectX::FXMVECTOR quaternion)
        {
            DirectX::XMFLOAT4 Value; //計算に使用するQuaternion成分
            DirectX::XMStoreFloat4(
                &Value,
                DirectX::XMQuaternionNormalize(quaternion)
            );

            const float PitchNumerator = 2.0f * (
                Value.w * Value.x + Value.y * Value.z
            ); //Pitchのatan2分子
            const float PitchDenominator = 1.0f - 2.0f * (
                Value.x * Value.x + Value.y * Value.y
            ); //Pitchのatan2分母
            const float YawSine = std::clamp(
                2.0f * (Value.w * Value.y - Value.z * Value.x),
                -1.0f,
                1.0f
            ); //Yawのasin入力
            const float RollNumerator = 2.0f * (
                Value.w * Value.z + Value.x * Value.y
            ); //Rollのatan2分子
            const float RollDenominator = 1.0f - 2.0f * (
                Value.y * Value.y + Value.z * Value.z
            ); //Rollのatan2分母

            return DirectX::XMFLOAT3(
                std::atan2(PitchNumerator, PitchDenominator),
                std::asin(YawSine),
                std::atan2(RollNumerator, RollDenominator)
            );
        }
    }

    //概要：原点、無回転、等倍のLocal Transformを作成する
    //引数：なし
    //戻り値：なし
    Transform::Transform()
        : LocalPosition(0.0f, 0.0f, 0.0f)
        , LocalRotation(0.0f, 0.0f, 0.0f)
        , LocalScale(1.0f, 1.0f, 1.0f)
    {
    }

    //概要：親ObjectからのLocal座標を変更する
    //引数：position=設定するXYZ座標
    //戻り値：なし
    void Transform::SetLocalPosition(const DirectX::XMFLOAT3& position)
    {
        LocalPosition = position;
    }

    //概要：親ObjectからのLocal回転角を変更する
    //引数：rotation=ラジアン単位のXYZ回転角
    //戻り値：なし
    void Transform::SetLocalRotation(const DirectX::XMFLOAT3& rotation)
    {
        LocalRotation = rotation;
    }

    //概要：親ObjectからのLocal拡縮率を変更する
    //引数：scale=設定するXYZ拡縮率
    //戻り値：なし
    void Transform::SetLocalScale(const DirectX::XMFLOAT3& scale)
    {
        LocalScale = scale;
    }

    //概要：親ObjectからのLocal座標を取得する
    //引数：なし
    //戻り値：XYZ Local座標
    const DirectX::XMFLOAT3& Transform::GetLocalPosition() const
    {
        return LocalPosition;
    }

    //概要：親ObjectからのLocal回転角を取得する
    //引数：なし
    //戻り値：ラジアン単位のXYZ回転角
    const DirectX::XMFLOAT3& Transform::GetLocalRotation() const
    {
        return LocalRotation;
    }

    //概要：親ObjectからのLocal拡縮率を取得する
    //引数：なし
    //戻り値：XYZ Local拡縮率
    const DirectX::XMFLOAT3& Transform::GetLocalScale() const
    {
        return LocalScale;
    }

    //概要：Local姿勢を拡縮、回転、平行移動の順で行列へ変換する
    //引数：なし
    //戻り値：親Transformを含まないLocal行列
    DirectX::XMMATRIX Transform::GetLocalMatrix() const
    {
        const DirectX::XMMATRIX ScaleMatrix = DirectX::XMMatrixScaling(
            LocalScale.x,
            LocalScale.y,
            LocalScale.z
        ); //Local拡縮行列
        const DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
            LocalRotation.x,
            LocalRotation.y,
            LocalRotation.z
        ); //Local回転行列
        const DirectX::XMMATRIX TranslationMatrix = DirectX::XMMatrixTranslation(
            LocalPosition.x,
            LocalPosition.y,
            LocalPosition.z
        ); //Local平行移動行列
        return ScaleMatrix * RotationMatrix * TranslationMatrix;
    }

    //概要：Local行列を座標、回転、拡縮へ分解して設定する
    //引数：matrix=分解するLocal行列
    //戻り値：Shear等がなく分解できた場合はtrue
    bool Transform::SetLocalMatrix(DirectX::FXMMATRIX matrix)
    {
        DirectX::XMVECTOR ScaleVector; //分解された拡縮率
        DirectX::XMVECTOR RotationQuaternion; //分解された回転Quaternion
        DirectX::XMVECTOR TranslationVector; //分解された座標

        if (!DirectX::XMMatrixDecompose(
            &ScaleVector,
            &RotationQuaternion,
            &TranslationVector,
            matrix
        ))
        {
            return false;
        }

        DirectX::XMStoreFloat3(&LocalScale, ScaleVector);
        DirectX::XMStoreFloat3(&LocalPosition, TranslationVector);
        LocalRotation = QuaternionToEuler(RotationQuaternion);
        return true;
    }
}
