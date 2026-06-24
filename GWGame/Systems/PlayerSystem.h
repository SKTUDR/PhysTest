#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../ImaseLib/SceneManager.h"
#include "../Scene/SceneId.h"

#include "../Components/Collider.h"
#include "../Components/GamePlay.h"
#include "InputState.h"

#include <SimpleMath.h>
#include <algorithm>

namespace ECS
{
    struct DeathEvent
    {
        // 死んだことを通知するだけ
    };


    // ============================================================================
    //  PlayerSystem
    // ============================================================================
    class PlayerSystem
    {
    public:
        

        // ---- Update -------------------------------------------------------------
        void Update(Imase::ISceneController<SceneId>& sceneController, World& world, GameContext& gameContext)
        {
            isGrounded = false;
            gameContext;
            // EventQueue::ForEach<T> を使って CollisionResult を列挙する（範囲ベース for は begin/end が必要なため）
            world.GetEventQueue().ForEach<CollisionResult>([&](const CollisionResult& result)
            {
                auto PEpair = GetOrderedPair<PlayerTagComp, EnemyTagComp>(world, result);
                auto PWpair = GetOrderedPair<PlayerTagComp, GroundTagComp>(world, result);

                
                
                // ---- ③ トリガーか物理応答かで処理を分岐 ----------------------
                if (result.isTriggerEvent)
                {
                    if (PEpair)
                    {
                        OnPlayerTriggerEnemy(sceneController, world, PEpair->first, PEpair->second, result);
                    }                    
                }
                else
                {
                    if (PEpair)
                    {
                        OnPlayerHitEnemy(sceneController, world, PEpair->first, PEpair->second, result);
                    }
                    if (PWpair)
                    {
                        isGrounded = true;
                    }

                }

                auto desc = QueryBuilder{}.All<PlayerTagComp, RigidbodyComp>().Build();
                world.Query(desc).Each<RigidbodyComp>([&](EntityID eid, RigidbodyComp& rb)
                                                          {

                        if (isGrounded)
                        {
                            rb.linearDamping = 4.0f;
                        }
                        else
                        {
                            rb.linearDamping = 0.01f;
                        }

                        //OutputDebugStringA((std::to_string(isGrounded) + "\r\n").c_str());
                    
                    });
            });
        }

    private:
        bool isGrounded = false;
        
        template <class T1, class T2> struct OrderedPair
        {
            EntityID first;
            EntityID second;
        };

        template <class TFirst, class TSecond>
        std::optional<OrderedPair<TFirst, TSecond>> GetOrderedPair(World& world, const CollisionResult& result)
        {
            if (!world.IsAlive(result.eid_a) || !world.IsAlive(result.eid_b))
                return std::nullopt;

            const bool aFirst = world.HasComponent<TFirst>(result.eid_a);
            const bool bFirst = world.HasComponent<TFirst>(result.eid_b);

            const bool aSecond = world.HasComponent<TSecond>(result.eid_a);
            const bool bSecond = world.HasComponent<TSecond>(result.eid_b);

            if (aFirst && bSecond)
                return {{result.eid_a, result.eid_b}};

            if (bFirst && aSecond)
                return {{result.eid_b, result.eid_a}};

            return std::nullopt;
        }

        void OnPlayerHitEnemy(Imase::ISceneController<SceneId>& sceneController, World& world, EntityID playerId,
                              EntityID enemyId, const CollisionResult& result)
        {
            world;
            sceneController;
            playerId;
            enemyId;
            result;

            //world.RequestDestroy(enemyId);
            //world.GetEventQueue().Push(DeathEvent());
        }

        void OnPlayerTriggerEnemy(Imase::ISceneController<SceneId>& sceneController, World& world, EntityID playerId,
                                  EntityID enemyId, const CollisionResult& result)
        {
            sceneController;
            world;
            playerId;
            enemyId;
            result;
        }
    };
} // namespace ECS