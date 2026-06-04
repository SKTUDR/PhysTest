#pragma once
#include "../ECS/Entity.h"

#include <vector>
#include <cstdint>

namespace ECS {

// ---- ContactPoint -----------------------------------------------------------
// 1接触点の情報。Capsule-AABB など複数接触が生じる形状では
// CollisionResult に複数格納する（今は 1 点で十分なら max_contacts=1）。
struct ContactPoint {
    DirectX::SimpleMath::Vector3  position;   // ワールド空間の接触点
    DirectX::SimpleMath::Vector3  normal;     // B→A 方向の衝突法線（正規化済み）
    float depth{};                            // 貫通深度 (>0 = 重なっている)
};

// ---- CollisionResult --------------------------------------------------------
// CollisionSystem が生成し、World のイベントキューに積む。
// 各 System はフレーム末に world.collision_events() を読んで処理する。
//
// 対称性: A-B ペアは 1 エントリのみ（A < B でソート済み）。
//         is_trigger の組み合わせは以下:
//           両方 false → response + event
//           いずれか true → event のみ（応答なし）
// --------------------------------------------------------------------------
struct CollisionResult {
    EntityID eid_a;              // エンティティ A (index 昇順)
    EntityID eid_b;              // エンティティ B
    ContactPoint contact;    // 代表接触点（多接触の場合は最深点
    float    impulse = 0.f;  // 付与した撃力の大きさ（response 後に確定）
    bool     isTriggerEvent = false; // true = 応答なし、通知のみ

    // どちらかが EntityID を持つか検索するヘルパー
    bool Involves(EntityID e) const noexcept {
        return eid_a == e || eid_b == e;
    }
    // 相手を返す
    EntityID Other(EntityID self) const noexcept {
        return (eid_a == self) ? eid_b : eid_a;
    }
    // 法線を self 視点に変換（B なら反転）
    DirectX::SimpleMath::Vector3 NormalFor(EntityID self) const noexcept {
        if (eid_a == self) return contact.normal;
        return DirectX::SimpleMath::Vector3{ -contact.normal.x, -contact.normal.y, -contact.normal.z };
    }
};

// ---- CollisionEventQueue ----------------------------------------------------
// World が1フレーム分の CollisionResult を保持するコンテナ。
// World::clear_collision_events() をフレーム先頭で呼ぶこと。
using CollisionEventQueue = std::vector<CollisionResult>;

} // namespace ECS
