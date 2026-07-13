//|| MainScene.cpp ||:::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  MainScene固有のDemoModel ObjectとComponentを構築する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  新規作成: DemoModel生成をGameAppからSceneへ移動
//||

#include "MainScene.h"

#include <DirectXMath.h>

#include "DirectX12.h"
#include "MessageLog.h"
#include "Object.h"
#include "ObjectManager.h"
#include "OBJModel.h"

namespace Engine
{
    OBJModel* DemoModel;
    Object* DemoObject;
    // 未初期化のMainSceneを作成する
    MainScene::MainScene() = default;

    // Sceneが所有するComponentを基底Destructorで終了する
    MainScene::~MainScene() = default;

    // DemoModel ObjectとOBJModel ComponentをMainSceneへ追加する
    // dx12: OBJとTextureのGPU Resource作成に使用する描画基盤
    // 戻り値: DemoModelの構築と読み込みに成功した場合はtrue
    bool MainScene::OnCreateSceneObjects(DirectX12& dx12)
    {
        ObjectManager& Objects =
            GetObjectManager(); // MainScene専用ObjectManager
        DemoObject =
            Objects.CreateObject<Object>("DemoModel"); // DemoModelを所有するObject

        if (DemoObject == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] MainScene | DemoModel owner Object creation failed."
            );
            return false;
        }

        DemoObject->SetPosition(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        DemoObject->SetScale(DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f));
        DemoObject->SetRotation(DirectX::XMFLOAT3(
            -DirectX::XM_PIDIV2,
            DirectX::XM_PI,
            0.0f
        ));

        DemoModel = Objects.AddComponent<OBJModel>(
            DemoObject->GetID(),
            "DemoModel"
        ); // Cat OBJを描画するModel Component

        if (DemoModel == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] MainScene | OBJModel Component creation failed."
            );
            return false;
        }

        if (!DemoModel->Load(
            dx12,
            L"12222_Cat_v1_l3.obj",
            L"Cat_diffuse.jpg"))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] MainScene | Demo OBJ or texture loading failed."
            );
            return false;
        }

        MessageLog::GetInstance().AddLog(
            "[Info] MainScene | DemoModel definition created."
        );
        return true;
    }

    void MainScene::Update(float deltaTime)
    {
		DemoObject->SetRotation(DirectX::XMFLOAT3(
            DemoObject->GetRotation().x,
            DemoObject->GetRotation().y + 1.0f * deltaTime,
            DemoObject->GetRotation().z
		));
	}

    // Clone先としてMainScene型の未初期化Instanceを作成する
    // 戻り値: 新しいMainScene
    std::unique_ptr<Scene> MainScene::CreateCloneInstance() const
    {
        return std::make_unique<MainScene>();
    }
}
