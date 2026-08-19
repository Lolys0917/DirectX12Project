# ゲーム組み込みAPI説明

更新日: 2026-08-19

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

通常のMain ProgramはSceneごとに一つのCPPを持ち、`Init`、`Update`、`End`だけを実装します。`GameEngineAPI.h`の組込みAPIへObject名を渡すと、Scene内で一意な安定IDへ内部解決されます。

```cpp
using namespace EngineGame;

namespace Game::MainScene
{
    void Init()
    {
        AddObject.CreateCapsuleModel("PlayerCapsule");
        Object.SetSize("PlayerCapsule", 1.0f, 2.0f, 1.0f);
    }

    void Update(float deltaTime) { (void)deltaTime; }
    void End() {}
}

ENGINE_REGISTER_SCENE(MainScene)
```

主な入口は`AddObject`、`Object`、`Scene`、`Input`、`Log`です。`Object`には`Exists`、`SetSize`、`SetPosition`、`SetTransform`、`Move`、`SetColor`、`MultiplyColor`、`Remove`、`AttachScript`があります。

上級者は次の入口から低Level C ABI関数表を直接利用できます。

```cpp
const EngineHostAPI* Host = Advanced.Host();
```

DLLのExportやScene ID解決は`Templates/EngineExtension/MainProgramAdapter.cpp`が受け持つため、通常は編集しません。互換用の`EngineProgramAPI`も残しているため、IDをCacheした高度なコードと名前中心のコードを混在できます。

## 組み込み例

- `OscillatingBox`: MainSceneが作成し、`box.horizontal_oscillation` Sub Scriptを自動Attachします。水色を直接設定後に色係数を乗算し、X軸の左右へ往復します。
- `MainOscillatingCapsule`: `Programs/MainScene.cpp`の`Init`が名前指定で作成します。橙色を直接設定後に色係数を乗算し、`Update`からZ軸の前後へ往復します。

実装例は`ScriptPrograms/BoxKeyboardColorScript.cpp`と`Programs/MainScene.cpp`にあります。
