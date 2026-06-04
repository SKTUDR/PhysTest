#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Physics.h"

namespace ECS
{

    // ============================================================================
    //  SimpleMovementSystem
    //  対象: TransformComp + VelocityComp を持つエンティティ
    //  処理: 等速直線運動のみ。加速度・ダンピングなし。
    //
    //  position += linear  * dt
    //  rotation *= Δrotation(angular * dt)
    // ============================================================================
    class SimpleMovementSystem
    {
    public:
        void Update(World& world, float dt)
        {
            auto desc = QueryBuilder{}
                            .All<TransformComp, VelocityComp>()
                            .None<AccelerationComp, RigidbodyComp>() // FullMovementSystem と重複しないよう除外
                            .Build();

            world.Query(desc).Each<TransformComp, VelocityComp>(
                [dt](EntityID, TransformComp& tr, VelocityComp& vel)
                {
                    // ---- 位置更新 ------------------------------------------------
                    tr.position += vel.linear * dt;

                    // ---- 回転更新 ------------------------------------------------
                    // angular = (yaw, pitch, roll) [rad/s] を dt 分回転させる
                    ApplyAngularVelocity(tr, vel.angular, dt);
                });
        }

    private:
        static void ApplyAngularVelocity(TransformComp& tr, const DirectX::SimpleMath::Vector3& angularVel,
                                         float dt) noexcept
        {
            const float speed = angularVel.Length();
            if (speed < 1e-6f)
                return;

            // 角速度ベクトルを軸・角度に分解してクォータニオンで回転
            DirectX::SimpleMath::Vector3 axis = angularVel / speed;
            const float angle = speed * dt;
            const auto delta = DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(axis, angle);
            tr.rotation = DirectX::SimpleMath::Quaternion::Concatenate(tr.rotation, delta);
            tr.rotation.Normalize();
        }
    };

    // ============================================================================
    //  FullMovementSystem
    //  対象: TransformComp + VelocityComp + AccelerationComp を持つエンティティ
    //  処理: 加速度の積分・ダンピング・速度クランプ・位置更新を行う。
    //
    //  更新順序（半陰的オイラー法）:
    //    1. velocity += acceleration * dt          （速度に加速度を積分）
    //    2. velocity *= (1 - damping * dt)         （ダンピング減衰）
    //    3. velocity.Clamp(maxSpeed)               （速度制限）
    //    4. position += velocity * dt              （位置に速度を積分）
    //    5. rotation += angular_velocity * dt      （回転に角速度を積分）
    // ============================================================================
    class FullMovementSystem
    {
    public:
        void Update(World& world, float dt)
        {
            auto desc = QueryBuilder{}
                .All<TransformComp, VelocityComp, AccelerationComp>()
                .None<RigidbodyComp>()
                .Build();

            world.Query(desc).Each<TransformComp, VelocityComp, AccelerationComp>(
                [dt](EntityID, TransformComp& tr, VelocityComp& vel, AccelerationComp& acc)
                {
                    // ---- 1. 速度に加速度を積分 ------------------------------------
                    vel.linear += acc.linear * dt;
                    vel.angular += acc.angular * dt;

                    // ---- 2. ダンピング（速度の減衰） ------------------------------
                    // damping=0 なら係数が 1.0 になり減衰しない
                    if (acc.linearDamping > 0.f)
                    {
                        const float factor = 1.f - std::min(acc.linearDamping * dt, 1.f);
                        vel.linear *= factor;
                    }
                    if (acc.angularDamping > 0.f)
                    {
                        const float factor = 1.f - std::min(acc.angularDamping * dt, 1.f);
                        vel.angular *= factor;
                    }

                    // ---- 3. 速度クランプ ------------------------------------------
                    if (acc.maxLinearSpeed > 0.f)
                    {
                        vel.ClampSpeed(acc.maxLinearSpeed);
                    }
                    if (acc.maxAngularSpeed > 0.f)
                    {
                        const float angSpeed = vel.angular.Length();
                        if (angSpeed > acc.maxAngularSpeed && angSpeed > 1e-6f)
                        {
                            vel.angular *= acc.maxAngularSpeed / angSpeed;
                        }
                    }

                    // ---- 4. 位置更新 ----------------------------------------------
                    tr.position += vel.linear * dt;

                    // ---- 5. 回転更新 ----------------------------------------------
                    ApplyAngularVelocity(tr, vel.angular, dt);
                });
        }

    private:
        static void ApplyAngularVelocity(TransformComp& tr, const DirectX::SimpleMath::Vector3& angularVel,
                                         float dt) noexcept
        {
            const float speed = angularVel.Length();
            if (speed < 1e-6f)
                return;

            DirectX::SimpleMath::Vector3 axis = angularVel / speed;
            const float angle = speed * dt;
            const auto delta = DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(axis, angle);
            tr.rotation = DirectX::SimpleMath::Quaternion::Concatenate(tr.rotation, delta);
            tr.rotation.Normalize();
        }
    };

} // namespace ECS