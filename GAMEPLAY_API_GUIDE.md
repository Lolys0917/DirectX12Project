# ゲーム組み込みAPI説明

更新日: 2026-08-20

## MainとSubの違い

Main Programは`Programs/`からDLL化され、各固定フレームの最初に1回実行されます。Scene作成、Object作成、検索、Transform、色、親子関係、Component、ScriptのAttachなど、ゲーム全体を操作します。

Sub Scriptは`ScriptPrograms/`からDLL化され、必ずObjectへAttachして使用します。AttachされていないSub Scriptは実行されません。処理順は`OnAttach`、`OnStart`、毎フレームの`Update`、`OnStop`、`OnDetach`です。

## Thread構成

- Windows UI Thread: タブ、Object Tree、Program入力、候補一覧を処理します。
- Source Save Worker: 入力停止後に複製された文字列をUTF-8で保存します。
- Compile Worker: 保存完了後にMain又はSub DLLをビルドします。
- Game Runtime Thread: Main、Scene、Attach済みSub Script、DirectX描画をこの順番で処理します。

UIとGameはObjectのPointerを直接共有しません。UIからは操作Queue、Gameからは読み取り用Snapshotを渡すことで競合を防ぎます。

## Sub Script簡易API

`GameScriptAPI.h`を読み込み、`ObjectScript`を継承します。

```cpp
class MoveScript final : public ObjectScript
{
public:
    //概要：Attach先Objectを操作するScriptを作成する
    //引数：host=Attach先ObjectのAPI
    //戻り値：なし
    explicit MoveScript(const EngineScriptHostAPI* host)
        : ObjectScript(host)
    {
    }

    //概要：Attach先を毎Frame上へ移動する
    //引数：deltaTime=前Frameからの秒数
    //戻り値：なし
    void Update(float deltaTime)
    {
        Move(0.0f, deltaTime, 0.0f);
    }
};
```

主な簡易関数は`GetKeyPress`、`SetPosition`、`Move`、`MoveWhenPressed`、`SetColor`、`MultiplyColor`です。`this->Position`は読取と代入の両方に使えます。

## 色の変更方法

RGBを直接指定する場合は`SetColor`を使います。

```cpp
SetColor(0.2f, 0.8f, 1.0f, 1.0f);
```

現在色へ係数を乗算する場合は`MultiplyColor`を使います。

```cpp
MultiplyColor(0.75f, 1.0f, 0.85f, 1.0f);
```

Main Programでは同じ機能を`SetObjectColor`と`MultiplyObjectColor`から利用します。

## Main Scene簡易API

`EngineExtensionAPI.h`の`EngineHostAPI`は追記専用のC ABI関数表です。Scene、Object、Component、Scriptの列挙・作成・検索・編集に加え、Transform、親子関係、絶対色、乗算色、Keyboard入力を外部DLLから操作できます。

通常のMain ProgramはSceneごとに一つのCPPを持ち、`Init`、`Update`、`End`を実装します。`GameEngineAPI.h`の組込みAPIへObject名を渡すと、Scene内で一意な安定IDへ内部解決されます。

Object生成関数は数値IDではなく`ObjectHandle`を返します。HandleはScene IDとObject IDを保持する非所有参照なので、変数や`std::vector`へ保存できます。Engine内部の生PointerをDLLへ公開しないため、再生停止時のScene復元やHot Reload後に古いPointerを誤参照しません。取得済みHandleの情報読取はID索引から直接行います。

```cpp
#include "GameEngineAPI.h"

#include <vector>

using namespace EngineGame;

namespace Game::MainScene
{
    ObjectHandle Player;
    std::vector<ObjectHandle> Enemies;

    void Init()
    {
        Player = AddObject.CreateCapsuleModel("PlayerCapsule");
        Player.SetSize(1.0f, 2.0f, 1.0f);

        Enemies = AddObject.CreateBoxes("Enemy", 100);
    }

    void Update(float deltaTime)
    {
        for (ObjectHandle& Enemy : Enemies)
        {
            Enemy.Move(deltaTime, 0.0f, 0.0f);
        }
    }

    void End() {}
}

ENGINE_REGISTER_SCENE(MainScene)
```

主な入口は`AddObject`、`Object`、`Scene`、`Input`、`Log`です。

- `AddObject.Create...`: 1個生成して`ObjectHandle`を返します。
- `AddObject.CreateMany`、`CreateBoxes`、`CreateCapsules`: まとめて生成し`std::vector<ObjectHandle>`を返します。
- `Object.Find("ExactName")`: 一意名から1個を取得します。
- `Object.FindAll("NamePart")`: 名前の一部が一致する全Objectを取得します。既定では大文字小文字を区別しません。
- `Object.FindByType(...)`: Objectの具象型で取得します。
- `Object.FindByComponent(...)`: 指定ComponentクラスがAttachされたObjectを取得します。
- `Object.FindByScript("script.key")`: 指定ScriptがAttachされたObjectを取得します。
- `ObjectHandle.GetComponent(...)`、`GetComponents(...)`: `ComponentHandle`を1個又は配列で取得します。

```cpp
const auto NamedEnemies = Object.FindAll("Enemy");
const auto CameraObjects = Object.FindByComponent(
    EngineExternalComponentType::Camera
);

for (const ObjectHandle& Object : CameraObjects)
{
    ComponentHandle Camera = Object.GetComponent(
        EngineExternalComponentType::Camera
    );
    Camera.SetActive(true);
}
```

個別Objectを簡潔に操作したい場合は`Object.SetPosition("Player", ...)`のような名前指定APIも引き続き利用できます。`ObjectHandle`には`SetSize`、`SetPosition`、`SetTransform`、`Move`、`SetColor`、`MultiplyColor`、`SetActive`、`AttachScript`、`Remove`があります。

上級者は次の入口から低Level C ABI関数表を直接利用できます。

```cpp
const EngineHostAPI* Host = Advanced.Host();
```

DLLのExportやScene ID解決は`Templates/EngineExtension/MainProgramAdapter.cpp`が受け持つため、通常は編集しません。互換用の`EngineProgramAPI`も残しているため、IDをCacheした高度なコードと名前中心のコードを混在できます。

## Init、終了順序、再生状態

高水準Main Programの状態はConstructor／Destructorではなく`Init`と`End`で管理します。再生停止後にも同じDLLからInstanceが再生成されるため、経過時間やHandle配列は`Init`で明示的に初期化してください。`Update`内の`static`局所変数は再生停止だけでは初期値へ戻らないため、再生状態には使用しません。

通常は`ENGINE_REGISTER_SCENE`で十分です。Scene間参照の解除順序が必要な場合だけ`StartDestroy`と`EndDestroy`を実装し、`ENGINE_REGISTER_SCENE_LIFECYCLE`を使います。

```cpp
void StartDestroy() { Enemies.clear(); } //全SceneのEndより前に参照を切る
void End() { /* 通常の終了処理 */ }
void EndDestroy() { /* 全SceneのEnd後に行う最終解放 */ }
```

複数Sceneの終了順は、全Sceneの`StartDestroy`、逆登録順の`End`、逆登録順の`EndDestroy`です。

- `Pause`: Scene、Object、変数を保持したまま固定更新だけを止めます。`Resume`で続きから再開します。
- `Stop`: 再生開始直前のScene定義へ戻し、Main Programを破棄・再生成して`Init`を実行します。
- `Tick`: 停止又は一時停止中に1固定Frameだけ進めます。

Keyboard入力はゲーム画面がFocusを持つ間だけ有効です。ゲーム画面をクリックするとProgram／TabからFocusが移り、Editor側のShortcutが同時に動作しません。

## 描画更新の扱い

位置、回転、ScaleはObjectの行列だけを更新します。同じ寸法や色を再指定した場合は何もしません。色変更は頂点Bufferを再作成・上書きせずDraw単位のRoot Constantsへ反映し、同一Device上のPrimitiveはRoot SignatureとPipeline Stateを共有します。形状の寸法や分割数が変わった場合だけMeshを再構築します。

Program Editorでは`AddObject.`、`Object.`などの`.`入力直後に、その入口で利用できる関数だけを候補表示します。Handle変数の`.`ではObject／Component操作候補を表示します。

## 組み込み例

- `OscillatingBox`: MainSceneが作成し、`box.horizontal_oscillation` Sub Scriptを自動Attachします。水色を直接設定後に色係数を乗算し、X軸の左右へ往復します。
- `MainOscillatingCapsule`: `Programs/MainScene.cpp`の`Init`が名前指定で作成します。橙色を直接設定後に色係数を乗算し、`Update`からZ軸の前後へ往復します。

実装例は`ScriptPrograms/BoxKeyboardColorScript.cpp`と`Programs/MainScene.cpp`にあります。
