#include "GameScriptAPI.h"

#include <cmath>
#include <cstdint>

using namespace EngineGame;

namespace
{
    class BoxHorizontalOscillationScript final : public ObjectScript
    {
    public:
        //概要：Attach先Boxを基準位置から左右へ往復させるSub Scriptを作成する
        //引数：host=Attach先Objectへ接続された低水準Host API
        //戻り値：なし
        explicit BoxHorizontalOscillationScript(const EngineScriptHostAPI* host)
            : ObjectScript(host)
            , Origin()
            , ElapsedTime(0.0f)
        {
        }

        //概要：BoxだけにAttachを許可して初期位置と二種類の色変更を設定する
        //引数：なし
        //戻り値：Attach先がBoxで初期化できた場合true
        bool OnAttach()
        {
            if (!IsObjectType(GameObjectType::Box))
            {
                return false;
            }

            Origin = this->Position;
            SetColor(0.2f, 0.8f, 1.0f, 1.0f);
            MultiplyColor(0.75f, 1.0f, 0.85f, 1.0f);
            return true;
        }

        //概要：Attach先BoxをSin波でX軸の左右へ往復させる
        //引数：deltaTime=前Frameからの秒数
        //戻り値：なし
        void Update(float deltaTime)
        {
            ElapsedTime += deltaTime;
            Float3 Next = Origin; //基準位置から求める今回の座標
            Next.x += std::sin(ElapsedTime * 2.0f) * 2.0f;
            this->Position = Next;
        }

    private:
        Float3 Origin; //Attach時に記録する往復運動の中心座標
        float ElapsedTime; //Sin波位相へ使用する経過秒数
    };

    const EngineScriptDescriptor Scripts[] =
    {
        MakeObjectScriptDescriptor<BoxHorizontalOscillationScript>(
            "box.horizontal_oscillation",
            "Box Horizontal Oscillation"
        )
    };
}

//概要：EngineへAttach前提のBox往復Sub Script関数表を公開する
//引数：requestedAbiVersion=Engineが要求するScript ABI版番号
//戻り値：互換Module定義、版不一致時はnullptr
ENGINE_SCRIPT_EXPORT const EngineScriptModuleDescriptor* ENGINE_SCRIPT_CALL EngineGetScriptModule(
    std::uint32_t requestedAbiVersion
)
{
    static const EngineScriptModuleDescriptor Module
    {
        sizeof(EngineScriptModuleDescriptor),
        EngineScriptAbiVersion,
        "EditorScriptPrograms",
        static_cast<std::uint32_t>(sizeof(Scripts) / sizeof(Scripts[0])),
        Scripts
    };
    return requestedAbiVersion == EngineScriptAbiVersion ? &Module : nullptr;
}
