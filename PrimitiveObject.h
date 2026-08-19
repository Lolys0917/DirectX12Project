//|| PrimitiveObject.h ||:::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  頂点メッシュを直接所有する基底的オブジェクトを定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  RenderContext共通Mesh描画とGPU終了処理を実装
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "IRenderable.h"
#include "Object.h"
#include "VertexMesh.h"

namespace Engine
{
    class PrimitiveObject : public Object, public IRenderable
    {
    public:
        //プリミティブオブジェクトを破棄する
        ~PrimitiveObject() override;

        //GPU Resourceと所有情報の重複を防ぐためCopy構築を禁止する
        //引数: コピー元Primitive Object
        PrimitiveObject(const PrimitiveObject&) = delete;

        //GPU Resourceと所有情報の重複を防ぐためCopy代入を禁止する
        //引数: コピー元Primitive Object
        //戻り値: 代入先Primitive Objectへの参照
        PrimitiveObject& operator=(const PrimitiveObject&) = delete;

        void SetColor(const DirectX::XMFLOAT4& color);
        //概要：頂点生成へ使用するRGBA色を取得する
        //引数：なし
        //戻り値：現在のRGBA色
        const DirectX::XMFLOAT4& GetColor() const { return Color; }

        //概要：Primitiveが所有する頂点Meshを取得する
        //引数：なし
        //戻り値：読み取り専用VertexMesh
        const VertexMesh& GetMesh() const { return Mesh; }

        //CPUメッシュからGPU資源を作成する
        //dx12 : DirectX12基盤
        //戻り値 : GPU資源作成に成功した場合はtrue
        bool CreateGPUResource(DirectX12& dx12) override;

        //プリミティブ固有の毎フレーム更新を行う
        //deltaTime : 前フレームからの経過秒
        void Update(float deltaTime) override;

        //現在Cameraの描画passへObject姿勢を適用して描画する
        //renderContext : DirectX12基盤とCameraを含む描画情報
        void Draw(const RenderContext& renderContext) override;

        //CPU Meshを保持したままGPU Resourceを解放する
        void Finalize() override;

    protected:
        //指定種別のプリミティブオブジェクトを作成する
        //objectType : オブジェクト種別
        explicit PrimitiveObject(ObjectType objectType);

        //形状固有のCPUメッシュを再構築する
        virtual void BuildMesh() = 0;

        //生成済み頂点とインデックスを内部メッシュへ設定する
        //vertices : 頂点一覧
        //indices : 三角形インデックス一覧
        void SetMeshData(
            const std::vector<Vertex>& vertices,
            const std::vector<std::uint32_t>& indices
        );

        VertexMesh Mesh; //CPU頂点とGPUバッファを保持するメッシュ
        DirectX::XMFLOAT4 Color; //形状生成時に頂点へ設定する色
    };
}
