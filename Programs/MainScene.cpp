#include "GameEngineAPI.h"

#include <cmath>

using namespace EngineGame;

namespace Game::MainScene
{
    void Init()
    {
        if (!Object.Exists("MainOscillatingCapsule"))
        {
            AddObject.CreateCapsuleModel("MainOscillatingCapsule");
        }

        Object.SetSize("MainOscillatingCapsule", 1.2f, 2.4f, 1.2f);
        Object.SetPosition("MainOscillatingCapsule", 3.0f, 1.0f, 2.0f);
        Object.SetColor("MainOscillatingCapsule", 0.95f, 0.4f, 0.18f);
        Object.MultiplyColor("MainOscillatingCapsule", 1.0f, 0.7f, 0.8f);
        Log("[Info] MainProgram | MainOscillatingCapsule created by high-level API.");
    }

    void Update(float deltaTime)
    {
        static float ElapsedTime = 0.0f;
        ElapsedTime += deltaTime;
        const float Z = 2.0f + std::sin(ElapsedTime * 1.5f) * 2.0f;
        Object.SetPosition("MainOscillatingCapsule", 3.0f, 1.0f, Z);
    }

    void End()
    {
        Log("[Info] MainProgram | MainScene End.");
    }
}

ENGINE_REGISTER_SCENE(MainScene)
