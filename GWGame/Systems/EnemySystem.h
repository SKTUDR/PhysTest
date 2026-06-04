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
            // PlayerTagComp を持つエンティティだけを対象にする
            auto desc = QueryBuilder{}.
                All<TransformComp, RigidbodyComp, EnemyTagComp>().
                Build();

            world.Query(desc).Each<TransformComp, RigidbodyComp>(
                [&](EntityID eid, TransformComp& tr, RigidbodyComp& rb)
                {
                    if (tr.position.z < -15.f) {
                        tr.position.z = 30.f; // 一定距離を超えたら後ろに戻す
                        tr.position.x = RandomGenerator::GetInstance().RandFloat(-5.f, 5.f); // X位置はランダム

                       

                    }
                    world.GetEventQueue().ForEach<CollisionResult>(
                        [&](const CollisionResult& result)
                        {
                            if (world.GetComponent<ColliderComp>(result.eid_a).layer == CollisionLayer::WORLD ||
                                world.GetComponent<ColliderComp>(result.eid_b).layer == CollisionLayer::WORLD)
                            {
                                rb.ApplyImpulse(DirectX::SimpleMath::Vector3(0.f, 0.f, -1.f));
                            }
                        });
                    //rb.AddForce(DirectX::SimpleMath::Vector3(0.f, 0.f, -1.f)); // 前方に力を加える


                });
           
        };
    };
}