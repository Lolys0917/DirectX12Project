//|| SceneManager.h ||::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  複数Sceneの所有、平均O(1)検索、Active状態及び画面へ表示する
//||  ViewSceneの遷移を管理するクラスを定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.30  Editor Snapshot用Scene ID列挙APIを追加
//||  2026_07_13  v1.20  編集: 派生Scene生成とMainScene設定APIを追加
//||  2026_07_13  v1.00  新規作成: 複数Active SceneとViewScene管理を追加
//||

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "EntityTypes.h"
#include "Scene.h"

namespace Engine
{
    class DirectX12;
    class RenderTexture;

    class SceneManager final
    {
    public:
        // 空のScene管理状態を作成する
        SceneManager();

        // 全Sceneを安全に終了して破棄する
        ~SceneManager();

        //Scene ID索引と所有権の重複を防ぐためCopy構築を禁止する
        //引数: コピー元SceneManager
        SceneManager(const SceneManager&) = delete;

        //Scene ID索引と所有権の重複を防ぐためCopy代入を禁止する
        //引数: コピー元SceneManager
        //戻り値: 代入先SceneManagerへの参照
        SceneManager& operator=(const SceneManager&) = delete;

        //Scene内参照と索引の不整合を防ぐためMove構築を禁止する
        //引数: 移動元SceneManager
        SceneManager(SceneManager&&) = delete;

        //Scene内参照と索引の不整合を防ぐためMove代入を禁止する
        //引数: 移動元SceneManager
        //戻り値: 代入先SceneManagerへの参照
        SceneManager& operator=(SceneManager&&) = delete;

        // 必須Objectを持つSceneを作成してSceneローカルIDを割り当てる
        // dx12: Scene内ComponentのGPU初期化に使用する描画基盤
        // name: 同名時に数値接尾辞を付ける希望Scene名
        // width: Camera RenderTextureの初期横幅
        // height: Camera RenderTextureの初期縦幅
        // 戻り値: 作成済みSceneID、失敗した場合は無効SceneID
        SceneID CreateScene(
            DirectX12& dx12,
            const std::string& name,
            uint32_t width,
            uint32_t height
        );

        //概要：指定Scene派生型を初期化してManagerへ登録する
        //引数：dx12=描画基盤、name=希望名、width=Camera幅、height=Camera高さ
        //戻り値：作成済みScene ID、失敗した場合は無効Scene ID
        template<typename SceneType>
        SceneID CreateScene(
            DirectX12& dx12,
            const std::string& name,
            uint32_t width,
            uint32_t height
        )
        {
            static_assert(
                std::is_base_of_v<Scene, SceneType>,
                "SceneType must derive from Scene."
            );
            static_assert(
                std::is_default_constructible_v<SceneType>,
                "SceneType must be default constructible."
            );

            return CreateSceneInstance(
                dx12,
                std::make_unique<SceneType>(),
                name,
                width,
                height
            );
        }

        //概要：指定Scene派生型を作成しActive、Main、View Sceneへ一括設定する
        //引数：dx12=描画基盤、name=希望名、width=Camera幅、height=Camera高さ
        //戻り値：Main Sceneへ設定したScene ID、失敗時は無効Scene ID
        template<typename SceneType>
        SceneID CreateMainScene(
            DirectX12& dx12,
            const std::string& name,
            uint32_t width,
            uint32_t height
        )
        {
            const SceneID NewSceneID = CreateScene<SceneType>(
                dx12,
                name,
                width,
                height
            ); // 登録済みの新しいMainScene候補

            if (!NewSceneID.IsValid())
            {
                return SceneID();
            }

            if (!SetMainScene(NewSceneID))
            {
                RemoveScene(NewSceneID);
                return SceneID();
            }

            return NewSceneID;
        }

        // 既存Sceneの定義を独立ObjectManagerへ複製する
        // dx12: 複製ComponentのGPU初期化に使用する描画基盤
        // sourceSceneID: 複製元SceneID
        // name: 複製先の希望Scene名
        // 戻り値: 複製済みSceneID、失敗した場合は無効SceneID
        SceneID DuplicateScene(
            DirectX12& dx12,
            SceneID sourceSceneID,
            const std::string& name
        );

        //全SceneのCPU定義とActive/Main/View関係を再生復元用に複製する
        std::unique_ptr<SceneManager> CloneDefinition() const;

        //現在状態を終了し、複製定義をComponent初期化後の稼働状態へ置き換える
        bool RestoreDefinition(
            std::unique_ptr<SceneManager> definition,
            DirectX12& dx12
        );

        // Sceneを終了してID Slotを再利用しないTombstoneへ変更する
        // sceneID: 削除対象SceneID
        // 戻り値: 削除に成功した場合はtrue、Sceneが存在しない場合はfalse
        bool RemoveScene(SceneID sceneID);

        // SceneIDからSceneを平均O(1)で検索する
        // sceneID: 検索対象SceneID
        // 戻り値: SceneへのPointer、存在しない場合はnullptr
        Scene* FindScene(SceneID sceneID);

        // SceneIDから読み取り専用Sceneを平均O(1)で検索する
        // sceneID: 検索対象SceneID
        // 戻り値: SceneへのPointer、存在しない場合はnullptr
        const Scene* FindScene(SceneID sceneID) const;

        // 解決済みScene名からSceneを平均O(1)で検索する
        // name: 検索対象の解決済みScene名
        // 戻り値: SceneへのPointer、存在しない場合はnullptr
        Scene* FindScene(const std::string& name);

        // 解決済みScene名から読み取り専用Sceneを平均O(1)で検索する
        // name: 検索対象の解決済みScene名
        // 戻り値: SceneへのPointer、存在しない場合はnullptr
        const Scene* FindScene(const std::string& name) const;

        // 有効Scene IDを登録順で取得する
        // 戻り値: Tombstoneを除くScene ID一覧
        std::vector<SceneID> GetSceneIDs() const;

        // SceneのActive状態を変更する
        // sceneID: 状態を変更するSceneID
        // active: 更新及び描画対象にする場合はtrue
        // 戻り値: 状態の設定に成功した場合はtrue、Sceneがない場合はfalse
        bool SetActive(SceneID sceneID, bool active);

        // SceneがActiveか判定する
        // sceneID: 判定対象SceneID
        // 戻り値: Activeの場合はtrue
        bool IsActive(SceneID sceneID) const;

        // MainSceneを変更し、同時にActive及びViewSceneへ設定する
        // sceneID: 新しいMainSceneID
        // 戻り値: MainSceneの変更に成功した場合はtrue
        bool SetMainScene(SceneID sceneID);

        // 現在のMainSceneIDを取得する
        // 戻り値: MainSceneID、未設定の場合は無効SceneID
        SceneID GetMainSceneID() const;

        // 現在のMainSceneを取得する
        // 戻り値: MainScene、未設定の場合はnullptr
        Scene* GetMainScene();
        const Scene* GetMainScene() const;

        // 常時画面へ表示するViewSceneを変更し、非Activeなら同時に有効化する
        // sceneID: 新しいViewSceneID
        // 戻り値: ViewSceneの変更に成功した場合はtrue
        bool SetViewScene(SceneID sceneID);

        // 現在のViewSceneIDを取得する
        // 戻り値: ViewSceneID、未設定の場合は無効SceneID
        SceneID GetViewSceneID() const;

        // 全Active Sceneを更新する
        // deltaTime: 前Frameからの経過秒数
        void UpdateActiveScenes(float deltaTime);

        // 全Active Sceneの全Camera RenderTextureへ描画する
        // dx12: 描画命令を記録中のDirectX 12描画基盤
        // clearColor: 各Camera出力を消去するRGBA色
        void RenderActiveScenes(
            DirectX12& dx12,
            const float clearColor[4]
        );

        // 全Scene内の全Camera RenderTextureを同じ寸法へ変更する
        // dx12: GPU待機とResource再生成に使用する描画基盤
        // width: 新しいRenderTexture横幅
        // height: 新しいRenderTexture縦幅
        // 戻り値: Active状態を問わず全Sceneの変更に成功した場合はtrue
        bool ResizeActiveScenes(
            DirectX12& dx12,
            uint32_t width,
            uint32_t height
        );

        // ViewSceneの出力RenderTextureを取得する
        // 戻り値: Primary CameraのRenderTexture、未設定の場合はnullptr
        RenderTexture* GetViewRenderTexture();
        const RenderTexture* GetViewRenderTexture() const;

        // 現在所有するScene数を取得する
        // 戻り値: Tombstoneを除いたScene数
        std::size_t GetSceneCount() const;

        // 現在ActiveなScene数を取得する
        // 戻り値: Active Scene数
        std::size_t GetActiveSceneCount() const;

        // 全SceneのComponentを終了してSceneを破棄する
        void Finalize();

    private:
        // 所有権を受け取ったSceneを初期化してManagerへ登録する
        // dx12 Scene内ComponentのGPU初期化に使用する描画基盤
        // scene 初期化する未登録Scene
        // name 同名時に数値接尾辞を付ける希望Scene名
        // width Camera RenderTextureの初期横幅
        // height Camera RenderTextureの初期縦幅
        // 戻り値: 作成済みSceneID、失敗した場合は無効SceneID
        SceneID CreateSceneInstance(
            DirectX12& dx12,
            std::unique_ptr<Scene> scene,
            const std::string& name,
            uint32_t width,
            uint32_t height
        );

        // 同名Sceneへ単調増加する数値接尾辞を付けて一意名を解決する
        // requestedName: 希望Scene名
        // 戻り値: SceneManager内で一意な解決済みScene名
        std::string ResolveSceneName(const std::string& requestedName);

        // 初期化済みSceneをManagerへ登録する
        // scene: 登録するSceneの所有権
        // resolvedName: 一意に解決済みのScene名
        // 戻り値: 登録済みSceneID、ID枯渇時は無効SceneID
        SceneID RegisterScene(
            std::unique_ptr<Scene> scene,
            const std::string& resolvedName
        );

        bool ActivateDefinitions(DirectX12& dx12);
        void SwapState(SceneManager& other) noexcept;

    private:
        std::vector<std::unique_ptr<Scene>> ScenesByID; // SceneIDを直接Indexにする安定Slot
        std::unordered_map<std::string, SceneID> SceneIDByName; // 解決済み名からIDへの索引
        std::unordered_map<std::string, uint32_t> SceneSuffixByName; // 基底名別の次接尾辞
        std::vector<SceneID> ActiveSceneIDs; // 更新及び描画順を保持するActive Scene一覧
        std::unordered_map<SceneID, std::size_t> ActiveIndexByID; // Active解除用の位置索引
        SceneID MainSceneID; // Engineの基準として常にActiveを維持するSceneID
        SceneID ViewSceneID; // 画面へ常時表示するSceneID
        std::size_t SceneCount; // Tombstoneを除いたScene数
    };
}
