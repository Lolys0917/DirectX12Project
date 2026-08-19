//|| ProgramSuggestionRegistry.h ||:::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Native、外部Main Program、Editor間で追加コード候補を安全に共有する
//||

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Engine
{
    class ProgramSuggestionRegistry final
    {
    public:
        //概要：Process内で共有するProgram候補Registryを取得する
        //引数：なし
        //戻り値：共有Registryへの参照
        static ProgramSuggestionRegistry& GetInstance();

        //概要：外部又はNative APIからコード補完候補を追加する
        //引数：suggestion=C++識別子として登録する候補
        //戻り値：新しい候補を追加した場合true、登録済みでも有効な場合true
        bool SetSuggestion(const std::string& suggestion);

        //概要：Editor表示用に全登録候補を所有権付きで複製する
        //引数：なし
        //戻り値：名前順のUTF-8候補一覧
        std::vector<std::string> GetSnapshot() const;

        //概要：候補の追加を検出する単調増加Revisionを取得する
        //引数：なし
        //戻り値：現在のRegistry Revision
        std::uint64_t GetRevision() const;

        ProgramSuggestionRegistry(const ProgramSuggestionRegistry&) = delete;
        ProgramSuggestionRegistry& operator=(const ProgramSuggestionRegistry&) = delete;

    private:
        //概要：空の候補集合と初期Revisionを作成する
        //引数：なし
        //戻り値：なし
        ProgramSuggestionRegistry();

        mutable std::mutex RegistryMutex; //複数Threadの候補追加と読取を保護するMutex
        std::vector<std::string> Suggestions; //外部から登録された一意な候補一覧
        std::uint64_t Revision; //候補集合の変更番号
    };
}
