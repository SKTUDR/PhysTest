#pragma once
#include "../Components/Collider.h"
#include "../ECS/Entity.h"

#include <SimpleMath.h>
#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace ECS
{

    // ============================================================================
    //  ワールド空間 AABB
    // ============================================================================
    struct WorldAABB
    {
        float minX, maxX;
        float minY, maxY;
        float minZ, maxZ;
    };

    inline WorldAABB CalcWorldAABB(const ColliderComp& col, const DirectX::SimpleMath::Vector3& worldPos,
                                   const DirectX::SimpleMath::Quaternion& worldRot) noexcept
    {
        WorldAABB out{};

        if (col.IsAABB())
        {
            const auto& aabb = std::get<AABB>(col.shape);
            out.minX = worldPos.x - aabb.halfExtents.x;
            out.maxX = worldPos.x + aabb.halfExtents.x;
            out.minY = worldPos.y - aabb.halfExtents.y;
            out.maxY = worldPos.y + aabb.halfExtents.y;
            out.minZ = worldPos.z - aabb.halfExtents.z;
            out.maxZ = worldPos.z + aabb.halfExtents.z;
        }
        else if (col.IsOBB())
        {
            const auto& obb = std::get<OBB>(col.shape);
            const DirectX::SimpleMath::Quaternion rot =
                DirectX::SimpleMath::Quaternion::Concatenate(col.localRotation, worldRot);

            const auto ax = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitX, rot);
            const auto ay = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitY, rot);
            const auto az = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitZ, rot);

            const float rx = std::abs(ax.x) * obb.halfExtents.x + std::abs(ay.x) * obb.halfExtents.y +
                             std::abs(az.x) * obb.halfExtents.z;
            const float ry = std::abs(ax.y) * obb.halfExtents.x + std::abs(ay.y) * obb.halfExtents.y +
                             std::abs(az.y) * obb.halfExtents.z;
            const float rz = std::abs(ax.z) * obb.halfExtents.x + std::abs(ay.z) * obb.halfExtents.y +
                             std::abs(az.z) * obb.halfExtents.z;

            out.minX = worldPos.x - rx;
            out.maxX = worldPos.x + rx;
            out.minY = worldPos.y - ry;
            out.maxY = worldPos.y + ry;
            out.minZ = worldPos.z - rz;
            out.maxZ = worldPos.z + rz;
        }
        else if (col.IsCapsule())
        {
            const auto& cap = std::get<Capsule>(col.shape);
            out.minX = worldPos.x - cap.radius;
            out.maxX = worldPos.x + cap.radius;
            out.minY = worldPos.y - cap.halfHeight - cap.radius;
            out.maxY = worldPos.y + cap.halfHeight + cap.radius;
            out.minZ = worldPos.z - cap.radius;
            out.maxZ = worldPos.z + cap.radius;
        }

        return out;
    }

    // ============================================================================
    //  OverlapPair キー  (eid_lo << 32 | eid_hi)
    // ============================================================================
    inline std::uint64_t MakePairKey(EntityID a, EntityID b) noexcept
    {
        const std::uint32_t lo = static_cast<std::uint32_t>(std::min(a.value, b.value));
        const std::uint32_t hi = static_cast<std::uint32_t>(std::max(a.value, b.value));
        return (static_cast<std::uint64_t>(lo) << 32) | hi;
    }

    // ============================================================================
    //  SweepAndPrune  (3軸 AND)
    //
    //  各軸で独立に挿入ソートしてオーバーラップセットを管理し、
    //  X・Y・Z すべての軸でオーバーラップしているペアのみを出力する。
    // ============================================================================
    class SweepAndPrune
    {
    public:
        struct Entry
        {
            EntityID eid;
            WorldAABB aabb;
            ColliderComp* col;
        };

        struct OverlapPair
        {
            EntityID eidA;
            EntityID eidB;
        };

        void Update(const std::vector<Entry>& entries)
        {
            BuildEidIndex(entries);
            const std::size_t n = entries.size();
            if (m_dirty || m_prevSize != n)
            {
                RebuildAll(entries);
                m_prevSize = n;
                m_dirty = false;
            }
            else
            {
                UpdateValues(entries);
            }

            InsertionSort(m_axisX, m_overlapX, /*axis=*/0);
            InsertionSort(m_axisY, m_overlapY, /*axis=*/1);
            InsertionSort(m_axisZ, m_overlapZ, /*axis=*/2);

            RefreshOverlapList();
        }

        const std::vector<OverlapPair>& GetOverlaps() const noexcept
        {
            return m_overlaps;
        }

        void MarkDirty() noexcept
        {
            m_dirty = true;
        }

    private:
        // ---- EndPoint -------------------------------------------------------
        struct EndPoint
        {
            float value;
            EntityID eid;
            bool isMin;
        };

        // 3軸分
        std::vector<EndPoint> m_axisX, m_axisY, m_axisZ;

        // 各軸のオーバーラップセット
        std::unordered_set<std::uint64_t> m_overlapX, m_overlapY, m_overlapZ;

        std::unordered_map<std::uint32_t, std::size_t> m_eidToIndex;

        std::vector<OverlapPair> m_overlaps;
        std::size_t m_prevSize = 0;

        bool m_dirty = true;
        bool m_overlapsDirty = true;

        // ---- Entry 逆引き ---------------------------------------------------
        const Entry* FindEntry(const std::vector<Entry>& entries, EntityID eid) const noexcept
        {
            for (const auto& e : entries)
                if (e.eid.value == eid.value)
                    return &e;
            return nullptr;
        }

        // ---- 軸ごとの min/max 値を返すヘルパ ---------------------------------
        static float GetMin(const WorldAABB& aabb, int axis)
        {
            if (axis == 0)
                return aabb.minX;
            if (axis == 1)
                return aabb.minY;
            return aabb.minZ;
        }
        static float GetMax(const WorldAABB& aabb, int axis)
        {
            if (axis == 0)
                return aabb.maxX;
            if (axis == 1)
                return aabb.maxY;
            return aabb.maxZ;
        }

        // ---- 1軸のエンドポイントリストを初期化 --------------------------------
        void BuildAxis(std::vector<EndPoint>& axis, std::unordered_set<std::uint64_t>& overlapSet,
                       const std::vector<Entry>& entries, int axisIdx)
        {
            axis.clear();
            axis.reserve(entries.size() * 2);
            overlapSet.clear();

            for (const auto& e : entries)
            {
                axis.push_back({GetMin(e.aabb, axisIdx), e.eid, true});
                axis.push_back({GetMax(e.aabb, axisIdx), e.eid, false});
            }

            std::sort(axis.begin(), axis.end(),
                      [](const EndPoint& a, const EndPoint& b)
                      {
                          if (a.value != b.value)
                              return a.value < b.value;
                          return a.isMin && !b.isMin;
                      });

            // 初期オーバーラップセット構築
            std::vector<EntityID> activeMin;
            for (const auto& ep : axis)
            {
                if (ep.isMin)
                {
                    for (const auto& other : activeMin)
                        overlapSet.insert(MakePairKey(ep.eid, other));
                    activeMin.push_back(ep.eid);
                }
                else
                {
                    auto it = std::find_if(activeMin.begin(), activeMin.end(),
                                           [&](const EntityID& id) { return id.value == ep.eid.value; });
                    if (it != activeMin.end())
                        activeMin.erase(it);
                }
            }
        }

        // UpdateValues 呼び出し前に毎フレーム構築
        void BuildEidIndex(const std::vector<Entry>& entries)
        {
            m_eidToIndex.clear();
            m_eidToIndex.reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
                m_eidToIndex[entries[i].eid.value] = i;
        }

        // ---- 全軸再構築 -------------------------------------------------------
        void RebuildAll(const std::vector<Entry>& entries)
        {
            BuildAxis(m_axisX, m_overlapX, entries, 0);
            BuildAxis(m_axisY, m_overlapY, entries, 1);
            BuildAxis(m_axisZ, m_overlapZ, entries, 2);
            RefreshOverlapList();
        }

        // UpdateValues 内
        void UpdateValues(const std::vector<Entry>& entries)
        {
            for (auto& ep : m_axisX)
            {
                auto it = m_eidToIndex.find(ep.eid.value);
                if (it == m_eidToIndex.end())
                    continue;
                const auto& aabb = entries[it->second].aabb;
                ep.value = ep.isMin ? aabb.minX : aabb.maxX;
            }
            for (auto& ep : m_axisY)
            {
                auto it = m_eidToIndex.find(ep.eid.value);
                if (it == m_eidToIndex.end())
                    continue;
                const auto& aabb = entries[it->second].aabb;
                ep.value = ep.isMin ? aabb.minY : aabb.maxY;
            }
            for (auto& ep : m_axisZ)
            {
                auto it = m_eidToIndex.find(ep.eid.value);
                if (it == m_eidToIndex.end())
                    continue;
                const auto& aabb = entries[it->second].aabb;
                ep.value = ep.isMin ? aabb.minZ : aabb.maxZ;
            }
        }

        // ---- 1軸の挿入ソート --------------------------------------------------
        void InsertionSort(std::vector<EndPoint>& axis, std::unordered_set<std::uint64_t>& overlapSet, int /*axisIdx*/)
        {
            const int sz = static_cast<int>(axis.size());
            for (int i = 1; i < sz; ++i)
            {
                EndPoint cur = axis[i];
                int j = i - 1;

                while (j >= 0 && ShouldSwap(axis[j], cur))
                {
                    const EndPoint& other = axis[j];

                    if (cur.eid.value != other.eid.value)
                    {
                        if (!other.isMin && cur.isMin)
                        {
                            overlapSet.insert(MakePairKey(cur.eid, other.eid)); // overlap開始
                            m_overlapsDirty = true;
                        }
                            
                        else if (other.isMin && !cur.isMin)
                        {
                            overlapSet.erase(MakePairKey(cur.eid, other.eid)); // overlap終了
                            m_overlapsDirty = true;
                        }
                            
                    }

                    axis[j + 1] = axis[j];
                    --j;
                }
                axis[j + 1] = cur;
            }
        }

        static bool ShouldSwap(const EndPoint& left, const EndPoint& right)
        {
            constexpr float EPS = 1e-4f;
            const float diff = left.value - right.value;
            if (diff > EPS)
                return true;
            if (diff < -EPS)
                return false;
            // 同値なら min を先
            return !left.isMin && right.isMin;
        }

        // ---- 3軸 AND でオーバーラップリストを更新 ----------------------------
        void RefreshOverlapList()
        {
            if (!m_overlapsDirty)
                return;

            m_overlaps.clear();
            m_overlapsDirty = false;

            const auto* smallest = &m_overlapX;

            if (m_overlapY.size() < smallest->size())
                smallest = &m_overlapY;

            if (m_overlapZ.size() < smallest->size())
                smallest = &m_overlapZ;

            // X ∩ Y ∩ Z
            for (const auto key : *smallest)
            {
                if (m_overlapX.count(key) && m_overlapY.count(key) && m_overlapZ.count(key))
                {
                    const std::uint32_t lo = static_cast<std::uint32_t>(key >> 32);
                    const std::uint32_t hi = static_cast<std::uint32_t>(key & 0xFFFFFFFF);
                    m_overlaps.push_back({EntityID{lo}, EntityID{hi}});
                }
            }
        }
    };

} // namespace ECS