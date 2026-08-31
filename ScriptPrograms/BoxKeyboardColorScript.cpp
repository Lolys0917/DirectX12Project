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
            , Speed(2.0f)
            , Distance(2.0f)
        {
            ExposeVariable("Speed", Speed);
            ExposeVariable("Distance", Distance);
            ExposeVariable("Origin", Origin);
            ExposeFunction("Reset", [this]()
            {
                ElapsedTime = 0.0f;
                this->Position = Origin;
            });
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
            Next.x += std::sin(ElapsedTime * Speed) * Distance;
            this->Position = Next;
        }

    private:
        Float3 Origin; //Attach時に記録する往復運動の中心座標
        float ElapsedTime; //Sin波位相へ使用する経過秒数
        float Speed; //Editorへ公開するSin波速度
        float Distance; //Editorへ公開する移動距離
    };

    const EngineScriptDescriptor Scripts[] =
    {
        MakeObjectScriptDescriptor<BoxHorizontalOscillationScript>(
            "box.horizontal_oscillation",
            "Box Horizontal Oscillation"
        )
    };
}
