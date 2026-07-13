//|| OBJModel.h ||::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Wavefront OBJとTextureを描画するModel Componentを定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.20  Root Constants、安全なOBJ解析、複製元情報を追加
//||  2026_07_13  v2.10  関数宣言コメントを規則へ統一
//||  2026_07_13  v2.00  Objectの姿勢を使用するModel Componentへ変更
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Model.h"
#include "Texture2D.h"

namespace Engine
{
    class DirectX12;
    struct RenderContext;

    struct OBJVertex
    {
        DirectX::XMFLOAT3 Position; //頂点座標
        DirectX::XMFLOAT3 Normal; //頂点法線
        DirectX::XMFLOAT2 UV; //Texture座標
    };

    struct OBJConstantBuffer
    {
        DirectX::XMFLOAT4X4 WorldViewProjection; //転置済みWVP行列
        DirectX::XMFLOAT4 Color; //Modelへ乗算する色
        int UseTexture; //Textureを使用する場合は1
        float Padding[3]; //ConstantBuffer境界をそろえる予約領域
    };

    class OBJModel final : public Model
    {
    public:
        //未登録状態のOBJ Model Componentを作成する
        OBJModel();

        //OBJ Model Componentを破棄する
        ~OBJModel() override;

        //GPU Resourceの二重所有を防ぐためCopy構築を禁止する
        //引数: コピー元OBJ Model
        OBJModel(const OBJModel&) = delete;

        //GPU Resourceの二重所有を防ぐためCopy代入を禁止する
        //引数: コピー元OBJ Model
        //戻り値: 代入先OBJ Modelへの参照
        OBJModel& operator=(const OBJModel&) = delete;

        //OBJとDiffuse Textureを読み込みGPU Resourceを作成する
        //引数: dx12 描画基盤、objPath OBJパス、texturePath Textureパス
        //戻り値: 読み込みとResource作成に成功した場合はtrue
        bool Load(
            DirectX12& dx12,
            const std::wstring& objPath,
            const std::wstring& texturePath
        );

        //OBJを単色Modelとして読み込みGPU Resourceを作成する
        //引数: dx12 描画基盤、objPath OBJパス、color 描画色
        //戻り値: 読み込みとResource作成に成功した場合はtrue
        bool Load(
            DirectX12& dx12,
            const std::wstring& objPath,
            const DirectX::XMFLOAT4& color
        );

        //複製済みCPU MeshからGPU Resourceを再作成する
        //引数: dx12 描画基盤
        //戻り値: Resource作成に成功またはMesh未設定の場合はtrue
        bool Initialize(DirectX12& dx12) override;

        void SetColor(const DirectX::XMFLOAT4& color);

        //Modelの時間依存情報を更新する
        //引数: deltaTime 前回更新からの秒数
        void Update(float deltaTime) override;

        //現在のCameraでOBJ Modelを描画する
        //引数: renderContext 描画基盤とCameraを持つContext
        void Draw(const RenderContext& renderContext) override;

        //OBJ Modelの終了処理を行う
        void Finalize() override;

        //未登録状態のOBJ Model定義を複製する
        //戻り値: CPU Meshと色を持つ複製Component
        std::unique_ptr<Component> Clone() const override;

    private:
        //OBJテキストから頂点とIndexを読み込む
        //引数: objPath 読み込むOBJファイル
        //戻り値: 描画可能な面を読み込めた場合はtrue
        bool LoadOBJFile(const std::wstring& objPath);

        //Model描画用RootSignatureを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateRootSignature(DirectX12& dx12);

        //Model描画用PipelineStateを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreatePipelineState(DirectX12& dx12);

        //頂点Bufferを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateVertexBuffer(DirectX12& dx12);

        //Index Bufferを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateIndexBuffer(DirectX12& dx12);

        DirectX::XMMATRIX GetWorldMatrix() const;

    private:
        std::vector<OBJVertex> Vertices; //OBJから読み込んだ頂点
        std::vector<uint32_t> Indices; //Triangle ListのIndex
        Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature; //Model描画RootSignature
        Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState; //Model描画PipelineState
        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer; //GPU頂点Buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer; //GPU Index Buffer
        D3D12_VERTEX_BUFFER_VIEW VertexBufferView; //頂点Buffer View
        D3D12_INDEX_BUFFER_VIEW IndexBufferView; //Index Buffer View
        Texture2D Texture; //Diffuse Texture
        DirectX::XMFLOAT4 Color; //Modelへ乗算する色
        std::wstring SourceOBJPath; //複製時に再読込するOBJ File Path
        std::wstring SourceTexturePath; //複製時に再読込するDiffuse Texture Path
        bool UseTexture; //現在のDiffuse TextureをShaderで使用する場合はtrue
        bool TextureRequested; //Texture付きLoadの再現を意図する場合はtrue
    };
}
