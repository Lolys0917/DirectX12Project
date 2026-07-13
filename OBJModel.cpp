//|| OBJModel.cpp ||::::::::::::::::::::::::::::
//||
//||  概要 :::::::::::::::::::::::::::::::::::::
//||
//||  Wavefront OBJとTextureを描画するModel Componentを実装する
//||
//||  更新内容 :::::::::::::::::::::::::::::::::
//||
//||  2026_07_13  v2.30  OBJ行解析、描画Resource及びTexture失敗をMessageLogへ記録
//||  2026_07_13  v2.20  Root Constants、安全なOBJ解析、複製元情報を追加
//||  2026_07_13  v2.10  C++変数命名と宣言コメントを規則へ統一
//||  2026_07_13  v2.00  Objectの姿勢とRenderContextを使用するComponentへ変更
//||  2026_06_01  v1.00  新規作成
//||

#include "OBJModel.h"

#include "DirectX12.h"
#include "Camera.h"
#include "MessageLog.h"
#include "Object.h"
#include "RenderContext.h"

#include <charconv>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <cstring>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine
{
    namespace
    {
        constexpr UINT OBJRootConstantCount = 24; //OBJ描画定数を構成する32bit値数
        static_assert(sizeof(OBJConstantBuffer) ==
            sizeof(UINT) * OBJRootConstantCount);

        struct OBJIndex
        {
            int Position = 0; //Position配列のOBJ Index
            int UV = 0; //Texture座標配列のOBJ Index
            int Normal = 0; //法線配列のOBJ Index

            //二つのOBJ Index組が一致するか判定する
            //引数: rhs 比較対象
            //戻り値: 全Indexが一致する場合はtrue
            bool operator==(const OBJIndex& rhs) const
            {
                return Position == rhs.Position &&
                    UV == rhs.UV &&
                    Normal == rhs.Normal;
            }
        };

        struct OBJIndexHash
        {
            //OBJ Index組のHash値を計算する
            //引数: key Hash対象
            //戻り値: 三つのIndexから生成したHash値
            size_t operator()(const OBJIndex& key) const
            {
                size_t PositionHash = std::hash<int>()(key.Position); //Position IndexのHash値
                size_t UVHash = std::hash<int>()(key.UV); //Texture座標IndexのHash値
                size_t NormalHash = std::hash<int>()(key.Normal); //法線IndexのHash値

                return PositionHash ^ (UVHash << 1) ^ (NormalHash << 2);
            }
        };

        //文字列全体を例外なしで符号付き整数へ変換する
        //引数: text 変換する文字列、value 変換結果
        //戻り値: 範囲内の整数へ完全変換できた場合はtrue
        bool ParseInteger(
            std::string_view text,
            int& value
        )
        {
            if (text.empty())
            {
                return false;
            }

            if (text.front() == '+')
            {
                text.remove_prefix(1);

                if (text.empty() || text.front() == '+' ||
                    text.front() == '-')
                {
                    return false;
                }
            }

            int ParsedValue = 0; //from_charsが返す整数
            const char* Begin = text.data(); //変換対象の先頭
            const char* End = Begin + text.size(); //変換対象の末尾
            const std::from_chars_result Result = std::from_chars(
                Begin,
                End,
                ParsedValue
            ); //例外を送出しない整数変換結果

            if (Result.ec != std::errc() || Result.ptr != End)
            {
                return false;
            }

            value = ParsedValue;
            return true;
        }

        //OBJ形式の正負Indexを検証してゼロ始まりIndexへ変換する
        //引数: index OBJ Index、count 対象配列要素数、required 必須Indexの場合true、resolvedIndex 変換結果
        //戻り値: 配列範囲内又は省略可能Indexの場合はtrue
        bool ResolveOBJIndex(
            int index,
            std::size_t count,
            bool required,
            int& resolvedIndex
        )
        {
            if (index == 0)
            {
                resolvedIndex = -1;
                return !required;
            }

            if (count == 0 || count >
                static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            {
                return false;
            }

            const std::int64_t ResolvedValue = index > 0
                ? static_cast<std::int64_t>(index) - 1
                : static_cast<std::int64_t>(count) +
                    static_cast<std::int64_t>(index); //正負表記を解決したIndex

            if (ResolvedValue < 0 ||
                ResolvedValue >= static_cast<std::int64_t>(count))
            {
                return false;
            }

            resolvedIndex = static_cast<int>(ResolvedValue);
            return true;
        }

        //Face要素のPosition/UV/Normal Indexを分解する
        //引数: token OBJ Faceの一要素、result 分解したOBJ Index組
        //戻り値: Token全体を有効なIndex表記として解析できた場合はtrue
        bool ParseFaceToken(
            const std::string& token,
            OBJIndex& result
        )
        {
            result = {};

            const std::string_view TokenView(token); //Slash位置を調べるToken全体
            const std::size_t FirstSlash = TokenView.find('/'); //Position後のSlash位置
            const std::string_view PositionPart = TokenView.substr(
                0,
                FirstSlash
            ); //必須Position Index文字列

            if (!ParseInteger(PositionPart, result.Position) ||
                result.Position == 0)
            {
                return false;
            }

            if (FirstSlash == std::string_view::npos)
            {
                return true;
            }

            const std::size_t SecondSlash = TokenView.find(
                '/',
                FirstSlash + 1
            ); //UV後のSlash位置
            const std::size_t UVEnd = SecondSlash == std::string_view::npos
                ? TokenView.size()
                : SecondSlash; //UV Index文字列の末尾
            const std::string_view UVPart = TokenView.substr(
                FirstSlash + 1,
                UVEnd - FirstSlash - 1
            ); //省略可能なUV Index文字列

            if (!UVPart.empty() && !ParseInteger(UVPart, result.UV))
            {
                return false;
            }

            if (SecondSlash == std::string_view::npos)
            {
                return true;
            }

            if (TokenView.find('/', SecondSlash + 1) !=
                std::string_view::npos)
            {
                return false;
            }

            const std::string_view NormalPart = TokenView.substr(
                SecondSlash + 1
            ); //省略可能なNormal Index文字列

            if (!NormalPart.empty() &&
                !ParseInteger(NormalPart, result.Normal))
            {
                return false;
            }

            return true;
        }

        const char* OBJShaderCode = R"(

cbuffer ObjectCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    int gUseTexture;
    float3 padding;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position =
        mul(float4(input.position, 1.0f), gWorldViewProjection);

    output.normal = input.normal;
    output.uv = input.uv;

    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    if (gUseTexture != 0)
    {
        return gTexture.Sample(gSampler, input.uv) * gColor;
    }

    return gColor;
}

)"; //OBJ Model描画用HLSL Source
    }

    //未登録状態のOBJ Model Componentを作成する
    OBJModel::OBJModel()
        : Model()
        , VertexBufferView{}
        , IndexBufferView{}
        , Color(1.0f, 1.0f, 1.0f, 1.0f)
        , UseTexture(false)
        , TextureRequested(false)
    {
    }

    //OBJ Model Componentを破棄する
    OBJModel::~OBJModel()
    {
        Finalize();
    }

    //OBJ Modelの終了処理を行う
    void OBJModel::Finalize()
    {
    }

    //未登録状態のOBJ Model定義を複製する
    //戻り値: CPU Meshと色を持つ複製Component
    std::unique_ptr<Component> OBJModel::Clone() const
    {
        auto Duplicate = std::make_unique<OBJModel>(); //未登録の複製Model
        Duplicate->Vertices = Vertices;
        Duplicate->Indices = Indices;
        Duplicate->Color = Color;
        Duplicate->SourceOBJPath = SourceOBJPath;
        Duplicate->SourceTexturePath = SourceTexturePath;
        Duplicate->UseTexture = UseTexture;
        Duplicate->TextureRequested = TextureRequested;
        CopyDefinitionTo(*Duplicate);
        return Duplicate;
    }

    //OBJとDiffuse Textureを読み込みGPU Resourceを作成する
    //引数: dx12 描画基盤、objPath OBJパス、texturePath Textureパス
    //戻り値: 読み込みとResource作成に成功した場合はtrue
    bool OBJModel::Load(
        DirectX12& dx12,
        const std::wstring& objPath,
        const std::wstring& texturePath
    )
    {
        SourceOBJPath = objPath;
        SourceTexturePath = texturePath;
        TextureRequested = true;
        UseTexture = true;
        Color = DirectX::XMFLOAT4(
            1.0f,
            1.0f,
            1.0f,
            1.0f
        );

        if (!LoadOBJFile(objPath)) return false;

        if (!CreateRootSignature(dx12) || !CreatePipelineState(dx12) ||
            !CreateVertexBuffer(dx12) || !CreateIndexBuffer(dx12))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] OBJModel | A model drawing resource could not be created."
            );
            return false;
        }

        if (!Texture.LoadFromFile(dx12, texturePath))
        {
            UseTexture = false;
            MessageLog::GetInstance().AddLog(
                "[Warning] OBJModel | Diffuse texture loading failed; a white fallback texture will be used."
            );

            if (!Texture.CreateWhiteTexture(dx12))
            {
                MessageLog::GetInstance().AddLog(
                    "[Error] OBJModel | Fallback white texture creation failed."
                );
                return false;
            }
        }

        return true;
    }

    //OBJを単色Modelとして読み込みGPU Resourceを作成する
    //引数: dx12 描画基盤、objPath OBJパス、color 描画色
    //戻り値: 読み込みとResource作成に成功した場合はtrue
    bool OBJModel::Load(
        DirectX12& dx12,
        const std::wstring& objPath,
        const DirectX::XMFLOAT4& color
    )
    {
        SourceOBJPath = objPath;
        SourceTexturePath.clear();
        TextureRequested = false;
        UseTexture = false;
        Color = color;

        if (!LoadOBJFile(objPath)) return false;

        if (!CreateRootSignature(dx12) || !CreatePipelineState(dx12) ||
            !CreateVertexBuffer(dx12) || !CreateIndexBuffer(dx12))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] OBJModel | A color model drawing resource could not be created."
            );
            return false;
        }

        if (!Texture.CreateWhiteTexture(dx12))
        {
            MessageLog::GetInstance().AddLog(
                "[Error] OBJModel | Color model white texture creation failed."
            );
            return false;
        }

        return true;
    }

    //複製済みCPU MeshからGPU Resourceを再作成する
    //引数: dx12 描画基盤
    //戻り値: Resource作成に成功またはMesh未設定の場合はtrue
    bool OBJModel::Initialize(DirectX12& dx12)
    {
        if (PipelineState && RootSignature && VertexBuffer && IndexBuffer &&
            Texture.IsValid())
        {
            return true;
        }

        if (!SourceOBJPath.empty())
        {
            if (TextureRequested)
            {
                return Load(
                    dx12,
                    SourceOBJPath,
                    SourceTexturePath
                );
            }

            return Load(dx12, SourceOBJPath, Color);
        }

        if (Vertices.empty() || Indices.empty())
        {
            return true;
        }

        UseTexture = false;

        if (!CreateRootSignature(dx12)) return false;
        if (!CreatePipelineState(dx12)) return false;
        if (!CreateVertexBuffer(dx12)) return false;
        if (!CreateIndexBuffer(dx12)) return false;

        return Texture.CreateWhiteTexture(dx12);
    }

    //OBJテキストから頂点とIndexを読み込む
    //引数: objPath 読み込むOBJファイル
    //戻り値: 描画可能な面を読み込めた場合はtrue
    bool OBJModel::LoadOBJFile(
        const std::wstring& objPath
    )
    {
        Vertices.clear();
        Indices.clear();

        std::ifstream File{ std::filesystem::path(objPath) }; //OBJ入力File Stream

        if (!File)
        {
            MessageLog::GetInstance().AddLog(
                "[Error] OBJModel | OBJ file could not be opened."
            );
            return false;
        }

        std::vector<DirectX::XMFLOAT3> Positions; //OBJ Position配列
        std::vector<DirectX::XMFLOAT3> Normals; //OBJ法線配列
        std::vector<DirectX::XMFLOAT2> UVs; //OBJ Texture座標配列

        std::unordered_map<
            OBJIndex,
            uint32_t,
            OBJIndexHash
        > VertexMap; //OBJ Index組から共有頂点IndexへのMap

        std::string Line; //現在解析中の一行
        std::size_t LineNumber = 0; // 現在解析中のOBJ行番号

        const auto ReportParseFailure = [&LineNumber](const char* reason)
        {
            char Message[320]{}; // 行番号と解析失敗理由を含む表示用メッセージ
            sprintf_s(
                Message,
                "[Error] OBJModel | Invalid OBJ data at line %zu: %s.",
                LineNumber,
                reason
            );
            MessageLog::GetInstance().AddLog(Message);
            return false;
        }; // 同一行で複数ログを発生させず呼び出し元へfalseを返す処理

        while (std::getline(File, Line))
        {
            ++LineNumber;
            std::stringstream Stream(Line); //現在行のToken解析Stream

            std::string Type; //現在行のOBJ要素種別
            Stream >> Type;

            if (Type == "v")
            {
                DirectX::XMFLOAT3 Position{}; //読み込んだPosition

                if (!(Stream >> Position.x >> Position.y >> Position.z))
                {
                    return ReportParseFailure("position requires three finite numbers");
                }

                Positions.push_back(Position);
            }
            else if (Type == "vt")
            {
                DirectX::XMFLOAT2 UV{}; //読み込んだTexture座標

                if (!(Stream >> UV.x >> UV.y))
                {
                    return ReportParseFailure("texture coordinate requires two numbers");
                }

                // OBJはV方向が逆になることが多いため反転
                UV.y = 1.0f - UV.y;

                UVs.push_back(UV);
            }
            else if (Type == "vn")
            {
                DirectX::XMFLOAT3 Normal{}; //読み込んだ法線

                if (!(Stream >> Normal.x >> Normal.y >> Normal.z))
                {
                    return ReportParseFailure("normal requires three numbers");
                }

                Normals.push_back(Normal);
            }
            else if (Type == "f")
            {
                std::vector<uint32_t> FaceIndices; //現在Faceの共有頂点Index一覧

                std::string Token; //現在解析中のFace要素

                while (Stream >> Token)
                {
                    OBJIndex RawIndex{}; //OBJ表記の未解決Index組

                    if (!ParseFaceToken(Token, RawIndex))
                    {
                        return ReportParseFailure("face token format is invalid");
                    }

                    OBJIndex ResolvedIndex{}; //ゼロ始まりへ解決したIndex組

                    if (!ResolveOBJIndex(
                        RawIndex.Position,
                        Positions.size(),
                        true,
                        ResolvedIndex.Position) ||
                        !ResolveOBJIndex(
                            RawIndex.UV,
                            UVs.size(),
                            false,
                            ResolvedIndex.UV) ||
                        !ResolveOBJIndex(
                            RawIndex.Normal,
                            Normals.size(),
                            false,
                            ResolvedIndex.Normal))
                    {
                        return ReportParseFailure("face index is outside the declared arrays");
                    }

                    auto VertexIterator = VertexMap.find(ResolvedIndex); //既存共有頂点の検索結果

                    if (VertexIterator != VertexMap.end())
                    {
                        FaceIndices.push_back(VertexIterator->second);
                    }
                    else
                    {
                        OBJVertex VertexData{}; //新規共有頂点

                        VertexData.Position =
                            Positions[ResolvedIndex.Position];

                        if (ResolvedIndex.Normal >= 0)
                        {
                            VertexData.Normal =
                                Normals[ResolvedIndex.Normal];
                        }
                        else
                        {
                            VertexData.Normal =
                                DirectX::XMFLOAT3(
                                    0.0f,
                                    1.0f,
                                    0.0f
                                );
                        }

                        if (ResolvedIndex.UV >= 0)
                        {
                            VertexData.UV =
                                UVs[ResolvedIndex.UV];
                        }
                        else
                        {
                            VertexData.UV =
                                DirectX::XMFLOAT2(
                                    0.0f,
                                    0.0f
                                );
                        }

                        uint32_t NewIndex =
                            static_cast<uint32_t>(
                                Vertices.size()
                                ); //新規共有頂点Index

                        Vertices.push_back(VertexData);

                        VertexMap.emplace(
                            ResolvedIndex,
                            NewIndex
                        );

                        FaceIndices.push_back(NewIndex);
                    }
                }

                if (FaceIndices.size() < 3)
                {
                    return ReportParseFailure("face has fewer than three vertices");
                }

                // 三角形・四角形・多角形をTriangle Fanで分解
                for (size_t Index = 1; Index + 1 < FaceIndices.size(); ++Index) //FaceをTriangle Fanへ分解する
                {
                    Indices.push_back(FaceIndices[0]);
                    Indices.push_back(FaceIndices[Index]);
                    Indices.push_back(FaceIndices[Index + 1]);
                }
            }
        }

        if (Vertices.empty() || Indices.empty())
        {
            return ReportParseFailure("file does not contain a drawable face");
        }

        return true;
    }

    //Model描画用RootSignatureを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool OBJModel::CreateRootSignature(
        DirectX12& dx12
    )
    {
        D3D12_DESCRIPTOR_RANGE SRVRange{}; //Diffuse Texture用Descriptor Range
        SRVRange.RangeType =
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        SRVRange.NumDescriptors = 1;
        SRVRange.BaseShaderRegister = 0;
        SRVRange.RegisterSpace = 0;
        SRVRange.OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER RootParameters[2]{}; //描画定数とTexture用Root Parameters

        RootParameters[0].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        RootParameters[0].Constants.ShaderRegister = 0;
        RootParameters[0].Constants.RegisterSpace = 0;
        RootParameters[0].Constants.Num32BitValues =
            OBJRootConstantCount;
        RootParameters[0].ShaderVisibility =
            D3D12_SHADER_VISIBILITY_ALL;

        RootParameters[1].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
        RootParameters[1].DescriptorTable.pDescriptorRanges =
            &SRVRange;
        RootParameters[1].ShaderVisibility =
            D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC Sampler{}; //Diffuse Texture用Sampler設定
        Sampler.Filter =
            D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        Sampler.AddressU =
            D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        Sampler.AddressV =
            D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        Sampler.AddressW =
            D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        Sampler.MipLODBias = 0.0f;
        Sampler.MaxAnisotropy = 1;
        Sampler.ComparisonFunc =
            D3D12_COMPARISON_FUNC_ALWAYS;
        Sampler.BorderColor =
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        Sampler.MinLOD = 0.0f;
        Sampler.MaxLOD = D3D12_FLOAT32_MAX;
        Sampler.ShaderRegister = 0;
        Sampler.RegisterSpace = 0;
        Sampler.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC Description{}; //RootSignature設定
        Description.NumParameters = 2;
        Description.pParameters = RootParameters;
        Description.NumStaticSamplers = 1;
        Description.pStaticSamplers = &Sampler;
        Description.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> Signature; //Serialize済みRootSignature
        Microsoft::WRL::ComPtr<ID3DBlob> Error; //Serialize失敗内容

        HRESULT Result = D3D12SerializeRootSignature(
            &Description,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &Signature,
            &Error
        ); //RootSignatureのSerialize結果

        if (FAILED(Result))
        {
            return false;
        }

        Result = dx12.GetDevice()->CreateRootSignature(
            0,
            Signature->GetBufferPointer(),
            Signature->GetBufferSize(),
            IID_PPV_ARGS(&RootSignature)
        );

        return SUCCEEDED(Result);
    }

    //Model描画用PipelineStateを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool OBJModel::CreatePipelineState(
        DirectX12& dx12
    )
    {
        Microsoft::WRL::ComPtr<ID3DBlob> VertexShaderBlob; //Compile済みVertex Shader
        Microsoft::WRL::ComPtr<ID3DBlob> PixelShaderBlob; //Compile済みPixel Shader
        Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob; //Shader Compile失敗内容

        UINT CompileFlags = 0; //Shader Compile Option

#if defined(_DEBUG)
        CompileFlags =
            D3DCOMPILE_DEBUG |
            D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT Result = D3DCompile(
            OBJShaderCode,
            std::strlen(OBJShaderCode),
            nullptr,
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_0",
            CompileFlags,
            0,
            &VertexShaderBlob,
            &ErrorBlob
        ); //Vertex Shader Compile結果

        if (FAILED(Result))
        {
            return false;
        }

        Result = D3DCompile(
            OBJShaderCode,
            std::strlen(OBJShaderCode),
            nullptr,
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_0",
            CompileFlags,
            0,
            &PixelShaderBlob,
            &ErrorBlob
        );

        if (FAILED(Result))
        {
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC InputLayout[3]{}; //Position、Normal、UVの頂点入力定義

        InputLayout[0].SemanticName = "POSITION";
        InputLayout[0].Format =
            DXGI_FORMAT_R32G32B32_FLOAT;
        InputLayout[0].InputSlot = 0;
        InputLayout[0].AlignedByteOffset = 0;
        InputLayout[0].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        InputLayout[1].SemanticName = "NORMAL";
        InputLayout[1].Format =
            DXGI_FORMAT_R32G32B32_FLOAT;
        InputLayout[1].InputSlot = 0;
        InputLayout[1].AlignedByteOffset = 12;
        InputLayout[1].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        InputLayout[2].SemanticName = "TEXCOORD";
        InputLayout[2].Format =
            DXGI_FORMAT_R32G32_FLOAT;
        InputLayout[2].InputSlot = 0;
        InputLayout[2].AlignedByteOffset = 24;
        InputLayout[2].InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDescription{}; //Model描画Pipeline設定
        PipelineDescription.pRootSignature =
            RootSignature.Get();

        PipelineDescription.VS.pShaderBytecode =
            VertexShaderBlob->GetBufferPointer();
        PipelineDescription.VS.BytecodeLength =
            VertexShaderBlob->GetBufferSize();

        PipelineDescription.PS.pShaderBytecode =
            PixelShaderBlob->GetBufferPointer();
        PipelineDescription.PS.BytecodeLength =
            PixelShaderBlob->GetBufferSize();

        PipelineDescription.InputLayout.pInputElementDescs =
            InputLayout;
        PipelineDescription.InputLayout.NumElements = 3;

        PipelineDescription.PrimitiveTopologyType =
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        PipelineDescription.NumRenderTargets = 1;
        PipelineDescription.RTVFormats[0] =
            dx12.GetBackBufferFormat();
        PipelineDescription.DSVFormat =
            dx12.GetDepthStencilFormat();

        PipelineDescription.SampleDesc.Count = 1;
        PipelineDescription.SampleDesc.Quality = 0;
        PipelineDescription.SampleMask = UINT_MAX;

        PipelineDescription.RasterizerState.FillMode =
            D3D12_FILL_MODE_SOLID;
        PipelineDescription.RasterizerState.CullMode =
            D3D12_CULL_MODE_NONE;
        PipelineDescription.RasterizerState.FrontCounterClockwise = FALSE;
        PipelineDescription.RasterizerState.DepthClipEnable = TRUE;
        PipelineDescription.RasterizerState.MultisampleEnable = FALSE;
        PipelineDescription.RasterizerState.AntialiasedLineEnable = FALSE;
        PipelineDescription.RasterizerState.ForcedSampleCount = 0;
        PipelineDescription.RasterizerState.ConservativeRaster =
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        PipelineDescription.BlendState.AlphaToCoverageEnable = FALSE;
        PipelineDescription.BlendState.IndependentBlendEnable = FALSE;

        D3D12_RENDER_TARGET_BLEND_DESC BlendDescription{}; //単一RenderTargetのBlend設定
        BlendDescription.BlendEnable = FALSE;
        BlendDescription.LogicOpEnable = FALSE;
        BlendDescription.SrcBlend =
            D3D12_BLEND_ONE;
        BlendDescription.DestBlend =
            D3D12_BLEND_ZERO;
        BlendDescription.BlendOp =
            D3D12_BLEND_OP_ADD;
        BlendDescription.SrcBlendAlpha =
            D3D12_BLEND_ONE;
        BlendDescription.DestBlendAlpha =
            D3D12_BLEND_ZERO;
        BlendDescription.BlendOpAlpha =
            D3D12_BLEND_OP_ADD;
        BlendDescription.LogicOp =
            D3D12_LOGIC_OP_NOOP;
        BlendDescription.RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;

        PipelineDescription.BlendState.RenderTarget[0] =
            BlendDescription;

        PipelineDescription.DepthStencilState.DepthEnable = TRUE;
        PipelineDescription.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ALL;
        PipelineDescription.DepthStencilState.DepthFunc =
            D3D12_COMPARISON_FUNC_LESS_EQUAL;
        PipelineDescription.DepthStencilState.StencilEnable = FALSE;

        PipelineDescription.IBStripCutValue =
            D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        Result = dx12.GetDevice()->CreateGraphicsPipelineState(
            &PipelineDescription,
            IID_PPV_ARGS(&PipelineState)
        );

        return SUCCEEDED(Result);
    }

    //頂点Bufferを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool OBJModel::CreateVertexBuffer(
        DirectX12& dx12
    )
    {
        const UINT BufferSize =
            static_cast<UINT>(
                sizeof(OBJVertex) * Vertices.size()
                ); //頂点BufferのByte数

        D3D12_HEAP_PROPERTIES HeapProperties{}; //Upload Heap設定
        HeapProperties.Type =
            D3D12_HEAP_TYPE_UPLOAD;
        HeapProperties.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProperties.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDescription{}; //頂点Buffer Resource設定
        ResourceDescription.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDescription.Width = BufferSize;
        ResourceDescription.Height = 1;
        ResourceDescription.DepthOrArraySize = 1;
        ResourceDescription.MipLevels = 1;
        ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDescription.SampleDesc.Count = 1;
        ResourceDescription.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT Result = dx12.GetDevice()->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&VertexBuffer)
        ); //頂点Buffer作成結果

        if (FAILED(Result))
        {
            return false;
        }

        void* MappedData = nullptr; //頂点BufferのCPU書込先

        Result = VertexBuffer->Map(
            0,
            nullptr,
            &MappedData
        );

        if (FAILED(Result))
        {
            return false;
        }

        std::memcpy(
            MappedData,
            Vertices.data(),
            BufferSize
        );

        VertexBuffer->Unmap(
            0,
            nullptr
        );

        VertexBufferView.BufferLocation =
            VertexBuffer->GetGPUVirtualAddress();
        VertexBufferView.SizeInBytes =
            BufferSize;
        VertexBufferView.StrideInBytes =
            sizeof(OBJVertex);

        return true;
    }

    //Index Bufferを作成する
    //引数: dx12 描画基盤
    //戻り値: 作成に成功した場合はtrue
    bool OBJModel::CreateIndexBuffer(
        DirectX12& dx12
    )
    {
        const UINT BufferSize =
            static_cast<UINT>(
                sizeof(uint32_t) * Indices.size()
                ); //Index BufferのByte数

        D3D12_HEAP_PROPERTIES HeapProperties{}; //Upload Heap設定
        HeapProperties.Type =
            D3D12_HEAP_TYPE_UPLOAD;
        HeapProperties.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProperties.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDescription{}; //Index Buffer Resource設定
        ResourceDescription.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDescription.Width = BufferSize;
        ResourceDescription.Height = 1;
        ResourceDescription.DepthOrArraySize = 1;
        ResourceDescription.MipLevels = 1;
        ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDescription.SampleDesc.Count = 1;
        ResourceDescription.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT Result = dx12.GetDevice()->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&IndexBuffer)
        ); //Index Buffer作成結果

        if (FAILED(Result))
        {
            return false;
        }

        void* MappedData = nullptr; //Index BufferのCPU書込先

        Result = IndexBuffer->Map(
            0,
            nullptr,
            &MappedData
        );

        if (FAILED(Result))
        {
            return false;
        }

        std::memcpy(
            MappedData,
            Indices.data(),
            BufferSize
        );

        IndexBuffer->Unmap(
            0,
            nullptr
        );

        IndexBufferView.BufferLocation =
            IndexBuffer->GetGPUVirtualAddress();
        IndexBufferView.SizeInBytes =
            BufferSize;
        IndexBufferView.Format =
            DXGI_FORMAT_R32_UINT;

        return true;
    }

    void OBJModel::SetColor(
        const DirectX::XMFLOAT4& color
    )
    {
        Color = color;
    }

    //Modelの時間依存情報を更新する
    //引数: deltaTime 前回更新からの秒数
    void OBJModel::Update(float deltaTime)
    {
        (void)deltaTime;
    }

    DirectX::XMMATRIX OBJModel::GetWorldMatrix() const
    {
        const Object* Owner = GetOwner(); //Model Componentを所有するObject
        return Owner != nullptr ? Owner->GetWorldMatrix() : DirectX::XMMatrixIdentity();
    }

    //現在のCameraでOBJ Modelを描画する
    //引数: renderContext 描画基盤とCameraを持つContext
    void OBJModel::Draw(const RenderContext& renderContext)
    {
        if (!PipelineState)
            return;

        if (!RootSignature)
            return;

        if (Indices.empty())
            return;

        DirectX12& Dx12 = renderContext.Graphics; //このpassで使用する描画基盤
        const Camera& ViewCamera = renderContext.ViewCamera; //このpassで使用するCamera
        ID3D12GraphicsCommandList* CommandList = Dx12.GetCommandList(); //描画命令の記録先

        if (CommandList == nullptr || !VertexBuffer || !IndexBuffer ||
            !Texture.IsValid())
        {
            return;
        }

        using namespace DirectX;

        XMMATRIX World =
            GetWorldMatrix(); //Owner ObjectのWorld行列

        XMMATRIX ViewProjection =
            ViewCamera.GetViewProjectionMatrix(); //現在CameraのViewProjection行列

        XMMATRIX WorldViewProjection =
            World * ViewProjection; //ModelのWVP行列

        XMMATRIX TransposedWorldViewProjection =
            XMMatrixTranspose(WorldViewProjection); //Shader転送用の転置済みWVP行列

        OBJConstantBuffer RootConstants{}; //今回のCamera passだけで使用する描画定数
        XMStoreFloat4x4(
            &RootConstants.WorldViewProjection,
            TransposedWorldViewProjection
        );

        RootConstants.Color =
            Color;

        RootConstants.UseTexture =
            UseTexture ? 1 : 0;

        ID3D12DescriptorHeap* DescriptorHeaps[] = //描画時に設定するTexture Heap
        {
            Texture.GetSRVHeap()
        };

        CommandList->SetDescriptorHeaps(
            1,
            DescriptorHeaps
        );

        CommandList->SetGraphicsRootSignature(
            RootSignature.Get()
        );

        CommandList->SetPipelineState(
            PipelineState.Get()
        );

        CommandList->SetGraphicsRoot32BitConstants(
            0,
            OBJRootConstantCount,
            &RootConstants,
            0
        );

        CommandList->SetGraphicsRootDescriptorTable(
            1,
            Texture.GetSRVGPUHandle()
        );

        CommandList->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
        );

        CommandList->IASetVertexBuffers(
            0,
            1,
            &VertexBufferView
        );

        CommandList->IASetIndexBuffer(
            &IndexBufferView
        );

        CommandList->DrawIndexedInstanced(
            static_cast<UINT>(Indices.size()),
            1,
            0,
            0,
            0
        );
    }
}
