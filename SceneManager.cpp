//|| SceneManager.cpp ||::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  複数Sceneの所有、平均O(1)検索、Active状態及びViewScene遷移を
//||  Tombstone方式の安定IDを用いて実装する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.30  Editor Snapshot用Scene ID列挙を追加
//||  2026_07_13  v1.20  編集: 派生Scene初期化とMainScene状態管理を追加
//||  2026_07_13  v1.10  編集: Scene作成、複製、Active索引及びResize失敗をログへ記録
//||  2026_07_13  v1.00  新規作成: 複数Active SceneとViewScene管理を追加
//||

#include "SceneManager.h"

#include <cstdio>
#include <utility>

#include "DirectX12.h"
#include "MessageLog.h"
#include "RenderTexture.h"
#include "Scene.h"

namespace
{
    /**
     * Scene処理失敗をSceneID付きでMessageLogへ追加する
     * @param operation 失敗した処理の説明
     * @param sceneID 対象Sceneの数値ID
     * @param permanent 内部索引不整合として残し続ける場合はtrue
     */
    void AddSceneFailureLog(
        const char* operation,
        std::uint32_t sceneID,
        bool permanent
    )
    {
        char Message[320]{}; // 処理内容とSceneIDを含む表示用メッセージ
        sprintf_s(
            Message,
            "[Error] SceneManager | %s SceneID=%u.",
            operation,
            sceneID
        );

        if (permanent)
        {
            Engine::MessageLog::GetInstance().AddPermanentLog(Message);
            return;
        }

        Engine::MessageLog::GetInstance().AddLog(Message);
    }
}

namespace Engine
{
    // 空のScene管理状態を作成する
    SceneManager::SceneManager()
        : ScenesByID(1) // 無効IDであるIndex 0を予約する
        , SceneIDByName()
        , SceneSuffixByName()
        , ActiveSceneIDs()
        , ActiveIndexByID()
        , MainSceneID()
        , ViewSceneID()
        , SceneCount(0)
    {
    }

    // 全Sceneを安全に終了して破棄する
    SceneManager::~SceneManager()
    {
        Finalize();
    }

    // 必須Objectを持つSceneを作成してSceneローカルIDを割り当てる
    // dx12: Scene内ComponentのGPU初期化に使用する描画基盤
    // name: 同名時に数値接尾辞を付ける希望Scene名
    // width: Camera RenderTextureの初期横幅
    // height: Camera RenderTextureの初期縦幅
    // 戻り値: 作成済みSceneID、失敗した場合は無効SceneID
    SceneID SceneManager::CreateScene(
        DirectX12& dx12,
        const std::string& name,
        uint32_t width,
        uint32_t height
    )
    {
        return CreateSceneInstance(
            dx12,
            std::make_unique<Scene>(),
            name,
            width,
            height
        );
    }

    // 既存Sceneの定義を独立ObjectManagerへ複製する
    // dx12: 複製ComponentのGPU初期化に使用する描画基盤
    // sourceSceneID: 複製元SceneID
    // name: 複製先の希望Scene名
    // 戻り値: 複製済みSceneID、失敗した場合は無効SceneID
    SceneID SceneManager::DuplicateScene(
        DirectX12& dx12,
        SceneID sourceSceneID,
        const std::string& name
    )
    {
        const Scene* SourceScene =
            FindScene(sourceSceneID); // 複製元Scene

        if (SourceScene == nullptr)
        {
            AddSceneFailureLog(
                "DuplicateScene could not find its source",
                sourceSceneID.GetValue(),
                false
            );
            return SceneID();
        }

        std::unique_ptr<Scene> NewScene =
            SourceScene->Clone(dx12); // 独立ObjectManagerを持つ複製Scene

        if (!NewScene)
        {
            AddSceneFailureLog(
                "Scene definition or GPU resource cloning failed for source",
                sourceSceneID.GetValue(),
                false
            );
            return SceneID();
        }

        const std::string ResolvedName =
            ResolveSceneName(name); // 同名を解消した複製先Scene名
        return RegisterScene(std::move(NewScene), ResolvedName);
    }

    // Sceneを終了してID Slotを再利用しないTombstoneへ変更する
    // sceneID: 削除対象SceneID
    // 戻り値: 削除に成功した場合はtrue、Sceneが存在しない場合はfalse
    bool SceneManager::RemoveScene(SceneID sceneID)
    {
        Scene* TargetScene = FindScene(sceneID); // 削除対象Scene

        if (TargetScene == nullptr)
        {
            return false;
        }

        if (MainSceneID == sceneID)
        {
            MainSceneID = SceneID();
        }

        SetActive(sceneID, false);
        SceneIDByName.erase(TargetScene->GetName());
        TargetScene->Finalize();
        ScenesByID[sceneID.GetValue()].reset();
        --SceneCount;

        return true;
    }

    // SceneIDからSceneを平均O(1)で検索する
    // sceneID: 検索対象SceneID
    // 戻り値: SceneへのPointer、存在しない場合はnullptr
    Scene* SceneManager::FindScene(SceneID sceneID)
    {
        const std::uint32_t IDValue = sceneID.GetValue(); // 直接Indexへ使用するID値

        if (!sceneID.IsValid() || IDValue >= ScenesByID.size())
        {
            return nullptr;
        }

        return ScenesByID[IDValue].get();
    }

    // SceneIDからSceneを平均O(1)で検索する
    // sceneID: 検索対象SceneID
    // 戻り値: SceneへのPointer、存在しない場合はnullptr
    const Scene* SceneManager::FindScene(SceneID sceneID) const
    {
        const std::uint32_t IDValue = sceneID.GetValue(); // 直接Indexへ使用するID値

        if (!sceneID.IsValid() || IDValue >= ScenesByID.size())
        {
            return nullptr;
        }

        return ScenesByID[IDValue].get();
    }

    // 解決済みScene名からSceneを平均O(1)で検索する
    // name: 検索対象の解決済みScene名
    // 戻り値: SceneへのPointer、存在しない場合はnullptr
    Scene* SceneManager::FindScene(const std::string& name)
    {
        const auto Found = SceneIDByName.find(name); // Scene名索引の検索結果
        return Found == SceneIDByName.end() ? nullptr : FindScene(Found->second);
    }

    // 解決済みScene名からSceneを平均O(1)で検索する
    // name: 検索対象の解決済みScene名
    // 戻り値: SceneへのPointer、存在しない場合はnullptr
    const Scene* SceneManager::FindScene(const std::string& name) const
    {
        const auto Found = SceneIDByName.find(name); // Scene名索引の検索結果
        return Found == SceneIDByName.end() ? nullptr : FindScene(Found->second);
    }

    // SceneのActive状態を変更する
    // sceneID: 状態を変更するSceneID
    // active: 更新及び描画対象にする場合はtrue
    // 戻り値: 状態の設定に成功した場合はtrue、Sceneがない場合はfalse
    bool SceneManager::SetActive(SceneID sceneID, bool active)
    {
        if (FindScene(sceneID) == nullptr)
        {
            AddSceneFailureLog(
                "SetActive rejected an unknown",
                sceneID.GetValue(),
                false
            );
            return false;
        }

        if (!active && MainSceneID == sceneID)
        {
            AddSceneFailureLog(
                "MainScene cannot be deactivated directly",
                sceneID.GetValue(),
                false
            );
            return false;
        }

        const auto Found = ActiveIndexByID.find(sceneID); // 現在のActive登録状態

        if (active)
        {
            if (Found == ActiveIndexByID.end())
            {
                std::size_t InsertIndex = 0; // SceneID登録順を維持する挿入位置

                while (InsertIndex < ActiveSceneIDs.size() &&
                    ActiveSceneIDs[InsertIndex].GetValue() < sceneID.GetValue())
                {
                    ++InsertIndex;
                }

                ActiveSceneIDs.insert(
                    ActiveSceneIDs.begin() + InsertIndex,
                    sceneID
                );

                for (std::size_t Position = InsertIndex;
                    Position < ActiveSceneIDs.size(); ++Position) // 挿入後の位置索引を更新する
                {
                    ActiveIndexByID[ActiveSceneIDs[Position]] = Position;
                }
            }

            return true;
        }

        if (Found == ActiveIndexByID.end())
        {
            return true;
        }

        const std::size_t RemovedIndex = Found->second; //削除するActive配列位置
        ActiveSceneIDs.erase(ActiveSceneIDs.begin() + RemovedIndex);
        ActiveIndexByID.erase(Found);

        for (std::size_t Index = RemovedIndex;
            Index < ActiveSceneIDs.size(); ++Index) // 削除後の位置索引を更新する
        {
            ActiveIndexByID[ActiveSceneIDs[Index]] = Index;
        }

        if (ViewSceneID == sceneID)
        {
            ViewSceneID = ActiveSceneIDs.empty()
                ? SceneID()
                : ActiveSceneIDs.front();
        }

        return true;
    }

    // MainSceneを変更し、同時にActive及びViewSceneへ設定する
    // sceneID: 新しいMainSceneID
    // 戻り値: MainSceneの変更に成功した場合はtrue
    bool SceneManager::SetMainScene(SceneID sceneID)
    {
        if (FindScene(sceneID) == nullptr || !SetActive(sceneID, true))
        {
            AddSceneFailureLog(
                "SetMainScene failed for",
                sceneID.GetValue(),
                false
            );
            return false;
        }

        MainSceneID = sceneID;

        if (!SetViewScene(sceneID))
        {
            MainSceneID = SceneID();
            AddSceneFailureLog(
                "MainScene could not become ViewScene",
                sceneID.GetValue(),
                false
            );
            return false;
        }

        char Message[160]{}; // 新しいMainScene IDを含む操作ログ
        sprintf_s(
            Message,
            "[Info] SceneManager | MainScene changed to SceneID=%u.",
            sceneID.GetValue()
        );
        MessageLog::GetInstance().AddLog(Message);
        return true;
    }

    // 現在のMainSceneIDを取得する
    // 戻り値: MainSceneID、未設定の場合は無効SceneID
    SceneID SceneManager::GetMainSceneID() const
    {
        return MainSceneID;
    }

    // 現在のMainSceneを取得する
    // 戻り値: MainScene、未設定の場合はnullptr
    Scene* SceneManager::GetMainScene()
    {
        return FindScene(MainSceneID);
    }

    // 現在のMainSceneを取得する
    // 戻り値: MainScene、未設定の場合はnullptr
    const Scene* SceneManager::GetMainScene() const
    {
        return FindScene(MainSceneID);
    }

    // SceneがActiveか判定する
    // sceneID: 判定対象SceneID
    // 戻り値: Activeの場合はtrue
    bool SceneManager::IsActive(SceneID sceneID) const
    {
        return ActiveIndexByID.contains(sceneID);
    }

    // 常時画面へ表示するViewSceneを変更し、非Activeなら同時に有効化する
    // sceneID: 新しいViewSceneID
    // 戻り値: ViewSceneの変更に成功した場合はtrue
    bool SceneManager::SetViewScene(SceneID sceneID)
    {
        if (FindScene(sceneID) == nullptr || !SetActive(sceneID, true))
        {
            AddSceneFailureLog(
                "SetViewScene failed for",
                sceneID.GetValue(),
                false
            );
            return false;
        }

        ViewSceneID = sceneID;
        char Message[160]{}; // 新しいViewScene IDを含む操作ログ
        sprintf_s(
            Message,
            "[Info] SceneManager | ViewScene changed to SceneID=%u.",
            sceneID.GetValue()
        );
        MessageLog::GetInstance().AddLog(Message);
        return true;
    }

    // 現在のViewSceneIDを取得する
    // 戻り値: ViewSceneID、未設定の場合は無効SceneID
    SceneID SceneManager::GetViewSceneID() const
    {
        return ViewSceneID;
    }

    // 全Active Sceneを更新する
    // deltaTime: 前Frameからの経過秒数
    void SceneManager::UpdateActiveScenes(float deltaTime)
    {
        for (SceneID ActiveSceneID : ActiveSceneIDs) // 登録順で処理するActive Scene
        {
            Scene* ActiveScene = FindScene(ActiveSceneID); // 今回更新するScene

            if (ActiveScene != nullptr)
            {
                ActiveScene->Update(deltaTime);
            }
            else
            {
                AddSceneFailureLog(
                    "Active update index refers to a missing",
                    ActiveSceneID.GetValue(),
                    true
                );
            }
        }
    }

    // 全Active Sceneの全Camera RenderTextureへ描画する
    // dx12: 描画命令を記録中のDirectX 12描画基盤
    // clearColor: 各Camera出力を消去するRGBA色
    void SceneManager::RenderActiveScenes(
        DirectX12& dx12,
        const float clearColor[4]
    )
    {
        for (SceneID ActiveSceneID : ActiveSceneIDs) // 登録順で描画するActive Scene
        {
            Scene* ActiveScene = FindScene(ActiveSceneID); // 今回描画するScene

            if (ActiveScene != nullptr)
            {
                ActiveScene->Render(dx12, clearColor);
            }
            else
            {
                AddSceneFailureLog(
                    "Active render index refers to a missing",
                    ActiveSceneID.GetValue(),
                    true
                );
            }
        }
    }

    // 全Scene内の全Camera RenderTextureを同じ寸法へ変更する
    // dx12: GPU待機とResource再生成に使用する描画基盤
    // width: 新しいRenderTexture横幅
    // height: 新しいRenderTexture縦幅
    // 戻り値: Active状態を問わず全Sceneの変更に成功した場合はtrue
    bool SceneManager::ResizeActiveScenes(
        DirectX12& dx12,
        uint32_t width,
        uint32_t height
    )
    {
        bool AllSucceeded = true; // 一部失敗後も残りSceneをResizeする集約結果

        for (std::size_t Index = 0; Index < ScenesByID.size(); ++Index) //Inactiveを含む全SceneをResizeする
        {
            std::unique_ptr<Scene>& OwnedScene = ScenesByID[Index]; // 現在ResizeするScene

            if (OwnedScene && !OwnedScene->Resize(dx12, width, height))
            {
                AddSceneFailureLog(
                    "Camera RenderTexture Resize failed for",
                    static_cast<std::uint32_t>(Index),
                    false
                );
                AllSucceeded = false;
            }
        }

        return AllSucceeded;
    }

    // ViewSceneの出力RenderTextureを取得する
    // 戻り値: Primary CameraのRenderTexture、未設定の場合はnullptr
    RenderTexture* SceneManager::GetViewRenderTexture()
    {
        Scene* ViewScene = FindScene(ViewSceneID); // 現在画面へ表示するScene
        return ViewScene == nullptr ? nullptr : ViewScene->GetRenderTexture();
    }

    // ViewSceneの出力RenderTextureを取得する
    // 戻り値: Primary CameraのRenderTexture、未設定の場合はnullptr
    const RenderTexture* SceneManager::GetViewRenderTexture() const
    {
        const Scene* ViewScene = FindScene(ViewSceneID); // 現在画面へ表示するScene
        return ViewScene == nullptr ? nullptr : ViewScene->GetRenderTexture();
    }

    //概要：有効Scene IDを登録順の安全なSnapshotとして取得する
    //引数：なし
    //戻り値：Tombstoneを除くScene ID一覧
    std::vector<SceneID> SceneManager::GetSceneIDs() const
    {
        std::vector<SceneID> Result; //返却するScene ID一覧
        Result.reserve(SceneCount);

        for (std::size_t Index = 1; Index < ScenesByID.size(); ++Index)
        {
            if (ScenesByID[Index] != nullptr)
            {
                Result.emplace_back(static_cast<std::uint32_t>(Index));
            }
        }

        return Result;
    }

    //概要：現在所有する有効Scene数を取得する
    //引数：なし
    //戻り値：Tombstoneを除くScene数
    std::size_t SceneManager::GetSceneCount() const
    {
        return SceneCount;
    }

    // 現在ActiveなScene数を取得する
    // 戻り値: Active Scene数
    std::size_t SceneManager::GetActiveSceneCount() const
    {
        return ActiveSceneIDs.size();
    }

    // 全SceneのComponentを終了してSceneを破棄する
    void SceneManager::Finalize()
    {
        for (std::unique_ptr<Scene>& OwnedScene : ScenesByID) // 全Scene Slotを終了する
        {
            if (OwnedScene)
            {
                OwnedScene->Finalize();
                OwnedScene.reset();
            }
        }

        ScenesByID.clear();
        ScenesByID.resize(1);
        SceneIDByName.clear();
        SceneSuffixByName.clear();
        ActiveSceneIDs.clear();
        ActiveIndexByID.clear();
        MainSceneID = SceneID();
        ViewSceneID = SceneID();
        SceneCount = 0;
    }

    // 所有権を受け取ったSceneを初期化してManagerへ登録する
    // dx12: Scene内ComponentのGPU初期化に使用する描画基盤
    // scene: 初期化する未登録Scene
    // name: 同名時に数値接尾辞を付ける希望Scene名
    // width: Camera RenderTextureの初期横幅
    // height: Camera RenderTextureの初期縦幅
    // 戻り値: 作成済みSceneID、失敗した場合は無効SceneID
    SceneID SceneManager::CreateSceneInstance(
        DirectX12& dx12,
        std::unique_ptr<Scene> scene,
        const std::string& name,
        uint32_t width,
        uint32_t height
    )
    {
        if (!scene)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] SceneManager | CreateScene received a null Scene instance."
            );
            return SceneID();
        }

        if (!scene->Initialize(dx12, width, height))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] SceneManager | Scene initialization failed during CreateScene."
            );
            return SceneID();
        }

        const std::string ResolvedName =
            ResolveSceneName(name); // 同名を解消した登録Scene名
        return RegisterScene(std::move(scene), ResolvedName);
    }

    // 同名Sceneへ単調増加する数値接尾辞を付けて一意名を解決する
    // requestedName: 希望Scene名
    // 戻り値: SceneManager内で一意な解決済みScene名
    std::string SceneManager::ResolveSceneName(const std::string& requestedName)
    {
        const std::string BaseName =
            requestedName.empty() ? "Scene" : requestedName; // 空名を補完した基底名

        if (!SceneIDByName.contains(BaseName))
        {
            return BaseName;
        }

        uint32_t& NextSuffix =
            SceneSuffixByName[BaseName]; // この基底名で次に試す数値接尾辞

        if (NextSuffix == 0)
        {
            NextSuffix = 1;
        }

        std::string CandidateName; // 数値接尾辞を加えた候補名

        do
        {
            CandidateName = BaseName + "_" + std::to_string(NextSuffix);
            ++NextSuffix;
        } while (SceneIDByName.contains(CandidateName));

        return CandidateName;
    }

    // 初期化済みSceneをManagerへ登録する
    // scene: 登録するSceneの所有権
    // resolvedName: 一意に解決済みのScene名
    // 戻り値: 登録済みSceneID、ID枯渇時は無効SceneID
    SceneID SceneManager::RegisterScene(
        std::unique_ptr<Scene> scene,
        const std::string& resolvedName
    )
    {
        if (!scene || resolvedName.empty() ||
            ScenesByID.size() > static_cast<std::size_t>(UINT32_MAX))
        {
            MessageLog::GetInstance().AddPermanentLog(
                "[Critical] SceneManager | Scene registration data was invalid or SceneID space was exhausted."
            );
            return SceneID();
        }

        const SceneID NewSceneID(
            static_cast<std::uint32_t>(ScenesByID.size())
        ); // 単調増加し再利用しないSceneID

        scene->AssignRegistration(NewSceneID, resolvedName);
        SceneIDByName.emplace(resolvedName, NewSceneID);
        ScenesByID.push_back(std::move(scene));
        ++SceneCount;

        return NewSceneID;
    }
}
