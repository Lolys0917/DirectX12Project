//|| Vertex.h ||::::::::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  共通頂点、マテリアル、頂点入力レイアウトを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  変数名をアッパーキャメル規則へ統一
//||

#pragma once

#include <cstdint>

#include <DirectXMath.h>
#include <d3d12.h>

namespace Engine
{
    struct Vertex
    {
        DirectX::XMFLOAT3 Position; //頂点座標
        DirectX::XMFLOAT3 Normal; //頂点法線
        DirectX::XMFLOAT2 UV; //テクスチャ座標
        DirectX::XMFLOAT4 Color; //頂点色
    };

    struct Material
    {
        DirectX::XMFLOAT4 BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f }; //基本色
        float Metallic = 0.0f; //金属度
        float Roughness = 0.5f; //粗さ
        float Padding[2] = {}; //定数バッファ境界調整
    };

    //共通頂点の入力レイアウトを取得する
    //戻り値 : 静的に保持する入力要素配列
    inline D3D12_INPUT_ELEMENT_DESC* GetVertexInputLayout()
    {
        static D3D12_INPUT_ELEMENT_DESC Layout[] = //共通頂点入力要素
        {
            {
                "POSITION",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "NORMAL",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TEXCOORD",
                0,
                DXGI_FORMAT_R32G32_FLOAT,
                0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "COLOR",
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            }
        };

        return Layout;
    }

    //共通頂点の入力要素数を取得する
    //戻り値 : 入力要素数
    inline std::uint32_t GetVertexInputLayoutCount()
    {
        return 4;
    }
}
