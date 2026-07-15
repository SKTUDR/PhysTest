#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Hierarchy.h"

#include <SimpleMath.h>
#include <vector>
#include <algorithm>

namespace ECS
{

    // ---- SceneGraphSystem -------------------------------------------------------
    // firstChild + nextSibling の侵入型リンクリストでシーングラフを管理する。
    //
    // Update() の処理:
    //   1. HierarchyComp を持つ全エンティティを収集
    //   2. depth 昇順ソート（親 → 子の順に処理するため）
    //   3. ルートは LocalTransform をそのまま WorldTransform に
    //      子は LocalMatrix × 親の WorldMatrix → TransformComp に書き戻す
    //
    // SetParent() / Detach() の処理:
    //   - 兄弟チェーンの先頭挿入（O(1)）
    //   - depth を子孫方向に再帰的に更新
    // ----------------------------------------------------------------------------
    class SceneGraphSystem
    {
    public:
        void Update(World& world)
        {
            // ---- 1. 全エンティティを収集 ----------------------------------------
            m_sortBuffer.clear();

            auto desc = QueryBuilder{}.All<TransformComp, LocalTransformComp, HierarchyComp>().Build();

            world.Query(desc).Each<HierarchyComp>([&](EntityID eid, HierarchyComp& h)
                                                  { m_sortBuffer.push_back({eid, h.depth}); });

            // ---- 2. depth 昇順ソート（安定ソートで同 depth 内の順序を保持）------
            std::stable_sort(m_sortBuffer.begin(), m_sortBuffer.end(),
                             [](const Entry& a, const Entry& b) { return a.depth < b.depth; });

            // ---- 3. ワールド行列を伝播 ------------------------------------------
            for (const auto& entry : m_sortBuffer)
            {
                if (!world.IsAlive(entry.eid))
                    continue;

                auto& tr = world.GetComponent<TransformComp>(entry.eid);
                auto& local = world.GetComponent<LocalTransformComp>(entry.eid);
                auto& hier = world.GetComponent<HierarchyComp>(entry.eid);

                DirectX::SimpleMath::Matrix worldMat;

                if (hier.IsRoot())
                {
                    // ルート: ローカル = ワールド
                    worldMat = local.ToLocalMatrix();
                }
                else
                {
                    // 子: ローカル行列 × 親のワールド行列
                    // 親は depth 昇順のため必ず処理済み
                    if (!world.IsAlive(hier.parent) || !world.HasComponent<TransformComp>(hier.parent))
                    {
                        worldMat = local.ToLocalMatrix();
                    }
                    else
                    {
                        const auto& parentTr = world.GetComponent<TransformComp>(hier.parent);
                        worldMat = local.ToLocalMatrix() * parentTr.ToWorldMatrix();
                    }
                }

                // TransformComp に分解して書き戻す
                DecomposeMatrix(worldMat, tr.position, tr.rotation, tr.scale);
            }
        }

        // =========================================================================
        //  SetParent
        //  child を parent の子として登録する。
        //
        //  兄弟チェーンへの挿入は先頭挿入（O(1)）。
        //  既存の親がある場合は先に Detach() を呼ぶこと。
        // =========================================================================
        static void SetParent(World& world, EntityID child, EntityID parent)
        {
            if (!world.HasComponent<HierarchyComp>(child))
                world.AddComponent<HierarchyComp>(child);
            if (!world.HasComponent<HierarchyComp>(parent))
                world.AddComponent<HierarchyComp>(parent);

            auto& childHier = world.GetComponent<HierarchyComp>(child);
            auto& parentHier = world.GetComponent<HierarchyComp>(parent);

            // 既存の親から切り離す
            if (!childHier.parent.IsNull() && world.IsAlive(childHier.parent))
            {
                DetachFromParent(world, child, childHier);
            }

            // 兄弟チェーンの先頭に挿入（O(1)）
            // child.nextSibling = parent.firstChild
            // parent.firstChild = child
            childHier.parent = parent;
            childHier.nextSibling = parentHier.firstChild;
            parentHier.firstChild = child;

            // depth を子孫方向に再帰更新
            UpdateDepthRecursive(world, child, parentHier.depth + 1);
        }

        // =========================================================================
        //  Detach
        //  child を親から切り離してルートに戻す。
        // =========================================================================
        static void Detach(World& world, EntityID child)
        {
            if (!world.HasComponent<HierarchyComp>(child))
                return;

            auto& childHier = world.GetComponent<HierarchyComp>(child);
            if (childHier.parent.IsNull())
                return; // 既にルート

            DetachFromParent(world, child, childHier);

            childHier.parent = EntityID::Null();
            UpdateDepthRecursive(world, child, 0);
        }

    private:
        struct Entry
        {
            EntityID eid;
            int depth;
        };
        std::vector<Entry> m_sortBuffer;

        // ---- 親の兄弟チェーンから child を取り除く（O(兄弟数)）----------------
        static void DetachFromParent(World& world, EntityID child, HierarchyComp& childHier)
        {
            if (!world.IsAlive(childHier.parent))
                return;
            if (!world.HasComponent<HierarchyComp>(childHier.parent))
                return;

            auto& parentHier = world.GetComponent<HierarchyComp>(childHier.parent);

            if (parentHier.firstChild == child)
            {
                // 先頭の子を削除
                parentHier.firstChild = childHier.nextSibling;
            }
            else
            {
                // 兄弟チェーンを辿って child の前のノードを見つける
                EntityID prev = parentHier.firstChild;
                while (!prev.IsNull() && world.IsAlive(prev))
                {
                    auto& prevHier = world.GetComponent<HierarchyComp>(prev);
                    if (prevHier.nextSibling == child)
                    {
                        prevHier.nextSibling = childHier.nextSibling;
                        break;
                    }
                    prev = prevHier.nextSibling;
                }
            }

            childHier.nextSibling = EntityID::Null();
        }

        // ---- depth を子孫方向に再帰的に更新 ------------------------------------
        // firstChild → nextSibling チェーンを辿る
        static void UpdateDepthRecursive(World& world, EntityID eid, int depth)
        {
            if (!world.IsAlive(eid))
                return;
            if (!world.HasComponent<ECS::HierarchyComp>(eid))
                return;

            auto& hier = world.GetComponent<HierarchyComp>(eid);
            hier.depth = depth;

            // 子を再帰更新
            EntityID child = hier.firstChild;
            while (!child.IsNull() && world.IsAlive(child))
            {
                const EntityID next = world.GetComponent<HierarchyComp>(child).nextSibling;
                UpdateDepthRecursive(world, child, depth + 1);
                child = next;
            }
        }

        // ---- 4x4 行列を position / rotation / scale に分解 --------------------
        static void DecomposeMatrix(const DirectX::SimpleMath::Matrix& mat, DirectX::SimpleMath::Vector3& outPos,
                                    DirectX::SimpleMath::Quaternion& outRot,
                                    DirectX::SimpleMath::Vector3& outScale) noexcept
        {
            outScale.x = DirectX::SimpleMath::Vector3(mat._11, mat._12, mat._13).Length();
            outScale.y = DirectX::SimpleMath::Vector3(mat._21, mat._22, mat._23).Length();
            outScale.z = DirectX::SimpleMath::Vector3(mat._31, mat._32, mat._33).Length();

            DirectX::SimpleMath::Matrix rotMat = mat;
            if (outScale.x > 1e-6f)
            {
                rotMat._11 /= outScale.x;
                rotMat._12 /= outScale.x;
                rotMat._13 /= outScale.x;
            }
            if (outScale.y > 1e-6f)
            {
                rotMat._21 /= outScale.y;
                rotMat._22 /= outScale.y;
                rotMat._23 /= outScale.y;
            }
            if (outScale.z > 1e-6f)
            {
                rotMat._31 /= outScale.z;
                rotMat._32 /= outScale.z;
                rotMat._33 /= outScale.z;
            }

            outRot = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(rotMat);
            outRot.Normalize();

            outPos = {mat._41, mat._42, mat._43};
        }
    };

} // namespace ECS