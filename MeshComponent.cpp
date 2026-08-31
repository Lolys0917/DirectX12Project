//|| MeshComponent.cpp ||:::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  メッシュコンポーネントの資源作成、描画、複製を実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v2.10  Component基底の初期化と終了契約を適用
//||  2026_07_13  v2.00  Owner姿勢を使用する共通Mesh描画へ変更
//||  2026_07_13  v1.00  新規作成
//||

#include "MeshComponent.h"

#include "Object.h"
#include "RenderContext.h"

namespace Engine
{
    //空のメッシュコンポーネントを作成する
    MeshComponent::MeshComponent()
        : Component(ComponentType::Mesh)
        , Mesh()
    {
    }

    //メッシュコンポーネントを破棄する
    MeshComponent::~MeshComponent() = default;

    //CPU側の頂点とインデックスを設定する
    //vertices : 頂点一覧
    //indices : 三角形インデックス一覧
    void MeshComponent::SetMeshData(
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices
    )
    {
        Mesh.Clear();
        Mesh.SetVertices(vertices);
        Mesh.SetIndices(indices);
    }

    //GPUメッシュ資源を作成する
    //dx12 : DirectX12基盤
    //戻り値 : GPU資源作成要求後はtrue
    bool MeshComponent::Initialize(DirectX12& dx12)
    {
        if (!Component::Initialize(dx12))
        {
            return false;
        }

        if (Mesh.GetVertexCount() == 0 || Mesh.GetIndexCount() == 0)
        {
            return true;
        }

        return Mesh.CreateGPUResource(dx12);
    }

    //現在の描画パスへメッシュを描画する
    //renderContext : DirectX12基盤とカメラを含む描画情報
    void MeshComponent::Draw(const RenderContext& renderContext)
    {
        const Object* OwnerObject = GetOwner(); //Mesh姿勢を所有するObject
        const DirectX::XMMATRIX World = OwnerObject != nullptr
            ? OwnerObject->GetWorldMatrix()
            : DirectX::XMMatrixIdentity(); //Owner未登録時は単位World行列
        Mesh.Draw(renderContext, World);
    }

    //CPUとGPUのメッシュ資源を破棄する
    void MeshComponent::Finalize()
    {
        Mesh.ReleaseGPUResource();
        Component::Finalize();
    }

    //未登録状態の複製定義を作成する
    //戻り値 : CPUメッシュを複製したコンポーネント
    std::unique_ptr<Component> MeshComponent::Clone() const
    {
        std::unique_ptr<MeshComponent> ClonedComponent =
            std::make_unique<MeshComponent>(); //複製コンポーネント

        ClonedComponent->SetMeshData(
            Mesh.GetVertices(),
            Mesh.GetIndices()
        );
        CopyDefinitionTo(*ClonedComponent);
        ClonedComponent->Mesh.CopyTextureDefinition(Mesh);
        return ClonedComponent;
    }
}
