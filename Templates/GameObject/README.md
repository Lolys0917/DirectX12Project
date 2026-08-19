# GameObjectテンプレート

`GameObjectTemplate.h` / `GameObjectTemplate.cpp` はEngine本体へ組み込み済みのNative Object雛形です。通常のTransform、親子関係、Component所有に加えて、ゲームでよく使うGameplay Tag、移動速度、最大体力を保持し、Object複製時にも設定が引き継がれます。

Native Mainでは `EngineAPI::CreateGameObjectTemplate` で生成し、返されたPointerから固有値を変更できます。外部Program DLLでは追記専用C ABIの `CreateGameObjectTemplate`、`GetGameObjectTemplateInfo`、`SetGameObjectTemplateInfo` を使います。

`GameObjectProgramTemplate.cpp` はProgramタブ又は外部拡張プロジェクトへコピーして使う例です。同名Objectを先に検索するため、Hot Reload時にObjectを重複生成しません。生成直後だけNative Rotation ScriptをAttachします。単独で確認する場合は `GameObjectProgramTemplate.vcxproj` をビルドしてください。
