//|| ProgramWorkspace.cpp ||:::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  Program永続化、簡易構文判定、Background DLL Buildを実装する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_19  v1.10  組込み名前API Templateと静的Debug CRT DLL生成へ更新
//||  2026_08_17  v1.00  新規作成
//||

#include "ProgramWorkspace.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace Engine
{
    namespace
    {
        //概要：UTF-16文字列を保存用UTF-8へ変換する
        //引数：text=変換するUTF-16文字列
        //戻り値：UTF-8文字列、変換失敗時は空文字列
        std::string WideToUtf8(const std::wstring& text)
        {
            if (text.empty())
            {
                return std::string();
            }

            const int Required = WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            ); //UTF-8変換後に必要なByte数

            if (Required <= 0)
            {
                return std::string();
            }

            std::string Result(static_cast<std::size_t>(Required), '\0'); //UTF-8出力Buffer
            WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                Result.data(),
                Required,
                nullptr,
                nullptr
            );
            return Result;
        }

        //概要：保存済みUTF-8をEditor用UTF-16へ変換する
        //引数：text=変換するUTF-8文字列
        //戻り値：UTF-16文字列、変換失敗時は空文字列
        std::wstring Utf8ToWide(const std::string& text)
        {
            if (text.empty())
            {
                return std::wstring();
            }

            int Required = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0
            ); //UTF-16変換後に必要な文字数
            UINT CodePage = CP_UTF8; //変換に使用するCode Page

            if (Required <= 0)
            {
                CodePage = CP_ACP;
                Required = MultiByteToWideChar(
                    CodePage,
                    0,
                    text.data(),
                    static_cast<int>(text.size()),
                    nullptr,
                    0
                );
            }

            if (Required <= 0)
            {
                return std::wstring();
            }

            std::wstring Result(static_cast<std::size_t>(Required), L'\0'); //UTF-16出力Buffer
            MultiByteToWideChar(
                CodePage,
                CodePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                text.data(),
                static_cast<int>(text.size()),
                Result.data(),
                Required
            );
            return Result;
        }

        //概要：Scene名からWindowsファイル名とC++識別子に使えるASCII名を作成する
        //引数：name=整形するUTF-8 Scene名
        //戻り値：空文字や記号を補正したScene基底名
        std::string SanitizeIdentifier(const std::string& name)
        {
            std::string Result; //英数字とUnderscoreだけを持つ識別子
            Result.reserve(name.size() + 1);
            bool RemovedCharacter = false; //UTF-8又は記号を除外した場合true

            for (unsigned char Character : name)
            {
                if (std::isalnum(Character) || Character == '_')
                {
                    Result.push_back(static_cast<char>(Character));
                }
                else
                {
                    RemovedCharacter = true;
                }
            }

            if (Result.empty())
            {
                Result = "Generated";
            }

            if (std::isdigit(static_cast<unsigned char>(Result.front())))
            {
                Result.insert(Result.begin(), '_');
            }

            if (RemovedCharacter)
            {
                std::uint32_t Hash = 2166136261u; //Scene名を区別するFNV-1a Hash

                for (unsigned char Character : name)
                {
                    Hash ^= Character;
                    Hash *= 16777619u;
                }

                char Suffix[16]{}; //識別子へ付加する固定幅Hash
                sprintf_s(Suffix, "_%08X", Hash);
                Result += Suffix;
            }

            return Result;
        }

        //概要：UTF-8文字列をC++の二重引用符内へ安全に埋め込める形へ変換する
        //引数：text=変換する文字列
        //戻り値：Backslash、引用符、改行をEscapeした文字列
        std::string EscapeCppString(const std::string& text)
        {
            std::string Result; //C++文字列Literalへ埋め込む内容
            Result.reserve(text.size());

            for (char Character : text)
            {
                switch (Character)
                {
                case '\\': Result += "\\\\"; break;
                case '"': Result += "\\\""; break;
                case '\r': Result += "\\r"; break;
                case '\n': Result += "\\n"; break;
                case '\t': Result += "\\t"; break;
                default: Result.push_back(Character); break;
                }
            }

            return Result;
        }

        //概要：MSBuild Command Lineへ渡すPathを二重引用符で囲む
        //引数：path=引用するPath
        //戻り値：空白を含んでも一引数になる文字列
        std::wstring QuotePath(const std::filesystem::path& path)
        {
            return L"\"" + path.wstring() + L"\"";
        }

        //概要：PathがProgram Workspaceで管理するC++ Source拡張子か判定する
        //引数：path=拡張子を確認するFile Path
        //戻り値：cpp、cxx、h、hppのいずれかの場合true
        bool IsProgramSourceExtension(const std::filesystem::path& path)
        {
            std::wstring Extension = path.extension().wstring(); //小文字化して比較する拡張子
            std::transform(
                Extension.begin(),
                Extension.end(),
                Extension.begin(),
                [](wchar_t character)
                {
                    return static_cast<wchar_t>(std::towlower(character));
                }
            );
            return Extension == L".cpp" || Extension == L".cxx" ||
                Extension == L".h" || Extension == L".hpp";
        }

    }

    //概要：Main又はObject Script用のBackground Compile Workspaceを作成する
    //引数：kind=Main Program又はObject ScriptのWorkspace種別
    //戻り値：なし
    ProgramWorkspace::ProgramWorkspace(ProgramWorkspaceKind kind)
        : ProjectRoot()
        , ProgramDirectory()
        , BuildProjectPath()
        , Kind(kind)
        , CompileWorker()
        , CompileResultMutex()
        , CompletedCompileResult()
        , Compiling(false)
        , SaveWorker()
        , SaveResultMutex()
        , CompletedSaveResult()
        , Saving(false)
    {
    }

    //概要：実行中Compile Workerの終了を待ってProgram Workspaceを破棄する
    //引数：なし
    //戻り値：なし
    ProgramWorkspace::~ProgramWorkspace()
    {
        Shutdown();
    }

    //概要：Project Rootを検出してPrograms保存先とCompile Projectを準備する
    //引数：なし
    //戻り値：Program Workspaceを使用可能にできた場合はtrue
    bool ProgramWorkspace::Initialize()
    {
        if (!DiscoverProjectRoot())
        {
            return false;
        }

        const bool ScriptWorkspace = Kind == ProgramWorkspaceKind::ObjectScript; //Script専用設定を使う場合true
        ProgramDirectory = ProjectRoot /
            (ScriptWorkspace ? L"ScriptPrograms" : L"Programs");
        BuildProjectPath = ProgramDirectory /
            (ScriptWorkspace ? L"UserScripts.vcxproj" : L"UserPrograms.vcxproj");
        std::error_code Error; //例外を使用しないDirectory作成結果
        std::filesystem::create_directories(ProgramDirectory, Error);
        std::filesystem::path DefaultSource; //Workspace種別に対応する既定Source
        return !Error && WriteBuildProject() && EnsureDefaultSource(DefaultSource);
    }

    //概要：WorkspaceがMain Program又はObject Scriptのどちらを扱うか取得する
    //引数：なし
    //戻り値：初期化時から変わらないWorkspace種別
    ProgramWorkspaceKind ProgramWorkspace::GetKind() const
    {
        return Kind;
    }

    //概要：Programソースを保存するDirectoryを取得する
    //引数：なし
    //戻り値：Project内Programs Directory
    const std::filesystem::path& ProgramWorkspace::GetDirectory() const
    {
        return ProgramDirectory;
    }

    //概要：管理対象のC++ソースとHeaderを名前順で取得する
    //引数：なし
    //戻り値：Programs直下のcpp、cxx、h、hpp一覧
    std::vector<std::filesystem::path> ProgramWorkspace::GetSourceFiles() const
    {
        std::vector<std::filesystem::path> Result; //UIへ返すProgramファイル一覧
        std::error_code Error; //Directory走査結果

        if (ProgramDirectory.empty() || !std::filesystem::exists(ProgramDirectory, Error))
        {
            return Result;
        }

        for (std::filesystem::directory_iterator Iterator(ProgramDirectory, Error), End;
            !Error && Iterator != End;
            Iterator.increment(Error))
        {
            if (!Iterator->is_regular_file(Error))
            {
                continue;
            }

            std::wstring Extension = Iterator->path().extension().wstring(); //小文字化する拡張子
            std::transform(
                Extension.begin(),
                Extension.end(),
                Extension.begin(),
                [](wchar_t character)
                {
                    return static_cast<wchar_t>(std::towlower(character));
                }
            );

            if (Extension == L".cpp" || Extension == L".cxx" ||
                Extension == L".h" || Extension == L".hpp")
            {
                Result.emplace_back(Iterator->path());
            }
        }

        std::sort(Result.begin(), Result.end());
        return Result;
    }

    //概要：重複しないNewScene.cpp又はNewScript.cppを作成する
    //引数：createdPath=作成したPathの格納先
    //戻り値：新しいProgramファイルを保存できた場合はtrue
    bool ProgramWorkspace::CreateSourceFile(std::filesystem::path& createdPath)
    {
        if (ProgramDirectory.empty())
        {
            return false;
        }

        const bool ScriptWorkspace = Kind == ProgramWorkspaceKind::ObjectScript; //Script雛形を作る場合true
        std::filesystem::path Candidate = ProgramDirectory /
            (ScriptWorkspace ? L"NewScript.cpp" : L"NewScene.cpp"); //新規ファイル候補
        std::error_code Error; //存在確認結果
        unsigned int Suffix = 1; //同名時に付加する番号

        while (std::filesystem::exists(Candidate, Error) && !Error)
        {
            Candidate = ProgramDirectory /
                (std::wstring(ScriptWorkspace ? L"NewScript_" : L"NewScene_") +
                    std::to_wstring(Suffix++) + L".cpp");
        }

        const std::wstring SourceIdentifier = Candidate.stem().wstring(); //重複番号込み識別子

        const std::wstring Template = ScriptWorkspace
            ? L"#include \"ScriptModuleAPI.h\"\r\n\r\n"
              L"//概要：新しいObject Scriptの毎Frame処理を実行する\r\n"
              L"//引数：instance=Script状態、deltaTime=前Frameからの秒数\r\n"
              L"//戻り値：なし\r\n"
              L"void ENGINE_SCRIPT_CALL UpdateNewScript(void* instance, float deltaTime)\r\n"
              L"{\r\n"
              L"    (void)instance;\r\n"
              L"    (void)deltaTime;\r\n"
              L"}\r\n"
            : L"#include \"GameEngineAPI.h\"\r\n\r\n"
              L"using namespace EngineGame;\r\n\r\n"
              L"namespace Game::" + SourceIdentifier + L"\r\n"
              L"{\r\n"
              L"    void Init()\r\n"
              L"    {\r\n"
              L"    }\r\n\r\n"
              L"    void Update(float deltaTime)\r\n"
              L"    {\r\n"
              L"        (void)deltaTime;\r\n"
              L"    }\r\n\r\n"
              L"    void End()\r\n"
              L"    {\r\n"
              L"    }\r\n"
              L"}\r\n\r\n"
              L"ENGINE_REGISTER_SCENE(" + SourceIdentifier + L")\r\n"; //Workspace種別に対応するSource初期内容

        if (!SaveSourceFile(Candidate, Template))
        {
            return false;
        }

        createdPath = Candidate;
        return true;
    }

    //概要：Sceneに対応するMain Programファイルがなければ生成する
    //引数：sceneName=解決済みScene名、sourcePath=対応ファイルPathの格納先
    //戻り値：既存又は新規ファイルを使用可能にできた場合はtrue
    bool ProgramWorkspace::EnsureSceneSource(
        const std::string& sceneName,
        std::filesystem::path& sourcePath
    )
    {
        if (Kind != ProgramWorkspaceKind::MainProgram)
        {
            sourcePath.clear();
            return false;
        }

        std::string Identifier = SanitizeIdentifier(sceneName); //ファイル名と関数名に使うScene名

        if (Identifier.size() < 5 || Identifier.substr(Identifier.size() - 5) != "Scene")
        {
            Identifier += "Scene";
        }

        sourcePath = ProgramDirectory / (Utf8ToWide(Identifier) + L".cpp");
        std::error_code Error; //既存ファイル確認結果

        if (std::filesystem::exists(sourcePath, Error))
        {
            return !Error;
        }

        const std::wstring WideIdentifier = Utf8ToWide(Identifier); //C++関数名用Scene識別子
        const std::wstring WideSceneName = Utf8ToWide(
            EscapeCppString(sceneName)
        ); //Engine上の実Scene名
        const std::wstring Template =
            L"#include \"GameEngineAPI.h\"\r\n\r\n"
            L"using namespace EngineGame;\r\n\r\n"
            L"namespace Game::" + WideIdentifier + L"\r\n"
            L"{\r\n"
            L"    void Init()\r\n"
            L"    {\r\n"
            L"    }\r\n\r\n"
            L"    void Update(float deltaTime)\r\n"
            L"    {\r\n"
            L"        (void)deltaTime;\r\n"
            L"    }\r\n\r\n"
            L"    void End()\r\n"
            L"    {\r\n"
            L"    }\r\n"
            L"}\r\n\r\n"
            L"ENGINE_REGISTER_NAMED_SCENE(" + WideIdentifier + L", \"" +
            WideSceneName + L"\")\r\n"; //Scene作成時に生成するMain Program雛形
        return SaveSourceFile(sourcePath, Template);
    }

    //概要：管理対象Sourceを安全な単一ファイル名へ変更する
    //引数：sourcePath=変更元Path、requestedName=新しいファイル名、renamedPath=変更後Pathの格納先
    //戻り値：重複せず名前を変更できた場合はtrue
    bool ProgramWorkspace::RenameSourceFile(
        const std::filesystem::path& sourcePath,
        const std::wstring& requestedName,
        std::filesystem::path& renamedPath
    )
    {
        if (!IsManagedSourcePath(sourcePath) || requestedName.empty())
        {
            return false;
        }

        const std::filesystem::path NamePath(requestedName); //Directory混入を確認する名前Path

        if (NamePath.has_parent_path() || NamePath.filename() != NamePath ||
            NamePath.extension().empty())
        {
            return false;
        }

        std::wstring Extension = NamePath.extension().wstring(); //小文字化して検証する拡張子
        std::transform(
            Extension.begin(),
            Extension.end(),
            Extension.begin(),
            [](wchar_t character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            }
        );

        if (Extension != L".cpp" && Extension != L".cxx" &&
            Extension != L".h" && Extension != L".hpp")
        {
            return false;
        }

        renamedPath = ProgramDirectory / NamePath.filename();
        std::error_code Error; //Rename処理結果

        if (renamedPath.lexically_normal() == sourcePath.lexically_normal())
        {
            return true;
        }

        if (std::filesystem::exists(renamedPath, Error))
        {
            return false;
        }

        std::filesystem::rename(sourcePath, renamedPath, Error);
        return !Error;
    }

    //概要：Programs Directory内のSourceファイルを削除する
    //引数：sourcePath=削除する管理対象Path
    //戻り値：ファイルを削除できた場合はtrue
    bool ProgramWorkspace::DeleteSourceFile(const std::filesystem::path& sourcePath)
    {
        if (!IsManagedSourcePath(sourcePath))
        {
            return false;
        }

        std::error_code Error; //削除処理結果
        return std::filesystem::remove(sourcePath, Error) && !Error;
    }

    //概要：UTF-8 SourceファイルをEditor用UTF-16へ読み込む
    //引数：sourcePath=読み込む管理対象Path、text=読み込んだ文字列の格納先
    //戻り値：ファイル全体を読み込めた場合はtrue
    bool ProgramWorkspace::LoadSourceFile(
        const std::filesystem::path& sourcePath,
        std::wstring& text
    ) const
    {
        if (!IsManagedSourcePath(sourcePath))
        {
            return false;
        }

        std::ifstream Stream(sourcePath, std::ios::binary); //UTF-8 Source入力Stream

        if (!Stream)
        {
            return false;
        }

        const std::string Bytes(
            (std::istreambuf_iterator<char>(Stream)),
            std::istreambuf_iterator<char>()
        ); //Source全Byte
        text = Utf8ToWide(Bytes);
        return Stream.good() || Stream.eof();
    }

    //概要：EditorのUTF-16文字列をUTF-8 Sourceファイルへ保存する
    //引数：sourcePath=保存する管理対象Path、text=保存する文字列
    //戻り値：ファイル全体を保存できた場合はtrue
    bool ProgramWorkspace::SaveSourceFile(
        const std::filesystem::path& sourcePath,
        const std::wstring& text
    ) const
    {
        if (!IsManagedSourcePath(sourcePath))
        {
            return false;
        }

        std::ofstream Stream(sourcePath, std::ios::binary | std::ios::trunc); //UTF-8 Source出力Stream

        if (!Stream)
        {
            return false;
        }

        const std::string Bytes = WideToUtf8(text); //保存するUTF-8 Byte列
        Stream.write(Bytes.data(), static_cast<std::streamsize>(Bytes.size()));
        return Stream.good();
    }

    //概要：Editor文字列Snapshotを所有権付きで受け取りBackground保存を開始する
    //引数：sourcePath=保存先Source、text=UI Threadで確定した全文、revision=Editor版番号
    //戻り値：保存Workerを開始できた場合true
    bool ProgramWorkspace::StartBackgroundSave(
        const std::filesystem::path& sourcePath,
        std::wstring text,
        std::uint64_t revision
    )
    {
        if (Saving.load() || !IsManagedSourcePath(sourcePath))
        {
            return false;
        }

        {
            std::lock_guard<std::mutex> Lock(SaveResultMutex); //未取得保存結果を保護するGuard

            if (CompletedSaveResult.has_value())
            {
                return false;
            }
        }

        if (SaveWorker.joinable())
        {
            SaveWorker.join();
        }

        Saving.store(true);
        SaveWorker = std::thread(
            &ProgramWorkspace::RunSaveWorker,
            this,
            sourcePath,
            std::move(text),
            revision
        );
        return true;
    }

    //概要：Background保存の完了結果を非Blockingで一度だけ取得する
    //引数：result=保存Path、Revision、成否の格納先
    //戻り値：未取得の完了結果が存在した場合true
    bool ProgramWorkspace::PollBackgroundSave(ProgramSaveResult& result)
    {
        std::lock_guard<std::mutex> Lock(SaveResultMutex); //保存結果取得を保護するGuard

        if (!CompletedSaveResult.has_value())
        {
            return false;
        }

        result = std::move(*CompletedSaveResult);
        CompletedSaveResult.reset();
        return true;
    }

    //概要：Source保存Workerが現在File書込み中か判定する
    //引数：なし
    //戻り値：Background保存中の場合true
    bool ProgramWorkspace::IsSaving() const
    {
        return Saving.load();
    }

    //概要：明示保存、名前変更、終了前に実行中のSource保存完了を待つ
    //引数：なし
    //戻り値：なし
    void ProgramWorkspace::WaitForBackgroundSave()
    {
        if (SaveWorker.joinable())
        {
            SaveWorker.join();
        }

        Saving.store(false);
    }

    //概要：Main Scene又はObject Scriptの既定Sourceを必要時だけ生成する
    //引数：sourcePath=既存又は生成した既定Source Pathの格納先
    //戻り値：Workspace既定Sourceを使用可能にできた場合はtrue
    bool ProgramWorkspace::EnsureDefaultSource(std::filesystem::path& sourcePath)
    {
        if (Kind == ProgramWorkspaceKind::ObjectScript)
        {
            return EnsureScriptTemplate(sourcePath);
        }

        sourcePath = ProgramDirectory / L"MainScene.cpp";
        std::error_code Error; //既存Scene Source確認結果

        if (std::filesystem::exists(sourcePath, Error))
        {
            return !Error;
        }

        const std::wstring Template =
            L"#include \"GameEngineAPI.h\"\r\n\r\n"
            L"using namespace EngineGame;\r\n\r\n"
            L"namespace Game::MainScene\r\n"
            L"{\r\n"
            L"    void Init()\r\n"
            L"    {\r\n"
            L"    }\r\n\r\n"
            L"    void Update(float deltaTime)\r\n"
            L"    {\r\n"
            L"        (void)deltaTime;\r\n"
            L"    }\r\n\r\n"
            L"    void End()\r\n"
            L"    {\r\n"
            L"    }\r\n"
            L"}\r\n\r\n"
            L"ENGINE_REGISTER_SCENE(MainScene)\r\n"; //既定Main Scene雛形
        return SaveSourceFile(sourcePath, Template);
    }

    //概要：Boxの色とKeyboard移動を試せるObject Script Module雛形を必要時だけ生成する
    //引数：sourcePath=既存又は生成したScript Source Pathの格納先
    //戻り値：Script Module Templateを使用可能にできた場合はtrue
    bool ProgramWorkspace::EnsureScriptTemplate(std::filesystem::path& sourcePath)
    {
        sourcePath = ProgramDirectory / L"BoxKeyboardColorScript.cpp";
        std::error_code Error; //Template存在確認結果

        if (std::filesystem::exists(sourcePath, Error))
        {
            return !Error;
        }

        const std::filesystem::path GameplayTemplatePath = ProjectRoot /
            L"Templates" / L"ObjectScript" /
            L"BoxKeyboardColorScript.cpp"; //簡易Game Script APIを使うCanonical Template
        std::ifstream GameplayTemplate(GameplayTemplatePath, std::ios::binary); //Template Source入力

        if (GameplayTemplate)
        {
            const std::string Bytes(
                (std::istreambuf_iterator<char>(GameplayTemplate)),
                std::istreambuf_iterator<char>()
            ); //UTF-8 Template全体

            if (SaveSourceFile(sourcePath, Utf8ToWide(Bytes)))
            {
                return true;
            }
        }

        const std::wstring Template =
            L"#include \"ScriptModuleAPI.h\"\r\n\r\n"
            L"#include <cstdint>\r\n"
            L"#include <new>\r\n\r\n"
            L"namespace\r\n"
            L"{\r\n"
            L"    struct BoxKeyboardColorState final\r\n"
            L"    {\r\n"
            L"        const EngineScriptHostAPI* Host = nullptr;\r\n"
            L"    };\r\n\r\n"
            L"    //概要：Box操作ScriptのInstanceを作成する\r\n"
            L"    //引数：host=所有Objectへ接続されたScript Host API\r\n"
            L"    //戻り値：作成したScript状態、API不整合又は確保失敗時はnullptr\r\n"
            L"    void* ENGINE_SCRIPT_CALL CreateBoxKeyboardColor(const EngineScriptHostAPI* host)\r\n"
            L"    {\r\n"
            L"        if (host == nullptr || host->Size < sizeof(EngineScriptHostAPI) ||\r\n"
            L"            host->AbiVersion != EngineScriptAbiVersion)\r\n"
            L"        {\r\n"
            L"            return nullptr;\r\n"
            L"        }\r\n\r\n"
            L"        auto* State = new (std::nothrow) BoxKeyboardColorState{};\r\n\r\n"
            L"        if (State != nullptr)\r\n"
            L"        {\r\n"
            L"            State->Host = host;\r\n"
            L"        }\r\n\r\n"
            L"        return State;\r\n"
            L"    }\r\n\r\n"
            L"    //概要：Box操作ScriptのInstanceを破棄する\r\n"
            L"    //引数：instance=CreateBoxKeyboardColorが返した状態\r\n"
            L"    //戻り値：なし\r\n"
            L"    void ENGINE_SCRIPT_CALL DestroyBoxKeyboardColor(void* instance)\r\n"
            L"    {\r\n"
            L"        delete static_cast<BoxKeyboardColorState*>(instance);\r\n"
            L"    }\r\n\r\n"
            L"    //概要：所有ObjectがBoxの場合だけScriptの接続を許可する\r\n"
            L"    //引数：instance=Script状態\r\n"
            L"    //戻り値：所有ObjectがBoxの場合は1、それ以外は0\r\n"
            L"    std::uint32_t ENGINE_SCRIPT_CALL AttachBoxKeyboardColor(void* instance)\r\n"
            L"    {\r\n"
            L"        const auto* State = static_cast<const BoxKeyboardColorState*>(instance);\r\n"
            L"        return State != nullptr && State->Host->GetObjectType(State->Host->Context) == 1u;\r\n"
            L"    }\r\n\r\n"
            L"    //概要：WASDでBoxを移動しR、G、B Keyで頂点色を変更する\r\n"
            L"    //引数：instance=Script状態、deltaTime=前Frameからの秒数\r\n"
            L"    //戻り値：なし\r\n"
            L"    void ENGINE_SCRIPT_CALL UpdateBoxKeyboardColor(void* instance, float deltaTime)\r\n"
            L"    {\r\n"
            L"        auto* State = static_cast<BoxKeyboardColorState*>(instance);\r\n\r\n"
            L"        if (State == nullptr || State->Host == nullptr)\r\n"
            L"        {\r\n"
            L"            return;\r\n"
            L"        }\r\n\r\n"
            L"        const EngineScriptHostAPI* Host = State->Host;\r\n"
            L"        float Position[3]{};\r\n"
            L"        Host->GetPosition(Host->Context, Position);\r\n"
            L"        const float Distance = 3.0f * deltaTime;\r\n"
            L"        if (Host->IsKeyDown(Host->Context, 'A') != 0) Position[0] -= Distance;\r\n"
            L"        if (Host->IsKeyDown(Host->Context, 'D') != 0) Position[0] += Distance;\r\n"
            L"        if (Host->IsKeyDown(Host->Context, 'W') != 0) Position[2] += Distance;\r\n"
            L"        if (Host->IsKeyDown(Host->Context, 'S') != 0) Position[2] -= Distance;\r\n"
            L"        Host->SetPosition(Host->Context, Position);\r\n\r\n"
            L"        float Color[4]{};\r\n"
            L"        if (Host->GetColor(Host->Context, Color) == 0) return;\r\n"
            L"        std::uint32_t ColorChanged = 0;\r\n"
            L"        if (Host->IsKeyDown(Host->Context, 'R') != 0) { Color[0] = 1.0f; Color[1] = 0.15f; Color[2] = 0.15f; ColorChanged = 1; }\r\n"
            L"        if (Host->IsKeyDown(Host->Context, 'G') != 0) { Color[0] = 0.15f; Color[1] = 1.0f; Color[2] = 0.15f; ColorChanged = 1; }\r\n"
            L"        if (Host->IsKeyDown(Host->Context, 'B') != 0) { Color[0] = 0.15f; Color[1] = 0.15f; Color[2] = 1.0f; ColorChanged = 1; }\r\n"
            L"        if (ColorChanged != 0) Host->SetColor(Host->Context, Color);\r\n"
            L"    }\r\n\r\n"
            L"    const EngineScriptDescriptor Scripts[] =\r\n"
            L"    {\r\n"
            L"        { sizeof(EngineScriptDescriptor), \"box.keyboard_color\", \"Box Keyboard Color\", CreateBoxKeyboardColor, DestroyBoxKeyboardColor, AttachBoxKeyboardColor, nullptr, UpdateBoxKeyboardColor, nullptr, nullptr }\r\n"
            L"    };\r\n"
            L"}\r\n\r\n"
            L"//概要：EngineへBox操作Script Moduleの関数表を公開する\r\n"
            L"//引数：requestedAbiVersion=Engineが要求するScript ABI版番号\r\n"
            L"//戻り値：互換Module定義、版不一致時はnullptr\r\n"
            L"ENGINE_SCRIPT_EXPORT const EngineScriptModuleDescriptor* ENGINE_SCRIPT_CALL EngineGetScriptModule(std::uint32_t requestedAbiVersion)\r\n"
            L"{\r\n"
            L"    static const EngineScriptModuleDescriptor Module\r\n"
            L"    {\r\n"
            L"        sizeof(EngineScriptModuleDescriptor),\r\n"
            L"        EngineScriptAbiVersion,\r\n"
            L"        \"EditorScriptPrograms\",\r\n"
            L"        static_cast<std::uint32_t>(sizeof(Scripts) / sizeof(Scripts[0])),\r\n"
            L"        Scripts\r\n"
            L"    };\r\n"
            L"    return requestedAbiVersion == EngineScriptAbiVersion ? &Module : nullptr;\r\n"
            L"}\r\n"; //Box入力、色変更、Module Exportを含むScript Tab Template
        return SaveSourceFile(sourcePath, Template);
    }

    //概要：未完成C++を実Compileへ渡さないため文字列と括弧構造を模倣判定する
    //引数：text=現在Editorにある単一Source全文
    //戻り値：Compile可否、位置、判定理由
    ProgramPreflightResult ProgramWorkspace::AnalyzeCompileReadiness(
        const std::wstring& text
    ) const
    {
        ProgramPreflightResult Result; //Editorへ返す事前判定結果

        if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos)
        {
            Result.Message = L"空のSourceはコンパイルしません。";
            return Result;
        }

        enum class LexicalState : std::uint8_t
        {
            Code,
            String,
            Character,
            LineComment,
            BlockComment
        }; //簡易C++字句状態

        struct Bracket final
        {
            wchar_t Character;
            std::size_t Index;
            int Line;
        }; //対応待ち括弧

        LexicalState State = LexicalState::Code; //現在の字句状態
        std::vector<Bracket> Brackets; //未閉鎖括弧Stack
        bool Escaped = false; //文字列内で直前がEscapeの場合true
        int Line = 1; //現在の1基点行番号

        for (std::size_t Index = 0; Index < text.size(); ++Index)
        {
            const wchar_t Character = text[Index]; //現在判定する文字
            const wchar_t Next = Index + 1 < text.size() ? text[Index + 1] : L'\0'; //次の文字

            if (Character == L'\n')
            {
                ++Line;

                if (State == LexicalState::LineComment)
                {
                    State = LexicalState::Code;
                }
            }

            if (State == LexicalState::LineComment)
            {
                continue;
            }

            if (State == LexicalState::BlockComment)
            {
                if (Character == L'*' && Next == L'/')
                {
                    State = LexicalState::Code;
                    ++Index;
                }

                continue;
            }

            if (State == LexicalState::String || State == LexicalState::Character)
            {
                const wchar_t Terminator = State == LexicalState::String ? L'"' : L'\''; //現在Literalの終端

                if (!Escaped && Character == Terminator)
                {
                    State = LexicalState::Code;
                }

                Escaped = !Escaped && Character == L'\\';

                if (Character != L'\\')
                {
                    Escaped = false;
                }

                continue;
            }

            if (Character == L'/' && Next == L'/')
            {
                State = LexicalState::LineComment;
                ++Index;
                continue;
            }

            if (Character == L'/' && Next == L'*')
            {
                State = LexicalState::BlockComment;
                ++Index;
                continue;
            }

            if (Character == L'"' || Character == L'\'')
            {
                State = Character == L'"' ? LexicalState::String : LexicalState::Character;
                Escaped = false;
                continue;
            }

            if (Character == L'(' || Character == L'[' || Character == L'{')
            {
                Brackets.push_back(Bracket{ Character, Index, Line });
                continue;
            }

            if (Character == L')' || Character == L']' || Character == L'}')
            {
                const wchar_t Expected = Character == L')'
                    ? L'('
                    : (Character == L']' ? L'[' : L'{'); //対応すべき開始括弧

                if (Brackets.empty() || Brackets.back().Character != Expected)
                {
                    Result.CharacterIndex = Index;
                    Result.Line = Line;
                    Result.Message = L"対応しない閉じ括弧があります。";
                    return Result;
                }

                Brackets.pop_back();
            }
        }

        if (State == LexicalState::String || State == LexicalState::Character ||
            State == LexicalState::BlockComment)
        {
            Result.CharacterIndex = text.size();
            Result.Line = Line;
            Result.Message = State == LexicalState::BlockComment
                ? L"ブロックコメントが閉じていません。"
                : L"文字列Literalが閉じていません。";
            return Result;
        }

        if (!Brackets.empty())
        {
            Result.CharacterIndex = Brackets.back().Index;
            Result.Line = Brackets.back().Line;
            Result.Message = L"開き括弧が閉じていません。";
            return Result;
        }

        Result.Ready = true;
        Result.Message = L"簡易構文判定を通過しました。";
        return Result;
    }

    //概要：全Program SourceをMSBuildでDLL化して世代別Hot Reload Pathへ複写する
    //引数：revision=対象Editor Revision、automatic=自動Compileの場合true
    //戻り値：起動可否、終了Code、標準出力、世代別DLL Pathを含むCompile結果
    ProgramCompileResult ProgramWorkspace::Compile(
        std::uint64_t revision,
        bool automatic
    ) const
    {
        ProgramCompileResult Result; //UIへ返すDLL Compile結果
        Result.PreflightSucceeded = true;
        Result.Automatic = automatic;
        Result.Revision = revision;
        const std::filesystem::path MSBuildPath = FindMSBuild(); //使用可能なMSBuild実体

        if (MSBuildPath.empty() || BuildProjectPath.empty())
        {
            Result.Output = L"MSBuild.exeが見つかりません。Visual Studio C++ Build Toolsを確認してください。";
            return Result;
        }

#if defined(_WIN64)
        const wchar_t* Platform = L"x64"; //現在Editorと同じTarget Platform
#else
        const wchar_t* Platform = L"Win32"; //現在Editorと同じTarget Platform
#endif

        std::wstring CommandLine = QuotePath(MSBuildPath) + L" " +
            QuotePath(BuildProjectPath) +
            L" /nologo /m:1 /t:Build /p:Configuration=Debug /p:Platform=" +
            Platform + L" /v:minimal"; //同期実行するMSBuild Command Line
        std::vector<wchar_t> MutableCommand(
            CommandLine.begin(),
            CommandLine.end()
        ); //CreateProcessWが変更可能なCommand Buffer
        MutableCommand.push_back(L'\0');

        SECURITY_ATTRIBUTES Security{}; //子Processへ書込Pipeだけを継承する設定
        Security.nLength = sizeof(SECURITY_ATTRIBUTES);
        Security.bInheritHandle = TRUE;
        HANDLE ReadPipe = nullptr; //親ProcessがCompile出力を読むPipe
        HANDLE WritePipe = nullptr; //子Processが標準出力を書き込むPipe

        if (!CreatePipe(&ReadPipe, &WritePipe, &Security, 0) ||
            !SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, 0))
        {
            if (ReadPipe != nullptr) CloseHandle(ReadPipe);
            if (WritePipe != nullptr) CloseHandle(WritePipe);
            Result.Output = L"Compile出力Pipeを作成できませんでした。";
            return Result;
        }

        STARTUPINFOW Startup{}; //標準出力をPipeへ接続する子Process設定
        Startup.cb = sizeof(STARTUPINFOW);
        Startup.dwFlags = STARTF_USESTDHANDLES;
        Startup.hStdOutput = WritePipe;
        Startup.hStdError = WritePipe;
        Startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION Process{}; //起動したMSBuild Process情報
        const BOOL Created = CreateProcessW(
            MSBuildPath.c_str(),
            MutableCommand.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            ProjectRoot.c_str(),
            &Startup,
            &Process
        ); //Worker Threadで待機するMSBuild起動結果
        CloseHandle(WritePipe);

        if (!Created)
        {
            CloseHandle(ReadPipe);
            Result.Output = L"MSBuild Processを起動できませんでした。";
            return Result;
        }

        Result.Started = true;
        std::string OutputBytes; //MSBuild標準出力全体
        std::array<char, 4096> Buffer{}; //Pipe読み取りBuffer
        DWORD ReadSize = 0; //今回Pipeから読み取ったByte数

        while (ReadFile(
            ReadPipe,
            Buffer.data(),
            static_cast<DWORD>(Buffer.size()),
            &ReadSize,
            nullptr
        ) && ReadSize > 0)
        {
            OutputBytes.append(Buffer.data(), ReadSize);
        }

        WaitForSingleObject(Process.hProcess, INFINITE);
        DWORD ExitCode = 1; //取得失敗時にCompile失敗とする終了Code
        GetExitCodeProcess(Process.hProcess, &ExitCode);
        CloseHandle(ReadPipe);
        CloseHandle(Process.hThread);
        CloseHandle(Process.hProcess);
        Result.ExitCode = ExitCode;
        Result.Succeeded = ExitCode == 0;
        Result.Output = Utf8ToWide(OutputBytes);

        if (Result.Succeeded)
        {
            const bool ScriptWorkspace = Kind == ProgramWorkspaceKind::ObjectScript; //Script DLL名を使う場合true
            const std::filesystem::path BuiltModule = ProgramDirectory /
                L".build" / Platform / L"Debug" /
                (ScriptWorkspace ? L"UserScripts.dll" : L"UserPrograms.dll"); //MSBuild生成DLL
            const std::filesystem::path ReloadDirectory = ProgramDirectory /
                L".hotreload"; //Load中DLLと次回Build出力を分離するDirectory
            std::error_code Error; //Directory作成又はDLL複写結果
            std::filesystem::create_directories(ReloadDirectory, Error);

            for (std::filesystem::directory_iterator Iterator(ReloadDirectory, Error), End;
                !Error && Iterator != End;
                Iterator.increment(Error))
            {
                if (Iterator->is_regular_file(Error) &&
                    Iterator->path().extension() == L".dll")
                {
                    std::error_code RemoveError; //Load中Fileを残す旧世代削除結果
                    std::filesystem::remove(Iterator->path(), RemoveError);
                }
            }

            Error.clear();
            const std::wstring GenerationName =
                std::wstring(ScriptWorkspace ? L"UserScripts_" : L"UserPrograms_") +
                std::to_wstring(revision) + L"_" +
                std::to_wstring(GetTickCount64()) + L".dll"; //上書きされない世代別名
            Result.ModulePath = ReloadDirectory / GenerationName;

            if (Error || !std::filesystem::copy_file(
                BuiltModule,
                Result.ModulePath,
                std::filesystem::copy_options::none,
                Error
            ))
            {
                Result.Succeeded = false;
                Result.Output += L"\r\nHot Reload用DLLを世代別Pathへ複写できませんでした。";
                Result.ModulePath.clear();
            }
        }

        if (Result.Succeeded && !CaptureLastSuccessfulSnapshot())
        {
            Result.Output += L"\r\n警告: 最終コンパイル成功Sourceの復元用保存に失敗しました。";
        }

        return Result;
    }

    //概要：UIを停止せず指定RevisionのProgram DLL Buildを開始する
    //引数：revision=保存済みEditor Revision、automatic=自動要求の場合true
    //戻り値：新しいWorkerを開始できた場合はtrue
    bool ProgramWorkspace::StartBackgroundCompile(
        std::uint64_t revision,
        bool automatic
    )
    {
        if (Compiling.load())
        {
            return false;
        }

        {
            std::lock_guard<std::mutex> Lock(CompileResultMutex); //未取得結果の上書きを防ぐGuard

            if (CompletedCompileResult.has_value())
            {
                return false;
            }
        }

        if (CompileWorker.joinable())
        {
            CompileWorker.join();
        }

        Compiling.store(true);
        CompileWorker = std::thread(
            &ProgramWorkspace::RunCompileWorker,
            this,
            revision,
            automatic
        );
        return true;
    }

    //概要：Background Compileの完了結果を非Blockingで一度だけ取得する
    //引数：result=完了結果の格納先
    //戻り値：未取得の完了結果が存在した場合はtrue
    bool ProgramWorkspace::PollBackgroundCompile(ProgramCompileResult& result)
    {
        std::lock_guard<std::mutex> Lock(CompileResultMutex); //完了結果の排他Guard

        if (!CompletedCompileResult.has_value())
        {
            return false;
        }

        result = std::move(*CompletedCompileResult);
        CompletedCompileResult.reset();
        return true;
    }

    //概要：MSBuild Workerが現在実行中か判定する
    //引数：なし
    //戻り値：Background Compile中の場合はtrue
    bool ProgramWorkspace::IsCompiling() const
    {
        return Compiling.load();
    }

    //概要：最後にCompile成功したSource一式のSnapshotが存在するか判定する
    //引数：なし
    //戻り値：復元可能なSourceが一つ以上存在する場合true
    bool ProgramWorkspace::HasLastSuccessfulSnapshot() const
    {
        const std::filesystem::path SnapshotDirectory =
            ProgramDirectory / L".lastgood"; //Compile成功Sourceの保存先
        std::error_code Error; //Snapshot走査結果

        for (std::filesystem::directory_iterator Iterator(SnapshotDirectory, Error), End;
            !Error && Iterator != End;
            Iterator.increment(Error))
        {
            if (Iterator->is_regular_file(Error) &&
                IsProgramSourceExtension(Iterator->path()))
            {
                return true;
            }
        }

        return false;
    }

    //概要：現在SourceをRecoveryへ退避して最後にCompile成功した一式へ戻す
    //引数：recoveryDirectory=復元前Sourceを保存したDirectoryの格納先
    //戻り値：Workspace全Sourceを成功Snapshotへ戻せた場合true
    bool ProgramWorkspace::RestoreLastSuccessfulSnapshot(
        std::filesystem::path& recoveryDirectory
    )
    {
        recoveryDirectory.clear();

        if (Compiling.load() || !HasLastSuccessfulSnapshot())
        {
            return false;
        }

        const std::filesystem::path SnapshotDirectory =
            ProgramDirectory / L".lastgood"; //復元元Compile成功Snapshot
        std::vector<std::filesystem::path> SnapshotSources; //復元するSnapshot Source一覧
        std::error_code Error; //Directory作成、走査、複写結果

        for (std::filesystem::directory_iterator Iterator(SnapshotDirectory, Error), End;
            !Error && Iterator != End;
            Iterator.increment(Error))
        {
            if (Iterator->is_regular_file(Error) &&
                IsProgramSourceExtension(Iterator->path()))
            {
                SnapshotSources.emplace_back(Iterator->path());
            }
        }

        if (Error || SnapshotSources.empty())
        {
            return false;
        }

        const std::vector<std::filesystem::path> CurrentSources = GetSourceFiles(); //復元前の全Source
        recoveryDirectory = ProgramDirectory / L".recovery" /
            (L"BeforeRestore_" + std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(recoveryDirectory, Error);

        if (Error)
        {
            recoveryDirectory.clear();
            return false;
        }

        for (const std::filesystem::path& Source : CurrentSources)
        {
            if (!std::filesystem::copy_file(
                Source,
                recoveryDirectory / Source.filename(),
                std::filesystem::copy_options::overwrite_existing,
                Error))
            {
                std::filesystem::remove_all(recoveryDirectory, Error);
                recoveryDirectory.clear();
                return false;
            }
        }

        for (const std::filesystem::path& Source : CurrentSources)
        {
            std::filesystem::remove(Source, Error);

            if (Error)
            {
                break;
            }
        }

        if (!Error)
        {
            for (const std::filesystem::path& Source : SnapshotSources)
            {
                if (!std::filesystem::copy_file(
                    Source,
                    ProgramDirectory / Source.filename(),
                    std::filesystem::copy_options::overwrite_existing,
                    Error))
                {
                    break;
                }
            }
        }

        if (!Error)
        {
            return true;
        }

        Error.clear();

        for (const std::filesystem::path& Source : GetSourceFiles())
        {
            std::filesystem::remove(Source, Error);
            Error.clear();
        }

        for (const std::filesystem::path& Source : CurrentSources)
        {
            std::filesystem::copy_file(
                recoveryDirectory / Source.filename(),
                Source,
                std::filesystem::copy_options::overwrite_existing,
                Error
            );
            Error.clear();
        }

        return false;
    }

    //概要：現在の全Sourceを最後にCompile成功した復元用Snapshotとして保存する
    //引数：なし
    //戻り値：Source一式を置換可能なSnapshotとして保存できた場合true
    bool ProgramWorkspace::CaptureLastSuccessfulSnapshot() const
    {
        const std::vector<std::filesystem::path> Sources = GetSourceFiles(); //成功Build対象Source一覧

        if (Sources.empty())
        {
            return false;
        }

        const std::filesystem::path PendingDirectory =
            ProgramDirectory / L".lastgood.pending"; //完成前Snapshot保存先
        const std::filesystem::path SnapshotDirectory =
            ProgramDirectory / L".lastgood"; //完成済みSnapshot保存先
        std::error_code Error; //Directory入替及びSource複写結果
        std::filesystem::remove_all(PendingDirectory, Error);
        Error.clear();
        std::filesystem::create_directories(PendingDirectory, Error);

        if (Error)
        {
            return false;
        }

        for (const std::filesystem::path& Source : Sources)
        {
            if (!std::filesystem::copy_file(
                Source,
                PendingDirectory / Source.filename(),
                std::filesystem::copy_options::overwrite_existing,
                Error))
            {
                std::filesystem::remove_all(PendingDirectory, Error);
                return false;
            }
        }

        std::filesystem::remove_all(SnapshotDirectory, Error);

        if (Error)
        {
            std::filesystem::remove_all(PendingDirectory, Error);
            return false;
        }

        std::filesystem::rename(PendingDirectory, SnapshotDirectory, Error);
        return !Error;
    }

    //概要：実行中Compile Workerの終了を待ち未取得結果を破棄する
    //引数：なし
    //戻り値：なし
    void ProgramWorkspace::Shutdown()
    {
        WaitForBackgroundSave();

        if (CompileWorker.joinable())
        {
            CompileWorker.join();
        }

        Compiling.store(false);
        {
            std::lock_guard<std::mutex> Lock(CompileResultMutex); //完了Compile結果の排他Guard
            CompletedCompileResult.reset();
        }
        {
            std::lock_guard<std::mutex> Lock(SaveResultMutex); //完了保存結果の排他Guard
            CompletedSaveResult.reset();
        }
    }

    //概要：MSBuildと世代別DLL複写をWorker Thread上で実行して結果を公開する
    //引数：revision=対象Editor Revision、automatic=自動要求の場合true
    //戻り値：なし
    void ProgramWorkspace::RunCompileWorker(
        std::uint64_t revision,
        bool automatic
    )
    {
        ProgramCompileResult Result = Compile(revision, automatic); //Worker上のCompile結果

        {
            std::lock_guard<std::mutex> Lock(CompileResultMutex); //完了結果公開の排他Guard
            CompletedCompileResult = std::move(Result);
        }

        Compiling.store(false);
    }

    //概要：UIから複製済みのSource文字列をWorker ThreadでUTF-8保存する
    //引数：sourcePath=保存先Source、text=所有権付き全文、revision=Editor版番号
    //戻り値：なし
    void ProgramWorkspace::RunSaveWorker(
        std::filesystem::path sourcePath,
        std::wstring text,
        std::uint64_t revision
    )
    {
        ProgramSaveResult Result; //UI Threadへ返すBackground保存結果
        Result.SourcePath = std::move(sourcePath);
        Result.Revision = revision;
        Result.Succeeded = SaveSourceFile(Result.SourcePath, text);

        {
            std::lock_guard<std::mutex> Lock(SaveResultMutex); //完了結果公開を保護するGuard
            CompletedSaveResult = std::move(Result);
        }

        Saving.store(false);
    }

    //概要：実行位置からDirectX12Project.vcxprojを持つProject Rootを検出する
    //引数：なし
    //戻り値：Project Rootを検出できた場合はtrue
    bool ProgramWorkspace::DiscoverProjectRoot()
    {
        std::vector<std::filesystem::path> Candidates; //上位検索を開始するDirectory候補
        std::error_code Error; //Current Path取得結果
        Candidates.emplace_back(std::filesystem::current_path(Error));
        wchar_t ExecutablePath[32768]{}; //Editor実行ファイルPath
        const DWORD Length = GetModuleFileNameW(
            nullptr,
            ExecutablePath,
            static_cast<DWORD>(std::size(ExecutablePath))
        ); //実行ファイルPath文字数

        if (Length > 0 && Length < std::size(ExecutablePath))
        {
            Candidates.emplace_back(
                std::filesystem::path(ExecutablePath).parent_path()
            );
        }

        for (std::filesystem::path Candidate : Candidates)
        {
            for (unsigned int Depth = 0; Depth < 8 && !Candidate.empty(); ++Depth)
            {
                if (std::filesystem::exists(
                    Candidate / L"DirectX12Project.vcxproj",
                    Error
                ))
                {
                    ProjectRoot = std::filesystem::weakly_canonical(Candidate, Error);
                    return !Error;
                }

                const std::filesystem::path Parent = Candidate.parent_path(); //次に調べる上位Directory

                if (Parent == Candidate)
                {
                    break;
                }

                Candidate = Parent;
            }
        }

        return false;
    }

    //概要：Programs内cppをHot Reload可能なDynamic LibraryとしてCompileするMSBuild Projectを生成する
    //引数：なし
    //戻り値：Build Projectを書き込めた場合はtrue
    bool ProgramWorkspace::WriteBuildProject() const
    {
        if (BuildProjectPath.empty() || ProjectRoot.empty())
        {
            return false;
        }

        std::wostringstream Xml; //生成するMSBuild Project XML
        const bool ScriptWorkspace = Kind == ProgramWorkspaceKind::ObjectScript; //Script Projectを生成する場合true
        Xml << LR"(<?xml version="1.0" encoding="utf-8"?>)" << L"\n";
        Xml << LR"(<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">)" << L"\n";
        Xml << L"  <ItemGroup Label=\"ProjectConfigurations\">\n";
        Xml << L"    <ProjectConfiguration Include=\"Debug|Win32\"><Configuration>Debug</Configuration><Platform>Win32</Platform></ProjectConfiguration>\n";
        Xml << L"    <ProjectConfiguration Include=\"Debug|x64\"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>\n";
        Xml << L"  </ItemGroup>\n";
        Xml << L"  <PropertyGroup Label=\"Globals\"><ProjectGuid>"
            << (ScriptWorkspace
                ? L"{087D7540-EDC9-4D21-9F0C-E10BA6123D9D}"
                : L"{5F67D4C8-76D6-4DAB-A032-835AE5B3F6B8}")
            << L"</ProjectGuid><Keyword>Win32Proj</Keyword><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>\n";
        Xml << L"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n";
        Xml << L"  <PropertyGroup Label=\"Configuration\"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><CharacterSet>Unicode</CharacterSet></PropertyGroup>\n";
        Xml << L"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n";
        Xml << L"  <PropertyGroup><OutDir>$(MSBuildThisFileDirectory).build\\$(Platform)\\$(Configuration)\\</OutDir><IntDir>$(MSBuildThisFileDirectory).build\\$(Platform)\\$(Configuration)\\obj\\</IntDir></PropertyGroup>\n";
        Xml << L"  <ItemDefinitionGroup><ClCompile><WarningLevel>Level3</WarningLevel><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary><AdditionalIncludeDirectories>$(MSBuildThisFileDirectory)..;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions></ClCompile><Link><SubSystem>Windows</SubSystem></Link></ItemDefinitionGroup>\n";
        Xml << L"  <ItemGroup><ClCompile Include=\"*.cpp\" /><ClCompile Include=\"*.cxx\" />";

        if (!ScriptWorkspace)
        {
            Xml << L"<ClCompile Include=\"..\\Templates\\EngineExtension\\MainProgramAdapter.cpp\" />";
        }

        Xml << L"</ItemGroup>\n";
        Xml << L"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n";
        Xml << L"</Project>\n";
        return SaveSourceFile(BuildProjectPath, Xml.str());
    }

    //概要：PathがPrograms直下から外れない管理対象ファイルか確認する
    //引数：path=確認するPath
    //戻り値：Programs直下のファイルPathの場合はtrue
    bool ProgramWorkspace::IsManagedSourcePath(const std::filesystem::path& path) const
    {
        if (ProgramDirectory.empty() || path.empty())
        {
            return false;
        }

        std::error_code Error; //Path正規化結果
        const std::filesystem::path NormalDirectory = std::filesystem::weakly_canonical(
            ProgramDirectory,
            Error
        ); //比較用Programs Directory
        const std::filesystem::path NormalParent = std::filesystem::weakly_canonical(
            path.parent_path(),
            Error
        ); //比較用対象Parent Directory
        return !Error && NormalDirectory == NormalParent;
    }

    //概要：環境変数とVisual Studio標準位置からMSBuild.exeを検索する
    //引数：なし
    //戻り値：使用可能なMSBuild Path、見つからない場合は空Path
    std::filesystem::path ProgramWorkspace::FindMSBuild() const
    {
        wchar_t SearchResult[32768]{}; //PATH環境変数から見つけたMSBuild Path
        const DWORD SearchLength = SearchPathW(
            nullptr,
            L"MSBuild.exe",
            nullptr,
            static_cast<DWORD>(std::size(SearchResult)),
            SearchResult,
            nullptr
        ); //PATH内MSBuild検索結果

        if (SearchLength > 0 && SearchLength < std::size(SearchResult))
        {
            return std::filesystem::path(SearchResult);
        }

        constexpr const wchar_t* Editions[] =
        {
            L"Community",
            L"Professional",
            L"Enterprise",
            L"BuildTools"
        }; //検索するVisual Studio Edition
        constexpr const wchar_t* ProgramFilesVariables[] =
        {
            L"ProgramFiles",
            L"ProgramFiles(x86)"
        }; //Visual Studioを配置できるProgram Files環境変数
        std::error_code Error; //MSBuild存在確認結果

        for (const wchar_t* Variable : ProgramFilesVariables)
        {
            wchar_t ProgramFilesPath[32768]{}; //現在環境変数のProgram Files Path
            const DWORD Length = GetEnvironmentVariableW(
                Variable,
                ProgramFilesPath,
                static_cast<DWORD>(std::size(ProgramFilesPath))
            ); //Visual Studio標準位置の基底Path長

            if (Length == 0 || Length >= std::size(ProgramFilesPath))
            {
                continue;
            }

            const std::filesystem::path VisualStudioRoot =
                std::filesystem::path(ProgramFilesPath) /
                L"Microsoft Visual Studio" / L"2022"; //VS 2022標準Root

            for (const wchar_t* Edition : Editions)
            {
                const std::filesystem::path Candidate = VisualStudioRoot / Edition /
                    L"MSBuild" / L"Current" / L"Bin" / L"MSBuild.exe"; //Edition別MSBuild候補

                if (std::filesystem::exists(Candidate, Error))
                {
                    return Candidate;
                }
            }
        }

        return std::filesystem::path();
    }
}
