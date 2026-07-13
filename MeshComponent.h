//|| MeshComponent.h ||:::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  オブジェクトへ付属して頂点メッシュを描画するコンポーネントを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Owner姿勢を使用する共通Mesh描画へ変更
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Component.h"
#include "VertexMesh.h"

namespace Engine
{
    class MeshComponent : public Component
    {
    public:
        //空のメッシュコンポーネントを作成する
        MeshComponent();

        //メッシュコンポーネントを破棄する
        ~MeshComponent() override;

        //CPU側の頂点とインデックスを設定する
        //vertices : 頂点一覧
        //indices : 三角形インデックス一覧
        void SetMeshData(
            const std::vector<Vertex>& vertices,
            const std::vector<std::uint32_t>& indices
        );

        VertexMesh& GetMesh() { return Mesh; }
        const VertexMesh& GetMesh() const { return Mesh; }

        //GPUメッシュ資源を作成する
        //dx12 : DirectX12基盤
        //戻り値 : GPU資源作成要求後はtrue
        bool Initialize(DirectX12& dx12) override;

        //現在の描画パスへメッシュを描画する
        //renderContext : DirectX12基盤とカメラを含む描画情報
        void Draw(const RenderContext& renderContext) override;

        //CPUとGPUのメッシュ資源を破棄する
        void Finalize() override;

        //未登録状態の複製定義を作成する
        //戻り値 : CPUメッシュを複製したコンポーネント
        std::unique_ptr<Component> Clone() const override;

    private:
        VertexMesh Mesh; //CPU頂点とGPUバッファを保持するメッシュ
    };
}
