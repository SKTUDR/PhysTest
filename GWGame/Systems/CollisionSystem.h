#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Collider.h"
#include "../Components/CollisionResult.h"
#include "../Components/Transform.h"

#include <SimpleMath.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace ECS
{

    // ============================================================================
    //  OBB SAT ヘルパー
    // ============================================================================

    

    // Quaternion から OBBData を生成
    inline void UpdateOBBData(World& world, EntityID& eid,
        const DirectX::SimpleMath::Vector3& center,
                               const DirectX::SimpleMath::Vector3& halfExtents,
                               const DirectX::SimpleMath::Quaternion& rot) noexcept
    {
        auto& o = world.GetComponent<ColliderComp>(eid).cachedOBB;
        o.center = center;
        o.halfExtents = halfExtents;
        o.axes[0] = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitX, rot);
        o.axes[1] = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitY, rot);
        o.axes[2] = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitZ, rot);
    }

    // OBB を軸に投影したときの半径
    inline float OBBProjectionRadius(const OBBData& obb, const DirectX::SimpleMath::Vector3& axis) noexcept
    {
        return obb.halfExtents.x * std::abs(obb.axes[0].Dot(axis)) +
               obb.halfExtents.y * std::abs(obb.axes[1].Dot(axis)) +
               obb.halfExtents.z * std::abs(obb.axes[2].Dot(axis));
    }

    // ---- AABB vs AABB -----------------------------------------------------------
    inline bool TestAABBvsAABB(const DirectX::SimpleMath::Vector3& centerA, const DirectX::SimpleMath::Vector3& halfA,
                               const DirectX::SimpleMath::Vector3& centerB, const DirectX::SimpleMath::Vector3& halfB,
                               ContactPoint& out) noexcept
    {
        const float dx = centerB.x - centerA.x;
        const float dy = centerB.y - centerA.y;
        const float dz = centerB.z - centerA.z;
        const float ox = (halfA.x + halfB.x) - std::abs(dx);
        const float oy = (halfA.y + halfB.y) - std::abs(dy);
        const float oz = (halfA.z + halfB.z) - std::abs(dz);
        if (ox <= 0.f || oy <= 0.f || oz <= 0.f)
            return false;

        if (ox < oy && ox < oz)
        {
            out.normal = {(dx < 0.f ? -1.f : 1.f), 0.f, 0.f};
            out.depth = ox;
        }
        else if (oy < oz)
        {
            out.normal = {0.f, (dy < 0.f ? -1.f : 1.f), 0.f};
            out.depth = oy;
        }
        else
        {
            out.normal = {0.f, 0.f, (dz < 0.f ? -1.f : 1.f)};
            out.depth = oz;
        }
        DirectX::SimpleMath::Vector3 minA = centerA - halfA;
        DirectX::SimpleMath::Vector3 maxA = centerA + halfA;
        DirectX::SimpleMath::Vector3 minB = centerB - halfB;
        DirectX::SimpleMath::Vector3 maxB = centerB + halfB;

        DirectX::SimpleMath::Vector3 intersectMin = DirectX::SimpleMath::Vector3::Max(minA, minB);
        DirectX::SimpleMath::Vector3 intersectMax = DirectX::SimpleMath::Vector3::Min(maxA, maxB);

        out.position = (intersectMin + intersectMax) * 0.5f;
        return true;
    }

    // ---- OBB vs OBB（SAT 15軸）-------------------------------------------------
    inline bool TestOBBvsOBB(const OBBData& A, const OBBData& B, ContactPoint& out) noexcept
    {
        constexpr float EPS = 1e-6f; // 縮退した外積軸の安定化

        // ---- 事前計算 -------------------------------------------------------
        const DirectX::SimpleMath::Vector3 d = B.center - A.center;

        // R[i][j] = A.axes[i] · B.axes[j]
        float R[3][3], AR[3][3];

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                R[i][j] = A.axes[i].Dot(B.axes[j]);
                AR[i][j] = std::abs(R[i][j]) + EPS;
            }
        }

        // A ローカル空間での中心間ベクトル
        const float t[3] = {d.Dot(A.axes[0]), d.Dot(A.axes[1]), d.Dot(A.axes[2])};

        const float eA[3] = {A.halfExtents.x, A.halfExtents.y, A.halfExtents.z};

        const float eB[3] = {B.halfExtents.x, B.halfExtents.y, B.halfExtents.z};

        float minOverlap = FLT_MAX;
        int bestAxisId = -1;

        // 0-2: A軸
        // 3-5: B軸
        // 6-14: 外積軸
        bool negateAxis = false;

        // ---- 分離軸テスト ----------------------------------------------------
        auto TestAxis = [&](float sep, float rA, float rB, int axisId) -> bool
        {
            const float absSep = std::abs(sep);
            const float overlap = rA + rB - absSep;

            if (overlap <= 0.0f)
                return false;

            if (overlap < minOverlap)
            {
                minOverlap = overlap;
                bestAxisId = axisId;
                negateAxis = (sep < 0.0f);
            }

            return true;
        };

        // ---- A の面軸 --------------------------------------------------------

        if (!TestAxis(t[0], eA[0], eB[0] * AR[0][0] + eB[1] * AR[0][1] + eB[2] * AR[0][2], 0))
            return false;

        if (!TestAxis(t[1], eA[1], eB[0] * AR[1][0] + eB[1] * AR[1][1] + eB[2] * AR[1][2], 1))
            return false;

        if (!TestAxis(t[2], eA[2], eB[0] * AR[2][0] + eB[1] * AR[2][1] + eB[2] * AR[2][2], 2))
            return false;

        // ---- B の面軸 --------------------------------------------------------

        if (!TestAxis(t[0] * R[0][0] + t[1] * R[1][0] + t[2] * R[2][0],
                      eA[0] * AR[0][0] + eA[1] * AR[1][0] + eA[2] * AR[2][0], eB[0], 3))
            return false;

        if (!TestAxis(t[0] * R[0][1] + t[1] * R[1][1] + t[2] * R[2][1],
                      eA[0] * AR[0][1] + eA[1] * AR[1][1] + eA[2] * AR[2][1], eB[1], 4))
            return false;

        if (!TestAxis(t[0] * R[0][2] + t[1] * R[1][2] + t[2] * R[2][2],
                      eA[0] * AR[0][2] + eA[1] * AR[1][2] + eA[2] * AR[2][2], eB[2], 5))
            return false;

        // ---- 外積軸 A0×B0 ～ A2×B2 -----------------------------------------

        if (AR[0][0] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[2] * R[1][0] - t[1] * R[2][0], eA[1] * AR[2][0] + eA[2] * AR[1][0],
                          eB[1] * AR[0][2] + eB[2] * AR[0][1], 6))
                return false;
        }

        if (AR[0][1] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[2] * R[1][1] - t[1] * R[2][1], eA[1] * AR[2][1] + eA[2] * AR[1][1],
                          eB[0] * AR[0][2] + eB[2] * AR[0][0], 7))
                return false;
        }

        if (AR[0][2] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[2] * R[1][2] - t[1] * R[2][2], eA[1] * AR[2][2] + eA[2] * AR[1][2],
                          eB[0] * AR[0][1] + eB[1] * AR[0][0], 8))
                return false;
        }

        if (AR[1][0] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[0] * R[2][0] - t[2] * R[0][0], eA[0] * AR[2][0] + eA[2] * AR[0][0],
                          eB[1] * AR[1][2] + eB[2] * AR[1][1], 9))
                return false;
        }

        if (AR[1][1] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[0] * R[2][1] - t[2] * R[0][1], eA[0] * AR[2][1] + eA[2] * AR[0][1],
                          eB[0] * AR[1][2] + eB[2] * AR[1][0], 10))
                return false;
        }

        if (AR[1][2] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[0] * R[2][2] - t[2] * R[0][2], eA[0] * AR[2][2] + eA[2] * AR[0][2],
                          eB[0] * AR[1][1] + eB[1] * AR[1][0], 11))
                return false;
        }

        if (AR[2][0] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[1] * R[0][0] - t[0] * R[1][0], eA[0] * AR[1][0] + eA[1] * AR[0][0],
                          eB[1] * AR[2][2] + eB[2] * AR[2][1], 12))
                return false;
        }

        if (AR[2][1] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[1] * R[0][1] - t[0] * R[1][1], eA[0] * AR[1][1] + eA[1] * AR[0][1],
                          eB[0] * AR[2][2] + eB[2] * AR[2][0], 13))
                return false;
        }

        if (AR[2][2] < 1.0f - 1e-4f)
        {
            if (!TestAxis(t[1] * R[0][2] - t[0] * R[1][2], eA[0] * AR[1][2] + eA[1] * AR[0][2],
                          eB[0] * AR[2][1] + eB[1] * AR[2][0], 14))
                return false;
        }

        // ---- 衝突 -----------------------------------------------------------

        DirectX::SimpleMath::Vector3 normal;

        if (bestAxisId < 3)
        {
            normal = A.axes[bestAxisId];
        }
        else if (bestAxisId < 6)
        {
            normal = B.axes[bestAxisId - 3];
        }
        else
        {
            static const int AI[9] = {0, 0, 0, 1, 1, 1, 2, 2, 2};

            static const int BI[9] = {0, 1, 2, 0, 1, 2, 0, 1, 2};

            const int idx = bestAxisId - 6;

            normal = A.axes[AI[idx]].Cross(B.axes[BI[idx]]);

            const float len = normal.Length();

            if (len < EPS)
            {
                normal = A.axes[0];
            }
            else
            {
                normal /= len;
            }
        }

        // 法線を A→B に揃える
        if (negateAxis)
            normal = -normal;

        if (d.Dot(normal) < 0.0f)
            normal = -normal;

        out.normal = normal;
        out.depth = minOverlap;

        out.positionA = A.center + normal * OBBProjectionRadius(A, normal);

        out.positionB = B.center - normal * OBBProjectionRadius(B, normal);

        out.position = (out.positionA + out.positionB) * 0.5f;

        return true;
    }

    // ---- OBB vs Capsule ---------------------------------------------------------
    inline bool TestOBBvsCapsule(const OBBData& obb, const DirectX::SimpleMath::Vector3& capsuleCenter, float radius,
                                 float halfHeight, ContactPoint& out) noexcept
    {
        const DirectX::SimpleMath::Vector3 top = capsuleCenter + DirectX::SimpleMath::Vector3::UnitY * halfHeight;
        const DirectX::SimpleMath::Vector3 bot = capsuleCenter - DirectX::SimpleMath::Vector3::UnitY * halfHeight;

        // OBB 上の最近傍点を求めるラムダ
        auto ClosestPointOnOBB = [&](const DirectX::SimpleMath::Vector3& p) -> DirectX::SimpleMath::Vector3
        {
            const DirectX::SimpleMath::Vector3 d = p - obb.center;
            DirectX::SimpleMath::Vector3 closest = obb.center;
            const float extArr[3] = {obb.halfExtents.x, obb.halfExtents.y, obb.halfExtents.z};
            for (int i = 0; i < 3; ++i)
            {
                float dist = d.Dot(obb.axes[i]);
                dist = std::clamp(dist, -extArr[i], extArr[i]);
                closest += obb.axes[i] * dist;
            }
            return closest;
        };

        // Capsule 軸上の3点からサンプルして最近傍を求める
        const DirectX::SimpleMath::Vector3 candidates[3] = {top, bot, capsuleCenter};
        float bestDist2 = FLT_MAX;
        DirectX::SimpleMath::Vector3 bestSegPt, bestObbPt;

        for (const auto& pt : candidates)
        {
            const DirectX::SimpleMath::Vector3 obbPt = ClosestPointOnOBB(pt);
            const float d2 = (pt - obbPt).LengthSquared();
            if (d2 < bestDist2)
            {
                bestDist2 = d2;
                bestSegPt = pt;
                bestObbPt = obbPt;
            }
        }

        const float dist = std::sqrt(bestDist2);
        if (dist >= radius)
            return false;

        out.depth = radius - dist;
        out.position = bestObbPt;
        if (dist > 1e-6f)
        {
            out.normal = (bestSegPt - bestObbPt) / dist;
        }
        else
        {
            out.normal = DirectX::SimpleMath::Vector3::UnitY;
        }
        return true;
    }

    // ---- Capsule vs AABB --------------------------------------------------------
    inline bool TestCapsulevsAABB(const DirectX::SimpleMath::Vector3& capsuleCenter, const Capsule& cap,
                                  const DirectX::SimpleMath::Vector3& boxCenter, const AABB& box,
                                  ContactPoint& out) noexcept
    {
        const DirectX::SimpleMath::Vector3 top = capsuleCenter + DirectX::SimpleMath::Vector3::UnitY * cap.halfHeight;
        const DirectX::SimpleMath::Vector3 bot = capsuleCenter - DirectX::SimpleMath::Vector3::UnitY * cap.halfHeight;

        auto ClampToBox = [&](const DirectX::SimpleMath::Vector3& p) -> DirectX::SimpleMath::Vector3
        {
            return {std::clamp(p.x, boxCenter.x - box.halfExtents.x, boxCenter.x + box.halfExtents.x),
                    std::clamp(p.y, boxCenter.y - box.halfExtents.y, boxCenter.y + box.halfExtents.y),
                    std::clamp(p.z, boxCenter.z - box.halfExtents.z, boxCenter.z + box.halfExtents.z)};
        };

        const DirectX::SimpleMath::Vector3 candidates[3] = {top, bot, capsuleCenter};
        float bestDist2 = FLT_MAX;
        DirectX::SimpleMath::Vector3 bestSegPt, bestBoxPt;

        for (const auto& pt : candidates)
        {
            const DirectX::SimpleMath::Vector3 boxPt = ClampToBox(pt);
            const float d2 = (pt - boxPt).LengthSquared();
            if (d2 < bestDist2)
            {
                bestDist2 = d2;
                bestSegPt = pt;
                bestBoxPt = boxPt;
            }
        }

        const float dist = std::sqrt(bestDist2);
        if (dist >= cap.radius)
            return false;

        out.depth = cap.radius - dist;
        out.position = bestBoxPt;
        if (dist > 1e-6f)
        {
            out.normal = (bestSegPt - bestBoxPt) / dist;
        }
        else
        {
            out.normal = DirectX::SimpleMath::Vector3::UnitY;
        }
        return true;
    }

    // ---- Capsule vs Capsule -----------------------------------------------------
    inline bool TestCapsuleCapsule(const DirectX::SimpleMath::Vector3& centerA, const Capsule& capA,
                                   const DirectX::SimpleMath::Vector3& centerB, const Capsule& capB,
                                   ContactPoint& out) noexcept
    {
        const float ay0 = centerA.y - capA.halfHeight;
        const float ay1 = centerA.y + capA.halfHeight;
        const float by0 = centerB.y - capB.halfHeight;
        const float by1 = centerB.y + capB.halfHeight;
        const float oy = std::min(ay1, by1) - std::max(ay0, by0);

        const float dx = centerB.x - centerA.x;
        const float dz = centerB.z - centerA.z;
        const float distXZ = std::sqrt(dx * dx + dz * dz);
        const float radSum = capA.radius + capB.radius;

        if (distXZ >= radSum || oy < -radSum)
            return false;

        out.depth = radSum - distXZ;
        if (distXZ > 1e-6f)
        {
            out.normal = {dx / distXZ, 0.f, dz / distXZ};
        }
        else
        {
            out.normal = DirectX::SimpleMath::Vector3::UnitX;
        }
        const float midY = (std::max(ay0, by0) + std::min(ay1, by1)) * 0.5f;
        out.position = {centerA.x + out.normal.x * capA.radius, midY, centerA.z + out.normal.z * capA.radius};
        return true;
    }

    // ============================================================================
    //  CollisionSystem
    // ============================================================================
    class CollisionSystem
    {
    public:

        CollisionSystem()
        {
            m_entries.reserve(1024);
        }

        void Update(World& world, float dt)
        {
            // ---- エンティティを収集 ---------------------------------------------
            m_entries.clear();
            m_sapEntries.clear();
            m_entryLookup.clear();
            

            auto desc = QueryBuilder{}.All<TransformComp, ColliderComp>().Build();
            world.Query(desc).Each<TransformComp, ColliderComp, RigidbodyComp>(
                [&](EntityID eid, TransformComp& tr, ColliderComp& col, RigidbodyComp& rb)
                {
                    

                    const DirectX::SimpleMath::Vector3 worldPos = {tr.position.x + col.localOffset.x,
                                                                   tr.position.y + col.localOffset.y,
                                                                   tr.position.z + col.localOffset.z};

                    // OBB の orientation を TransformComp::rotation と同期
                    if (col.IsOBB())
                    {
                        auto& obb = std::get<OBB>(col.shape);
                        obb.orientation = DirectX::SimpleMath::Quaternion::Concatenate(col.localRotation, tr.rotation);
                        UpdateOBBData(world, eid, worldPos, obb.halfExtents, obb.orientation);
                    }
                    if (col.IsAABB())
                    {
                        auto& aabb = std::get<AABB>(col.shape);
                        UpdateOBBData(world, eid, worldPos, aabb.halfExtents,
                                      DirectX::SimpleMath::Quaternion::Identity);
                    }

                    m_entries.push_back({eid, worldPos, tr.rotation, &col, &rb});
                });

            // ---- Broad-phase: Sweep and Prune -----------------------------------
            {
                const std::size_t n = m_entries.size();

                // エンティティ数が変化したら SAP に通知
                if (n != m_prevEntryCount)
                {
                    world.GetPhysicsWorld().MarkBroadPhaseDirty();
                    m_prevEntryCount = n;
                }

                // SweepAndPrune::Entry に変換
                for (const auto& e : m_entries)
                {
                    m_sapEntries.push_back({e.m_eid, CalcWorldAABB(*e.m_col, e.m_worldPos, e.m_worldRot), e.m_col});
                }

                world.GetPhysicsWorld().UpdateBroadPhase(m_sapEntries);
            }

           // ---- eid → Entry の逆引き（vector で O(1) アクセス）--------------
            // EntityID::Index() は 0 始まりの連番なので vector の添字に使える。
            // 上限は World が保持する最大エンティティ数に合わせてリサイズする。
            //
            // m_entryLookup[index] = Entry へのポインタ（未登録は nullptr）
            {
                // 今フレームの最大インデックスを求めてリサイズ
                uint32_t maxIndex = 0;
                for (const auto& e : m_entries)
                    maxIndex = std::max(maxIndex, e.m_eid.Index());

                m_entryLookup.assign(maxIndex + 1, nullptr);

                for (auto& e : m_entries)
                    m_entryLookup[e.m_eid.Index()] = &e;
            }

            // ---- Narrow-phase: SAP で絞ったペアにのみ適用 ----------------------
            for (const auto& pair : world.GetPhysicsWorld().GetOverlaps())
            {
                const Entry* ea = m_entryLookup[pair.eidA.Index()];
                const Entry* eb = m_entryLookup[pair.eidB.Index()];

                if (!ea || !eb)
                    continue;

                if (!ea->m_col->CanCollideWith(*eb->m_col))
                    continue;

                const bool swapped = ea->m_eid.value > eb->m_eid.value;
                const Entry* A = swapped ? eb : ea;
                const Entry* B = swapped ? ea : eb;

                ContactPoint contact{};
                bool hit = DispatchNarrow(*A->m_col, A->m_worldPos, *B->m_col, B->m_worldPos, contact);
                
                if (!hit)
                    continue;

                CollisionResult result;
                result.eid_a = A->m_eid;
                result.eid_b = B->m_eid;
                result.aLayer = A->m_col->layer;
                result.bLayer = B->m_col->layer;
                result.contact = contact;
                result.impulse = 0.f;
                result.isTriggerEvent = A->m_col->IsTrigger() || B->m_col->IsTrigger();
                world.GetEventQueue().Push(std::move(result));
            }
            //// ---- O(n^2) broad-phase --------------------------------------------
            //for (std::size_t i = 0; i < m_entries.size(); ++i)
            //{
            //    for (std::size_t j = i + 1; j < m_entries.size(); ++j)
            //    {
            //        auto& ea = m_entries[i];
            //        auto& eb = m_entries[j];

            //        if (!ea.m_col->CanCollideWith(*eb.m_col))
            //            continue;

            //        ContactPoint contact{};
            //        bool hit = DispatchNarrow(*ea.m_col, ea.m_worldPos, *eb.m_col, eb.m_worldPos, contact);
            //        if (!hit)
            //            continue;

            //        const bool isTrig = ea.m_col->IsTrigger() || eb.m_col->IsTrigger();

            //        CollisionResult result;
            //        const bool swapped = ea.m_eid.value > eb.m_eid.value;
            //        result.eid_a = swapped ? eb.m_eid : ea.m_eid;
            //        result.eid_b = swapped ? ea.m_eid : eb.m_eid;
            //        result.aLayer = swapped ? eb.m_col->layer : ea.m_col->layer;
            //        result.bLayer = swapped ? ea.m_col->layer : eb.m_col->layer;
            //        result.contact = contact;
            //        if (swapped)
            //            result.contact.normal = -contact.normal; // ← 向きを合わせる

            //        result.impulse = 0.f;
            //        result.isTriggerEvent = isTrig;
            //        world.GetEventQueue().Push(std::move(result));
            //    }
            //}
        }

    private:
        std::size_t m_prevEntryCount = 0;

        struct Entry
        {
            EntityID m_eid;
            DirectX::SimpleMath::Vector3 m_worldPos;
            DirectX::SimpleMath::Quaternion m_worldRot;
            ColliderComp* m_col;
            RigidbodyComp* m_rb;
        };

        std::vector<Entry> m_entries;
        std::vector<const Entry*> m_entryLookup;
        std::vector<SweepAndPrune::Entry> m_sapEntries;

        // ---- narrow-phase ディスパッチ ------------------------------------------
        static bool DispatchNarrow(const ColliderComp& colA, const DirectX::SimpleMath::Vector3& posA,
                                   const ColliderComp& colB, const DirectX::SimpleMath::Vector3& posB,
                                   ContactPoint& out)
        {
            // AABB vs AABB
            if (colA.IsAABB() && colB.IsAABB())
            {
                return TestAABBvsAABB(posA, std::get<AABB>(colA.shape).halfExtents, posB,
                                      std::get<AABB>(colB.shape).halfExtents, out);
            }

            // OBB vs OBB
            if (colA.IsOBB() && colB.IsOBB())
            {
                const auto& obbA = std::get<OBB>(colA.shape);
                const auto& obbB = std::get<OBB>(colB.shape);
                return TestOBBvsOBB(colA.cachedOBB,
                                    colB.cachedOBB, out);
            }

            // OBB vs AABB（AABB を単位回転の OBB として扱う）
            if (colA.IsOBB() && colB.IsAABB())
            {
                const auto& obbA = std::get<OBB>(colA.shape);
                const auto& aabbB = std::get<AABB>(colB.shape);
                return TestOBBvsOBB(colA.cachedOBB,
                                    colB.cachedOBB,
                                    out);
            }
            if (colA.IsAABB() && colB.IsOBB())
            {
                const auto& aabbA = std::get<AABB>(colA.shape);
                const auto& obbB = std::get<OBB>(colB.shape);
                bool hit =
                    TestOBBvsOBB(colB.cachedOBB,
                                 colA.cachedOBB, out);
                if (hit)
                    out.normal = -out.normal;
                return hit;
            }

            // OBB vs Capsule
            if (colA.IsOBB() && colB.IsCapsule())
            {
                const auto& obbA = std::get<OBB>(colA.shape);
                const auto& capB = std::get<Capsule>(colB.shape);
                return TestOBBvsCapsule(colA.cachedOBB, posB, capB.radius,
                                        capB.halfHeight, out);
            }
            if (colA.IsCapsule() && colB.IsOBB())
            {
                const auto& capA = std::get<Capsule>(colA.shape);
                const auto& obbB = std::get<OBB>(colB.shape);
                bool hit = TestOBBvsCapsule(colB.cachedOBB, posA, capA.radius,
                                            capA.halfHeight, out);
                if (hit)
                    out.normal = -out.normal;
                return hit;
            }

            // Capsule vs AABB
            if (colA.IsCapsule() && colB.IsAABB())
            {
                return TestCapsulevsAABB(posA, std::get<Capsule>(colA.shape), posB, std::get<AABB>(colB.shape), out);
            }
            if (colA.IsAABB() && colB.IsCapsule())
            {
                bool hit =
                    TestCapsulevsAABB(posB, std::get<Capsule>(colB.shape), posA, std::get<AABB>(colA.shape), out);
                if (hit)
                    out.normal = -out.normal;
                return hit;
            }

            // Capsule vs Capsule
            if (colA.IsCapsule() && colB.IsCapsule())
            {
                return TestCapsuleCapsule(posA, std::get<Capsule>(colA.shape), posB, std::get<Capsule>(colB.shape),
                                          out);
            }

            return false;
        }
    };

} // namespace ECS