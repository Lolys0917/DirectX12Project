# Assets

`.asset` はEngineデータ、`VisualScripts/*.vscript` は簡易ノードグラフの正規中間表現です。
Visual Scriptは `node <安定ID> <Operation> <引数...>` を読み込みます。現在の実行ノードは
`Rotate x y z`、`Move x y z`、`PulseScale min max speed` です。

安定したnode/link ID、型付きpublic変数、event節を保持するため、後からC++ ASTとの双方向変換を
追加してもファイル形式を置き換えず拡張できます。
