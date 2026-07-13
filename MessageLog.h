//|| MessageLog.h ||::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  通常ログと一括消去対象外の常設ログを安全に共有する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v1.00  ログ追加、一括消去、常設ログ、更新検出を実装
//||

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Engine
{
    struct MessageLogSnapshot final
    {
        std::vector<std::string> PermanentLogs; //一括消去の対象外となる常設ログ一覧
        std::vector<std::string> Logs; //一括消去の対象となる通常ログ一覧
        std::uint64_t Revision = 0; //Snapshotを作成した時点の更新番号
    };

    class MessageLog final
    {
    public:
        //Process内で共有するMessageLogを取得する
        //戻り値: 共有MessageLogへの参照
        static MessageLog& GetInstance();

        //通常ログを末尾へ追加する
        //引数: message 追加する文字列
        void AddLog(const std::string& message);

        //一括消去の対象外となる常設ログを末尾へ追加する
        //引数: message 追加する文字列
        void AddPermanentLog(const std::string& message);

        //通常ログだけを一括消去して常設ログを保持する
        void ClearLogs();

        //表示用のログと更新番号を同一時点の値として複製する
        //戻り値: 常設ログ、通常ログ、更新番号を持つSnapshot
        MessageLogSnapshot GetSnapshot() const;

        //表示内容の更新判定に使う現在の更新番号を取得する
        //戻り値: ログ内容が変化するたび増加する更新番号
        std::uint64_t GetRevision() const;

        //共有状態の重複を防ぐためCopy構築を禁止する
        //引数: コピー元MessageLog
        MessageLog(const MessageLog&) = delete;

        //共有状態の重複を防ぐためCopy代入を禁止する
        //引数: コピー元MessageLog
        //戻り値: 代入先MessageLogへの参照
        MessageLog& operator=(const MessageLog&) = delete;

    private:
        //空のログと初期更新番号を作成する
        MessageLog();

        mutable std::mutex LogMutex; //複数Threadからログ状態を保護するMutex
        std::vector<std::string> PermanentLogs; //一括消去後も残る常設ログ配列
        std::vector<std::string> Logs; //一括消去できる通常ログ配列
        std::uint64_t Revision; //ログ内容の変更を識別する単調増加番号
    };
}
