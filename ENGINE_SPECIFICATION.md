# DirectX 12 エンジン基盤仕様

更新日: 2026-07-13

## 1. 設計方針

本基盤は、Object がゲーム世界上の実体と姿勢を持ち、機能を Component として保持する構成とする。

- `Object`: 名前、ID、Position、Rotation、Scale、Component 所有権を持つ。
- `PrimitiveObject`: 頂点形状を持つ基本 Object の親クラス。
- `Box`、`Sphere`、`Plane`、`Cylinder`、`HalfSphere`、`Capsule`: `PrimitiveObject` を継承する。
- `Camera`、`Grid`、`Polygon`、`Model`、`Collider`: `Component` を継承する。
- `MainScene`: `Scene` を継承し、起動時に必要なゲーム固有ObjectとComponentを定義する。
- Component の寿命は所有 Object の寿命を超えない。
- Object と Component の登録、ID 付与、検索、削除は Scene ごとの `ObjectManager` だけが行う。

PrimitiveObject は ObjectManager の描画ライフサイクルへ統合し、VertexMesh の共通 Vertex Color Pipeline で描画する。MeshComponent と Polygon も同じ Pipeline を使用し、所有 Object のワールド行列を適用する。CPU Mesh の変更後は次の Draw 時に GPU Buffer を遅延再生成する。

従来の `Transform` 構造体は、Object 以外から使用されず、Component 側の座標とも責務が重複していたため削除する。必要な Position、Rotation、Scale とワールド行列生成は Object が直接保持する。Collider の Center などは所有 Object からのローカルオフセットとして各 Component が保持する。

## 2. ID と名前

### 2.1 スコープ

ObjectID と ComponentID は Scene 内でのみ有効な強い型とする。Scene をまたぐ参照は `SceneID + ObjectID` または `SceneID + ComponentID` の組で表現する。

- 無効値は `0`。
- 有効な ID は単調増加で発行する。
- 削除済み ID は再利用しない。
- 削除済みスロットは tombstone とし、古い ID が別の実体を指さないようにする。

### 2.2 Object 名

Object は `ObjectType × ResolvedName = ObjectID` で登録する。同じ ObjectType 内で同名が指定された場合は、次のように解決する。

```text
Player
Player_1
Player_2
```

異なる ObjectType では同名を使用できる。名前の接尾辞番号は削除後も巻き戻さない。

### 2.3 Component 名

Component は `OwnerObjectID × ComponentType × ResolvedName = ComponentID` で登録する。同じ Object に付く同じ ComponentType の同名だけが接尾辞付与の対象となる。これにより、ComponentID から所有 Object を特定でき、Object と Component の取り違えを防げる。

### 2.4 検索量

- ID 検索: ID を安定 vector slot として使用するため厳密に O(1)。
- Type × Name 検索: `vector<unordered_map>` を使用し、リスト走査なし、平均 O(1)。文字列長とハッシュ衝突を含む最悪計算量は O(n) になり得る。
- Update と Draw: 有効な要素を処理するため O(n)。Find の要件とは別である。

検索 API は見つからない場合に例外で暗黙挿入せず、`nullptr` または無効な ID を返す。

## 3. Object と Component のライフサイクル

1. `ObjectManager::CreateObject` が Object を生成し、型別プールと ID 索引へ登録する。
2. `ObjectManager::AddComponent` が所有 Object に Component を追加し、Owner、型、解決済み名、ComponentID を設定する。
3. Scene 初期化時に Component の `Initialize` を呼ぶ。
4. Active Scene のみ `Update` する。
5. Active Scene の全 Camera ごとに描画 Component を `Draw` する。
6. 削除時は Component の索引を先に無効化してから Object を tombstone にする。
7. Scene 破棄時は GPU 使用完了後に Component、Object、ObjectManager の順で破棄する。

Scene のデフォルトコピーは行わない。複製 API は Object と Component を新しい ObjectManager へ複製し、GPU リソースを複製先の描画環境で再初期化する。

## 4. Collider

Collider は衝突形状の共通 Component とし、次を用意する。

- `BoxCollider`
- `SphereCollider`
- `CapsuleCollider`
- `CylinderCollider`
- `PlaneCollider`

共通情報は Active、IsTrigger、Center、LayerMask とする。各形状は所有 Object の Position、Rotation、Scale とローカル形状値からワールド AABB を求める。現段階では登録と broad-phase 用境界の提供までを基盤範囲とし、形状同士の narrow-phase、MeshCollider、物理応答、衝突イベント配送は `CollisionSystem` の拡張項目とする。

## 5. Scene

各 Scene は ObjectManager を必ず 1 個所有し、初期化時に次の必須要素を作成する。

- `MainCamera` Object + `Camera` Component
- `DebugGrid` Object + `Grid` Component

基底 `Scene::Initialize` は必須CameraとGridを作成した後、派生型の `OnCreateSceneObjects` を呼び出し、最後に全Componentを初期化する。ゲーム固有モデルやObjectを `GameApp` に直接記述してはならず、`MainScene::OnCreateSceneObjects` のような派生Scene側へ定義する。

Scene複製時は `CreateCloneInstance` により派生型を維持する。`MainScene` を複製した結果が基底 `Scene` に変わってはならない。

各 Camera は専用の Color RenderTexture、RTV、SRV、DepthTexture、DSV を所有する。異なる解像度の Camera が swap chain の DepthBuffer を共有してはならない。

Scene の代表出力は PrimaryCamera の RenderTexture とする。他 Camera の RenderTexture も ComponentID で取得でき、ミニマップやユーザー独自合成に利用できる。

## 6. SceneManager

- Scene は複数同時に Active にできる。
- `CreateScene<SceneType>` は指定したScene派生型を作成、初期化、登録する。
- `CreateMainScene<SceneType>` は派生Sceneの作成後、Active、MainScene、ViewSceneへの設定を一括して行う。
- MainScene はEngineの基準Sceneとして常にActiveを維持し、直接の非Active化を拒否する。
- 起動時の `GameApp` は `CreateMainScene<MainScene>` を呼ぶだけとし、ObjectやModelを直接生成しない。
- Inactive Scene は Object と RenderTexture を保持するが Update と Render を行わない。
- `ViewScene` は画面へ常時表示する Scene を 1 つ指す。
- `ViewScene` は実行中に変更できる。
- Inactive Scene を `ViewScene` に指定した場合は同時に Active へ変更する。
- 描画順は SceneID の登録順とし、画面への最終表示だけは現在の `ViewScene` 出力を使用する。

描画順序は次のとおり。

1. 全 Active Scene を列挙する。
2. 各 Scene の全 Camera について専用 RenderTexture を開始する。
3. その Camera を含む RenderContext で Grid、Model、Polygon 等を描画する。
4. Camera の RenderTexture を SRV または CopySource 状態へ戻す。
5. `ViewScene` の代表 RenderTexture を左側 viewport の back buffer へ転送する。
6. Present する。

同一 CommandListへ複数 Camera passを記録するため、Grid、OBJModel、VertexMesh のCamera別WVPは永続Mapした単一ConstantBufferへ書き込まない。各 Draw の値を Root Constants としてCommandListへ直接記録し、後続Cameraによる上書きを防ぐ。

## 7. Windows 標準 UI

外部 UI ライブラリは使用しない。Win32 と Windows Common Controls のみを使用する。

- 左: DXGI swap chain の対象となる子 viewport。`ViewScene` の RenderTexture を表示する。
- 中央: ドラッグ可能な splitter。左右の最小幅を守って配置を変更する。
- 右: `UIDemo.png`、Start、Stop、Tick、FPS Edit、FPS Trackbar、ログ一覧、一括消去ボタン。
- 右操作領域は、上段を Start / Stop / Tick と状態表示、中央を UIDemo と FPS 設定、下段をメッセージログとして配置する。
- 右パネルの範囲管理用Windowは描画せず、機能コントロールを親Editor Window上の前面へ明示的に固定して背景に隠れないようにする。
- 初期分割比は左 68% / 右 32%、左描画領域の最小幅は 320 論理 px、右操作パネルの最小幅は 380 論理 px とする。
- 右操作パネルは 18 論理 px の外周余白、8 論理 px のコントロール間隔、16 論理 px のセクション間隔をDPIへ換算して使用する。
- 最小クライアント高さは 640 論理 px とし、画像を必要に応じて縮めてもログ一覧には最低 120 論理 px を確保する。
- フォント: `Yu Gothic UI`、`Meiryo`、Windows の既定 GUI フォントの順でフォールバックする。
- PNG 読み込み: Windows Imaging Component を使用する。
- FPS 範囲: 1～240、初期値 60。
- Start: Active Scene の連続 Update を開始する。
- Stop: Update を停止し、最後の描画結果を保持する。
- Tick: Stop 中に `1 / TargetFPS` 秒だけ 1 回 Update する。
- Edit と Trackbar の値は常に同期し、範囲外値はクランプする。
- splitter や親 Window のサイズ変更時は、GPU 完了を待って swap chain とActive/Inactiveを含む全SceneのCamera RenderTextureを安全に再生成する。Resource作成失敗時はCameraの論理解像度を変更しない。

## 8. メッセージログ

- Process 内で共有する `MessageLog` は、通常ログと常設ログをそれぞれ `vector<string>` で登録順に保持する。
- `AddLog` は通常ログを追加し、`ClearLogs` は通常ログだけを一括消去する。
- `AddPermanentLog` で追加した常設ログは `ClearLogs` の対象外とし、実行中に必ず残す情報へ使用する。
- 同一文字列の常設ログは重複登録せず、毎Frame発生する恒常障害でも一覧とMemoryを増やし続けない。
- 表示側は `GetRevision` で変更を検出し、変更時だけ `GetSnapshot` でMutex保護された一貫したログ一覧を取得する。
- Windows標準 `LISTBOX` は常設ログを先頭に `[常設]` 付きで表示し、その後へ通常ログを登録順に表示する。
- UIの「ログを一括消去」は `ClearLogs` だけを実行し、常設ログを保持する。
- 例外、初期化失敗、無効引数などにより処理を中断または無視する箇所は、戻る前に原因を通常ログへ追加する。
- GPU同期、Present、Resource転送、Scene索引又はRollback失敗のように継続時の安全性へ影響する障害は常設ログとして残す。

## 9. アセットとビルド

- `UIDemo.png`、`Cat_diffuse.jpg`、デモ OBJ は実行出力先へコピーする。
- UI の画像とフォント設定は Windows UI リソース設定クラスへ集約する。
- 全構成を Unicode、Windows subsystem、C++20 に統一する。
- Debug/Release と CRT の組み合わせを一致させる。
- ソースは UTF-8 としてコンパイルする。
- `d3dx12.h` は外部提供ファイルのためプロジェクト固有の命名・コメント修正対象外とする。

## 10. コーディング規則

- クラス、関数、型、ローカル変数、メンバー変数は UpperCamelCase。
- 関数引数だけ lowerCamelCase。
- 関数宣言の直上に機能、引数、戻り値の説明を書く。短い Set/Get は省略できる。
- 変数宣言の行末に意味を書く。
- 全 `.h` / `.cpp` の先頭に概要と更新内容を書く。
- 追加、削除、編集を行った理由と内容を更新履歴へ記載する。
