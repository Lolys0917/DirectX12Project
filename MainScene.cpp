//|| MainScene.cpp ||:::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  MainScene固有のDemoModel ObjectとComponentを構築する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.10  Global Pointerを削除しObjectManager経由の更新へ修正
//||  2026_07_13  v1.00  新規作成: DemoModel生成をGameAppからSceneへ移動
//||

#include "MainScene.h"

#include <DirectXMath.h>

#include "Box.h"
#include "DirectX12.h"
#include "MessageLog.h"
#include "Object.h"
#include "ObjectManager.h"
#include "OBJModel.h"

namespace Engine
{
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
        Object* DemoObject =
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

        OBJModel* DemoModel = Objects.AddComponent<OBJModel>(
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

        Box* OscillatingBox = Objects.CreateObject<Box>(
            "OscillatingBox"
        ); //外部Sub ScriptをAttachする左右移動Box

        if (OscillatingBox == nullptr)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] MainScene | OscillatingBox creation failed."
            );
            return false;
        }

        OscillatingBox->SetPosition(DirectX::XMFLOAT3(-3.0f, 1.0f, 2.0f));
        OscillatingBox->SetSize(1.4f, 1.4f, 1.4f);
        OscillatingBox->SetColor(DirectX::XMFLOAT4(0.15f, 0.7f, 0.95f, 1.0f));

        MessageLog::GetInstance().AddLog(
            "[Info] MainScene | DemoModel definition created."
        );
        MessageLog::GetInstance().AddLog(
            "[Info] MainScene | OscillatingBox definition created for an attached Sub Script."
        );
        return true;
    }

    //概要：基底Sceneの全Componentを更新した後にMain固有の回転処理を実行する
    //引数：deltaTime=前フレームからの経過秒数
    //戻り値：なし
    void MainScene::Update(float deltaTime)
    {
        Scene::Update(deltaTime);

        Object* DemoObject = GetObjectManager().FindObject(
            ObjectType::Object,
            "DemoModel"
        ); //MainプログラムがネイティブAPIで直接操作するデモObject

        if (DemoObject == nullptr)
        {
            return;
        }

        const DirectX::XMFLOAT3& Rotation = DemoObject->GetRotation(); //更新前の回転角
        DemoObject->SetRotation(DirectX::XMFLOAT3(
            Rotation.x,
            Rotation.y + deltaTime,
            Rotation.z
        ));
    }

    // Clone先としてMainScene型の未初期化Instanceを作成する
    // 戻り値: 新しいMainScene
    std::unique_ptr<Scene> MainScene::CreateCloneInstance() const
    {
        return std::make_unique<MainScene>();
    }
}
