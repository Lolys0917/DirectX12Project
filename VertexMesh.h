//|| VertexMesh.h ||::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  共通頂点とIndex及び対応するGPU Bufferを管理する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v2.10  GPU Resource準備状態の読取APIを追加
//||  2026_07_13  v2.00  共通Color MeshのGPU BufferとPipeline描画を実装
//||  2026_07_13  v1.10  命名と宣言コメントを規則へ統一
//||  2026_06_01  v1.00  新規作成
//||

#pragma once
#include <vector>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "Vertex.h"

namespace Engine
{
    class DirectX12;
    struct RenderContext;

    class VertexMesh
    {
    public:
        //空のCPU Meshと無効なGPU Buffer Viewを作成する
        VertexMesh();

        //CPU MeshとGPU Bufferを解放する
        ~VertexMesh();

        //CPU MeshとGPU Bufferを初期状態へ戻す
        void Clear();

        void SetVertices(const std::vector<Vertex>& vertices);
        void SetIndices(const std::vector<uint32_t>& indices);

        //頂点をCPU Mesh末尾へ追加する
        //引数: vertex 追加する頂点
        void AddVertex(const Vertex& vertex);

        //三頂点のIndexからTriangleをCPU Mesh末尾へ追加する
        //引数: i0 Triangle第一頂点Index、i1 第二頂点Index、i2 第三頂点Index
        void AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2);

        //多角形のIndex列をTriangle Fanとして三角形へ分解する
        //引数: faceIndices 多角形を周回順で表す頂点Index列
        void AddPolygonFace(const std::vector<uint32_t>& faceIndices);

        //CPU Meshに対応するGPU Bufferを作成する
        //引数: dx12 Resource作成に使用する描画基盤
        //戻り値: 全Resource作成に成功した場合はtrue
        bool CreateGPUResource(DirectX12& dx12);

        //現在のCamera passへWorld姿勢を適用して描画する
        //引数: renderContext 描画基盤とCamera、world MeshのWorld行列
        void Draw(
            const RenderContext& renderContext,
            const DirectX::XMMATRIX& world
        );

        //CPU Meshを保持したままGPU Resourceだけを解放する
        void ReleaseGPUResource();

        const std::vector<Vertex>& GetVertices() const;
        const std::vector<uint32_t>& GetIndices() const;

        uint32_t GetVertexCount() const;
        uint32_t GetIndexCount() const;

        //CPU Meshと一致するGPU Resourceが描画可能か確認する
        //戻り値 : Resource作成済みかつCPU変更後の再生成が不要な場合はtrue
        bool IsGPUResourceReady() const;

    private:
        //World行列を受け取るRoot Constants用RootSignatureを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateRootSignature(DirectX12& dx12);

        //共通Vertex Color描画用PipelineStateを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreatePipelineState(DirectX12& dx12);

        //Upload Heapへ頂点Bufferを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateVertexBuffer(DirectX12& dx12);

        //Upload HeapへIndex Bufferを作成する
        //引数: dx12 描画基盤
        //戻り値: 作成に成功した場合はtrue
        bool CreateIndexBuffer(DirectX12& dx12);

        std::vector<Vertex> Vertices; //CPU側の頂点一覧
        std::vector<uint32_t> Indices; //CPU側のTriangle Index一覧
        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer; //GPU頂点Buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer; //GPU Index Buffer
        D3D12_VERTEX_BUFFER_VIEW VertexBufferView; //頂点Buffer View
        D3D12_INDEX_BUFFER_VIEW IndexBufferView; //Index Buffer View
        Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature; //WVP Root Constants用RootSignature
        Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState; //共通Vertex Color描画Pipeline
        bool GPUResourceReady; //全GPU Resourceが描画可能な場合true
        bool GPUResourceDirty; //CPU Mesh変更後にGPU再作成が必要な場合true
    };
}
