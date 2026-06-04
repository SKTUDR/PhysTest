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

    // ============================================================================
    //  PlayerSystem
    // ============================================================================
    class PlayerSystem
    {
    public:
        

        // ---- Update -------------------------------------------------------------
        void Update(Imase::ISceneController<SceneId>& sceneController, World& world, GameContext& gameContext)
        {
            gameContext;
            // EventQueue::ForEach<T> を使って CollisionResult を列挙する（範囲ベース for は begin/end が必要なため）
            world.GetEventQueue().ForEach<CollisionResult>([&](const CollisionResult& result)
            {
                // ---- レイヤーフィルタ（ColliderComp で確認）----------------
                // CollisionSystem が積んだイベントでも念のため再確認する。
                // ColliderComp がない場合はスキップ
                if (!world.HasComponent<ColliderComp>(result.eid_a))
                    return;
                if (!world.HasComponent<ColliderComp>(result.eid_b))
                    return;

                const auto& colA = world.GetComponent<ColliderComp>(result.eid_a);
                const auto& colB = world.GetComponent<ColliderComp>(result.eid_b);

                // レイヤーが想定外のペアは処理しない
                if (!colA.CanCollideWith(colB))
                    return;

                // ---- タグで役割を特定 ---------------------------------------
                EntityID playerId = EntityID::Null();
                EntityID enemyId = EntityID::Null();

                // PLAYER レイヤー × ENEMY レイヤーのペアかを確認
                const bool aIsPlayer =
                    Any(colA.layer & CollisionLayer::PLAYER) && world.HasComponent<PlayerTagComp>(result.eid_a);
                const bool bIsPlayer =
                    Any(colB.layer & CollisionLayer::PLAYER) && world.HasComponent<PlayerTagComp>(result.eid_b);
                const bool aIsEnemy =
                    Any(colA.layer & CollisionLayer::ENEMY) && world.HasComponent<EnemyTagComp>(result.eid_a);
                const bool bIsEnemy =
                    Any(colB.layer & CollisionLayer::ENEMY) && world.HasComponent<EnemyTagComp>(result.eid_b);

                if (aIsPlayer && bIsEnemy)
                {
                    playerId = result.eid_a;
                    enemyId = result.eid_b;
                }
                else if (bIsPlayer && aIsEnemy)
                {
                    playerId = result.eid_b;
                    enemyId = result.eid_a;
                }
                else
                {
                    return; // プレイヤー×エネミー以外は別のSystemが担当
                }

                // ---- ③ トリガーか物理応答かで処理を分岐 ----------------------
                if (result.isTriggerEvent)
                {
                    OnPlayerTriggerEnemy(sceneController, world, playerId, enemyId, result);
                }
                else
                {
                    OnPlayerHitEnemy(sceneController, world, playerId, enemyId, result);
                }
            });
        }

    private:
        bool IsPlayer(const World& world, EntityID eid) const noexcept
        {
            return world.IsAlive(eid) && world.HasComponent<PlayerTagComp>(eid);
        }

        bool IsEnemy(const World& world, EntityID eid) const noexcept
        {
            return world.IsAlive(eid) && world.HasComponent<EnemyTagComp>(eid);
        }

        void OnPlayerHitEnemy(Imase::ISceneController<SceneId>& sceneController, World& world, EntityID playerId,
                              EntityID enemyId, const CollisionResult& result)
        {
            //world.Destroy(playerId);
            //sceneController.RequestSwitch(SceneId::SceneA);
            world;
            sceneController;
            playerId;
            enemyId;
            result;
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