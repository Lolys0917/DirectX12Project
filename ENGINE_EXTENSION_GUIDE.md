# Background Compile・Hot Reload・外部APIメモ

更新日: 2026-09-02

## 実装場所

| 用途 | 場所 |
|---|---|
| DLL境界の追記専用C ABI | `EngineExtensionAPI.h` |
| DLL読込、検証、状態移行、Hot Reload | `ExtensionSystem.h` / `ExtensionSystem.cpp` |
| Native内部の読取・編集Facade | `EngineAPI.h` / `EngineAPI.cpp` |
| 自動保存、簡易構文判定、Background MSBuild | `ProgramWorkspace.h` / `ProgramWorkspace.cpp` |
| ProgramタブのDebounce、コード補完、結果表示、反映要求 | `WinApp.h` / `WinApp.cpp` |
| 全Scene共通Main Program | `Programs/Main.h` / `Programs/Main.cpp` |
| SceneごとのMain Program | `Programs/MainScene.cpp` |
| Engine所有Dear ImGui描画層 | `ImGuiLayer.h` / `ImGuiLayer.cpp` |
| Main Programから隠蔽するDLL Adapter | `Templates/EngineExtension/MainProgramAdapter.cpp` |
| Program DLLビルド定義 | `Programs/UserPrograms.vcxproj` |
| Object ScriptタブのBox・Keyboard・色変更例 | `ScriptPrograms/BoxKeyboardColorScript.cpp` |
| Object Script用の簡易Gameplay API | `GameScriptAPI.h` |
| 簡易Object Scriptの複製用テンプレート | `Templates/ObjectScript/` |
| Main／Sub、Thread、色、候補API説明 | `GAMEPLAY_API_GUIDE.md` |
| Script DLLビルド定義 | `ScriptPrograms/UserScripts.vcxproj` |
| 外部追加機能の複製用テンプレート | `Templates/EngineExtension/` |
| 組込み済みゲーム用Object雛形 | `GameObjectTemplate.h` / `GameObjectTemplate.cpp` |
| ゲーム用Object外部Program例 | `Templates/GameObject/` |

## 動作順

1. Program入力後900ms変更がなければUI上で文字列Snapshotを一度だけ複製し、保存WorkerがUTF-8で書き込む。
2. 通常文字列、文字Literal、行・ブロックコメントを除外して `()[]{}` の対応を調べ、未完成コードを模倣判定する。
3. 保存完了と判定通過後だけMSBuildをCompile Workerで実行する。編集UI、保存、Game Runtimeは互いを待たない。
4. 成功DLLを `Programs/.hotreload/UserPrograms_<revision>_<time>.dll` へ複写する。
5. メインスレッドのフレーム境界でDLLとABIを検証し、共通`Main::Init`からScene名に対応する`Init`を呼ぶ。
6. 共通`Main::Update`が`RunScenes`又は`RunScene`でActive Sceneを更新し、Scene削除、Hot Reload又はDLL解放時にSceneの`End`、最後に共通`Main::End`を呼ぶ。
7. 失敗時は稼働中の旧DLLを残す。

手動コンパイルボタンも同じBackground経路を使い、Debounceだけを省略する。自動処理は実コンパイラの代用ではないため、簡易判定後の型・Link ErrorはMSBuild診断へ表示する。

コンパイル成功時はSource一式を各Workspaceの `.lastgood/` へ保存する。「正常版」ボタンは現在Sourceを `.recovery/` へ退避してから、この最後の成功状態へ戻す。エディターの通常編集は `Ctrl+Z`、`Ctrl+Y`、`Ctrl+Shift+Z` に対応し、色分け更新はUndo履歴へ記録しない。

Program入力中は2文字以上のPrefixに対してC++キーワード、外部Engine API、現在Source内の識別子を候補表示する。上下／Page Up／Page Downで選び、Tabだけで確定する。Enterは候補を閉じて通常の改行を行う。マウスでは候補のクリック又はダブルクリックで書き込む。`Ctrl+Space`で候補一覧を明示表示できる。通常のスペース、記号、コメント、文字列、文字リテラル、プリプロセッサ行では候補を表示しない。

「メイン」と「スクリプト」は保存先とDLL ABIを分離する。メインは `EngineHostAPI` を使って毎フレームの先頭で全体制御し、スクリプトは `EngineScriptHostAPI` を使ってAttach先Objectの毎フレーム処理を行う。API Header、Template、両Workspaceへ追加した外部Sourceの識別子は再走査され、コード補完候補へ自動追加される。

## 公開API

`EngineHostAPI` はScene、Object、Componentの列挙と情報取得に加え、Scene有効化、View Scene変更、Object作成・削除・名前・有効状態・Transform・親、Component削除・名前・有効状態を編集できる。IDは外部では32bit整数、Native内部では強いID型として検証される。

Main Program向けの`GameEngineAPI.h`は、このC ABIを文字列中心の組込みC++ APIへ包む。Scene内のObject名は型に関係なく一意で、呼出時に名前から安定IDへ解決する。通常のSceneコードはHost PointerとIDを保持せず、次の高級APIだけで記述する。

```cpp
using namespace EngineGame;

void Game::MainScene::Init()
{
    AddObject.CreateCapsuleModel("PlayerCapsule");
    Object.SetSize("PlayerCapsule", 1.0f, 2.0f, 1.0f);
    Object.SetPosition("PlayerCapsule", 0.0f, 1.0f, 0.0f);
}
```

各Scene Sourceは`Init()`、`Update(float deltaTime)`、`End()`と末尾の`ENGINE_REGISTER_SCENE`だけを持つ。共通`Main.cpp`は`Init()`、`Update(float deltaTime)`、`End()`、`UserInterface()`と`ENGINE_REGISTER_MAIN`を持つ。初期化は`InitializeScene("SceneName")`を必要な順に呼んだ後、`InitializeScenes()`で残りを初期化できる。更新も`RunScene("SceneName", deltaTime)`を必要な順に呼び、最後に`RunScenes(deltaTime)`を呼ぶと、未更新Sceneだけを登録順で続けられる。DLL Export、Instance作成、Scene ID解決は固定Adapterが担当する。`SetSize`の3値はX幅、Y高さ、Z奥行きである。CapsuleとCylinderはX/Zの大きい方を直径、Yを全高として使用し、Sphere系は最大軸を直径として使用する。

Hierarchy整理用には`AddObject.CreateFolder`を使い、`ObjectHandle::SetParent`又は一括生成関数の`parentObjectID`へFolder IDを渡す。Folderは非描画Objectであり、子Objectの親子Transformと再帰削除は既存Hierarchy規則に従う。

`UserInterface()`では`ImGui.Begin`、`Text`、`Button`、`BeginTabBar`、`BeginTabItem`、`CollapsingHeader`、`ProgressBar`、`PlotLines`などを使用できる。Dear ImGui Context、Descriptor Heap、DirectX 12 BackendはEngineが所有し、停止中もUI構築Callbackを呼ぶ。C ABIへは関数Pointerと描画中だけ有効な数値配列Pointerのみを末尾追加し、外部DLLへDear ImGuiのC++型や所有権を渡さない。

組込み例の`Programs/Main.cpp`は0.5秒ごとに状態Snapshotを更新し、`Game State`でScene／Object／Component／Script数、Object型、代表ObjectのTransformを表示する。`PC State`ではWindowsの読取APIからシステム／プロセスCPU、物理Memory、Working Set、Private Memory、Thread、Handleを表示する。CPU、Memory、Frame timeは負荷Bar又は履歴Graphで描画し、Monitor自体が大量Object走査の恒常負荷にならないよう毎描画では再集計しない。

上級者は`Advanced.Host()`から追記専用`EngineHostAPI`を取得し、Scene、Object、Component、Scriptなど既存の低Level APIへ到達できる。旧形式の明示Hostコード向けに`EngineProgramAPI`も互換入口として残す。

ゲーム組込み向けとして、Engine RevisionとMain／View Scene ID取得、名前によるScene／Object検索、Scene作成・複製・削除・Main切替、Child列挙、Object複製、Script一覧とAttachを末尾へ追記した。`GameObjectTemplate` はGameplay Tag、移動速度、最大体力を持ち、Nativeと外部C APIの両方から作成・読取・編集・複製できる。

PrimitiveのRGBA色を読み書きする `GetObjectColor`／`SetObjectColor` と、Virtual Keyの押下を読む `IsKeyDown` も末尾へ追記した。Object Script側には所有Objectの型、色、Keyboard状態を扱う同等APIがある。

色はRGBを置換する`SetObjectColor`／`SetColor`と、現在色へRGBA係数を掛ける`MultiplyObjectColor`／`MultiplyColor`の二系統を持つ。`SetProgramSuggestion`は外部Main DLLからC++識別子をコード補完へ追加する。

Native C++側では `EngineAPI` に同等の `Get*IDs`、`TryGet*Info`、`Set*`、`Rename*`、`Remove*` を用意している。将来の改造WindowはこのFacade又は `EngineHostAPI` のどちらからでも構築できる。

Visual Scriptingでは`GameEngineAPI.h`の呼出をコード生成先とし、C ABIの関数名・引数型をNode定義の安定キーにする。コードからGraphを生成する段階ではこの組込みAPI呼出だけを抽出し、Graphからコードを生成する段階では同じ呼出へ戻す。実行中Flow表示はNode IDを別途実行Traceへ記録する層で行い、Engine内部状態やDLL ABIへUI固有型を混入させない。

## DLLとLIB差し替え方針

現在の実行ファイルはEngine本体を直接Linkしているため、稼働中のEngine本体そのものをDLL差し替えする段階にはまだ分離されていない。今回Hot ReloadできるのはProgram／追加機能DLLである。

将来 `Bootstrap.exe + EngineCore.dll + EngineSDK.lib` へ分離する場合も `EngineExtensionAPI.h` を境界にする。`EngineCore.dll` とImport Libraryの `EngineSDK.lib` は同じ配布単位で置換し、Editor再起動時に読み直す。稼働中のEngineCore自体を解放せず上書きしない。ABIは次を守る。

- `Size` と `AbiVersion` を必ず先頭に置く。
- 既存フィールドを削除、並べ替え、意味変更しない。
- 新APIは構造体末尾へ追加し、`Size` で存在確認する。
- STL型、C++例外、所有権をDLL境界へ出さない。
- DLLが確保したInstanceは同じDLLの `Destroy` で解放する。
- 非互換変更時だけABI版を上げ、旧版入口を一定期間残す。
