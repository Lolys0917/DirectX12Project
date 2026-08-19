# DirectX 12 エンジン基盤仕様

更新日: 2026-08-20

## 1. 設計方針

本基盤は、Object がゲーム世界上の実体と姿勢を持ち、機能を Component として保持する構成とする。

- `Object`: 名前、ID、Position、Rotation、Scale、Component 所有権を持つ。
- `PrimitiveObject`: 頂点形状を持つ基本 Object の親クラス。
- `Box`、`Sphere`、`Plane`、`Cylinder`、`HalfSphere`、`Capsule`: `PrimitiveObject` を継承する。
- `Camera`、`Grid`、`Polygon`、`Model`、`Collider`: `Component` を継承する。
- `MainScene`: `Scene` を継承し、起動時に必要なゲーム固有ObjectとComponentを定義する。
- `EngineAPI`: C++ネイティブのMainプログラムと外部Toolが描画、Scene、Object、Scriptの全公開APIへ到達するFacadeとする。
- `Script`: ObjectへComponentとして差し込み、毎フレーム実行するSubプログラム基底とする。
- Component の寿命は所有 Object の寿命を超えない。
- Object と Component の登録、ID 付与、検索、削除は Scene ごとの `ObjectManager` だけが行う。

PrimitiveObject は ObjectManager の描画ライフサイクルへ統合し、VertexMesh の共通 Vertex Color Pipeline で描画する。MeshComponent と Polygon も同じ Pipeline を使用し、所有 Object のワールド行列を適用する。同一DeviceのRoot SignatureとPipeline Stateは共有する。色はDraw単位のRoot Constantsで上書きし、寸法又は分割数が変わった場合だけCPU MeshとGPU Bufferを遅延再生成する。

従来の `Transform` 構造体は、Object 以外から使用されず、Component 側の座標とも責務が重複していたため削除する。必要な Position、Rotation、Scale とワールド行列生成は Object が直接保持する。Collider の Center などは所有 Object からのローカルオフセットとして各 Component が保持する。

## 2. ID と名前

### 2.1 スコープ

ObjectID と ComponentID は Scene 内でのみ有効な強い型とする。Scene をまたぐ参照は `SceneID + ObjectID` または `SceneID + ComponentID` の組で表現する。

- 無効値は `0`。
- 有効な ID は単調増加で発行する。
- 削除済み ID は再利用しない。
- 削除済みスロットは tombstone とし、古い ID が別の実体を指さないようにする。

### 2.2 Object 名

Object は `ResolvedName = ObjectID` でSceneごとに登録する。型に関係なく同名が指定された場合は、次のように解決する。

```text
Player
Player_1
Player_2
```

異なる ObjectType でも同名は使用しない。これにより組込みAPIと将来のVisual ScriptingはObject型を別途保持せず、名前だけで安定IDへ解決できる。名前の接尾辞番号は削除後も巻き戻さない。

### 2.3 Component 名

Component は `OwnerObjectID × ComponentType × ResolvedName = ComponentID` で登録する。同じ Object に付く同じ ComponentType の同名だけが接尾辞付与の対象となる。これにより、ComponentID から所有 Object を特定でき、Object と Component の取り違えを防げる。

### 2.4 検索量

- ID 検索: ID を安定 vector slot として使用するため厳密に O(1)。
- Name検索: Sceneごとの`unordered_map`を使用し、リスト走査なし、平均 O(1)。Type × Name検索も名前解決後に型を検証する。文字列長とハッシュ衝突を含む最悪計算量は O(n) になり得る。
- Handle情報取得: `SceneID + ObjectID`又は`SceneID + ComponentID`から安定Slotを直接参照しO(1)。外部DLLには生Pointerではなく非所有Handleを公開する。
- 部分名／型／Attach済みComponent検索: 結果集合を作る操作なので有効Objectを走査しO(n)。取得後のHandle操作はID直接参照へ戻る。
- Update と Draw: 有効な要素を処理するため O(n)。Find の要件とは別である。

検索 API は見つからない場合に例外で暗黙挿入せず、`nullptr` または無効な ID を返す。

## 3. Object と Component のライフサイクル

1. `ObjectManager::CreateObject` が Object を生成し、Scene内名前索引と ID 索引へ登録する。
2. `ObjectManager::AddComponent` が所有 Object に Component を追加し、Owner、型、解決済み名、ComponentID を設定する。
3. Scene 初期化時に Component の `Initialize` を呼ぶ。派生Componentは最初に `Component::Initialize` を呼び、Owner、ComponentID、解決済み名の登録完了を検証する。
4. Active Scene のみ `Update` する。
5. Active Scene の全 Camera ごとに描画 Component を `Draw` する。
6. 削除時は Component の索引を先に無効化してから Object を tombstone にする。
7. Scene 破棄時は GPU 使用完了後に Component、Object、ObjectManager の順で破棄する。

Script Componentは `OnAttach`、`OnStart`、`OnUpdate`、`OnStop`、`OnDetach` の順序を保証する。Object又はScriptが非Activeの場合は `OnUpdate` を呼ばない。実行中に追加したScriptは `Scene::InitializePendingComponents` により次の更新前に初期化する。

Main ProgramはConstructor／Destructorではなく`Init`、`Update`、`End`を高水準Lifecycleとする。順序依存の参照解除が必要なSceneは任意の`StartDestroy`と`EndDestroy`を持てる。全Sceneの`StartDestroy`を登録順、`End`を逆登録順、`EndDestroy`を逆登録順で実行する。

派生Componentの終了処理は固有GPU Resource又はDLL Instanceを解放した後に `Component::Finalize` を呼ぶ。Renderable ObjectはActive状態に関係なく登録時にGPU Resourceを初期化する。これにより、非Active状態で複製したObjectを後からActiveにしても未初期化Resourceを描画しない。

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
- 右: Engine、再生、ログ、メイン、スクリプトのTabを持つ操作パネル。
- Engine Tab: Scene→Object→Component／ScriptのTree、Object追加、Script差込、DLL読込ボタン。
- 再生Tab: `UIDemo.png`、Start／Resume、Pause、Stop、Tick、FPS Edit、FPS Trackbar。
- ログTab: ログ一覧、一括消去ボタン。
- メインTab: `Programs/` のC++を編集し、各フレームでScene／Object／Sub Scriptより先に実行する全体制御DLLを生成する。
- スクリプトTab: `ScriptPrograms/` のC++を編集し、Objectへ差し込むUnityのScript相当のDLLを生成する。
- 右操作領域は、上段を Start / Pause / Stop / Tick と状態表示、中央を UIDemo と FPS 設定、下段をメッセージログとして配置する。
- 右パネルの範囲管理用Windowは描画せず、機能コントロールを親Editor Window上の前面へ明示的に固定して背景に隠れないようにする。
- 初期分割比は左 68% / 右 32%、左描画領域の最小幅は 320 論理 px、右操作パネルの最小幅は 380 論理 px とする。
- 右操作パネルは 18 論理 px の外周余白、8 論理 px のコントロール間隔、16 論理 px のセクション間隔をDPIへ換算して使用する。
- 最小クライアント高さは 640 論理 px とし、画像を必要に応じて縮めてもログ一覧には最低 120 論理 px を確保する。
- フォント: `Yu Gothic UI`、`Meiryo`、Windows の既定 GUI フォントの順でフォールバックする。
- PNG 読み込み: Windows Imaging Component を使用する。
- FPS 範囲: 1～240、初期値 60。
- Start: Active Scene の連続 Update を開始する。
- Pause: Scene、Object、Main Program状態を保持したままUpdateを一時停止する。StartはResume表示となり続きから再開する。
- Stop: Updateを停止し、Scene定義を再生開始直前へ戻してMain Programを再生成する。
- Tick: Stop又はPause中に`1 / TargetFPS`秒だけ1回Updateする。Stop直後のTickは先に復元状態をSnapshot化する。
- Keyboard入力は描画ViewportがFocusを持つ場合だけ有効とする。Viewportクリック時はEditor／TabからFocusを移す。
- Edit と Trackbar の値は常に同期し、範囲外値はクランプする。
- Tree空白又はSceneの右クリックはObject追加、DLL読込、更新を提供する。
- Objectの右クリックはScript差込、複製、名前変更、有効切替、削除を提供する。
- Component又はScriptの右クリックは名前変更、有効切替、削除を提供する。
- Primary Camera Object及びPrimary Camera Componentの削除は描画出力維持のため拒否する。
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
- Editorが生成する `UserPrograms` と `UserScripts` はHot Reload用のDebug構成だけを持ち、Debug CRTを静的リンクする。これによりRelease Engineから読み込む場合も開発PC固有の `VCRUNTIME140D.dll` 又は `ucrtbased.dll` を要求しない。
- ソースは UTF-8 としてコンパイルする。
- `d3dx12.h` は外部提供ファイルのためプロジェクト固有の命名・コメント修正対象外とする。
- `SampleRotationScript.dll` は外部Script Moduleの動作例としてApplicationと同じ構成、Platformの出力先へ生成する。

環境変数 `DX12_ENGINE_DIAGNOSTICS=1` を設定して起動すると、通常UI操作へ入る前にScene、Object、Component、名前API、寸法API、Script、DLL、描画、Resize、終了処理を連続検証して自動終了する。結果は `DX12_ENGINE_DIAGNOSTIC_REPORT` で指定したUTF-8 Logへ保存する。DLL診断を含める場合は `DX12_ENGINE_DIAGNOSTIC_SCRIPT_DLL` と `DX12_ENGINE_DIAGNOSTIC_EXTENSION_DLL` に対象Pathを設定する。

## 10. コーディング規則

- クラス、関数、型、ローカル変数、メンバー変数は UpperCamelCase。
- 関数引数だけ lowerCamelCase。
- コメントメモは全ての関数定義の直上へ必ず書き、短いSet/GetやConstructorも省略しない。公開HeaderのAPI説明は別途併記できる。
- 関数定義コメントは `概要：...`、`引数：引数名=意味、...`、`戻り値：...` の順とし、引数又は戻り値がない場合は `なし` と書く。
- 変数宣言の行末に意味を書く。
- 全 `.h` / `.cpp` の先頭に概要と更新内容を書く。
- 追加、削除、編集を行った理由と内容を更新履歴へ記載する。

## 11. Main／Subプログラム境界

通常のMainプログラムは `GameEngineAPI.h` を入口とし、Sceneごとの一つのCPPへ `Init`、`Update`、`End`を実装する。Object生成と設定はScene内一意名を使う高級APIを標準とし、DLL Export、Instance、Scene ID解決は固定Adapterへ隠す。上級者は`Advanced.Host()`から`EngineHostAPI`へ降り、Scene、Object、Component、Script Registry相当の詳細操作を組み合わせられる。毎フレームの順序は `Main Program → Active Scene → Object／Component／Sub Script` とする。

Subプログラムは `Script` 派生Componentであり、EditorのEngine TabからObjectへ差し込む。Native ScriptとDLL Scriptは同じComponent ID、Active状態、初期化、複製、削除経路を使う。

## 12. DLL Script ABI

各DLLは同じ文字列 `EngineGetScriptModule` をExportできる。HostはDLLごとの `HMODULE` を `LoadLibraryW` で取得し、各Handleを引数として `GetProcAddress` を呼ぶため、Export名が同じでも関数PointerはModule別に区別される。

取得した関数PointerはModuleを解放すると無効になるため、`ScriptModuleManager` はModuleごとのHandle、関数表、Registry Keyを保持する。既存Script Componentは共有所有権でModule寿命を延長し、最後の利用者が破棄されるまで `FreeLibrary` しない。

DLL境界ではC++ Classや標準Libraryの所有権を直接渡さない。`Create` で生成した不透明Instanceは同じDLLの `Destroy` で破棄し、Object操作はVersionとSizeを持つC ABIの `EngineScriptHostAPI` 関数表を通す。詳細と実装例は `SCRIPT_SYSTEM.md` 及び `Samples/RotationScriptModule` を参照する。

## 13. コンパイル時Feature要求

高水準APIの利用に応じた基礎機能追加は、Source文字列検索ではなくModule DescriptorのFeature Manifestとして扱う。各Featureは安定`FeatureID`、必要Component型、保存Layout版、更新関数、Editor公開Propertyを定義する。

- Main／Sub ProgramのBuild時に、利用した高水準WrapperがFeature要求をManifestへ出力する。
- ScriptをObjectへAttachするとき、EngineはScriptのFeature要求を統合し、未登録の基礎Componentと専用Storageだけを追加する。
- 同じFeatureを複数Scriptが要求してもObject上では共有し、最後の要求元が外れた場合だけ削除候補にする。
- 頻繁に更新するFeatureは型別Dense Storage、任意機能はSparse Storageを選択できる。外部HandleはStorage移動後も安定IDから解決する。
- 値にはDirty bit又はRevisionを持たせ、変更されていないCPU／GPU Dataは更新しない。
- 自動追加されたComponentもObject Treeへ表示し、Manifestで公開した数値をInspectorから編集できる。

第一段階として、本版は安定Handle、ID直接参照、型／Attach状態検索、同値設定の省略、描画色定数化を実装した。Feature Manifest生成とStorage自動選択は、Component Descriptor ABIを定めた後に実装する。
