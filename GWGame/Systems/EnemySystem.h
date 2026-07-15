#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Gameplay.h"
#include "../Components/Rigidbody.h"

#include <SimpleMath.h>
#include <limits>

namespace ECS
{
    struct EnemyDeathEvent
    {
        // 敵が死んだことを通知する
        EntityID eid = EntityID::Null();
    };

    // ---- EnemySystem ------------------------------------------------------------
    // 処理:
    //   1. プレイヤーの位置を取得（PlayerTagComp で検索）
    //   2. 全エネミーをプレイヤー方向に向ける（Y 軸回転のみ・Pitch は変えない）
    //   3. TransformComp::Forward() 方向に RigidbodyComp::AddForce() で力をかける
    //
    // 呼び出し順:
    //   EnemySystem::Update()
    //   PhysicsSystem::ResolveCollisions()
    //   PhysicsSystem::IntegrateVelocity()
    //   PhysicsSystem::IntegrateTransform()
    // ----------------------------------------------------------------------------
    class EnemySystem
    {
    public:
        struct Params
        {
            float moveForce = 3000.f;    // 前進力 (N)
            float turnSpeed = 5.f;     // 旋回速度 (rad/s)・Slerp の速度
            float stopDistance = 1.5f; // この距離以内では力をかけない
        };

        
        explicit EnemySystem(Params params = {}) : m_params(params)
        {
        }

        void Update(World& world, float dt)
        {
            isGrounded = false;
            // EventQueue::ForEach<T> を使って CollisionResult を列挙する（範囲ベース for は begin/end が必要なため）
            world.GetEventQueue().ForEach<CollisionResult>(
                [&](const CollisionResult& result)
                {
                    if (!world.IsAlive(result.eid_a) || !world.IsAlive(result.eid_b))
                        return;

                    auto EWpair = GetOrderedPair<EnemyTagComp, GroundTagComp>(world, result);
                    auto ERpair = GetOrderedPair<EnemyTagComp, RubbleTagComp>(world, result);
                   
                    if (EWpair)
                    {
                        isGrounded = true;
                    }
                    if (ERpair)
                    {
                        const auto& eRb = world.GetComponent<RigidbodyComp>(ERpair->first);
                        const auto& rubRb = world.GetComponent<RigidbodyComp>(ERpair->second);

                        DirectX::SimpleMath::Vector3 relativeVelocity = eRb.velocity - rubRb.velocity;

                        float speed = relativeVelocity.Length();
                        if (speed > .1f)
                        {
                            health -= speed * 0.5f;
                            if (health <= 0.0f && !isDead)
                            {
                                isDead = true;
                                world.GetEventQueue().Push(EnemyDeathEvent{.eid = ERpair->first});
                            }
                        }
                    }
                    auto desc = QueryBuilder{}.All<EnemyTagComp, RigidbodyComp>().Build();
                    world.Query(desc).Each<RigidbodyComp>(
                        [&](EntityID eid, RigidbodyComp& rb)
                        {
                            if (isGrounded)
                            {
                                rb.linearDamping = 4.0f;
                            }
                            else
                            {
                                rb.linearDamping = 0.01f;
                            }

                            // OutputDebugStringA((std::to_string(isGrounded) + "\r\n").c_str());
                        });
                });


            // ---- プレイヤーの位置を取得 -----------------------------------------
            // 複数プレイヤーが存在する場合は最近傍を使う
            DirectX::SimpleMath::Vector3 playerPos = DirectX::SimpleMath::Vector3::Zero;
            bool playerFound = false;

            auto playerDesc = QueryBuilder{}.All<TransformComp, PlayerTagComp>().Build();

            world.Query(playerDesc)
                .Each<TransformComp>(
                    [&](EntityID, const TransformComp& tr)
                    {
                        playerPos = tr.position;
                        playerFound = true;
                    });

            if (!playerFound)
                return;

            // ---- 全エネミーを処理 -----------------------------------------------
            auto enemyDesc = QueryBuilder{}.All<LocalTransformComp, EnemyTagComp, RigidbodyComp>().Build();

            world.Query(enemyDesc).Each<LocalTransformComp, RigidbodyComp>(
                [&](EntityID, LocalTransformComp& tr, RigidbodyComp& rb)
                {
                    if (rb.isKinematic)
                        return;

                    const DirectX::SimpleMath::Vector3 toPlayer = playerPos - tr.localPosition;
                    const float distSq = toPlayer.LengthSquared();

                    // ---- 1. プレイヤー方向を向く（Y 軸回転のみ）--------------------
                    // XZ 平面上の方向ベクトルだけで目標クォータニオンを作り
                    // 現在の rotation から Slerp で補間することで滑らかに旋回する
                    if (distSq > 1e-4f)
                    {
                        // XZ 平面に投影（Y 成分を除去して水平方向のみ）
                        DirectX::SimpleMath::Vector3 flatDir = {toPlayer.x, 0.f, toPlayer.z};
                        if (flatDir.LengthSquared() > 1e-6f)
                        {
                            flatDir.Normalize();

                            // 目標クォータニオン: -Z(Forward) を flatDir に向ける
                            const DirectX::SimpleMath::Quaternion targetRot =
                                DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(
                                    DirectX::SimpleMath::Matrix::CreateWorld(
                                        DirectX::SimpleMath::Vector3::Zero, flatDir, DirectX::SimpleMath::Vector3::Up));

                            // Slerp で旋回速度を制限
                            const float t = std::min(m_params.turnSpeed * dt, 1.f);
                            tr.localRotation = DirectX::SimpleMath::Quaternion::Slerp(tr.localRotation, targetRot, t);
                            tr.localRotation.Normalize();
                        }
                    }

                    // ---- 2. Forward 方向に力をかける --------------------------------
                    // stopDistance 以内に入ったら力をかけない（振動防止）
                    if (distSq > m_params.stopDistance * m_params.stopDistance)
                    {
                        const DirectX::SimpleMath::Vector3 forward = tr.Forward();
                        rb.AddForce(forward * m_params.moveForce);
                    }
                });
        }

    private:
        bool isGrounded = false;
        float health = 100.0f;
        bool isDead = false;
        Params m_params;

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
    };

} // namespace ECS