#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../ECS/EventQueue.h"
#include "../Components/Transform.h"
#include "../Components/Rigidbody.h"
#include "../Components/CollisionResult.h"

#include <SimpleMath.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ECS
{

    // ============================================================================
    //  PhysicsSystem
    //
    //  Game::Update() での呼び出し順:
    //    CollisionSystem::Update()          ← 衝突判定・イベント積む
    //    PhysicsSystem::ResolveCollisions() ← 速度へのインパルス付与
    //    PhysicsSystem::IntegrateVelocity()    ← 速度の積分
    //    PhysicsSystem::IntegrateTransform()   ← 位置の更新
    // 
    //    位置補正はすべて PhysicsSystem::ResolveCollisions() 内のスラブ法で行う。
    // ============================================================================
    class PhysicsSystem
    {
    public:
        struct Params
        {
            DirectX::SimpleMath::Vector3 gravity = {0.f, -9.81f , 0.f};
            float sleepThreshold = 0.01f;
            int solverIterations = 10;
            // 位置補正パラメータ（スラブ法）
            // SLOP: 許容する最小貫通深度。小さすぎるとジッター、大きすぎると浮く。
            float positionSlop = 0.005f;
            // CORRECTION: 補正率 [0-1]。1.0 だと過補正でガタつく。0.2〜0.4 が安定。
            float positionCorrection = -.2f;

            float groundNormalThreshold = 0.3f;

            bool useGravity = true;
        };

        explicit PhysicsSystem(Params params = {}) : m_params(params)
        {
        }

        // ---- 衝突応答フェーズ（CollisionSystem の後・積分の前に呼ぶ）-----------
        void ResolveCollisions(World& world, float dt)
        {
            // ---- インパルス反復（速度のみ）--------------------------------------
            // 複数接触の連立を近似的に解くため反復する。
            for (int iter = 0; iter < m_params.solverIterations; ++iter)
            {
                world.GetEventQueue().ForEach<CollisionResult>(
                    [&](const CollisionResult& result)
                    {
                        if (result.isTriggerEvent)
                            return;
                        ResolveImpulse(world, result, dt); // 速度インパルスのみ
                    });
            }

            // ---- 位置補正（1回だけ）--------------------------------------------
            // インパルス反復とは分離して1回のみ実行することでガタつきを防ぐ。
            world.GetEventQueue().ForEach<CollisionResult>(
                [&](const CollisionResult& result)
                {
                    if (result.isTriggerEvent)
                        return;
                    ResolvePosition(world, result); // 位置補正のみ
                });

            // ---- 衝突法線収集 --------------------------------------------
            m_contactNormals.clear();
            world.GetEventQueue().ForEach<CollisionResult>(
                [&](const CollisionResult& result)
                {
                    if (result.isTriggerEvent)
                        return;
                    m_contactNormals.push_back({result.eid_a, result.contact.normal});
                    m_contactNormals.push_back({result.eid_b, -result.contact.normal});
                });
        }

        // ---- 積分フェーズ（衝突応答の後に呼ぶ）---------------------------------
        void IntegrateVelocity(World& world, float dt)
        {
            auto desc = QueryBuilder{}.All<TransformComp, RigidbodyComp>().Build();
            world.Query(desc).Each<TransformComp, RigidbodyComp>(
                [&](EntityID eid, TransformComp& tr, RigidbodyComp& rb)
                {
                    if (rb.isKinematic)
                        return;

                    // freezeRotationの処理
                    if (rb.freezeRotation)
                    {
                        rb.angularVelocity = DirectX::SimpleMath::Vector3::Zero;
                        rb.torqueAccum = DirectX::SimpleMath::Vector3::Zero;
                    }

                    // 外力 ➔ 速度
                    rb.velocity += rb.forceAccum * rb.invMass * dt;

                    // 重力 ➔ 速度
                    if (m_params.useGravity)
                        rb.velocity += m_params.gravity * dt;

                    // トルク ➔ 角速度
                    if (!rb.freezeRotation)
                        rb.IntegrateAngularVelocity(tr.rotation, dt);
                });
        }

        void IntegrateTransform(World& world, float dt)
        {
            auto desc = QueryBuilder{}.All<TransformComp, RigidbodyComp>().Build();
            world.Query(desc).Each<TransformComp, RigidbodyComp>(
                [&](EntityID eid, TransformComp& tr, RigidbodyComp& rb)
                {
                    if (rb.isKinematic)
                        return;

                    // ダンピング（速度が確定した後にかけるのが一般的）
                    rb.velocity *= std::max(0.f, 1.f - rb.linearDamping * dt);
                    rb.angularVelocity *= std::max(0.f, 1.f - rb.angularDamping * dt);

                    // スリープ判定
                    const float speedSq = rb.velocity.LengthSquared() + rb.angularVelocity.LengthSquared();
                    if (speedSq < m_params.sleepThreshold * m_params.sleepThreshold)
                    {
                        rb.velocity = DirectX::SimpleMath::Vector3::Zero;
                        rb.angularVelocity = DirectX::SimpleMath::Vector3::Zero;
                        rb.ClearForces();
                        return;
                    }

                    // 確定した安全な速度で、位置と回転を更新
                    tr.position += rb.velocity * dt;
                    if (rb.angularVelocity.LengthSquared() > 1e-12f)
                    {
                        ApplyAngularVelocity(tr, rb.angularVelocity, dt);
                    }

                    rb.ClearForces();
                });
            
        }

        // ---- 旧 API 互換: Update() で両方まとめて呼ぶ場合 ----------------------
        // ※ 推奨は Game::Update() で ResolveCollisions() → IntegratePhase() を
        //   別々に呼ぶこと（呼び出し順を Game 側でコントロールできる）
        void Update(World& world, float dt)
        {
            IntegrateVelocity(world, dt);
            ResolveCollisions(world, dt);
            IntegrateTransform(world, dt);
        }

    private:
        Params m_params;

        // ResolveCollisions() が収集・IntegratePhase() が参照する接触法線
        struct ContactNormal
        {
            EntityID eid;
            DirectX::SimpleMath::Vector3 normal;
        };
        std::vector<ContactNormal> m_contactNormals;

        // ---- 速度インパルスのみ（反復ループ内で呼ぶ）---------------------------
        void ResolveImpulse(World& world, const CollisionResult& result, float dt)
        {
            const bool hasRbA = world.HasComponent<RigidbodyComp>(result.eid_a);
            const bool hasRbB = world.HasComponent<RigidbodyComp>(result.eid_b);
            if (!hasRbA && !hasRbB)
                return;

            RigidbodyComp* rbA = hasRbA ? &world.GetComponent<RigidbodyComp>(result.eid_a) : nullptr;
            RigidbodyComp* rbB = hasRbB ? &world.GetComponent<RigidbodyComp>(result.eid_b) : nullptr;

            if (rbA && rbA->isKinematic)
                rbA = nullptr;
            if (rbB && rbB->isKinematic)
                rbB = nullptr;
            if (!rbA && !rbB)
                return;

            // 逆質量取得
            // invMass = 0 の物体は「無限質量」とみなされる
            const float invMassA = rbA ? rbA->invMass : 0.f;
            const float invMassB = rbB ? rbB->invMass : 0.f;
            // 両方とも無限質量なら解けない
            if (invMassA + invMassB < 1e-8f)
                return;

            auto GetColliderCenter = [&](EntityID eid, bool hasRb) -> DirectX::SimpleMath::Vector3
            {
                if (!hasRb)
                    return DirectX::SimpleMath::Vector3::Zero;
                const auto& tr = world.GetComponent<TransformComp>(eid);
                const auto* col =
                    world.HasComponent<ColliderComp>(eid) ? &world.GetComponent<ColliderComp>(eid) : nullptr;
                if (col)
                    return tr.position + col->localOffset;
                return tr.position;
            };

            const DirectX::SimpleMath::Vector3 centerA = GetColliderCenter(result.eid_a, hasRbA);
            const DirectX::SimpleMath::Vector3 centerB = GetColliderCenter(result.eid_b, hasRbB);

            // 重心 → 接触点ベクトル
            // 回転速度計算に必要
            const DirectX::SimpleMath::Vector3 rA = result.contact.position - centerA;
            const DirectX::SimpleMath::Vector3 rB = result.contact.position - centerB;

            // 衝突法線
            const DirectX::SimpleMath::Vector3 n = result.contact.normal;

            // 接触点での速度
            // 回転している物体は端ほど速く動くため、
            // 角速度由来の速度も加える
            const DirectX::SimpleMath::Vector3 vA =
                rbA ? rbA->velocity + rbA->angularVelocity.Cross(rA) : DirectX::SimpleMath::Vector3::Zero;
            const DirectX::SimpleMath::Vector3 vB =
                rbB ? rbB->velocity + rbB->angularVelocity.Cross(rB) : DirectX::SimpleMath::Vector3::Zero;

            // 相対速度
            const DirectX::SimpleMath::Vector3 vRel = vA - vB;


            const float vRelN = vRel.Dot(n);
            if (vRelN < 0.f)
                return;

            const bool frozenA = rbA ? rbA->freezeRotation : true;
            const bool frozenB = rbB ? rbB->freezeRotation : true;

            // 回転慣性項
            // rot を TransformComp から取得
            const DirectX::SimpleMath::Quaternion rotA = world.HasComponent<TransformComp>(result.eid_a)
                                                             ? world.GetComponent<TransformComp>(result.eid_a).rotation
                                                             : DirectX::SimpleMath::Quaternion::Identity;
            const DirectX::SimpleMath::Quaternion rotB = world.HasComponent<TransformComp>(result.eid_b)
                                                             ? world.GetComponent<TransformComp>(result.eid_b).rotation
                                                             : DirectX::SimpleMath::Quaternion::Identity;

            // ワールド空間の慣性テンソル逆行列
            const DirectX::SimpleMath::Matrix iWorldInvA =
                rbA ? rbA->CalcWorldInvInertia(rotA) : DirectX::SimpleMath::Matrix::Identity;
            const DirectX::SimpleMath::Matrix iWorldInvB =
                rbB ? rbB->CalcWorldInvInertia(rotB) : DirectX::SimpleMath::Matrix::Identity;

            // InertiaTerm: I_world_inv * (r × axis) × r)・axis
            auto InertiaTerm = [](const DirectX::SimpleMath::Vector3& r, const DirectX::SimpleMath::Vector3& axis,
                                  const DirectX::SimpleMath::Matrix& iWorldInv, bool frozen) -> float
            {
                if (frozen)
                    return 0.f;
                const DirectX::SimpleMath::Vector3 rCrossAxis = r.Cross(axis);
                // I_world_inv * (r × axis)
                const DirectX::SimpleMath::Vector3 iRCrossAxis =
                    DirectX::SimpleMath::Vector3::Transform(rCrossAxis, iWorldInv);
                return iRCrossAxis.Cross(r).Dot(axis);
            };

            const float inertiaA = rbA ? InertiaTerm(rA, n, iWorldInvA, frozenA) : 0.f;
            const float inertiaB = rbB ? InertiaTerm(rB, n, iWorldInvB, frozenB) : 0.f;

            // インパルス分母
            const float denom = invMassA + invMassB + inertiaA + inertiaB;
            if (denom < 1e-8f)
                return;

            /// 反発係数
            float restitution = std::max((rbA ? rbA->restitution : 0.f), (rbB ? rbB->restitution : 0.f));
                
            const float baumgarte = 0.2f;
            const float slop = 0.01f;

            //float bias = (baumgarte / dt) * std::max(result.contact.depth - slop, 0.f);

            const float j = (-(1.f + restitution) * vRelN /* + bias*/) / denom;

            // 法線方向インパルス
            const DirectX::SimpleMath::Vector3 impulse = n * j;

            // 線形速度へ適用
            if (rbA)
                rbA->ApplyImpulse(impulse);
            if (rbB)
                rbB->ApplyImpulse(-impulse);

            // 回転速度へ適用
            if (rbA)
                rbA->ApplyAngularImpulse(rA.Cross(impulse), rotA);
            if (rbB)
                rbB->ApplyAngularImpulse(-rB.Cross(impulse), rotB);

            // ---- 摩擦 -----------------------------------------------------------
            const DirectX::SimpleMath::Vector3 tangent = vRel - n * vRelN;
            const float tLen = tangent.Length();
            if (tLen > 1e-6f)
            {
                // 正規化接線
                const DirectX::SimpleMath::Vector3 t = tangent / tLen;

                // 接線方向相対速度
                const float vRelT = vRel.Dot(t);

                // 接線方向回転慣性
                const float inertTA = rbA ? InertiaTerm(rA, t, iWorldInvA, frozenA) : 0.f;
                const float inertTB = rbB ? InertiaTerm(rB, t, iWorldInvB, frozenB) : 0.f;

                // 摩擦方向分母
                const float denomT = invMassA + invMassB + inertTA + inertTB;

                // 摩擦係数
                const float friction = ((rbA ? rbA->friction : 0.f) + (rbB ? rbB->friction : 0.f)) * 0.5f;

                // 理想摩擦インパルス
                float jt = -vRelT / denomT;

                // クーロン摩擦制限
                jt = std::clamp(jt, -std::abs(j) * friction, std::abs(j) * friction);

                // 摩擦インパルス
                const DirectX::SimpleMath::Vector3 fImpulse = t * jt;

                // 線形速度へ適用
                if (rbA)
                    rbA->ApplyImpulse(fImpulse);
                if (rbB)
                    rbB->ApplyImpulse(-fImpulse);

                // 摩擦による回転速度へ適用
                if (rbA) 
                    rbA->ApplyAngularImpulse(rA.Cross(fImpulse), rotA);
                if (rbB) 
                    rbB->ApplyAngularImpulse(-rB.Cross(fImpulse), rotB);
            }
        }

        // ---- 位置補正のみ（ループ外で1回だけ呼ぶ）-----------------------------
        void ResolvePosition(World& world, const CollisionResult& result)
        {
            const bool hasRbA = world.HasComponent<RigidbodyComp>(result.eid_a);
            const bool hasRbB = world.HasComponent<RigidbodyComp>(result.eid_b);
            if (!hasRbA && !hasRbB)
                return;

            RigidbodyComp* rbA = hasRbA ? &world.GetComponent<RigidbodyComp>(result.eid_a) : nullptr;
            RigidbodyComp* rbB = hasRbB ? &world.GetComponent<RigidbodyComp>(result.eid_b) : nullptr;

            if (rbA && rbA->isKinematic)
                rbA = nullptr;
            if (rbB && rbB->isKinematic)
                rbB = nullptr;
            if (!rbA && !rbB)
                return;

            const float invMassA = rbA ? rbA->invMass : 0.f;
            const float invMassB = rbB ? rbB->invMass : 0.f;
            const float invMassSum = invMassA + invMassB;
            if (invMassSum < 1e-8f)
                return;

            const float depth = result.contact.depth;
            if (depth <= m_params.positionSlop)
                return;

            // 1. 速度インパルスと同様に、接触点へのベクトルを計算
            const auto& trA = world.GetComponent<TransformComp>(result.eid_a);
            const auto& trB = world.GetComponent<TransformComp>(result.eid_b);

            auto GetColliderCenter = [&](EntityID eid, bool hasRb) -> DirectX::SimpleMath::Vector3
            {
                if (!hasRb)
                    return DirectX::SimpleMath::Vector3::Zero;
                const auto& tr = world.GetComponent<TransformComp>(eid);
                const auto* col =
                    world.HasComponent<ColliderComp>(eid) ? &world.GetComponent<ColliderComp>(eid) : nullptr;
                if (col)
                    return tr.position + col->localOffset;
                return tr.position;
            };

            const DirectX::SimpleMath::Vector3 centerA = GetColliderCenter(result.eid_a, hasRbA);
            const DirectX::SimpleMath::Vector3 centerB = GetColliderCenter(result.eid_b, hasRbB);

            DirectX::SimpleMath::Vector3 rA = result.contact.position - centerA;
            DirectX::SimpleMath::Vector3 rB = result.contact.position - centerB;
            DirectX::SimpleMath::Vector3 n = result.contact.normal;

            // 2. 回転を考慮した位置補正用の分母（Inertia項）を計算
            const DirectX::SimpleMath::Matrix iWorldInvA =
                rbA ? rbA->CalcWorldInvInertia(trA.rotation) : DirectX::SimpleMath::Matrix::Identity;
            const DirectX::SimpleMath::Matrix iWorldInvB =
                rbB ? rbB->CalcWorldInvInertia(trB.rotation) : DirectX::SimpleMath::Matrix::Identity;

            auto InertiaTerm = [](const DirectX::SimpleMath::Vector3& r, const DirectX::SimpleMath::Vector3& axis,
                                  const DirectX::SimpleMath::Matrix& iWorldInv, bool frozen) -> float
            {
                if (frozen)
                    return 0.f;
                const DirectX::SimpleMath::Vector3 rCrossAxis = r.Cross(axis);
                // I_world_inv * (r × axis)
                const DirectX::SimpleMath::Vector3 iRCrossAxis =
                    DirectX::SimpleMath::Vector3::Transform(rCrossAxis, iWorldInv);
                return iRCrossAxis.Cross(r).Dot(axis);
            };

            float inertiaA = rbA ? InertiaTerm(rA, n, iWorldInvA, rbA->freezeRotation) : 0.f;
            float inertiaB = rbB ? InertiaTerm(rB, n, iWorldInvB, rbB->freezeRotation) : 0.f;

            float denom = invMassA + invMassB + inertiaA + inertiaB;
            if (denom < 1e-8f)
                return;

            // 3. 疑似位置インパルス（マニチュード）の計算
            // positionCorrection は 正の値（0.2〜0.8程度）にしてください
            float positionCorrectionFactor = 0.4f; // 40%ずつめり込みを解消
            float pJ = ((depth - m_params.positionSlop) * positionCorrectionFactor) / denom;

            DirectX::SimpleMath::Vector3 pImpulse = n * pJ;

            // 4. 重心位置の補正（平行移動）
            if (rbA)
                world.GetComponent<TransformComp>(result.eid_a).position += -pImpulse * invMassA;
            if (rbB)
                world.GetComponent<TransformComp>(result.eid_b).position -= -pImpulse * invMassB;

            // 5. 回転（rotation）の補正！
            if (rbA && !rbA->freezeRotation)
            {
                // Vector3::Transform(ベクトル, 行列) の順で渡す
                DirectX::SimpleMath::Vector3 pdTheta =
                    DirectX::SimpleMath::Vector3::Transform(rA.Cross(pImpulse), iWorldInvA);
                ApplyAngularVelocity(world.GetComponent<TransformComp>(result.eid_a), pdTheta, 1.0f);
            }
            if (rbB && !rbB->freezeRotation)
            {
                DirectX::SimpleMath::Vector3 pdTheta =
                    DirectX::SimpleMath::Vector3::Transform(-rB.Cross(pImpulse), iWorldInvB);
                ApplyAngularVelocity(world.GetComponent<TransformComp>(result.eid_b), pdTheta, 1.0f);
            }


            //const float mag = (depth - m_params.positionSlop) * m_params.positionCorrection / invMassSum;
            //const DirectX::SimpleMath::Vector3 correction = result.contact.normal * mag;

            //if (rbA && world.HasComponent<TransformComp>(result.eid_a))
            //{
            //    world.GetComponent<TransformComp>(result.eid_a).position += correction * invMassA;
            //}
            //if (rbB && world.HasComponent<TransformComp>(result.eid_b))
            //{
            //    world.GetComponent<TransformComp>(result.eid_b).position -= correction * invMassB;
            //}
        }

        // ---- 角速度 → TransformComp::rotation -----------------------------
        static void ApplyAngularVelocity(TransformComp& tr, const DirectX::SimpleMath::Vector3& angVel,
                                         float dt) noexcept
        {
            const float speed = angVel.Length();
            if (speed < 1e-6f)
                return;

            const auto delta = DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(angVel / speed, speed * dt);
            tr.rotation = DirectX::SimpleMath::Quaternion::Concatenate(delta, tr.rotation);
            tr.rotation.Normalize();
        }
    };

} // namespace ECS

