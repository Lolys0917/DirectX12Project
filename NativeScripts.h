#pragma once

#include <DirectXMath.h>

#include <string>
#include <vector>

#include "Script.h"

namespace Engine
{
    // 明示的に追加したObjectだけに再生タブの重力設定を適用する。
    class GravityScript final : public Script
    {
    public:
        GravityScript();
        std::vector<ScriptExposedMember> GetExposedMembers() const override;
        bool SetExposedMember(const std::string& name, const std::string& value) override;
        bool InvokeExposedFunction(const std::string& name) override;
    protected:
        std::unique_ptr<Script> CloneScript() const override;
        bool OnAttach() override;
        void OnUpdate(float deltaTime) override;
    private:
        DirectX::XMFLOAT3 Velocity{};
        float GravityScale = 1.0f;
        float GroundOffset = 0.5f;
    };

    class BobbingScript final : public Script
    {
    public:
        BobbingScript(float amplitude = 1.0f, float frequency = 1.0f);
        std::vector<ScriptExposedMember> GetExposedMembers() const override;
        bool SetExposedMember(const std::string& name, const std::string& value) override;
        bool InvokeExposedFunction(const std::string& name) override;
    protected:
        std::unique_ptr<Script> CloneScript() const override;
        bool OnAttach() override;
        void OnUpdate(float deltaTime) override;
    private:
        float Amplitude;
        float Frequency;
        DirectX::XMFLOAT3 Axis{ 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT3 Origin{};
        float Time = 0.0f;
    };

    class OrbitScript final : public Script
    {
    public:
        OrbitScript(float radius = 3.0f, float radiansPerSecond = 1.0f);
        std::vector<ScriptExposedMember> GetExposedMembers() const override;
        bool SetExposedMember(const std::string& name, const std::string& value) override;
        bool InvokeExposedFunction(const std::string& name) override;
    protected:
        std::unique_ptr<Script> CloneScript() const override;
        bool OnAttach() override;
        void OnUpdate(float deltaTime) override;
    private:
        float Radius;
        float RadiansPerSecond;
        DirectX::XMFLOAT3 Center{};
        float Angle = 0.0f;
    };

    class PulseScaleScript final : public Script
    {
    public:
        PulseScaleScript(float minimum = 0.75f, float maximum = 1.25f, float speed = 2.0f);
        std::vector<ScriptExposedMember> GetExposedMembers() const override;
        bool SetExposedMember(const std::string& name, const std::string& value) override;
        bool InvokeExposedFunction(const std::string& name) override;
    protected:
        std::unique_ptr<Script> CloneScript() const override;
        bool OnAttach() override;
        void OnUpdate(float deltaTime) override;
    private:
        float Minimum;
        float Maximum;
        float Speed;
        DirectX::XMFLOAT3 BaseScale{ 1.0f, 1.0f, 1.0f };
        float Time = 0.0f;
    };

    class ColorPulseScript final : public Script
    {
    public:
        explicit ColorPulseScript(float speed = 2.0f);
        std::vector<ScriptExposedMember> GetExposedMembers() const override;
        bool SetExposedMember(const std::string& name, const std::string& value) override;
        bool InvokeExposedFunction(const std::string& name) override;
    protected:
        std::unique_ptr<Script> CloneScript() const override;
        bool OnAttach() override;
        void OnUpdate(float deltaTime) override;
    private:
        float Speed;
        DirectX::XMFLOAT4 ColorA{ 0.2f, 0.55f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 ColorB{ 1.0f, 0.25f, 0.2f, 1.0f };
        float Time = 0.0f;
    };

    class VisualScript final : public Script
    {
    public:
        VisualScript();
        std::vector<ScriptExposedMember> GetExposedMembers() const override;
        bool SetExposedMember(const std::string& name, const std::string& value) override;
        bool InvokeExposedFunction(const std::string& name) override;
    protected:
        std::unique_ptr<Script> CloneScript() const override;
        bool OnAttach() override;
        void OnUpdate(float deltaTime) override;
    private:
        struct Node final
        {
            std::string Operation;
            float Values[4]{};
        };
        bool ReloadGraph();
        std::string AssetPath = "Assets/VisualScripts/Rotation.vscript";
        std::vector<Node> Nodes;
        DirectX::XMFLOAT3 BaseScale{ 1.0f, 1.0f, 1.0f };
        float Time = 0.0f;
    };
}
