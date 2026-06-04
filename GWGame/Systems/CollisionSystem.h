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

    // SAT 計算用の OBB 表現（ワールド空間）
    struct OBBData
    {
        DirectX::SimpleMath::Vector3 m_center;
        DirectX::SimpleMath::Vector3 m_halfExtents;
        std::array<DirectX::SimpleMath::Vector3, 3> m_axes; // [0]=X [1]=Y [2]=Z
    };

    // Quaternion から OBBData を生成
    inline OBBData MakeOBBData(const DirectX::SimpleMath::Vector3& center,
                               const DirectX::SimpleMath::Vector3& halfExtents,
                               const DirectX::SimpleMath::Quaternion& rot) noexcept
    {
        OBBData o;
        o.m_center = center;
        o.m_halfExtents = halfExtents;
        o.m_axes[0] = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitX, rot);
        o.m_axes[1] = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitY, rot);
        o.m_axes[2] = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitZ, rot);
        return o;
    }

    // OBB を軸に投影したときの半径
    inline float OBBProjectionRadius(const OBBData& obb, const DirectX::SimpleMath::Vector3& axis) noexcept
    {
        return obb.m_halfExtents.x * std::abs(obb.m_axes[0].Dot(axis)) +
               obb.m_halfExtents.y * std::abs(obb.m_axes[1].Dot(axis)) +
               obb.m_halfExtents.z * std::abs(obb.m_axes[2].Dot(axis));
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
        const DirectX::SimpleMath::Vector3 d = B.m_center - A.m_center;

        // テスト軸: A の3軸 + B の3軸 + 外積9本 = 15本
        DirectX::SimpleMath::Vector3 testAxes[15];
        for (int i = 0; i < 3; ++i)
            testAxes[i] = A.m_axes[i];
        for (int i = 0; i < 3; ++i)
            testAxes[3 + i] = B.m_axes[i];
        int k = 6;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                testAxes[k++] = A.m_axes[i].Cross(B.m_axes[j]);

        float minDepth = FLT_MAX;
        DirectX::SimpleMath::Vector3 bestAxis = DirectX::SimpleMath::Vector3::UnitY;

        for (const auto& axis : testAxes)
        {
            const float lenSq = axis.LengthSquared();
            if (lenSq < 1e-10f)
                continue;
            const DirectX::SimpleMath::Vector3 normAxis = axis / std::sqrt(lenSq);

            const float projD = std::abs(d.Dot(normAxis));
            const float projA = OBBProjectionRadius(A, normAxis);
            const float projB = OBBProjectionRadius(B, normAxis);
            const float overlap = projA + projB - projD;

            constexpr float EPS = 1e-6f;

            if (overlap <= EPS)
                return false;

            if (overlap < minDepth)
            {
                minDepth = overlap;
                bestAxis = normAxis;
            }
        }

        // 法線が A→B 方向を向くように符号を揃える
        if (d.Dot(bestAxis) < 0.f)
            bestAxis = -bestAxis;

        out.normal = bestAxis;
        out.depth = minDepth;
        out.position = A.m_center + bestAxis * OBBProjectionRadius(A, bestAxis);
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
            const DirectX::SimpleMath::Vector3 d = p - obb.m_center;
            DirectX::SimpleMath::Vector3 closest = obb.m_center;
            const float extArr[3] = {obb.m_halfExtents.x, obb.m_halfExtents.y, obb.m_halfExtents.z};
            for (int i = 0; i < 3; ++i)
            {
                float dist = d.Dot(obb.m_axes[i]);
                dist = std::clamp(dist, -extArr[i], extArr[i]);
                closest += obb.m_axes[i] * dist;
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
        void Update(World& world, float /*dt*/)
        {

            // ---- エンティティを収集 ---------------------------------------------
            struct Entry
            {
                EntityID m_eid;
                DirectX::SimpleMath::Vector3 m_worldPos;
                DirectX::SimpleMath::Quaternion m_worldRot;
                ColliderComp* m_col;
            };

            std::vector<Entry> m_entries;
            m_entries.reserve(256);

            auto desc = QueryBuilder{}.All<TransformComp, ColliderComp>().Build();

            world.Query(desc).Each<TransformComp, ColliderComp>(
                [&](EntityID eid, TransformComp& tr, ColliderComp& col)
                {
                    // ---- OBB の orientation を TransformComp::rotation と同期 ------
                    // PhysicsSystem が tr.rotation を更新したら、ここで OBB に反映する。
                    // col.localRotation はモデルとコライダーの向きのズレ補正分。
                    if (col.IsOBB())
                    {
                        auto& obb = std::get<OBB>(col.shape);

                        // ワールド回転 = tr.rotation × col.localRotation
                        const DirectX::SimpleMath::Quaternion worldRot =
                            DirectX::SimpleMath::Quaternion::Concatenate(col.localRotation, tr.rotation);
                        obb.orientation = worldRot;
                    }

                    const DirectX::SimpleMath::Vector3 worldPos = {tr.position.x + col.localOffset.x,
                                                                   tr.position.y + col.localOffset.y,
                                                                   tr.position.z + col.localOffset.z};
                    m_entries.push_back({eid, worldPos, tr.rotation, &col});
                });

            // ---- O(n^2) broad-phase --------------------------------------------
            for (std::size_t i = 0; i < m_entries.size(); ++i)
            {
                for (std::size_t j = i + 1; j < m_entries.size(); ++j)
                {
                    auto& ea = m_entries[i];
                    auto& eb = m_entries[j];

                    if (!ea.m_col->CanCollideWith(*eb.m_col))
                        continue;

                    ContactPoint contact{};
                    bool hit = DispatchNarrow(*ea.m_col, ea.m_worldPos, *eb.m_col, eb.m_worldPos, contact);
                    if (!hit)
                        continue;

                    const bool isTrig = ea.m_col->IsTrigger() || eb.m_col->IsTrigger();

                    CollisionResult result;
                    result.eid_a = (ea.m_eid.value < eb.m_eid.value) ? ea.m_eid : eb.m_eid;
                    result.eid_b = (ea.m_eid.value < eb.m_eid.value) ? eb.m_eid : ea.m_eid;
                    result.contact = contact;
                    result.impulse = 0.f;
                    result.isTriggerEvent = isTrig;
                    world.GetEventQueue().Push(std::move(result));
                }
            }
        }

    private:
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
                return TestOBBvsOBB(MakeOBBData(posA, obbA.halfExtents, obbA.orientation),
                                    MakeOBBData(posB, obbB.halfExtents, obbB.orientation), out);
            }

            // OBB vs AABB（AABB を単位回転の OBB として扱う）
            if (colA.IsOBB() && colB.IsAABB())
            {
                const auto& obbA = std::get<OBB>(colA.shape);
                const auto& aabbB = std::get<AABB>(colB.shape);
                return TestOBBvsOBB(MakeOBBData(posA, obbA.halfExtents, obbA.orientation),
                                    MakeOBBData(posB, aabbB.halfExtents, DirectX::SimpleMath::Quaternion::Identity),
                                    out);
            }
            if (colA.IsAABB() && colB.IsOBB())
            {
                const auto& aabbA = std::get<AABB>(colA.shape);
                const auto& obbB = std::get<OBB>(colB.shape);
                bool hit =
                    TestOBBvsOBB(MakeOBBData(posB, obbB.halfExtents, obbB.orientation),
                                 MakeOBBData(posA, aabbA.halfExtents, DirectX::SimpleMath::Quaternion::Identity), out);
                if (hit)
                    out.normal = -out.normal;
                return hit;
            }

            // OBB vs Capsule
            if (colA.IsOBB() && colB.IsCapsule())
            {
                const auto& obbA = std::get<OBB>(colA.shape);
                const auto& capB = std::get<Capsule>(colB.shape);
                return TestOBBvsCapsule(MakeOBBData(posA, obbA.halfExtents, obbA.orientation), posB, capB.radius,
                                        capB.halfHeight, out);
            }
            if (colA.IsCapsule() && colB.IsOBB())
            {
                const auto& capA = std::get<Capsule>(colA.shape);
                const auto& obbB = std::get<OBB>(colB.shape);
                bool hit = TestOBBvsCapsule(MakeOBBData(posB, obbB.halfExtents, obbB.orientation), posA, capA.radius,
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