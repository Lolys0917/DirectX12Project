//|| MessageLog.cpp ||::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  通常ログと一括消去対象外の常設ログを安全に共有する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.01  編集: 同一の常設ログが重複して蓄積しないように変更
//||  2026_07_13  v1.00  ログ追加、一括消去、常設ログ、更新検出を実装
//||

#include "MessageLog.h"

#include <algorithm>

namespace Engine
{
    //Process内で共有するMessageLogを取得する
    //戻り値: 共有MessageLogへの参照
    MessageLog& MessageLog::GetInstance()
    {
        static MessageLog Instance; //初回呼び出し時にThread Safeに生成する共有Log
        return Instance;
    }

    //空のログと初期更新番号を作成する
    MessageLog::MessageLog()
        : Revision(0)
    {
    }

    //通常ログを末尾へ追加する
    //引数: message 追加する文字列
    void MessageLog::AddLog(const std::string& message)
    {
        const std::lock_guard<std::mutex> Lock(LogMutex); //追加中のログ配列を保護するLock
        Logs.emplace_back(message);
        ++Revision;
    }

    //一括消去の対象外となる常設ログを末尾へ追加する
    //引数: message 追加する文字列
    void MessageLog::AddPermanentLog(const std::string& message)
    {
        const std::lock_guard<std::mutex> Lock(LogMutex); //追加中の常設ログ配列を保護するLock

        if (std::find(PermanentLogs.begin(), PermanentLogs.end(), message) !=
            PermanentLogs.end())
        {
            return;
        }

        PermanentLogs.emplace_back(message);
        ++Revision;
    }

    //通常ログだけを一括消去して常設ログを保持する
    void MessageLog::ClearLogs()
    {
        const std::lock_guard<std::mutex> Lock(LogMutex); //消去中の通常ログ配列を保護するLock

        if (Logs.empty())
        {
            return;
        }

        Logs.clear();
        ++Revision;
    }

    //表示用のログと更新番号を同一時点の値として複製する
    //戻り値: 常設ログ、通常ログ、更新番号を持つSnapshot
    MessageLogSnapshot MessageLog::GetSnapshot() const
    {
        const std::lock_guard<std::mutex> Lock(LogMutex); //複製中の全ログ状態を保護するLock
        MessageLogSnapshot Snapshot; //表示Threadへ所有権付きで渡す一貫したログ状態
        Snapshot.PermanentLogs = PermanentLogs;
        Snapshot.Logs = Logs;
        Snapshot.Revision = Revision;
        return Snapshot;
    }

    //表示内容の更新判定に使う現在の更新番号を取得する
    //戻り値: ログ内容が変化するたび増加する更新番号
    std::uint64_t MessageLog::GetRevision() const
    {
        const std::lock_guard<std::mutex> Lock(LogMutex); //更新番号の読み取りを保護するLock
        return Revision;
    }
}
