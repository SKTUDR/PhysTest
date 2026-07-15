#pragma once

#include "../ECS/World.h"
#include "../GameContext.h"
#include "../Components/Components.h"

namespace ECS
{
    class RubbleController
    {
    public:
        void Update(ECS::World& world, float dt, GameContext& gameContext)
        {
            gameContext;
            auto desc = QueryBuilder{}.All<LocalTransformComp , RigidbodyComp, RubbleTagComp>().Build();

            world.GetEventQueue()
                .ForEach<PhysicsImpulseEvent>([&](PhysicsImpulseEvent PIE)
                    {
                       
                        if (PIE.impulse < -5.f)
                        {
                            
                            if (world.HasComponent<RubbleTagComp>(PIE.eid_a))
                            {
                                world.GetComponent<RigidbodyComp>(PIE.eid_a).isKinematic = false;
                            }
                            if (world.HasComponent<RubbleTagComp>(PIE.eid_b))
                            {
                                world.GetComponent<RigidbodyComp>(PIE.eid_b).isKinematic = false;
                            }
                        }
                        
                    });

            
        };
    };
}