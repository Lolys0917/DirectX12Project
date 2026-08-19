//|| EngineDiagnostics.h ||::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Engineの主要Lifecycleと組込みAPIを実GPU上で検証する診断機能を定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.00  新規作成
//||

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Engine
{
    class EngineAPI;

    struct EngineDiagnosticResult final
    {
        std::size_t PassedCheckCount = 0; //成功した個別検証数
        std::vector<std::string> Failures; //失敗した検証名
        std::vector<std::string> EngineLogs; //診断完了時のEngine通常ログ

        //全検証に成功したか確認する
        //戻り値: 失敗項目がない場合はtrue
        bool Passed() const;

        //保存と自動確認に使用するUTF-8診断Reportを作成する
        //戻り値: 成否、件数、失敗項目及びEngine Logを含む文字列
        std::string ToText() const;
    };

    //環境変数で明示された診断起動か確認する
    //戻り値: DX12_ENGINE_DIAGNOSTICSが0以外の場合はtrue
    bool IsEngineDiagnosticModeEnabled();

    //初期化済みEngineの主要LifecycleとAPIを連続検証する
    //引数: engine=Game Thread上の初期化済みEngine API
    //戻り値: 個別検証結果
    EngineDiagnosticResult RunEngineDiagnostics(EngineAPI& engine);

    //環境変数で指定されたPathへ診断Reportを保存する
    //引数: result=保存する診断結果
    //戻り値: Reportを保存できた場合はtrue
    bool WriteEngineDiagnosticReport(const EngineDiagnosticResult& result);
}
