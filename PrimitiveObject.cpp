//|| PrimitiveObject.cpp ||:::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  プリミティブ共通の色、GPU資源作成、描画処理を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  RenderContext共通Mesh描画とGPU終了処理を実装
//||  2026_07_13  v1.00  新規作成
//||

#include "PrimitiveObject.h"

namespace Engine
{
    //指定種別のプリミティブオブジェクトを作成する
    //objectType : オブジェクト種別
    PrimitiveObject::PrimitiveObject(ObjectType objectType)
        : Object(objectType)
        , Mesh()
        , Color(1.0f, 1.0f, 1.0f, 1.0f)
    {
    }

    //プリミティブオブジェクトを破棄する
    PrimitiveObject::~PrimitiveObject() = default;

    void PrimitiveObject::SetColor(const DirectX::XMFLOAT4& color)
    {
        Color = color;
        BuildMesh();
    }

    //CPUメッシュからGPU資源を作成する
    //dx12 : DirectX12基盤
    //戻り値 : GPU資源作成に成功した場合はtrue
    bool PrimitiveObject::CreateGPUResource(DirectX12& dx12)
    {
        if (Mesh.GetVertexCount() == 0)
        {
            BuildMesh();
        }

        return Mesh.CreateGPUResource(dx12);
    }

    //プリミティブ固有の毎フレーム更新を行う
    //deltaTime : 前フレームからの経過秒
    void PrimitiveObject::Update(float deltaTime)
    {
        (void)deltaTime;
    }

    //現在Cameraの描画passへObject姿勢を適用して描画する
    //renderContext : DirectX12基盤とCameraを含む描画情報
    void PrimitiveObject::Draw(const RenderContext& renderContext)
    {
        Mesh.Draw(renderContext, GetWorldMatrix());
    }

    //CPU Meshを保持したままGPU Resourceを解放する
    void PrimitiveObject::Finalize()
    {
        Mesh.ReleaseGPUResource();
    }

    //生成済み頂点とインデックスを内部メッシュへ設定する
    //vertices : 頂点一覧
    //indices : 三角形インデックス一覧
    void PrimitiveObject::SetMeshData(
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices
    )
    {
        Mesh.Clear();
        Mesh.SetVertices(vertices);
        Mesh.SetIndices(indices);
    }
}
