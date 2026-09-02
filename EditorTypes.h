//|| EditorTypes.h ||::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  エンジン状態をWindowsエディターへ渡すスナップショットと操作要求を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.00  新規作成
//||

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EntityTypes.h"
#include "PlaybackSettings.h"

namespace Engine
{
    struct RenderWindowSize final
    {
        std::uint32_t Width; //DirectX描画用子Windowの幅
        std::uint32_t Height; //DirectX描画用子Windowの高さ
    };

    struct EditorVector3 final
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
    };

    struct EditorTransformInfo final
    {
        EditorVector3 Position;
        EditorVector3 Rotation;
        EditorVector3 Scale{ 1.0f, 1.0f, 1.0f };
    };

    struct EditorColor final
    {
        float Red = 1.0f;
        float Green = 1.0f;
        float Blue = 1.0f;
        float Alpha = 1.0f;
    };

    struct EditorComponentInfo final
    {
        ComponentID ID; //Scene内のComponent ID
        std::string Name; //解決済みComponent名
        std::string TypeName; //UI表示用Component型名
        bool Active = true; //更新と描画の対象である場合true
        bool Script = false; //差し込みScript Componentである場合true
        struct ExposedMember final
        {
            std::string Name; //Scriptが公開した変数又は関数名
            std::string Type; //Float、Vector3、Bool、Action等
            std::string Value; //EditorとScript間の可搬文字列表現
            bool Function = false; //呼出可能なpublic関数の場合true
            bool ReadOnly = false; //Editorから変更できない場合true
        };
        std::vector<ExposedMember> ExposedMembers; //選択時にInspectorへ表示する公開項目
    };

    struct EditorObjectInfo final
    {
        ObjectID ID; //Scene内のObject ID
        ObjectType Type = ObjectType::Object; //Objectの具象種別
        std::string Name; //解決済みObject名
        bool Active = true; //Objectが有効である場合true
        std::string Group; //空文字は非Group
        std::string Tag = "Untagged"; //検索及びGameplay分類Tag
        std::uint32_t Layer = 0; //0から31のLayer
        std::int32_t GroupOrder = 0; //Group間の処理順
        std::int32_t ExecutionOrder = 0; //Group内の処理順
        ObjectID ParentID; //Rootの場合は無効ID
        EditorTransformInfo LocalTransform; //親ObjectからのLocal姿勢
        std::vector<EditorComponentInfo> Components; //所有Component一覧
    };

    struct EditorSceneInfo final
    {
        SceneID ID; //SceneManager内のScene ID
        std::string Name; //解決済みScene名
        bool Active = false; //Sceneが更新と描画の対象である場合true
        bool ViewScene = false; //画面へ表示中のSceneである場合true
        std::vector<EditorObjectInfo> Objects; //Sceneが所有するObject一覧
    };

    struct EditorScriptInfo final
    {
        std::string Key; //Registry内で一意なScript識別子
        std::string DisplayName; //UIへ表示するScript名
        std::string ModuleName; //Native又はDLL Moduleの表示名
    };

    struct EditorSnapshot final
    {
        std::wstring PreviewStatus;
        std::vector<std::wstring> ReferencedAssets;
        std::wstring SkyTexturePath;
        bool HasSkyStatus = false;
        std::uint64_t PreviewRequestID = 0;
        std::uint64_t Revision = 0; //構造変更ごとに増える更新番号
        SceneID ViewSceneID; //空白操作の既定対象Scene
        std::vector<EditorSceneInfo> Scenes; //全SceneとObject階層
        std::vector<EditorScriptInfo> Scripts; //差し込み可能なScript一覧
    };

    enum class EditorCommandType : std::uint8_t
    {
        None,
        CreateObject,
        DuplicateObject,
        DeleteObject,
        RenameObject,
        ToggleObjectActive,
        SetObjectTransform,
        SetObjectOrganization,
        ToggleGroupActive,
        SetObjectParent,
        SetViewScene,
        AttachScript,
        DeleteComponent,
        RenameComponent,
        ToggleComponentActive,
        SetScriptMember,
        InvokeScriptFunction,
        LoadScriptModule,
        LoadExtensionModule,
        UnloadExtensionModule,
        SetEditorSelection,
        Refresh
        , SetPlaybackSettings
        , PreviewAsset
        , PreviewView
        , PreviewTexture
        , ApplyObjectTexture
        , SetSkyTexture
        , ImportModel
    };

    struct EditorCommand final
    {
        EditorCommandType Type = EditorCommandType::None; //実行する操作
        PlaybackSettings Playback;
        std::uint64_t PreviewRequestID = 0;
        SceneID Scene; //操作対象Scene
        ObjectID Object; //操作対象Object
        ObjectID Parent; //親変更又は新規Child作成先Object
        ComponentID Component; //操作対象Component
        ObjectType ObjectKind = ObjectType::Object; //新規作成するObject種別
        std::string Text; //新しい名前又はScript Key
        std::string Group; //Objectの処理Group
        std::string Tag; //ObjectのTag
        std::string Member; //Script公開メンバー又は関数名
        std::string Value; //Script公開メンバーの新しい値
        std::wstring Path; //読み込むDLLのファイルパス
        EditorTransformInfo Transform; //Objectへ設定するLocal姿勢
        std::uint32_t Layer = 0; //Objectの0から31のLayer
        std::int32_t GroupOrder = 0; //Group間処理順
        std::int32_t ExecutionOrder = 0; //同一Group内処理順
        bool KeepWorldTransform = true; //親変更時にWorld姿勢を維持する場合true
    };
}
