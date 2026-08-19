# Main／Sub プログラム構成

更新日: 2026-08-19

## Main プログラム

`Engine.h` がネイティブMain層向けの統合Headerで、`EngineAPI` から `GameApp`、`DirectX12`、`SceneManager`、`ScriptRegistry`、`ScriptModuleManager` へ到達できる。描画APIやScene構築をC++へ直書きする場合はこの層を使う。

エディターの「メイン」タブは `Programs/` を編集し、`EngineGetExtensionModule` を公開するMain DLLを生成する。Mainは各フレームで最初に1回実行され、その後にActive Scene、Object、Component、Sub Scriptが更新される。この順序により、Mainで変更したSceneやObjectの状態を同じフレームのSub Scriptから参照できる。

`MainScene` はMainプログラム例である。Scene基底の更新を維持したまま、`ObjectManager` のScene内一意名検索を通じてデモObjectを直接回転させる。

## Sub プログラム

`Script` はObjectへComponentとして差し込むSubプログラム基底で、実行順は次のとおり。

エディターの「スクリプト」タブは `ScriptPrograms/` を編集し、UnityのMonoBehaviour相当のDLL Scriptを生成する。コンパイル成功後は外部Script Moduleとして自動登録され、「Engine」タブでObjectを選んで「Script差込」からAttachできる。

1. `OnAttach`: Objectへ追加後の初期化時に1回
2. `OnStart`: 最初のUpdate直前に1回
3. `OnUpdate`: ObjectとScriptが有効な各フレーム
4. `OnStop`: 実行済みScriptの終了時に1回
5. `OnDetach`: Object又はScriptの削除時に1回

Native Scriptは `Script` を継承し、`ScriptRegistry::RegisterNativeScript` へ登録する。`RotationScript` が最小例である。

## DLL Script

DLLは `ScriptModuleAPI.h` だけを共有し、`EngineGetScriptModule` をExportする。エンジンはDLLごとに `HMODULE` を保持し、そのHandleを指定して `GetProcAddress` を呼ぶため、すべてのDLLが同じExport名を使える。

DLLのInstanceはDLLの `Create` で生成し、同じDLLの `Destroy` で破棄する。C++の `new`／`delete` や標準ライブラリ所有権をDLL境界で混在させない。Object操作はVersionとSizeを持つC ABIの `EngineScriptHostAPI` 経由で行う。

`Samples/RotationScriptModule` は実際にビルド可能なDLL例である。Debug x64では `x64/Debug/SampleRotationScript.dll` が生成され、エディターの「Engine」タブにある「DLL読込」から読み込める。

`ScriptPrograms/BoxKeyboardColorScript.cpp` はスクリプトタブの既定例である。BoxだけにAttachでき、初期位置を記録して左右へ往復する。`SetColor`によるRGB直接指定と`MultiplyColor`による現在色への乗算を試せる。ゲーム組込み向けの `GameScriptAPI.h` では、`GetKeyPress(UpArrow)`、`this->Position`、`SetPosition`、`MoveWhenPressed`も短く記述できる。従来の詳細な `EngineScriptHostAPI` は削除せず、この簡易APIの内部と高度な用途の両方で利用できる。

再利用用のソースと記述例は `Templates/ObjectScript/BoxKeyboardColorScript.cpp` と `Templates/ObjectScript/README.md` に置く。Script Workspaceへ初期ソースを生成するときもこのテンプレートを使用する。

同じ `ModuleName` を持つ新しい外部DLLを読み込んだ場合、RegistryのFactoryを新世代へ置き換える。すでにAttach済みのInstanceは旧DLLの共有所有権を保ち、新しくAttachするScriptから新世代を使う。

## エディター操作

「Engine」タブはScene→Object→Component／ScriptのTreeを表示する。

- Tree空白又はSceneを右クリック: Object追加、DLL読込、更新
- Objectを右クリック: Script差込、複製、名前変更、有効切替、削除
- Component／Scriptを右クリック: 名前変更、有効切替、削除
- Deleteキー: 選択Object又はComponentの削除

メイン／スクリプト両タブの入力候補は固定API一覧だけでなく、`EngineAPI.h`、`EngineExtensionAPI.h`、`ScriptModuleAPI.h`、`GameScriptAPI.h`、`Templates/`、`Programs/`、`ScriptPrograms/` を走査する。外部からHeaderやTemplate Sourceを追加した場合も、識別子がTab補完へ反映される。候補確定はTabキーのみとする。

Primary Camera ObjectとPrimary Camera Componentは描画出力を維持するため削除を拒否する。
