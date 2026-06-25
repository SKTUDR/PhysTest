#pragma once
#include "../ECS/Entity.h"
#include "../Components/CollisionLayer.h"
#include <SimpleMath.h>
#include <vector>

namespace ECS
{

    // ---- ContactPoint -----------------------------------------------------------
    struct ContactPoint
    {
        DirectX::SimpleMath::Vector3 normal;    // 衝突法線（A→B方向）
        DirectX::SimpleMath::Vector3 position;  // 接触点（ワールド空間・代表点）
        DirectX::SimpleMath::Vector3 positionA; // A 側の接触点
        DirectX::SimpleMath::Vector3 positionB; // B 側の接触点
        float depth = 0.f;                      // 貫通深度
    };

    // ---- CollisionResult --------------------------------------------------------
    // 複数接触点対応。contacts に最大4点を格納する。
    // contactCount が 0 のときは contacts[0] のみ有効。
    // ----------------------------------------------------------------------------
    struct CollisionResult
    {
        EntityID eid_a;
        EntityID eid_b;
        CollisionLayer aLayer = CollisionLayer::NONE;
        CollisionLayer bLayer = CollisionLayer::NONE;

        // 代表接触点（後方互換・1点判定用）
        ContactPoint contact;

        // 複数接触点（Sutherland-Hodgman クリッピング結果・最大4点）
        static constexpr int MAX_CONTACTS = 4;
        ContactPoint contacts[MAX_CONTACTS];
        int contactCount = 0;

        float impulse = 0.f;
        bool isTriggerEvent = false;

        // 接触点を追加（MAX_CONTACTS を超えたら最大深度のものと入れ替え）
        void AddContact(const ContactPoint& cp)
        {
            if (contactCount < MAX_CONTACTS)
            {
                contacts[contactCount++] = cp;
                return;
            }
            // 最小深度の接触点を置き換える
            int minIdx = 0;
            for (int i = 1; i < MAX_CONTACTS; ++i)
            {
                if (contacts[i].depth < contacts[minIdx].depth)
                    minIdx = i;
            }
            if (cp.depth > contacts[minIdx].depth)
                contacts[minIdx] = cp;
        }

        bool involves(EntityID e) const noexcept
        {
            return eid_a == e || eid_b == e;
        }
        EntityID other(EntityID self) const noexcept
        {
            return (eid_a == self) ? eid_b : eid_a;
        }
    };

    using CollisionEventQueue = std::vector<CollisionResult>;

} // namespace ECS