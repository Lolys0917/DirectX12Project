//|| Grid.h ||::::::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  各Camera passへデバッグ用Gridを描画するComponentを定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.20  Camera pass別WVPをRoot Constantsへ変更
//||  2026_07_13  v2.10  関数宣言コメントを規則へ統一
//||  2026_07_13  v2.00  Component化し固定Camera依存を削除
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>

#include <vector>

#include "Component.h"

namespace Engine
{
    struct GridVertex
    {
        DirectX::XMFLOAT3 Position; //Grid線の頂点座標
        DirectX::XMFLOAT4 Color; //Grid線の頂点色
    };

    struct GridConstantBuffer
    {
        DirectX::XMFLOAT4X4 WorldViewProjection; //転置済みWVP行列
    };

    class Grid final : public Component
    {
    public:
        static constexpr ComponentType StaticType = ComponentType::Grid; //Manager登録で使用するComponent型

        //未登録状態のGrid Componentを作成する
        Grid();

        //Grid Componentを破棄する
        ~Grid() override;

        //GPU Resourceの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元Grid
        Grid(const Grid&) = delete;

        //GPU Resourceの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元Grid
        //戻り値: 代入先Gridへの参照
        Grid& operator=(const Grid&) = delete;

        //Grid用GPU Resourceを作成する
        //引数: dx12 描画基盤
        //戻り値: 全Resource作成に成功した場合はtrue
        bool Initialize(DirectX12& dx12) override;

        //Gridの時間依存情報を更新する
        //引数: deltaTime 前回更新からの秒数
        void Update(float deltaTime) override;

        //RenderContextのCameraでGridを描画する
        //引数: renderContext 描画基盤と現在のCamera
        void Draw(const RenderContext& renderContext) override;

        //Gridが所有するGPU Resourceを解放する
        void Finalize() override;

        //未登録状態のGrid定義を複製する
        //戻り値: GPU Resourceを持たない複製Component
        std::unique_ptr<Component> Clone() const override;

    private:
        //デバッグ表示用GridのLine List頂点を構築する
        void BuildGrid();

        //Grid描画用RootSignatureを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateRootSignature(DirectX12& dx12);

        //Grid描画用PipelineStateを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreatePipelineState(DirectX12& dx12);

        //Grid頂点Bufferを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateVertexBuffer(DirectX12& dx12);

    private:
        std::vector<GridVertex> Vertices; //Gridを構成するLine List頂点
        Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature; //Grid描画RootSignature
        Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState; //Grid描画PipelineState
        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer; //Grid頂点Buffer
        D3D12_VERTEX_BUFFER_VIEW VertexBufferView; //Grid頂点Buffer View
    };
}
