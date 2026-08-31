//|| GameScriptAPI.h ||:::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  ObjectへAttachするゲームScript向けに入力、Transform、色の簡易C++ APIを提供する
//||  低水準のEngineScriptHostAPIは変更せず内部実装として保持する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_18  v1.00  新規作成
//||

#pragma once

#include "ScriptModuleAPI.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace EngineGame
{
    enum class GameKey : std::uint32_t
    {
        Backspace = 0x08,
        Tab = 0x09,
        Enter = 0x0D,
        Shift = 0x10,
        Control = 0x11,
        Escape = 0x1B,
        Space = 0x20,
        LeftArrow = 0x25,
        UpArrow = 0x26,
        RightArrow = 0x27,
        DownArrow = 0x28,
        A = 0x41,
        B = 0x42,
        D = 0x44,
        G = 0x47,
        R = 0x52,
        S = 0x53,
        W = 0x57
    };

    inline constexpr GameKey LeftArrow = GameKey::LeftArrow;
    inline constexpr GameKey UpArrow = GameKey::UpArrow;
    inline constexpr GameKey RightArrow = GameKey::RightArrow;
    inline constexpr GameKey DownArrow = GameKey::DownArrow;
    inline constexpr GameKey KeyA = GameKey::A;
    inline constexpr GameKey KeyB = GameKey::B;
    inline constexpr GameKey KeyD = GameKey::D;
    inline constexpr GameKey KeyG = GameKey::G;
    inline constexpr GameKey KeyR = GameKey::R;
    inline constexpr GameKey KeyS = GameKey::S;
    inline constexpr GameKey KeyW = GameKey::W;

    enum class GameObjectType : std::uint32_t
    {
        Object,
        Box,
        Sphere,
        Plane,
        Cylinder,
        HalfSphere,
        Capsule,
        SkyBox
    };

    struct Float3 final
    {
        union
        {
            float X;
            float x;
        };
        union
        {
            float Y;
            float y;
        };
        union
        {
            float Z;
            float z;
        };

        //概要：大文字又は小文字Memberで参照できる3次元値を作成する
        //引数：xValue=X値、yValue=Y値、zValue=Z値
        //戻り値：なし
        constexpr Float3(
            float xValue = 0.0f,
            float yValue = 0.0f,
            float zValue = 0.0f
        )
            : X(xValue)
            , Y(yValue)
            , Z(zValue)
        {
        }
    };

    struct Color4 final
    {
        float Red = 1.0f;
        float Green = 1.0f;
        float Blue = 1.0f;
        float Alpha = 1.0f;
    };

    class PositionProperty final
    {
    public:
        //概要：所有ObjectのPositionを読み書きするPropertyを作成する
        //引数：host=低水準Script Host API
        //戻り値：なし
        explicit PositionProperty(const EngineScriptHostAPI* host = nullptr)
            : Host(host)
        {
        }

        //概要：Propertyから所有Objectの現在Positionを読み取る
        //引数：なし
        //戻り値：所有ObjectのPosition、Host不整合時はZero
        operator Float3() const
        {
            Float3 Result; //Game Scriptへ返すPosition

            if (Host != nullptr && Host->GetPosition != nullptr)
            {
                float Value[3]{}; //低水準Hostから受け取るXYZ
                Host->GetPosition(Host->Context, Value);
                Result = { Value[0], Value[1], Value[2] };
            }

            return Result;
        }

        //概要：Propertyへの代入で所有ObjectのPositionを変更する
        //引数：value=設定するXYZ
        //戻り値：連続代入に利用できるProperty参照
        PositionProperty& operator=(const Float3& value)
        {
            if (Host != nullptr && Host->SetPosition != nullptr)
            {
                const float Value[3]{ value.X, value.Y, value.Z }; //低水準Hostへ渡すXYZ
                Host->SetPosition(Host->Context, Value);
            }

            return *this;
        }

    private:
        const EngineScriptHostAPI* Host; //Object Transformへ接続する低水準API
    };

    class ColorProperty final
    {
    public:
        //概要：所有PrimitiveのColorを読み書きするPropertyを作成する
        //引数：host=低水準Script Host API
        //戻り値：なし
        explicit ColorProperty(const EngineScriptHostAPI* host = nullptr)
            : Host(host)
        {
        }

        //概要：Propertyから所有Primitiveの現在Colorを読み取る
        //引数：なし
        //戻り値：所有PrimitiveのRGBA、非Primitive時はWhite
        operator Color4() const
        {
            Color4 Result; //Game Scriptへ返すRGBA

            if (Host != nullptr && Host->GetColor != nullptr)
            {
                float Value[4]{}; //低水準Hostから受け取るRGBA

                if (Host->GetColor(Host->Context, Value) != 0)
                {
                    Result = { Value[0], Value[1], Value[2], Value[3] };
                }
            }

            return Result;
        }

        //概要：Propertyへの代入で所有PrimitiveのColorを変更する
        //引数：value=設定するRGBA
        //戻り値：連続代入に利用できるProperty参照
        ColorProperty& operator=(const Color4& value)
        {
            if (Host != nullptr && Host->SetColor != nullptr)
            {
                const float Value[4]
                {
                    value.Red,
                    value.Green,
                    value.Blue,
                    value.Alpha
                }; //低水準Hostへ渡すRGBA
                Host->SetColor(Host->Context, Value);
            }

            return *this;
        }

    private:
        const EngineScriptHostAPI* Host; //Primitive Colorへ接続する低水準API
    };

    class ObjectScript
    {
    private:
        enum class PublicType { Float, Integer, Boolean, Vector3, Color, String, Function };
        struct PublicBinding final
        {
            std::string Name;
            PublicType Type = PublicType::Float;
            void* Value = nullptr;
            std::function<void()> Function;
            bool ReadOnly = false;
        };
        const EngineScriptHostAPI* Host; //従来の詳細操作を保持する低水準Host API
        std::vector<PublicBinding> PublicBindings;

    public:
        PositionProperty Position; //this->Position形式で扱うTransform Property
        ColorProperty Color; //this->Color形式で扱うPrimitive Color Property

        //概要：低水準Host APIを簡易Game Script APIへ接続する
        //引数：host=Engineが所有Objectごとに提供するHost API
        //戻り値：なし
        explicit ObjectScript(const EngineScriptHostAPI* host)
            : Host(host)
            , Position(host)
            , Color(host)
        {
        }

        //概要：Game Script基底を破棄する
        //引数：なし
        //戻り値：なし
        ~ObjectScript() = default;

        void ExposeVariable(const std::string& name, float& value, bool readOnly = false)
        { PublicBindings.push_back({ name, PublicType::Float, &value, {}, readOnly }); }
        void ExposeVariable(const std::string& name, std::int32_t& value, bool readOnly = false)
        { PublicBindings.push_back({ name, PublicType::Integer, &value, {}, readOnly }); }
        void ExposeVariable(const std::string& name, bool& value, bool readOnly = false)
        { PublicBindings.push_back({ name, PublicType::Boolean, &value, {}, readOnly }); }
        void ExposeVariable(const std::string& name, Float3& value, bool readOnly = false)
        { PublicBindings.push_back({ name, PublicType::Vector3, &value, {}, readOnly }); }
        void ExposeVariable(const std::string& name, Color4& value, bool readOnly = false)
        { PublicBindings.push_back({ name, PublicType::Color, &value, {}, readOnly }); }
        void ExposeVariable(const std::string& name, std::string& value, bool readOnly = false)
        { PublicBindings.push_back({ name, PublicType::String, &value, {}, readOnly }); }
        void ExposeFunction(const std::string& name, std::function<void()> function)
        { PublicBindings.push_back({ name, PublicType::Function, nullptr, std::move(function), false }); }

        std::uint32_t GetExposedMemberCount() const
        { return static_cast<std::uint32_t>(PublicBindings.size()); }

        bool GetExposedMemberInfo(
            std::uint32_t index,
            EngineScriptExposedMemberInfo& information
        ) const
        {
            if (index >= PublicBindings.size()) return false;
            const PublicBinding& Binding = PublicBindings[index];
            information = EngineScriptExposedMemberInfo{};
            information.Size = sizeof(EngineScriptExposedMemberInfo);
            information.Function = Binding.Type == PublicType::Function;
            information.ReadOnly = Binding.ReadOnly;
            std::snprintf(information.Name, sizeof(information.Name), "%s", Binding.Name.c_str());
            const char* TypeName = "Float";
            std::string Value;
            char Buffer[256]{};
            switch (Binding.Type)
            {
            case PublicType::Float:
                std::snprintf(Buffer, sizeof(Buffer), "%g", *static_cast<float*>(Binding.Value)); break;
            case PublicType::Integer:
                TypeName = "Integer"; std::snprintf(Buffer, sizeof(Buffer), "%d", *static_cast<std::int32_t*>(Binding.Value)); break;
            case PublicType::Boolean:
                TypeName = "Bool"; std::snprintf(Buffer, sizeof(Buffer), "%s", *static_cast<bool*>(Binding.Value) ? "true" : "false"); break;
            case PublicType::Vector3:
            {
                TypeName = "Vector3"; const Float3& V = *static_cast<Float3*>(Binding.Value);
                std::snprintf(Buffer, sizeof(Buffer), "%g,%g,%g", V.X, V.Y, V.Z); break;
            }
            case PublicType::Color:
            {
                TypeName = "Color"; const Color4& V = *static_cast<Color4*>(Binding.Value);
                std::snprintf(Buffer, sizeof(Buffer), "%g,%g,%g,%g", V.Red, V.Green, V.Blue, V.Alpha); break;
            }
            case PublicType::String:
                TypeName = "String"; std::snprintf(Buffer, sizeof(Buffer), "%s", static_cast<std::string*>(Binding.Value)->c_str()); break;
            case PublicType::Function:
                TypeName = "Action"; break;
            }
            std::snprintf(information.Type, sizeof(information.Type), "%s", TypeName);
            std::snprintf(information.Value, sizeof(information.Value), "%s", Buffer);
            return true;
        }

        bool SetExposedMember(const std::string& name, const std::string& value)
        {
            for (PublicBinding& Binding : PublicBindings)
            {
                if (Binding.Name != name || Binding.ReadOnly || Binding.Type == PublicType::Function) continue;
                switch (Binding.Type)
                {
                case PublicType::Float: *static_cast<float*>(Binding.Value) = std::strtof(value.c_str(), nullptr); return true;
                case PublicType::Integer: *static_cast<std::int32_t*>(Binding.Value) = static_cast<std::int32_t>(std::strtol(value.c_str(), nullptr, 10)); return true;
                case PublicType::Boolean: *static_cast<bool*>(Binding.Value) = value == "true" || value == "1"; return true;
                case PublicType::Vector3:
                {
                    Float3& V = *static_cast<Float3*>(Binding.Value);
                    return ::sscanf_s(value.c_str(), "%f,%f,%f", &V.X, &V.Y, &V.Z) == 3;
                }
                case PublicType::Color:
                {
                    Color4& V = *static_cast<Color4*>(Binding.Value);
                    return ::sscanf_s(value.c_str(), "%f,%f,%f,%f", &V.Red, &V.Green, &V.Blue, &V.Alpha) == 4;
                }
                case PublicType::String: *static_cast<std::string*>(Binding.Value) = value; return true;
                default: return false;
                }
            }
            return false;
        }

        bool InvokeExposedFunction(const std::string& name)
        {
            for (PublicBinding& Binding : PublicBindings)
            {
                if (Binding.Name == name && Binding.Type == PublicType::Function && Binding.Function)
                { Binding.Function(); return true; }
            }
            return false;
        }

        //概要：指定Keyboard Keyが現在押されているか取得する
        //引数：key=GameKeyで指定するKey
        //戻り値：押されている場合true
        bool GetKeyPress(GameKey key) const
        {
            return Host != nullptr && Host->IsKeyDown != nullptr &&
                Host->IsKeyDown(
                    Host->Context,
                    static_cast<std::uint32_t>(key)
                ) != 0;
        }

        //概要：所有Objectが指定Object型か判定する
        //引数：type=比較するGameObjectType
        //戻り値：所有Object型が一致する場合true
        bool IsObjectType(GameObjectType type) const
        {
            return Host != nullptr && Host->GetObjectType != nullptr &&
                Host->GetObjectType(Host->Context) == static_cast<std::uint32_t>(type);
        }

        //概要：所有ObjectのIDを取得する
        //引数：なし
        //戻り値：所有Object ID、Host不整合時は0
        std::uint32_t GetObjectID() const
        {
            return Host != nullptr && Host->GetObjectID != nullptr
                ? Host->GetObjectID(Host->Context)
                : 0;
        }

        //概要：所有ObjectのPositionをXYZ指定で変更する
        //引数：x=X座標、y=Y座標、z=Z座標
        //戻り値：なし
        void SetPosition(float x, float y, float z)
        {
            Position = Float3{ x, y, z };
        }

        //概要：所有ObjectのPositionへ移動量を加算する
        //引数：x=X移動量、y=Y移動量、z=Z移動量
        //戻り値：なし
        void Move(float x, float y, float z)
        {
            Float3 Next = Position; //加算前の所有Object Position
            Next.X += x;
            Next.Y += y;
            Next.Z += z;
            Position = Next;
        }

        //概要：指定Keyが押されている間だけ所有Objectを移動する
        //引数：key=移動Key、x=X移動量、y=Y移動量、z=Z移動量
        //戻り値：移動を適用した場合true
        bool MoveWhenPressed(GameKey key, float x, float y, float z)
        {
            if (!GetKeyPress(key))
            {
                return false;
            }

            Move(x, y, z);
            return true;
        }

        //概要：所有PrimitiveのColorをRGBA指定で変更する
        //引数：red=赤、green=緑、blue=青、alpha=透明度
        //戻り値：なし
        void SetColor(float red, float green, float blue, float alpha = 1.0f)
        {
            Color = Color4{ red, green, blue, alpha };
        }

        //概要：所有Primitiveの現在RGBA色へ指定係数を乗算する
        //引数：red=赤係数、green=緑係数、blue=青係数、alpha=透明度係数
        //戻り値：色を乗算できた場合true
        bool MultiplyColor(
            float red,
            float green,
            float blue,
            float alpha = 1.0f
        )
        {
            if (Host == nullptr || Host->MultiplyColor == nullptr)
            {
                return false;
            }

            const float Multiplier[4]{ red, green, blue, alpha }; //低水準Hostへ渡すRGBA係数
            return Host->MultiplyColor(Host->Context, Multiplier) != 0;
        }

        //概要：指定Keyが押されている間だけ所有PrimitiveのColorを変更する
        //引数：key=変更Key、color=設定するRGBA
        //戻り値：色変更を要求した場合true
        bool SetColorWhenPressed(GameKey key, const Color4& color)
        {
            if (!GetKeyPress(key))
            {
                return false;
            }

            Color = color;
            return true;
        }

        //概要：指定Keyが押されている間だけ現在色へRGBA係数を乗算する
        //引数：key=変更Key、multiplier=現在色へ乗算するRGBA係数
        //戻り値：色乗算を適用した場合true
        bool MultiplyColorWhenPressed(GameKey key, const Color4& multiplier)
        {
            return GetKeyPress(key) && MultiplyColor(
                multiplier.Red,
                multiplier.Green,
                multiplier.Blue,
                multiplier.Alpha
            );
        }

        //概要：ObjectへAttach可能かを派生Scriptから上書きする既定処理
        //引数：なし
        //戻り値：既定ではtrue
        bool OnAttach()
        {
            return true;
        }

        //概要：初回Update直前に派生Scriptから上書きする既定処理
        //引数：なし
        //戻り値：なし
        void OnStart()
        {
        }

        //概要：派生Scriptが毎Frame処理を実装する既定処理
        //引数：deltaTime=前Frameからの秒数
        //戻り値：なし
        void Update(float deltaTime)
        {
            (void)deltaTime;
        }

        //概要：実行終了時に派生Scriptから上書きする既定処理
        //引数：なし
        //戻り値：なし
        void OnStop()
        {
        }

        //概要：Objectから外れる時に派生Scriptから上書きする既定処理
        //引数：なし
        //戻り値：なし
        void OnDetach()
        {
        }
    };

    //概要：指定Game Script型のInstanceをDLL内で作成する
    //引数：host=所有Objectへ接続された低水準Host API
    //戻り値：作成したScript Instance、確保失敗時はnullptr
    template<class ScriptType>
    void* ENGINE_SCRIPT_CALL CreateObjectScript(const EngineScriptHostAPI* host)
    {
        static_assert(std::is_base_of_v<ObjectScript, ScriptType>);

        if (host == nullptr || host->Size < sizeof(EngineScriptHostAPI) ||
            host->AbiVersion != EngineScriptAbiVersion)
        {
            return nullptr;
        }

        return new (std::nothrow) ScriptType(host);
    }

    //概要：指定Game Script型のInstanceをDLL内で破棄する
    //引数：instance=CreateObjectScriptが返したInstance
    //戻り値：なし
    template<class ScriptType>
    void ENGINE_SCRIPT_CALL DestroyObjectScript(void* instance)
    {
        delete static_cast<ScriptType*>(instance);
    }

    //概要：指定Game Script型のAttach処理を呼び出す
    //引数：instance=対象Script Instance
    //戻り値：Attachを許可する場合1、それ以外は0
    template<class ScriptType>
    std::uint32_t ENGINE_SCRIPT_CALL AttachObjectScript(void* instance)
    {
        auto* Script = static_cast<ScriptType*>(instance); //AttachするGame Script
        return Script != nullptr && Script->OnAttach() ? 1u : 0u;
    }

    //概要：指定Game Script型のStart処理を呼び出す
    //引数：instance=対象Script Instance
    //戻り値：なし
    template<class ScriptType>
    void ENGINE_SCRIPT_CALL StartObjectScript(void* instance)
    {
        auto* Script = static_cast<ScriptType*>(instance); //開始するGame Script

        if (Script != nullptr)
        {
            Script->OnStart();
        }
    }

    //概要：指定Game Script型の毎Frame処理を呼び出す
    //引数：instance=対象Script Instance、deltaTime=前Frameからの秒数
    //戻り値：なし
    template<class ScriptType>
    void ENGINE_SCRIPT_CALL UpdateObjectScript(void* instance, float deltaTime)
    {
        auto* Script = static_cast<ScriptType*>(instance); //更新するGame Script

        if (Script != nullptr)
        {
            Script->Update(deltaTime);
        }
    }

    //概要：指定Game Script型のStop処理を呼び出す
    //引数：instance=対象Script Instance
    //戻り値：なし
    template<class ScriptType>
    void ENGINE_SCRIPT_CALL StopObjectScript(void* instance)
    {
        auto* Script = static_cast<ScriptType*>(instance); //停止するGame Script

        if (Script != nullptr)
        {
            Script->OnStop();
        }
    }

    //概要：指定Game Script型のDetach処理を呼び出す
    //引数：instance=対象Script Instance
    //戻り値：なし
    template<class ScriptType>
    void ENGINE_SCRIPT_CALL DetachObjectScript(void* instance)
    {
        auto* Script = static_cast<ScriptType*>(instance); //切断するGame Script

        if (Script != nullptr)
        {
            Script->OnDetach();
        }
    }

    template<class ScriptType>
    std::uint32_t ENGINE_SCRIPT_CALL GetObjectScriptExposedMemberCount(void* instance)
    {
        auto* Script = static_cast<ScriptType*>(instance);
        return Script == nullptr ? 0u : Script->GetExposedMemberCount();
    }

    template<class ScriptType>
    bool ENGINE_SCRIPT_CALL GetObjectScriptExposedMemberInfo(
        void* instance,
        std::uint32_t index,
        EngineScriptExposedMemberInfo* information
    )
    {
        auto* Script = static_cast<ScriptType*>(instance);
        return Script != nullptr && information != nullptr &&
            Script->GetExposedMemberInfo(index, *information);
    }

    template<class ScriptType>
    bool ENGINE_SCRIPT_CALL SetObjectScriptExposedMember(
        void* instance,
        const char* name,
        const char* value
    )
    {
        auto* Script = static_cast<ScriptType*>(instance);
        return Script != nullptr && name != nullptr && value != nullptr &&
            Script->SetExposedMember(name, value);
    }

    template<class ScriptType>
    bool ENGINE_SCRIPT_CALL InvokeObjectScriptExposedFunction(
        void* instance,
        const char* name
    )
    {
        auto* Script = static_cast<ScriptType*>(instance);
        return Script != nullptr && name != nullptr &&
            Script->InvokeExposedFunction(name);
    }

    //概要：Game Script型からEngine登録用の低水準Descriptorを作成する
    //引数：typeKey=Module内一意Key、displayName=Editor表示名
    //戻り値：EngineScriptDescriptor関数表
    template<class ScriptType>
    EngineScriptDescriptor MakeObjectScriptDescriptor(
        const char* typeKey,
        const char* displayName
    )
    {
        static_assert(std::is_base_of_v<ObjectScript, ScriptType>);
        return EngineScriptDescriptor
        {
            sizeof(EngineScriptDescriptor),
            typeKey,
            displayName,
            CreateObjectScript<ScriptType>,
            DestroyObjectScript<ScriptType>,
            AttachObjectScript<ScriptType>,
            StartObjectScript<ScriptType>,
            UpdateObjectScript<ScriptType>,
            StopObjectScript<ScriptType>,
            DetachObjectScript<ScriptType>,
            GetObjectScriptExposedMemberCount<ScriptType>,
            GetObjectScriptExposedMemberInfo<ScriptType>,
            SetObjectScriptExposedMember<ScriptType>,
            InvokeObjectScriptExposedFunction<ScriptType>
        };
    }
}
