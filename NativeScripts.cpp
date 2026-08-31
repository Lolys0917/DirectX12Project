#include "NativeScripts.h"
#include "PlaybackSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "Object.h"
#include "PrimitiveObject.h"

namespace Engine
{
    namespace
    {
        std::string FloatText(float value)
        {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), "%g", value);
            return Buffer;
        }

        std::string VectorText(const DirectX::XMFLOAT3& value)
        {
            char Buffer[128]{};
            std::snprintf(Buffer, sizeof(Buffer), "%g,%g,%g", value.x, value.y, value.z);
            return Buffer;
        }

        std::string ColorText(const DirectX::XMFLOAT4& value)
        {
            char Buffer[160]{};
            std::snprintf(Buffer, sizeof(Buffer), "%g,%g,%g,%g",
                value.x, value.y, value.z, value.w);
            return Buffer;
        }

        bool ParseFloat(const std::string& text, float& value)
        {
            return ::sscanf_s(text.c_str(), "%f", &value) == 1 && std::isfinite(value);
        }

        bool ParseVector(const std::string& text, DirectX::XMFLOAT3& value)
        {
            return ::sscanf_s(text.c_str(), "%f,%f,%f", &value.x, &value.y, &value.z) == 3;
        }

        bool ParseColor(const std::string& text, DirectX::XMFLOAT4& value)
        {
            return ::sscanf_s(text.c_str(), "%f,%f,%f,%f",
                &value.x, &value.y, &value.z, &value.w) == 4;
        }
    }

    GravityScript::GravityScript() : Script("native.gravity", "重力 / Gravity") {}

    std::vector<ScriptExposedMember> GravityScript::GetExposedMembers() const
    {
        return { { "GravityScale", "Float", FloatText(GravityScale) },
            { "GroundOffset", "Float", FloatText(GroundOffset) },
            { "Velocity", "Vector3", VectorText(Velocity), false, true },
            { "ResetVelocity", "Action", "", true } };
    }
    bool GravityScript::SetExposedMember(const std::string& name, const std::string& text)
    {
        float value = 0;
        if (!ParseFloat(text, value) || value < 0 || value > 1000) return false;
        if (name == "GravityScale") { GravityScale = value; return true; }
        if (name == "GroundOffset") { GroundOffset = value; return true; }
        return false;
    }
    bool GravityScript::InvokeExposedFunction(const std::string& name)
    {
        if (name != "ResetVelocity") return false;
        Velocity = {}; return true;
    }
    std::unique_ptr<Script> GravityScript::CloneScript() const
    {
        auto copy = std::make_unique<GravityScript>();
        copy->GravityScale = GravityScale; copy->GroundOffset = GroundOffset;
        return copy;
    }
    bool GravityScript::OnAttach() { Velocity = {}; return GetOwner() != nullptr; }
    void GravityScript::OnUpdate(float deltaTime)
    {
        if (!GetOwner() || deltaTime <= 0) return;
        const auto& s = ActivePlaybackSettings;
        auto p = GetOwner()->GetPosition();
        // 重力だけを細分化し、大きいゲーム速度でも床を突き抜けないようにする。
        const int count = std::clamp(int(std::ceil(deltaTime / (1.0f / 120))), 1, 1200);
        const float dt = deltaTime / count;
        for (int i = 0; i < count; ++i)
        {
            const float damping = std::exp(-s.LinearDrag * dt);
            Velocity.x = (Velocity.x + s.GravityX * GravityScale * dt) * damping;
            Velocity.y = (Velocity.y + s.GravityY * GravityScale * dt) * damping;
            Velocity.z = (Velocity.z + s.GravityZ * GravityScale * dt) * damping;
            const float speed = std::sqrt(Velocity.x * Velocity.x + Velocity.y * Velocity.y + Velocity.z * Velocity.z);
            if (speed > s.MaxFallSpeed)
            {
                const float ratio = s.MaxFallSpeed / speed;
                Velocity.x *= ratio; Velocity.y *= ratio; Velocity.z *= ratio;
            }
            p.x += Velocity.x * dt; p.y += Velocity.y * dt; p.z += Velocity.z * dt;
            if (s.GroundEnabled && p.y < s.GroundHeight + GroundOffset)
            {
                p.y = s.GroundHeight + GroundOffset;
                if (Velocity.y < 0) Velocity.y = -Velocity.y * s.Restitution;
                if (std::abs(Velocity.y) < 0.05f) Velocity.y = 0;
            }
        }
        GetOwner()->SetPosition(p);
    }

    BobbingScript::BobbingScript(float amplitude, float frequency)
        : Script("native.bobbing", "Bobbing Script")
        , Amplitude(amplitude)
        , Frequency(frequency)
    {
    }

    std::vector<ScriptExposedMember> BobbingScript::GetExposedMembers() const
    {
        return {
            { "Amplitude", "Float", FloatText(Amplitude) },
            { "Frequency", "Float", FloatText(Frequency) },
            { "Axis", "Vector3", VectorText(Axis) },
            { "Reset", "Action", "", true }
        };
    }

    bool BobbingScript::SetExposedMember(const std::string& name, const std::string& value)
    {
        if (name == "Amplitude") return ParseFloat(value, Amplitude);
        if (name == "Frequency") return ParseFloat(value, Frequency);
        if (name == "Axis") return ParseVector(value, Axis);
        return false;
    }

    bool BobbingScript::InvokeExposedFunction(const std::string& name)
    {
        if (name != "Reset" || GetOwner() == nullptr) return false;
        Time = 0.0f;
        GetOwner()->SetPosition(Origin);
        return true;
    }

    std::unique_ptr<Script> BobbingScript::CloneScript() const
    {
        auto Result = std::make_unique<BobbingScript>(Amplitude, Frequency);
        Result->Axis = Axis;
        return Result;
    }

    bool BobbingScript::OnAttach()
    {
        if (GetOwner() == nullptr) return false;
        Origin = GetOwner()->GetPosition();
        Time = 0.0f;
        return true;
    }

    void BobbingScript::OnUpdate(float deltaTime)
    {
        if (GetOwner() == nullptr) return;
        Time += deltaTime;
        const float Offset = std::sin(Time * Frequency) * Amplitude;
        GetOwner()->SetPosition({
            Origin.x + Axis.x * Offset,
            Origin.y + Axis.y * Offset,
            Origin.z + Axis.z * Offset
        });
    }

    OrbitScript::OrbitScript(float radius, float radiansPerSecond)
        : Script("native.orbit", "Orbit Script")
        , Radius(radius)
        , RadiansPerSecond(radiansPerSecond)
    {
    }

    std::vector<ScriptExposedMember> OrbitScript::GetExposedMembers() const
    {
        return {
            { "Center", "Vector3", VectorText(Center) },
            { "Radius", "Float", FloatText(Radius) },
            { "RadiansPerSecond", "Float", FloatText(RadiansPerSecond) },
            { "Reset", "Action", "", true }
        };
    }

    bool OrbitScript::SetExposedMember(const std::string& name, const std::string& value)
    {
        if (name == "Center") return ParseVector(value, Center);
        if (name == "Radius") return ParseFloat(value, Radius);
        if (name == "RadiansPerSecond") return ParseFloat(value, RadiansPerSecond);
        return false;
    }

    bool OrbitScript::InvokeExposedFunction(const std::string& name)
    {
        if (name != "Reset" || GetOwner() == nullptr) return false;
        Angle = 0.0f;
        GetOwner()->SetPosition({ Center.x + Radius, Center.y, Center.z });
        return true;
    }

    std::unique_ptr<Script> OrbitScript::CloneScript() const
    {
        auto Result = std::make_unique<OrbitScript>(Radius, RadiansPerSecond);
        Result->Center = Center;
        return Result;
    }

    bool OrbitScript::OnAttach()
    {
        if (GetOwner() == nullptr) return false;
        const DirectX::XMFLOAT3 Position = GetOwner()->GetPosition();
        Center = { Position.x - Radius, Position.y, Position.z };
        Angle = 0.0f;
        return true;
    }

    void OrbitScript::OnUpdate(float deltaTime)
    {
        if (GetOwner() == nullptr) return;
        Angle += RadiansPerSecond * deltaTime;
        GetOwner()->SetPosition({
            Center.x + std::cos(Angle) * Radius,
            Center.y,
            Center.z + std::sin(Angle) * Radius
        });
    }

    PulseScaleScript::PulseScaleScript(float minimum, float maximum, float speed)
        : Script("native.pulse_scale", "Pulse Scale Script")
        , Minimum(minimum)
        , Maximum(maximum)
        , Speed(speed)
    {
    }

    std::vector<ScriptExposedMember> PulseScaleScript::GetExposedMembers() const
    {
        return {
            { "Minimum", "Float", FloatText(Minimum) },
            { "Maximum", "Float", FloatText(Maximum) },
            { "Speed", "Float", FloatText(Speed) },
            { "Reset", "Action", "", true }
        };
    }

    bool PulseScaleScript::SetExposedMember(const std::string& name, const std::string& value)
    {
        if (name == "Minimum") return ParseFloat(value, Minimum);
        if (name == "Maximum") return ParseFloat(value, Maximum);
        if (name == "Speed") return ParseFloat(value, Speed);
        return false;
    }

    bool PulseScaleScript::InvokeExposedFunction(const std::string& name)
    {
        if (name != "Reset" || GetOwner() == nullptr) return false;
        Time = 0.0f;
        GetOwner()->SetScale(BaseScale);
        return true;
    }

    std::unique_ptr<Script> PulseScaleScript::CloneScript() const
    {
        return std::make_unique<PulseScaleScript>(Minimum, Maximum, Speed);
    }

    bool PulseScaleScript::OnAttach()
    {
        if (GetOwner() == nullptr) return false;
        BaseScale = GetOwner()->GetScale();
        Time = 0.0f;
        return true;
    }

    void PulseScaleScript::OnUpdate(float deltaTime)
    {
        if (GetOwner() == nullptr) return;
        Time += deltaTime;
        const float Mix = (std::sin(Time * Speed) + 1.0f) * 0.5f;
        const float Scale = Minimum + (Maximum - Minimum) * Mix;
        GetOwner()->SetScale({ BaseScale.x * Scale, BaseScale.y * Scale, BaseScale.z * Scale });
    }

    ColorPulseScript::ColorPulseScript(float speed)
        : Script("native.color_pulse", "Color Pulse Script")
        , Speed(speed)
    {
    }

    std::vector<ScriptExposedMember> ColorPulseScript::GetExposedMembers() const
    {
        return {
            { "Speed", "Float", FloatText(Speed) },
            { "ColorA", "Color", ColorText(ColorA) },
            { "ColorB", "Color", ColorText(ColorB) },
            { "SwapColors", "Action", "", true }
        };
    }

    bool ColorPulseScript::SetExposedMember(const std::string& name, const std::string& value)
    {
        if (name == "Speed") return ParseFloat(value, Speed);
        if (name == "ColorA") return ParseColor(value, ColorA);
        if (name == "ColorB") return ParseColor(value, ColorB);
        return false;
    }

    bool ColorPulseScript::InvokeExposedFunction(const std::string& name)
    {
        if (name != "SwapColors") return false;
        std::swap(ColorA, ColorB);
        return true;
    }

    std::unique_ptr<Script> ColorPulseScript::CloneScript() const
    {
        auto Result = std::make_unique<ColorPulseScript>(Speed);
        Result->ColorA = ColorA;
        Result->ColorB = ColorB;
        return Result;
    }

    bool ColorPulseScript::OnAttach()
    {
        return dynamic_cast<PrimitiveObject*>(GetOwner()) != nullptr;
    }

    void ColorPulseScript::OnUpdate(float deltaTime)
    {
        auto* Primitive = dynamic_cast<PrimitiveObject*>(GetOwner());
        if (Primitive == nullptr) return;
        Time += deltaTime;
        const float Mix = (std::sin(Time * Speed) + 1.0f) * 0.5f;
        Primitive->SetColor({
            ColorA.x + (ColorB.x - ColorA.x) * Mix,
            ColorA.y + (ColorB.y - ColorA.y) * Mix,
            ColorA.z + (ColorB.z - ColorA.z) * Mix,
            ColorA.w + (ColorB.w - ColorA.w) * Mix
        });
    }

    VisualScript::VisualScript()
        : Script("native.visual_script", "Visual Script")
    {
    }

    std::vector<ScriptExposedMember> VisualScript::GetExposedMembers() const
    {
        return {
            { "AssetPath", "Asset", AssetPath },
            { "Reload", "Action", "", true }
        };
    }

    bool VisualScript::SetExposedMember(const std::string& name, const std::string& value)
    {
        if (name != "AssetPath" || value.empty()) return false;
        AssetPath = value;
        return ReloadGraph();
    }

    bool VisualScript::InvokeExposedFunction(const std::string& name)
    {
        return name == "Reload" && ReloadGraph();
    }

    std::unique_ptr<Script> VisualScript::CloneScript() const
    {
        auto Result = std::make_unique<VisualScript>();
        Result->AssetPath = AssetPath;
        return Result;
    }

    bool VisualScript::OnAttach()
    {
        if (GetOwner() == nullptr) return false;
        BaseScale = GetOwner()->GetScale();
        Time = 0.0f;
        return ReloadGraph();
    }

    bool VisualScript::ReloadGraph()
    {
        std::ifstream Stream(AssetPath);
        if (!Stream) return false;
        std::vector<Node> Parsed;
        std::string Line;
        while (std::getline(Stream, Line))
        {
            std::istringstream Tokens(Line);
            std::string Keyword;
            Node Current;
            Tokens >> Keyword;
            if (Keyword != "node") continue;
            std::uint32_t ID = 0;
            Tokens >> ID >> Current.Operation;
            (void)ID;
            for (float& Value : Current.Values)
            {
                if (!(Tokens >> Value)) Value = 0.0f;
            }
            if (!Current.Operation.empty()) Parsed.push_back(Current);
        }
        Nodes = std::move(Parsed);
        return true;
    }

    void VisualScript::OnUpdate(float deltaTime)
    {
        Object* Owner = GetOwner();
        if (Owner == nullptr) return;
        Time += deltaTime;
        for (const Node& Current : Nodes)
        {
            if (Current.Operation == "Rotate")
            {
                const DirectX::XMFLOAT3 Rotation = Owner->GetRotation();
                Owner->SetRotation({
                    Rotation.x + Current.Values[0] * deltaTime,
                    Rotation.y + Current.Values[1] * deltaTime,
                    Rotation.z + Current.Values[2] * deltaTime
                });
            }
            else if (Current.Operation == "Move")
            {
                const DirectX::XMFLOAT3 Position = Owner->GetPosition();
                Owner->SetPosition({
                    Position.x + Current.Values[0] * deltaTime,
                    Position.y + Current.Values[1] * deltaTime,
                    Position.z + Current.Values[2] * deltaTime
                });
            }
            else if (Current.Operation == "PulseScale")
            {
                const float Mix = (std::sin(Time * Current.Values[2]) + 1.0f) * 0.5f;
                const float Scale = Current.Values[0] +
                    (Current.Values[1] - Current.Values[0]) * Mix;
                Owner->SetScale({ BaseScale.x * Scale, BaseScale.y * Scale, BaseScale.z * Scale });
            }
        }
    }
}
