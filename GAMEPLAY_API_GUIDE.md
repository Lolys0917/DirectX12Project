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

## Main外部API

`EngineExtensionAPI.h`の`EngineHostAPI`は追記専用のC ABI関数表です。Scene、Object、Component、Scriptの列挙・作成・検索・編集に加え、Transform、親子関係、絶対色、乗算色、Keyboard入力を外部DLLから操作できます。

外部Main Programから候補を追加する場合は次のように登録します。

```cpp
host->SetProgramSuggestion(host->Context, "MyGameplayFunction");
```

登録名はC++識別子として有効な文字列に限られます。追加後はMain／スクリプト両タブの候補へ反映されます。候補は大文字小文字を区別せず検索され、Tabキーだけで確定します。

## 組み込み例

- `OscillatingBox`: MainSceneが作成し、`box.horizontal_oscillation` Sub Scriptを自動Attachします。水色を直接設定後に色係数を乗算し、X軸の左右へ往復します。
- `MainOscillatingCapsule`: `Programs/ExtensionMain.cpp`が外部APIから作成します。橙色を直接設定後に色係数を乗算し、Main ProgramからZ軸の前後へ往復します。

実装例は`ScriptPrograms/BoxKeyboardColorScript.cpp`と`Programs/ExtensionMain.cpp`にあります。
