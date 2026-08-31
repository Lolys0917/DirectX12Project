#include "WinApp.h"
#include "MessageLog.h"
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <DirectXMath.h>

#pragma comment(lib, "shell32.lib")

namespace Engine
{
    namespace
    {
        constexpr int MediaButtonBase = 1100, MediaListID = 1120, MediaSearchID = 1121;
        constexpr int ValueBase = 1140, ApplyID = 1160, ResetID = 1161, GroundID = 1162;
        const wchar_t* ValueNames[] = { L"ゲーム速度 (0–10倍)", L"重力 X (m/s²)", L"重力 Y (m/s²)",
            L"重力 Z (m/s²)", L"空気抵抗 (0–100)", L"反発係数 (0–1)", L"床の高さ (m)",
            L"最高速度 (m/s)", L"カメラ速度 (m/s)", L"背景の向き (±360°)", L"背景の明るさ (0–10)" };
        const wchar_t* Keys[] = { L"TimeScale", L"GravityX", L"GravityY", L"GravityZ", L"LinearDrag",
            L"Restitution", L"GroundHeight", L"MaxFallSpeed", L"CameraSpeed", L"SkyYaw", L"SkyExposure" };
        std::vector<float*> Values(PlaybackSettings& s)
        {
            return { &s.TimeScale, &s.GravityX, &s.GravityY, &s.GravityZ, &s.LinearDrag,
                &s.Restitution, &s.GroundHeight, &s.MaxFallSpeed, &s.CameraSpeed, &s.SkyYaw, &s.SkyExposure };
        }
        std::wstring Lower(std::wstring s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return s;
        }
        bool IsImage(const std::filesystem::path& path)
        {
            auto e = Lower(path.extension().wstring());
            return e == L".dds" || e == L".png" || e == L".jpg" || e == L".jpeg" ||
                e == L".bmp" || e == L".tif" || e == L".tiff";
        }
        bool IsModel(const std::filesystem::path& path) { return Lower(path.extension().wstring()) == L".obj"; }
        const wchar_t* MediaFilter = L"画像 / OBJモデル\0*.dds;*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.obj\0DDS画像\0*.dds\0OBJモデル\0*.obj\0\0";
        std::filesystem::path CopyAsset(const std::filesystem::path& source, const std::filesystem::path& root)
        {
            std::error_code error;
            const auto directory = root / (IsModel(source) ? L"Models" : L"Textures");
            std::filesystem::create_directories(directory, error);
            if (error) return {};
            auto destination = directory / source.filename();
            if (std::filesystem::equivalent(source, destination, error)) return destination;
            error.clear();
            for (unsigned suffix = 1; std::filesystem::exists(destination, error) && !error; ++suffix)
                destination = directory / (source.stem().wstring() + L"_" + std::to_wstring(suffix) + source.extension().wstring());
            if (error || !std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, error)) return {};
            return destination;
        }
    }

    bool WinApp::CreateAssetPreviewControls()
    {
        const auto Make = [this](const wchar_t* cls, const wchar_t* label, DWORD style, int id, DWORD ex = 0)
        {
            HWND control = CreateWindowExW(ex, cls, label, WS_CHILD | WS_CLIPSIBLINGS | style,
                0, 0, 1, 1, Hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), Instance, nullptr);
            if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
            return control;
        };
        for (int i = 0; i < PlaybackValueCount; ++i)
        {
            PlaybackValueLabels[i] = Make(WC_STATICW, ValueNames[i], SS_LEFT, 0);
            PlaybackValueEdits[i] = Make(WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL | ES_RIGHT, ValueBase + i, WS_EX_CLIENTEDGE);
            SendMessageW(PlaybackValueEdits[i], EM_SETLIMITTEXT, 24, 0);
            if (!PlaybackValueLabels[i] || !PlaybackValueEdits[i]) return false;
        }
        PlaybackApplyButton = Make(WC_BUTTONW, L"設定を適用・保存", WS_TABSTOP | BS_PUSHBUTTON, ApplyID);
        PlaybackResetButton = Make(WC_BUTTONW, L"初期値に戻す", WS_TABSTOP | BS_PUSHBUTTON, ResetID);
        GroundCheck = Make(WC_BUTTONW, L"床との簡易衝突を有効にする", WS_TABSTOP | BS_AUTOCHECKBOX, GroundID);
        PlaybackHelpLabel = Make(WC_STATICW, L"重力はEngineで「重力 / Gravity」を追加したObjectに適用。速度0は更新停止。Tickにもゲーム速度を反映します。", SS_LEFT, 0);
        MediaList = Make(WC_LISTBOXW, L"", WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, MediaListID, WS_EX_CLIENTEDGE);
        MediaSearch = Make(WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, MediaSearchID, WS_EX_CLIENTEDGE);
        SendMessageW(MediaSearch, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"ファイル名で検索（DDS / 画像 / OBJ）"));
        MediaPathLabel = Make(WC_STATICW, L"", SS_LEFT | SS_PATHELLIPSIS, 0);
        MediaStatusLabel = Make(WC_STATICW, L"ファイルを読み込むか、一覧から選択してください。", SS_LEFT, 0);
        MediaTargetLabel = Make(WC_STATICW, L"", SS_LEFT | SS_ENDELLIPSIS, 0);
        const wchar_t* labels[] = { L"ファイル読込", L"一覧を更新", L"保存場所", L"シーンへ追加", L"選択Objectに貼付", L"背景に使用",
            L"背景を解除", L"モデルに画像…", L"←", L"→", L"↑", L"↓", L"正面", L"＋", L"−", L"名前変更…", L"表示" };
        for (int i = 0; i < 17; ++i)
        {
            MediaButtons[i] = Make(WC_BUTTONW, labels[i], WS_TABSTOP | BS_PUSHBUTTON, MediaButtonBase + i);
            if (!MediaButtons[i]) return false;
        }
        std::error_code error;
        MediaRoot = std::filesystem::absolute(L"Assets", error);
        wchar_t executable[32768]{};
        GetModuleFileNameW(nullptr, executable, 32768);
        // exeを出力フォルダーから起動しても同じプロジェクトのアセットを使用する。
        for (auto candidate = std::filesystem::path(executable).parent_path(); !candidate.empty();)
        {
            if (std::filesystem::exists(candidate / L"DirectX12Project.vcxproj", error))
            { MediaRoot = candidate / L"Assets"; break; }
            const auto parent = candidate.parent_path();
            if (parent == candidate) break;
            candidate = parent;
        }
        error.clear();
        std::filesystem::create_directories(MediaRoot, error);
        return !error && PlaybackApplyButton && PlaybackResetButton && GroundCheck && PlaybackHelpLabel &&
            MediaList && MediaSearch && MediaPathLabel && MediaStatusLabel && MediaTargetLabel;
    }

    void WinApp::UpdatePlaybackSettingEdits()
    {
        auto values = Values(PlaybackValues);
        for (int i = 0; i < PlaybackValueCount; ++i)
        {
            wchar_t text[40]{}; swprintf_s(text, L"%.6g", *values[i]);
            SetWindowTextW(PlaybackValueEdits[i], text);
        }
        SendMessageW(GroundCheck, BM_SETCHECK, PlaybackValues.GroundEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    void WinApp::ApplyPlaybackSettings(bool reset)
    {
        PlaybackSettings candidate;
        if (!reset)
        {
            auto values = Values(candidate);
            for (int i = 0; i < PlaybackValueCount; ++i)
            {
                auto text = GetControlText(PlaybackValueEdits[i]);
                wchar_t* end = nullptr;
                *values[i] = std::wcstof(text.c_str(), &end);
                while (end && std::iswspace(*end)) ++end;
                if (end == text.c_str() || !end || *end || !std::isfinite(*values[i]))
                { SetWindowTextW(PlaybackHelpLabel, L"入力エラー：すべての項目に有効な数値を入力してください。設定は変更していません。"); return; }
            }
            candidate.GroundEnabled = SendMessageW(GroundCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        }
        if (!candidate.IsValid())
        { SetWindowTextW(PlaybackHelpLabel, L"設定の範囲外です。重力は±1000、床は±100000、最高速度0.1–10000、カメラ0–1000。他は各ラベルの範囲で入力してください。"); return; }
        PlaybackValues = candidate;
        if (reset) SetTargetFrameRate(60);
        UpdatePlaybackSettingEdits();
        EditorCommand command; command.Type = EditorCommandType::SetPlaybackSettings; command.Playback = PlaybackValues;
        QueueEditorCommand(std::move(command));
        SavePlaybackSettings();
    }

    void WinApp::LoadPlaybackSettings()
    {
        auto path = (MediaRoot / L"EditorSettings.ini").wstring();
        PlaybackSettings candidate;
        auto values = Values(candidate);
        for (int i = 0; i < PlaybackValueCount; ++i)
        {
            wchar_t text[80]{}, defaultValue[40]{}; swprintf_s(defaultValue, L"%.6g", *values[i]);
            GetPrivateProfileStringW(L"Playback", Keys[i], defaultValue, text, 80, path.c_str());
            wchar_t* end = nullptr; const float value = std::wcstof(text, &end);
            if (end != text && !*end && std::isfinite(value)) *values[i] = value;
        }
        candidate.GroundEnabled = GetPrivateProfileIntW(L"Playback", L"GroundEnabled", 1, path.c_str()) != 0;
        if (candidate.IsValid()) PlaybackValues = candidate;
        SetTargetFrameRate(std::clamp(GetPrivateProfileIntW(L"Playback", L"FPS", 60, path.c_str()), 1u, 240u));
        UpdatePlaybackSettingEdits();
        EditorCommand command; command.Type = EditorCommandType::SetPlaybackSettings; command.Playback = PlaybackValues;
        QueueEditorCommand(std::move(command));
        wchar_t sky[32768]{};
        GetPrivateProfileStringW(L"Playback", L"SkyTexture", L"Textures/joran-quinten-CRmulUkILVg-unsplash.dds", sky, 32768, path.c_str());
        if (*sky)
        {
            SkyAssetPath = std::filesystem::path(sky);
            if (SkyAssetPath.is_relative()) SkyAssetPath = MediaRoot / SkyAssetPath;
            std::error_code error;
            if (std::filesystem::is_regular_file(SkyAssetPath, error))
            {
                EditorCommand background; background.Type = EditorCommandType::SetSkyTexture; background.Path = SkyAssetPath.wstring();
                QueueEditorCommand(std::move(background));
            }
        }
    }

    void WinApp::SavePlaybackSettings()
    {
        const auto path = (MediaRoot / L"EditorSettings.ini").wstring();
        const auto temporary = path + L".tmp";
        // Unicodeのアセット名を保持し、書き込み完了後だけ設定ファイルを置換する。
        std::ofstream file(std::filesystem::path(temporary), std::ios::binary | std::ios::trunc);
        file.put(char(0xff)); file.put(char(0xfe)); file.close();
        bool ok = bool(file);
        auto Write = [&](const wchar_t* key, const std::wstring& value) {
            ok = WritePrivateProfileStringW(L"Playback", key, value.c_str(), temporary.c_str()) && ok;
        };
        auto values = Values(PlaybackValues);
        for (int i = 0; i < PlaybackValueCount; ++i)
        { wchar_t text[40]{}; swprintf_s(text, L"%.6g", *values[i]); Write(Keys[i], text); }
        Write(L"FPS", std::to_wstring(TargetFrameRate));
        Write(L"GroundEnabled", PlaybackValues.GroundEnabled ? L"1" : L"0");
        Write(L"SkyTexture", SkyAssetPath.empty() ? L"" : SkyAssetPath.lexically_relative(MediaRoot).wstring());
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary.c_str());
        ok = ok && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        SetWindowTextW(PlaybackHelpLabel, ok
            ? L"設定を保存しました。重力はEngineで「重力 / Gravity」を追加したObjectに適用。床の判定はLocal座標です。"
            : L"設定は反映しましたが保存できません。Assetsの書き込み権限を確認してください。");
    }

    void WinApp::RefreshMediaAssets(const std::filesystem::path& preferred)
    {
        if (!MediaList) return;
        auto selected = preferred.empty() ? SelectedMediaPath : preferred;
        auto search = Lower(GetControlText(MediaSearch));
        MediaFiles.clear();
        auto Add = [&](const std::filesystem::path& path) {
            if ((IsImage(path) || IsModel(path)) && Lower(path.filename().wstring()).find(search) != std::wstring::npos)
                MediaFiles.push_back(path);
        };
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(MediaRoot, std::filesystem::directory_options::skip_permission_denied, error), end;
            !error && it != end; it.increment(error))
            if (it->is_regular_file(error)) Add(it->path());
        error.clear();
        for (std::filesystem::directory_iterator it(MediaRoot.parent_path(), error), end; !error && it != end; it.increment(error))
            if (it->is_regular_file(error)) Add(it->path());
        std::sort(MediaFiles.begin(), MediaFiles.end());
        SendMessageW(MediaList, WM_SETREDRAW, FALSE, 0);
        SendMessageW(MediaList, LB_RESETCONTENT, 0, 0);
        int selection = -1;
        for (std::size_t i = 0; i < MediaFiles.size(); ++i)
        {
            auto label = std::wstring(IsModel(MediaFiles[i]) ? L"[OBJ] " : L"[画像] ") +
                MediaFiles[i].lexically_relative(MediaRoot.parent_path()).wstring();
            SendMessageW(MediaList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (MediaFiles[i] == selected) selection = int(i);
        }
        SendMessageW(MediaList, LB_SETHORIZONTALEXTENT, ScaleByDpi(680), 0);
        SendMessageW(MediaList, LB_SETCURSEL, selection, 0);
        SendMessageW(MediaList, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(MediaList, nullptr, TRUE);
        UpdateMediaTarget();
    }

    void WinApp::ImportMediaAssets()
    {
        std::vector<wchar_t> paths(65536);
        OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = Hwnd;
        dialog.lpstrFilter = MediaFilter; dialog.lpstrFile = paths.data(); dialog.nMaxFile = DWORD(paths.size());
        dialog.lpstrTitle = L"画像・OBJモデルをAssetsへ取り込む（複数選択可）";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&dialog)) return;
        const std::filesystem::path first(paths.data());
        const wchar_t* next = paths.data() + std::wcslen(paths.data()) + 1;
        std::vector<std::filesystem::path> sources;
        if (!*next) sources.push_back(first);
        else for (; *next; next += std::wcslen(next) + 1) sources.push_back(first / next);
        std::filesystem::path last;
        unsigned failed = 0;
        for (const auto& source : sources)
        {
            if (!IsImage(source) && !IsModel(source)) { ++failed; continue; }
            auto copied = CopyAsset(source, MediaRoot);
            if (copied.empty()) ++failed; else last = copied;
        }
        SetWindowTextW(MediaSearch, L"");
        RefreshMediaAssets(last);
        if (!last.empty()) SelectMediaAsset();
        if (failed) MessageLog::GetInstance().AddLog("[Warning] Assets | Some files were unsupported or could not be copied; existing files were preserved.");
    }

    void WinApp::SelectMediaAsset()
    {
        const auto index = SendMessageW(MediaList, LB_GETCURSEL, 0, 0);
        if (index < 0 || std::size_t(index) >= MediaFiles.size()) return;
        SelectedMediaPath = MediaFiles[std::size_t(index)];
        SetWindowTextW(MediaPathLabel, SelectedMediaPath.wstring().c_str());
        SetWindowTextW(MediaStatusLabel, L"読み込み中…");
        PreviewYaw = PreviewPitch = 0; PreviewZoom = 1;
        EditorCommand command; command.Type = EditorCommandType::PreviewAsset; command.Path = SelectedMediaPath.wstring();
        command.PreviewRequestID = ++LatestPreviewRequestID;
        QueueEditorCommand(std::move(command));
        UpdateMediaTarget();
    }

    void WinApp::QueuePreviewView()
    {
        EditorCommand command; command.Type = EditorCommandType::PreviewView;
        command.Transform.Rotation = { PreviewPitch, PreviewYaw, 0 };
        command.Transform.Scale.X = PreviewZoom;
        QueueEditorCommand(std::move(command));
    }

    void WinApp::UpdateMediaTarget()
    {
        if (!MediaTargetLabel) return;
        auto* object = GetSelectedObjectInfo();
        std::wstring target = L"貼付先：EngineタブでObjectを選択";
        if (object)
        {
            const int size = MultiByteToWideChar(CP_UTF8, 0, object->Name.c_str(), -1, nullptr, 0);
            std::wstring name(std::max(1, size), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, object->Name.c_str(), -1, name.data(), int(name.size()));
            target = L"貼付先：" + std::wstring(name.c_str());
        }
        SetWindowTextW(MediaTargetLabel, target.c_str());
        const bool selected = SendMessageW(MediaList, LB_GETCURSEL, 0, 0) >= 0;
        EnableWindow(MediaButtons[3], selected && IsModel(SelectedMediaPath));
        EnableWindow(MediaButtons[4], selected && IsImage(SelectedMediaPath) && object);
        EnableWindow(MediaButtons[5], selected && IsImage(SelectedMediaPath));
        EnableWindow(MediaButtons[15], selected);
        EnableWindow(MediaButtons[16], selected);
        for (int i = 8; i <= 14; ++i) EnableWindow(MediaButtons[i], IsModel(SelectedMediaPath));
        EnableWindow(MediaButtons[7], IsModel(SelectedMediaPath));
    }

    bool WinApp::HandleAssetPreviewCommand(int id, int notification)
    {
        if (id == ApplyID && notification == BN_CLICKED) { ApplyPlaybackSettings(); return true; }
        if (id == ResetID && notification == BN_CLICKED) { ApplyPlaybackSettings(true); return true; }
        if (id == MediaSearchID && notification == EN_CHANGE) { RefreshMediaAssets(); return true; }
        if (id == MediaListID && (notification == LBN_SELCHANGE || notification == LBN_DBLCLK)) { SelectMediaAsset(); return true; }
        if (id < MediaButtonBase || id >= MediaButtonBase + 17 || notification != BN_CLICKED) return false;
        const int action = id - MediaButtonBase;
        if (action == 0) { ImportMediaAssets(); return true; }
        if (action == 1) { RefreshMediaAssets(); return true; }
        if (action == 2) { ShellExecuteW(Hwnd, L"open", MediaRoot.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return true; }
        if (action >= 8 && action <= 14)
        {
            constexpr float step = DirectX::XM_PI / 12;
            if (action == 8) PreviewYaw -= step;
            if (action == 9) PreviewYaw += step;
            if (action == 10) PreviewPitch -= step;
            if (action == 11) PreviewPitch += step;
            PreviewPitch = std::remainder(PreviewPitch, DirectX::XM_2PI);
            PreviewYaw = std::remainder(PreviewYaw, DirectX::XM_2PI);
            if (action == 12) { PreviewYaw = PreviewPitch = 0; PreviewZoom = 1; }
            if (action == 13) PreviewZoom = std::max(0.25f, PreviewZoom / 1.2f);
            if (action == 14) PreviewZoom = std::min(4.0f, PreviewZoom * 1.2f);
            QueuePreviewView(); return true;
        }
        if (action == 16) { SelectMediaAsset(); return true; }
        if (action == 15)
        {
            // 保存Dialogで新しい名前を受け取る。同名を上書きしない。
            std::vector<wchar_t> filename(32768);
            wcscpy_s(filename.data(), filename.size(), SelectedMediaPath.filename().c_str());
            const auto parent = SelectedMediaPath.parent_path().wstring();
            OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = Hwnd;
            dialog.lpstrFile = filename.data(); dialog.nMaxFile = DWORD(filename.size()); dialog.lpstrInitialDir = parent.c_str();
            dialog.lpstrTitle = L"Assets内のファイル名を変更（参照中のファイルは変更不可）";
            dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (!GetSaveFileNameW(&dialog)) return true;
            auto destination = std::filesystem::path(filename.data()).lexically_normal();
            std::error_code error;
            auto relative = SelectedMediaPath.lexically_relative(MediaRoot);
            // Sceneや保存済み設定のパスを壊さないため、使用中の名前変更は行わない。
            if (relative.empty() || *relative.begin() == L".." || destination.parent_path() != SelectedMediaPath.parent_path() ||
                Lower(destination.extension().wstring()) != Lower(SelectedMediaPath.extension().wstring()) ||
                std::filesystem::exists(destination, error) || SelectedMediaPath == SkyAssetPath)
            { SetWindowTextW(MediaStatusLabel, L"名前変更不可：Assets内の同じフォルダー・同じ拡張子で、未使用の名前を指定してください。背景は先に解除してください。"); return true; }
            const bool referenced = std::any_of(CurrentEditorSnapshot.ReferencedAssets.begin(),
                CurrentEditorSnapshot.ReferencedAssets.end(), [&](const std::wstring& path) {
                    return !path.empty() && std::filesystem::equivalent(SelectedMediaPath, path, error);
                });
            if (referenced || CurrentPlaybackState != PlaybackState::Stopped)
            { SetWindowTextW(MediaStatusLabel, L"シーンに適用したファイルです。参照を保つため名前変更を止めました。"); return true; }
            std::filesystem::rename(SelectedMediaPath, destination, error);
            if (error) SetWindowTextW(MediaStatusLabel, L"名前を変更できませんでした。");
            else { RefreshMediaAssets(destination); SelectMediaAsset(); }
            return true;
        }
        EditorCommand command; command.Path = SelectedMediaPath.wstring();
        command.Scene = SelectedEditorSceneID;
        if (auto* node = GetSelectedTreeNode()) { command.Scene = node->Scene; command.Object = node->Object; }
        if (action == 3) command.Type = EditorCommandType::ImportModel;
        if (action == 4) command.Type = EditorCommandType::ApplyObjectTexture;
        if (action == 5 || action == 6)
        {
            command.Type = EditorCommandType::SetSkyTexture;
            if (action == 6) command.Path.clear();
        }
        if (action == 7)
        {
            wchar_t path[32768]{};
            OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = Hwnd;
            dialog.lpstrFilter = L"画像\0*.dds;*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0\0";
            dialog.lpstrFile = path; dialog.nMaxFile = 32768; dialog.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            dialog.lpstrTitle = L"プレビュー中のモデルに適用する画像";
            if (!GetOpenFileNameW(&dialog)) return true;
            command.Type = EditorCommandType::PreviewTexture; command.Path = path;
        }
        command.PreviewRequestID = ++LatestPreviewRequestID;
        QueueEditorCommand(std::move(command));
        SetWindowTextW(MediaStatusLabel, L"適用中…");
        return true;
    }

    void WinApp::SetAssetPreviewVisibility()
    {
        const auto font = SendMessageW(FrameRateEditHwnd, WM_GETFONT, 0, 0);
        const auto Show = [font](HWND hwnd, bool show) {
            if (!hwnd) return;
            SendMessageW(hwnd, WM_SETFONT, font, FALSE);
            ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
            if (show) SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        };
        for (int i = 0; i < PlaybackValueCount; ++i)
        { Show(PlaybackValueLabels[i], ActiveTabIndex == 1); Show(PlaybackValueEdits[i], ActiveTabIndex == 1); }
        for (auto hwnd : { PlaybackApplyButton, PlaybackResetButton, GroundCheck, PlaybackHelpLabel }) Show(hwnd, ActiveTabIndex == 1);
        for (auto hwnd : MediaButtons) Show(hwnd, ActiveTabIndex == 7);
        for (auto hwnd : { MediaList, MediaSearch, MediaPathLabel, MediaStatusLabel, MediaTargetLabel, PreviewImageHwnd }) Show(hwnd, ActiveTabIndex == 7);
        Show(PreviewLabelHwnd, false);
        Show(MediaButtons[16], false);
    }

    void WinApp::LayoutPlaybackSettings(int left, int top, int width, int bottom)
    {
        const int available = bottom - top;
        const int gap = ScaleByDpi(available < ScaleByDpi(415) ? 2 : 5);
        // 最小ウィンドウでも保存結果・入力エラーを読むための高さを確保する。
        const int reserved = ScaleByDpi(30 + 48) + gap * 2;
        const int row = std::clamp((available - reserved) / (PlaybackValueCount + 1) - gap,
            ScaleByDpi(20), ScaleByDpi(27));
        const int edit = ScaleByDpi(92);
        for (int i = 0; i < PlaybackValueCount; ++i)
        {
            MoveWindow(PlaybackValueLabels[i], left, top, std::max(1, width - edit - gap), row, TRUE);
            MoveWindow(PlaybackValueEdits[i], left + width - edit, top, edit, row, TRUE);
            top += row + gap;
        }
        MoveWindow(GroundCheck, left, top, width, row, TRUE); top += row + gap;
        const int half = (width - gap) / 2;
        MoveWindow(PlaybackApplyButton, left, top, half, ScaleByDpi(30), TRUE);
        MoveWindow(PlaybackResetButton, left + half + gap, top, half, ScaleByDpi(30), TRUE);
        top += ScaleByDpi(30) + gap;
        MoveWindow(PlaybackHelpLabel, left, top, width, std::max(1, bottom - top), TRUE);
    }

    void WinApp::LayoutAssetPreviewControls(int left, int top, int width, int bottom)
    {
        const int gap = ScaleByDpi(5), row = ScaleByDpi(28), third = (width - gap * 2) / 3;
        for (int i = 0; i < 3; ++i) MoveWindow(MediaButtons[i], left + i * (third + gap), top, third, row, TRUE);
        top += row + gap;
        MoveWindow(MediaSearch, left, top, width, row, TRUE); top += row + gap;
        const int listHeight = std::clamp((bottom - top) / 4, ScaleByDpi(75), ScaleByDpi(155));
        MoveWindow(MediaList, left, top, width, listHeight, TRUE); top += listHeight + gap;
        MoveWindow(MediaPathLabel, left, top, std::max(1, width - third), row, TRUE);
        MoveWindow(MediaButtons[15], left + width - third, top, third, row, TRUE); top += row + gap;
        const int footer = 4 * (row + gap) + ScaleByDpi(46);
        const int previewHeight = std::max(ScaleByDpi(70), bottom - top - footer);
        const int arrowWidth = ScaleByDpi(28);
        MoveWindow(PreviewImageHwnd, left + arrowWidth + gap, top, std::max(1, width - 2 * (arrowWidth + gap)), previewHeight, TRUE);
        MoveWindow(MediaButtons[8], left, top + previewHeight / 2 - row / 2, arrowWidth, row, TRUE);
        MoveWindow(MediaButtons[9], left + width - arrowWidth, top + previewHeight / 2 - row / 2, arrowWidth, row, TRUE);
        top += previewHeight + gap;
        const int turns[] = {10,11,12,13,14,16,7};
        // 画像選択は下の広い操作列に配置し、回転ボタンを押しやすくする。
        const int turnButtonWidth = (width - third - gap * 5) / 5;
        for (int i = 0; i < 5; ++i) MoveWindow(MediaButtons[turns[i]], left + i * (turnButtonWidth + gap), top, turnButtonWidth, row, TRUE);
        MoveWindow(MediaButtons[7], left + width - third, top, third, row, TRUE);
        ShowWindow(MediaButtons[16], SW_HIDE);
        top += row + gap;
        MoveWindow(MediaStatusLabel, left, top, width, ScaleByDpi(42), TRUE); top += ScaleByDpi(42) + gap;
        MoveWindow(MediaTargetLabel, left, top, width, row, TRUE); top += row + gap;
        const int actions[] = {3,4,5};
        for (int i = 0; i < 3; ++i) MoveWindow(MediaButtons[actions[i]], left + i * (third + gap), top, third, row, TRUE);
        top += row + gap;
        MoveWindow(MediaButtons[6], left, top, width, row, TRUE);
    }
}
