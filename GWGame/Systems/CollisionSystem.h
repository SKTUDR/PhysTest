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

    inline void UpdateOBBData(World& world, EntityID& eid, const DirectX::SimpleMath::Vector3& center,
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

        const DirectX::SimpleMath::Vector3 iMin = DirectX::SimpleMath::Vector3::Max(centerA - halfA, centerB - halfB);
        const DirectX::SimpleMath::Vector3 iMax = DirectX::SimpleMath::Vector3::Min(centerA + halfA, centerB + halfB);
        out.position = (iMin + iMax) * 0.5f;
        return true;
    }

    // ============================================================================
    //  Sutherland-Hodgman クリッピング（OBB 多点接触生成）
    //
    //  参照面（reference face）と入射面（incident face）でクリッピングし
    //  最大 4 点の接触点を生成する。
    // ============================================================================

    // 参照面の 4 頂点を返す
    // faceAxis: 0=X, 1=Y, 2=Z  sign: +1/-1
    inline std::array<DirectX::SimpleMath::Vector3, 4> GetFaceVertices(const OBBData& obb, int faceAxis,
                                                                       float sign) noexcept
    {
        // 参照面の中心
        const DirectX::SimpleMath::Vector3 faceCenter =
            obb.center + obb.axes[faceAxis] * (obb.halfExtents.ToArray()[faceAxis] * sign);

        // 残り 2 軸
        const int a1 = (faceAxis + 1) % 3;
        const int a2 = (faceAxis + 2) % 3;
        const DirectX::SimpleMath::Vector3 e1 = obb.axes[a1] * obb.halfExtents.ToArray()[a1];
        const DirectX::SimpleMath::Vector3 e2 = obb.axes[a2] * obb.halfExtents.ToArray()[a2];

        return {faceCenter + e1 + e2, faceCenter - e1 + e2, faceCenter - e1 - e2, faceCenter + e1 - e2};
    }

    // 平面（法線 n・距離 d）で頂点リストをクリップする（Sutherland-Hodgman の 1 辺分）
    // 平面の内側 = n・v >= d
    inline std::vector<DirectX::SimpleMath::Vector3>
    ClipPolygonByPlane(const std::vector<DirectX::SimpleMath::Vector3>& poly, const DirectX::SimpleMath::Vector3& n,
                       float d) noexcept
    {
        std::vector<DirectX::SimpleMath::Vector3> result;
        const int cnt = static_cast<int>(poly.size());
        if (cnt == 0)
            return result;

        for (int i = 0; i < cnt; ++i)
        {
            const DirectX::SimpleMath::Vector3& curr = poly[i];
            const DirectX::SimpleMath::Vector3& next = poly[(i + 1) % cnt];
            const float dCurr = n.Dot(curr) - d;
            const float dNext = n.Dot(next) - d;

            if (dCurr >= 0.f)
                result.push_back(curr);

            // 辺が平面を横切る場合は交点を追加
            if ((dCurr > 0.f) != (dNext > 0.f))
            {
                const float t = dCurr / (dCurr - dNext);
                result.push_back(curr + (next - curr) * t);
            }
        }
        return result;
    }

    // OBB 多点接触生成（Sutherland-Hodgman）
    // out_contacts に最大 4 点の ContactPoint を格納する
    inline void GenerateOBBContacts(const OBBData& refOBB, const OBBData& incOBB,
                                    const DirectX::SimpleMath::Vector3& normal, float depth,
                                    CollisionResult& result) noexcept
    {
        // ---- 参照面を選択（法線に最も近い refOBB の面）-------------------------
        int refFaceAxis = 0;
        float maxDot = std::abs(refOBB.axes[0].Dot(normal));
        for (int i = 1; i < 3; ++i)
        {
            const float d = std::abs(refOBB.axes[i].Dot(normal));
            if (d > maxDot)
            {
                maxDot = d;
                refFaceAxis = i;
            }
        }
        const float refSign = (refOBB.axes[refFaceAxis].Dot(normal) > 0.f) ? 1.f : -1.f;
        const DirectX::SimpleMath::Vector3 refFaceNormal = refOBB.axes[refFaceAxis] * refSign;

        // ---- 入射面を選択（-normal に最も近い incOBB の面）---------------------
        int incFaceAxis = 0;
        float minDot = incOBB.axes[0].Dot(normal);
        for (int i = 1; i < 3; ++i)
        {
            const float d = incOBB.axes[i].Dot(normal);
            if (d < minDot)
            {
                minDot = d;
                incFaceAxis = i;
            }
        }
        const float incSign = (incOBB.axes[incFaceAxis].Dot(normal) < 0.f) ? 1.f : -1.f;

        // ---- 入射面の 4 頂点取得 ------------------------------------------------
        const auto incVertices = GetFaceVertices(incOBB, incFaceAxis, incSign);
        std::vector<DirectX::SimpleMath::Vector3> poly(incVertices.begin(), incVertices.end());

        // ---- 参照面の 4 側面でクリッピング---------------------------------------
        for (int i = 0; i < 3; ++i)
        {
            if (i == refFaceAxis)
                continue;
            const DirectX::SimpleMath::Vector3& sideAxis = refOBB.axes[i];
            const float halfExt = refOBB.halfExtents.ToArray()[i];
            const float sideD = refOBB.center.Dot(sideAxis);

            poly = ClipPolygonByPlane(poly, sideAxis, sideD - halfExt);
            if (poly.empty())
                return;
            poly = ClipPolygonByPlane(poly, -sideAxis, -(sideD + halfExt));
            if (poly.empty())
                return;
        }

        // ---- 参照面より下にある点を接触点として採用 ----------------------------
        const float refFaceD = refOBB.center.Dot(refFaceNormal) + refOBB.halfExtents.ToArray()[refFaceAxis];

        for (const auto& v : poly)
        {
            const float penetration = refFaceD - v.Dot(refFaceNormal);
            if (penetration < 0.f)
                continue; // 参照面の外側はスキップ

            ContactPoint cp;
            cp.normal = normal;
            cp.depth = penetration;
            cp.positionB = v;                        // 入射面上の点
            cp.positionA = v + normal * penetration; // 参照面上の対応点
            cp.position = (cp.positionA + cp.positionB) * 0.5f;
            result.AddContact(cp);
        }

        // contactCount が 0 なら SAT の代表1点をフォールバックとして使う
        if (result.contactCount == 0)
        {
            ContactPoint cp;
            cp.normal = normal;
            cp.depth = depth;
            cp.positionA = refOBB.center + normal * OBBProjectionRadius(refOBB, normal);
            cp.positionB = incOBB.center - normal * OBBProjectionRadius(incOBB, normal);
            cp.position = (cp.positionA + cp.positionB) * 0.5f;
            result.AddContact(cp);
        }

        // 代表接触点（最大深度）を contact にコピー
        int maxIdx = 0;
        for (int i = 1; i < result.contactCount; ++i)
            if (result.contacts[i].depth > result.contacts[maxIdx].depth)
                maxIdx = i;
        result.contact = result.contacts[maxIdx];
    }

    // ---- OBB vs OBB（Gottschalk SAT 1996）--------------------------------------
    inline bool TestOBBvsOBB(const OBBData& A, const OBBData& B, ContactPoint& out) noexcept
    {
        constexpr float EPS = 1e-6f;
        const DirectX::SimpleMath::Vector3 d = B.center - A.center;

        float R[3][3], AR[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
            {
                R[i][j] = A.axes[i].Dot(B.axes[j]);
                AR[i][j] = std::abs(R[i][j]) + EPS;
            }

        const float t[3] = {d.Dot(A.axes[0]), d.Dot(A.axes[1]), d.Dot(A.axes[2])};
        const float eA[3] = {A.halfExtents.x, A.halfExtents.y, A.halfExtents.z};
        const float eB[3] = {B.halfExtents.x, B.halfExtents.y, B.halfExtents.z};

        float minOverlap = FLT_MAX;
        int bestAxisId = -1;
        bool negateAxis = false;

        auto TestAxis = [&](float sep, float rA, float rB, int id) -> bool
        {
            const float ov = rA + rB - std::abs(sep);
            if (ov <= 0.f)
                return false;
            if (ov < minOverlap)
            {
                minOverlap = ov;
                bestAxisId = id;
                negateAxis = (sep < 0.f);
            }
            return true;
        };

        if (!TestAxis(t[0], eA[0], eB[0] * AR[0][0] + eB[1] * AR[0][1] + eB[2] * AR[0][2], 0))
            return false;
        if (!TestAxis(t[1], eA[1], eB[0] * AR[1][0] + eB[1] * AR[1][1] + eB[2] * AR[1][2], 1))
            return false;
        if (!TestAxis(t[2], eA[2], eB[0] * AR[2][0] + eB[1] * AR[2][1] + eB[2] * AR[2][2], 2))
            return false;

        if (!TestAxis(t[0] * R[0][0] + t[1] * R[1][0] + t[2] * R[2][0],
                      eA[0] * AR[0][0] + eA[1] * AR[1][0] + eA[2] * AR[2][0], eB[0], 3))
            return false;
        if (!TestAxis(t[0] * R[0][1] + t[1] * R[1][1] + t[2] * R[2][1],
                      eA[0] * AR[0][1] + eA[1] * AR[1][1] + eA[2] * AR[2][1], eB[1], 4))
            return false;
        if (!TestAxis(t[0] * R[0][2] + t[1] * R[1][2] + t[2] * R[2][2],
                      eA[0] * AR[0][2] + eA[1] * AR[1][2] + eA[2] * AR[2][2], eB[2], 5))
            return false;

        if (AR[0][0] < 1.f - 1e-4f && !TestAxis(t[2] * R[1][0] - t[1] * R[2][0], eA[1] * AR[2][0] + eA[2] * AR[1][0],
                                                eB[1] * AR[0][2] + eB[2] * AR[0][1], 6))
            return false;
        if (AR[0][1] < 1.f - 1e-4f && !TestAxis(t[2] * R[1][1] - t[1] * R[2][1], eA[1] * AR[2][1] + eA[2] * AR[1][1],
                                                eB[0] * AR[0][2] + eB[2] * AR[0][0], 7))
            return false;
        if (AR[0][2] < 1.f - 1e-4f && !TestAxis(t[2] * R[1][2] - t[1] * R[2][2], eA[1] * AR[2][2] + eA[2] * AR[1][2],
                                                eB[0] * AR[0][1] + eB[1] * AR[0][0], 8))
            return false;
        if (AR[1][0] < 1.f - 1e-4f && !TestAxis(t[0] * R[2][0] - t[2] * R[0][0], eA[0] * AR[2][0] + eA[2] * AR[0][0],
                                                eB[1] * AR[1][2] + eB[2] * AR[1][1], 9))
            return false;
        if (AR[1][1] < 1.f - 1e-4f && !TestAxis(t[0] * R[2][1] - t[2] * R[0][1], eA[0] * AR[2][1] + eA[2] * AR[0][1],
                                                eB[0] * AR[1][2] + eB[2] * AR[1][0], 10))
            return false;
        if (AR[1][2] < 1.f - 1e-4f && !TestAxis(t[0] * R[2][2] - t[2] * R[0][2], eA[0] * AR[2][2] + eA[2] * AR[0][2],
                                                eB[0] * AR[1][1] + eB[1] * AR[1][0], 11))
            return false;
        if (AR[2][0] < 1.f - 1e-4f && !TestAxis(t[1] * R[0][0] - t[0] * R[1][0], eA[0] * AR[1][0] + eA[1] * AR[0][0],
                                                eB[1] * AR[2][2] + eB[2] * AR[2][1], 12))
            return false;
        if (AR[2][1] < 1.f - 1e-4f && !TestAxis(t[1] * R[0][1] - t[0] * R[1][1], eA[0] * AR[1][1] + eA[1] * AR[0][1],
                                                eB[0] * AR[2][2] + eB[2] * AR[2][0], 13))
            return false;
        if (AR[2][2] < 1.f - 1e-4f && !TestAxis(t[1] * R[0][2] - t[0] * R[1][2], eA[0] * AR[1][2] + eA[1] * AR[0][2],
                                                eB[0] * AR[2][1] + eB[1] * AR[2][0], 14))
            return false;

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
            normal = (len < EPS) ? A.axes[0] : normal / len;
        }

        if (negateAxis)
            normal = -normal;
        if (d.Dot(normal) < 0.f)
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

        auto ClosestPointOnOBB = [&](const DirectX::SimpleMath::Vector3& p) -> DirectX::SimpleMath::Vector3
        {
            const DirectX::SimpleMath::Vector3 dv = p - obb.center;
            DirectX::SimpleMath::Vector3 closest = obb.center;
            const float ext[3] = {obb.halfExtents.x, obb.halfExtents.y, obb.halfExtents.z};
            for (int i = 0; i < 3; ++i)
            {
                float dist = dv.Dot(obb.axes[i]);
                dist = std::clamp(dist, -ext[i], ext[i]);
                closest += obb.axes[i] * dist;
            }
            return closest;
        };

        const DirectX::SimpleMath::Vector3 cands[3] = {top, bot, capsuleCenter};
        float bestD2 = FLT_MAX;
        DirectX::SimpleMath::Vector3 bestSeg, bestObb;

        for (const auto& pt : cands)
        {
            const DirectX::SimpleMath::Vector3 op = ClosestPointOnOBB(pt);
            const float d2 = (pt - op).LengthSquared();
            if (d2 < bestD2)
            {
                bestD2 = d2;
                bestSeg = pt;
                bestObb = op;
            }
        }

        const float dist = std::sqrt(bestD2);
        if (dist >= radius)
            return false;

        out.depth = radius - dist;
        out.position = bestObb;
        out.normal = (dist > 1e-6f) ? (bestSeg - bestObb) / dist : DirectX::SimpleMath::Vector3::UnitY;
        return true;
    }

    // ---- Capsule vs AABB --------------------------------------------------------
    inline bool TestCapsulevsAABB(const DirectX::SimpleMath::Vector3& capsuleCenter, const Capsule& cap,
                                  const DirectX::SimpleMath::Vector3& boxCenter, const AABB& box,
                                  ContactPoint& out) noexcept
    {
        const DirectX::SimpleMath::Vector3 top = capsuleCenter + DirectX::SimpleMath::Vector3::UnitY * cap.halfHeight;
        const DirectX::SimpleMath::Vector3 bot = capsuleCenter - DirectX::SimpleMath::Vector3::UnitY * cap.halfHeight;

        auto Clamp = [&](const DirectX::SimpleMath::Vector3& p) -> DirectX::SimpleMath::Vector3
        {
            return {std::clamp(p.x, boxCenter.x - box.halfExtents.x, boxCenter.x + box.halfExtents.x),
                    std::clamp(p.y, boxCenter.y - box.halfExtents.y, boxCenter.y + box.halfExtents.y),
                    std::clamp(p.z, boxCenter.z - box.halfExtents.z, boxCenter.z + box.halfExtents.z)};
        };

        const DirectX::SimpleMath::Vector3 cands[3] = {top, bot, capsuleCenter};
        float bestD2 = FLT_MAX;
        DirectX::SimpleMath::Vector3 bestSeg, bestBox;

        for (const auto& pt : cands)
        {
            const DirectX::SimpleMath::Vector3 bp = Clamp(pt);
            const float d2 = (pt - bp).LengthSquared();
            if (d2 < bestD2)
            {
                bestD2 = d2;
                bestSeg = pt;
                bestBox = bp;
            }
        }

        const float dist = std::sqrt(bestD2);
        if (dist >= cap.radius)
            return false;

        out.depth = cap.radius - dist;
        out.position = bestBox;
        out.normal = (dist > 1e-6f) ? (bestSeg - bestBox) / dist : DirectX::SimpleMath::Vector3::UnitY;
        return true;
    }

    // ---- Capsule vs Capsule -----------------------------------------------------
    inline bool TestCapsuleCapsule(const DirectX::SimpleMath::Vector3& cA, const Capsule& capA,
                                   const DirectX::SimpleMath::Vector3& cB, const Capsule& capB,
                                   ContactPoint& out) noexcept
    {
        const float ay0 = cA.y - capA.halfHeight, ay1 = cA.y + capA.halfHeight;
        const float by0 = cB.y - capB.halfHeight, by1 = cB.y + capB.halfHeight;
        const float oy = std::min(ay1, by1) - std::max(ay0, by0);
        const float dx = cB.x - cA.x, dz = cB.z - cA.z;
        const float distXZ = std::sqrt(dx * dx + dz * dz), radSum = capA.radius + capB.radius;
        if (distXZ >= radSum || oy < -radSum)
            return false;
        out.depth = radSum - distXZ;
        out.normal = (distXZ > 1e-6f) ? DirectX::SimpleMath::Vector3{dx / distXZ, 0.f, dz / distXZ}
                                      : DirectX::SimpleMath::Vector3::UnitX;
        const float midY = (std::max(ay0, by0) + std::min(ay1, by1)) * 0.5f;
        out.position = {cA.x + out.normal.x * capA.radius, midY, cA.z + out.normal.z * capA.radius};
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

            // ---- Broad-phase ----------------------------------------------------
            {
                const std::size_t n = m_entries.size();
                if (n != m_prevEntryCount)
                {
                    world.GetPhysicsWorld().MarkBroadPhaseDirty();
                    m_prevEntryCount = n;
                }
                for (const auto& e : m_entries)
                    m_sapEntries.push_back({e.m_eid, CalcWorldAABB(*e.m_col, e.m_worldPos, e.m_worldRot), e.m_col});
                world.GetPhysicsWorld().UpdateBroadPhase(m_sapEntries);
            }

            // ---- 逆引きテーブル -------------------------------------------------
            {
                uint32_t maxIndex = 0;
                for (const auto& e : m_entries)
                    maxIndex = std::max(maxIndex, e.m_eid.Index());
                m_entryLookup.assign(maxIndex + 1, nullptr);
                for (auto& e : m_entries)
                    m_entryLookup[e.m_eid.Index()] = &e;
            }

            // ---- Narrow-phase ---------------------------------------------------
            for (const auto& pair : world.GetPhysicsWorld().GetOverlaps())
            {
                const uint32_t idxA = pair.eidA.Index();
                const uint32_t idxB = pair.eidB.Index();
                if (idxA >= m_entryLookup.size() || idxB >= m_entryLookup.size())
                    continue;

                const Entry* ea = m_entryLookup[idxA];
                const Entry* eb = m_entryLookup[idxB];
                if (!ea || !eb)
                    continue;
                if (!ea->m_col->CanCollideWith(*eb->m_col))
                    continue;

                // eid 昇順で A/B を決定
                const bool swapped = ea->m_eid.value > eb->m_eid.value;
                const Entry* A = swapped ? eb : ea;
                const Entry* B = swapped ? ea : eb;

                CollisionResult result;
                result.eid_a = A->m_eid;
                result.eid_b = B->m_eid;
                result.aLayer = A->m_col->layer;
                result.bLayer = B->m_col->layer;
                result.isTriggerEvent = A->m_col->IsTrigger() || B->m_col->IsTrigger();

                // ---- Dispatch ---------------------------------------------------
                bool hit = false;

                // OBB 系は多点接触を生成
                if ((A->m_col->IsOBB() || A->m_col->IsAABB()) && (B->m_col->IsOBB() || B->m_col->IsAABB()))
                {
                    ContactPoint cp{};
                    hit = TestOBBvsOBB(A->m_col->cachedOBB, B->m_col->cachedOBB, cp);
                    if (hit)
                    {
                        result.contact = cp;
                        // Sutherland-Hodgman で多点接触を生成
                        GenerateOBBContacts(A->m_col->cachedOBB, B->m_col->cachedOBB, cp.normal, cp.depth, result);
                    }
                }
                else
                {
                    // Capsule 系は 1 点接触
                    ContactPoint cp{};
                    hit = DispatchNarrow(*A->m_col, A->m_worldPos, *B->m_col, B->m_worldPos, cp);
                    if (hit)
                    {
                        result.contact = cp;
                        result.contacts[0] = cp;
                        result.contactCount = 1;
                    }
                }

                if (!hit)
                    continue;
                world.GetEventQueue().Push(std::move(result));
            }
        }

    private:
        struct Entry
        {
            EntityID m_eid;
            DirectX::SimpleMath::Vector3 m_worldPos;
            DirectX::SimpleMath::Quaternion m_worldRot;
            ColliderComp* m_col;
            RigidbodyComp* m_rb;
        };

        std::size_t m_prevEntryCount = 0;
        std::vector<Entry> m_entries;
        std::vector<const Entry*> m_entryLookup;
        std::vector<SweepAndPrune::Entry> m_sapEntries;

        static bool DispatchNarrow(const ColliderComp& colA, const DirectX::SimpleMath::Vector3& posA,
                                   const ColliderComp& colB, const DirectX::SimpleMath::Vector3& posB,
                                   ContactPoint& out)
        {
            if (colA.IsOBB() && colB.IsCapsule())
            {
                const auto& capB = std::get<Capsule>(colB.shape);
                return TestOBBvsCapsule(colA.cachedOBB, posB, capB.radius, capB.halfHeight, out);
            }
            if (colA.IsCapsule() && colB.IsOBB())
            {
                const auto& capA = std::get<Capsule>(colA.shape);
                bool hit = TestOBBvsCapsule(colB.cachedOBB, posA, capA.radius, capA.halfHeight, out);
                if (hit)
                    out.normal = -out.normal;
                return hit;
            }
            if (colA.IsCapsule() && colB.IsAABB())
                return TestCapsulevsAABB(posA, std::get<Capsule>(colA.shape), posB, std::get<AABB>(colB.shape), out);
            if (colA.IsAABB() && colB.IsCapsule())
            {
                bool hit =
                    TestCapsulevsAABB(posB, std::get<Capsule>(colB.shape), posA, std::get<AABB>(colA.shape), out);
                if (hit)
                    out.normal = -out.normal;
                return hit;
            }
            if (colA.IsCapsule() && colB.IsCapsule())
                return TestCapsuleCapsule(posA, std::get<Capsule>(colA.shape), posB, std::get<Capsule>(colB.shape),
                                          out);
            return false;
        }
    };

} // namespace ECS