#pragma once

#include "../ECS/World.h"
#include "../GameContext.h"
#include "../Components/Components.h"

#include "../KUtil/Random.h"

namespace ECS
{
    class EnemySystem
    {
    public:
        void Update(ECS::World& world, float dt, GameContext& gameContext)
        {
            gameContext;
            auto desc = QueryBuilder{}.
                All<TransformComp, RigidbodyComp, EnemyTagComp>().
                Build();

            world.Query(desc).ParallelEach<TransformComp, RigidbodyComp>(
                [&](EntityID eid, TransformComp& tr, RigidbodyComp& rb)
                {
                    if (gameContext.keyboardTracker.IsKeyPressed(Keyboard::Space))
                    {
                        rb.isKinematic = false;
                    }

                });
           
        };
    };
}