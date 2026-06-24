#pragma once

#include "../ECS/World.h"
#include "../GameContext.h"
#include "../Components/Components.h"
#include "../ImaseLib/SceneManager.h"
#include "../Scene/SceneId.h"

#include "../KUtil/Random.h"

namespace ECS
{
    class GameDirector
    {
    public:
        void Update(Imase::ISceneController<SceneId>& sceneController, ECS::World& world,  GameContext& gameContext)
        {
            sceneController;
            world;
            gameContext;
            
            // プレイヤーが死んでいたらシーンAに戻る
            world.GetEventQueue().ForEach<DeathEvent>([&](DeathEvent DE)
                                                      { sceneController.RequestSwitch(SceneId::SceneA); });

            
        };
    };
}