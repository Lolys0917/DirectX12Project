# Object Script簡易APIテンプレート

このテンプレートはゲーム組み込みを主目的にした高水準APIの例です。`GameScriptAPI.h`だけを読み込めば、入力、Transform、色をObject単位で扱えます。

```cpp
if (GetKeyPress(UpArrow))
{
    Float3 pos = this->Position;
    this->SetPosition(pos.x, pos.y + 1.0f, pos.z);
}
```

同じ処理は、毎Frameの移動量を含めて1行にもできます。

```cpp
MoveWhenPressed(UpArrow, 0.0f, 1.0f * deltaTime, 0.0f);
```

従来の`EngineScriptHostAPI`は`GameScriptAPI.h`の内部でそのまま利用されています。高度な処理では`ScriptModuleAPI.h`を直接使う既存方式も選べます。
