# Live Editing / Organization Guide

## Objectの分類と処理順

EngineタブでObjectを選択すると、`Group`、`Tag`、`Layer`、`Group順`、`Object順`を編集できます。
同名Groupの`Group順`は一括で更新されます。右クリックの「Group全体を有効化／無効化」で
Group単位の状態変更もできます。

毎フレームの順序は次の安定キーで決定します。

1. Group順
2. Group名（非Groupは空名）
3. Object順
4. Object ID
5. Component ID（アタッチ順）

Renderable列とComponent/Script列は従来どおり分離し、各列の中だけを上記のキーで安定整列します。
ゲーム処理と描画はEditor UIから分離したGame Thread上で実行します。

Main Programからは次のように設定、検索できます。

```cpp
Player.SetOrganization("Gameplay", "Player", 3, 10, 0);
auto Enemies = Object.FindByGroup("Enemies");
auto Damageables = Object.FindByTag("Damageable");
auto WorldObjects = Object.FindByLayer(2);
```

## 差分Hot Reload

`AddObject.CreateMany`、`CreateBoxes`、`CreateCapsules`は宣言的な同期APIです。
同じ名前のObjectを再利用し、今回の宣言数から外れた前世代Objectは削除せず無効化します。

```cpp
constexpr std::uint32_t StressObjectCount = 512;
StressObjects = AddObject.CreateBoxes("StressBox", StressObjectCount);
```

`512`を小さくすると余剰Objectはinactiveで残り、大きく戻すと同じObject IDを再有効化します。
宣言そのものをコードから消した場合も、前世代だけに存在したProgram管理Objectはinactiveになります。
停止後に最初から再生したときだけ、既存のPlayback Snapshot復元でScene定義を作り直します。

## Scriptのpublic変数と関数

Native Scriptは`GetExposedMembers`、`SetExposedMember`、`InvokeExposedFunction`を実装します。
DLL Object Scriptではコンストラクタから型付き公開項目を登録できます。

```cpp
ExposeVariable("Speed", Speed);
ExposeVariable("Origin", Origin);
ExposeFunction("Reset", [this]() {
    ElapsedTime = 0.0f;
    Position = Origin;
});
```

EngineタブでScript Componentを選択すると、公開変数の編集と公開関数の実行ができます。
組込みScriptはRotation、Bobbing、Orbit、Pulse Scale、Color Pulse、Visual Scriptです。

## Asset / Visual Script

「アセット」タブで`.asset`、「Visual」タブで`.vscript`を新規作成、編集、保存、削除できます。
Visual Scriptは安定node IDを持つ正規グラフIRとして保存します。現在の実行nodeは次の3種類です。

- `node <id> Rotate x y z`
- `node <id> Move x y z`
- `node <id> PulseScale min max speed`

Objectへ`Visual Script`をアタッチし、公開`AssetPath`へファイルを指定して`Reload`を実行します。
形式には型付き`public`、`event`、安定IDを残しているため、将来のC++ AST→Graphと
Graph→C++生成を同じ中間表現へ接続できます。

## 入力再生による状態追従について

入力再生は有効ですが、Hot Reloadの標準経路ではなく、決定論的Replayを選べる上位モードにするのが安全です。
入力だけでは乱数、物理演算、非同期Asset完了、時刻、外部通信、Thread実行順を再現できません。
採用する場合は固定tickごとに入力、乱数seed、非決定イベントを記録し、一定間隔のWorld Snapshotから
裏側のHeadless Worldを高速再生します。現在の差分inactive方式は即時反映用として残し、Replay成功時だけ
再構築WorldをFrame境界で入れ替える二段構成が適しています。
