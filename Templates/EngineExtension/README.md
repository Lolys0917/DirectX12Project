# Engine Extension 追加テンプレート

`ExtensionTemplate.cpp` の `EngineGetExtensionModule` は全DLLで共通名です。EngineはDLLごとのモジュールハンドルからこの入口を取得するため、別DLLに同名関数があっても衝突しません。

1. このフォルダーを複製します。
2. `ModuleName` と処理を変更します。
3. `EngineExtensionTemplate.vcxproj` をビルドします。
4. 生成DLLをProgram Workspace又は将来の改造Windowから `LoadExtensionModule` として読み込みます。

ABI構造体の既存フィールドは削除・並べ替えせず、末尾への追加だけを行ってください。DLLで確保したメモリは同じDLLの `Destroy` で解放します。
