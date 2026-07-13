//|| FBXModel.h ||:::::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  FBX読込を追加するためのModel Component拡張点を定義する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.00  Component型Modelの拡張宣言へ変更
//||  2026_06_01  v1.00  新規作成
//||

#pragma once

#include <string>

#include "Model.h"

namespace Engine
{
    class FBXModel : public Model
    {
    public:
        //空のFBX Model Componentを作成する
        FBXModel();

        //FBX Model Componentが所有するResourceを解放する
        ~FBXModel() override;

        //FBXファイルをModel Componentへ読み込む
        //引数: filePath 読み込むFBXファイル
        //戻り値: 読み込みに成功した場合はtrue
        bool Load(const std::wstring& filePath);

    private:
        //FBX SDK接続後にMesh情報を読み込む
        //引数: filePath 読み込むFBXファイル
        //戻り値: 読み込みに成功した場合はtrue
        bool LoadMeshes(const std::wstring& filePath);

        //FBX SDK接続後にMaterial情報を読み込む
        //引数: filePath 読み込むFBXファイル
        //戻り値: 読み込みに成功した場合はtrue
        bool LoadMaterials(const std::wstring& filePath);

        //FBX SDK接続後にTexture情報を読み込む
        //引数: filePath 読み込むFBXファイル
        //戻り値: 読み込みに成功した場合はtrue
        bool LoadTextures(const std::wstring& filePath);

        //FBX SDK接続後にSkeleton情報を読み込む
        //引数: filePath 読み込むFBXファイル
        //戻り値: 読み込みに成功した場合はtrue
        bool LoadSkeleton(const std::wstring& filePath);

        //FBX SDK接続後にAnimation情報を読み込む
        //引数: filePath 読み込むFBXファイル
        //戻り値: 読み込みに成功した場合はtrue
        bool LoadAnimation(const std::wstring& filePath);
    };
}
