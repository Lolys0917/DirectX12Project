#include "GameEngineAPI.h"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace EngineGame;

namespace Game::MainScene
{
    constexpr std::uint32_t StressObjectCount = 1024;
    ObjectHandle MainCapsule; //個別操作するObjectの安定Handle
    std::vector<ObjectHandle> StressObjects; //一括操作するObject Handle配列
    float ElapsedTime = 0.0f; //Initで再生ごとに初期化するScene状態

    void Init()
    {
        ElapsedTime = 0.0f;
        MainCapsule = Object.Find("MainOscillatingCapsule");

        if (!MainCapsule)
        {
            MainCapsule = AddObject.CreateCapsuleModel("MainOscillatingCapsule");
        }

        MainCapsule.SetSize(1.2f, 2.4f, 1.2f);
        MainCapsule.SetPosition(3.0f, 1.0f, 2.0f);
        MainCapsule.SetColor(0.95f, 0.4f, 0.18f);

        StressObjects = Object.FindAll("StressBox");

        if (StressObjects.empty())
        {
            StressObjects = AddObject.CreateBoxes("StressBox", StressObjectCount);
        }

        for (std::size_t Index = 0; Index < StressObjects.size(); ++Index)
        {
            const float Red = 0.2f + static_cast<float>(Index % 5) * 0.12f;
            StressObjects[Index].SetSize(0.55f, 0.55f, 0.55f);
            StressObjects[Index].SetColor(Red, 0.65f, 1.0f);
        }

        Log("[Info] MainProgram | Object handles and stress-test array initialized.");
    }

    void Update(float deltaTime)
    {
        ElapsedTime += deltaTime;
        const float Z = 2.0f + std::sin(ElapsedTime * 1.5f) * 2.0f;
        MainCapsule.SetPosition(3.0f, 1.0f, Z);

        for (std::size_t Index = 0; Index < StressObjects.size(); ++Index)
        {
            const float Column = static_cast<float>(Index % 8);
            const float Row = static_cast<float>(Index / 8);
            const float Height = 0.5f + std::sin(
                ElapsedTime * 2.0f + static_cast<float>(Index) * 0.25f
            ) * 0.75f;
            StressObjects[Index].SetPosition(
                (Column - 3.5f) * 1.1f,
                Height,
                5.0f + Row * 1.1f
            );
        }
    }

    void StartDestroy()
    {
        MainCapsule = {};
        StressObjects.clear();
    }

    void End()
    {
        Log("[Info] MainProgram | MainScene End.");
    }

    void EndDestroy()
    {
        ElapsedTime = 0.0f;
    }
}

ENGINE_REGISTER_SCENE_LIFECYCLE(MainScene)
