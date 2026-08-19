//|| ProgramSuggestionRegistry.cpp ||:::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  外部登録されたコード補完候補をThread Safeに保存する
//||

#include "ProgramSuggestionRegistry.h"

#include <algorithm>
#include <cctype>

namespace Engine
{
    //概要：Process内で共有するProgram候補Registryを取得する
    //引数：なし
    //戻り値：共有Registryへの参照
    ProgramSuggestionRegistry& ProgramSuggestionRegistry::GetInstance()
    {
        static ProgramSuggestionRegistry Instance; //初回だけ安全に作成される共有Registry
        return Instance;
    }

    //概要：空の候補集合と初期Revisionを作成する
    //引数：なし
    //戻り値：なし
    ProgramSuggestionRegistry::ProgramSuggestionRegistry()
        : RegistryMutex()
        , Suggestions()
        , Revision(0)
    {
    }

    //概要：外部又はNative APIからコード補完候補を追加する
    //引数：suggestion=C++識別子として登録する候補
    //戻り値：新しい候補を追加した場合true、登録済みでも有効な場合true
    bool ProgramSuggestionRegistry::SetSuggestion(const std::string& suggestion)
    {
        if (suggestion.empty() || suggestion.size() > 80 ||
            !(std::isalpha(static_cast<unsigned char>(suggestion.front())) ||
                suggestion.front() == '_'))
        {
            return false;
        }

        for (char Character : suggestion)
        {
            if (!std::isalnum(static_cast<unsigned char>(Character)) &&
                Character != '_')
            {
                return false;
            }
        }

        const std::lock_guard<std::mutex> Lock(RegistryMutex); //候補集合を変更するGuard

        if (std::find(Suggestions.begin(), Suggestions.end(), suggestion) !=
            Suggestions.end())
        {
            return true;
        }

        Suggestions.emplace_back(suggestion);
        std::sort(Suggestions.begin(), Suggestions.end());
        ++Revision;
        return true;
    }

    //概要：Editor表示用に全登録候補を所有権付きで複製する
    //引数：なし
    //戻り値：名前順のUTF-8候補一覧
    std::vector<std::string> ProgramSuggestionRegistry::GetSnapshot() const
    {
        const std::lock_guard<std::mutex> Lock(RegistryMutex); //複製中の候補集合を保護するGuard
        return Suggestions;
    }

    //概要：候補の追加を検出する単調増加Revisionを取得する
    //引数：なし
    //戻り値：現在のRegistry Revision
    std::uint64_t ProgramSuggestionRegistry::GetRevision() const
    {
        const std::lock_guard<std::mutex> Lock(RegistryMutex); //Revision読取を保護するGuard
        return Revision;
    }
}
