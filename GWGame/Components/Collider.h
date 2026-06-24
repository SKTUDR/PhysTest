#pragma once
#include "../ECS/ComponentType.h"
#include "CollisionLayer.h"

#include <variant>
#include <array>

namespace ECS {

// ---- Shape variants ---------------------------------------------------------

struct AABB {
    DirectX::SimpleMath::Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };  // ローカル中心からの半幅
};

struct OBB {
    DirectX::SimpleMath::Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };
    DirectX::SimpleMath::Quaternion orientation;                         // ローカル回転（ボーン連動など）
};

struct Capsule {
    float radius      = 0.4f;  // 半径
    float halfHeight = 0.9f;  // 円柱部分の半高さ（端球は含まない）
    // 軸は常にローカル Y 軸。回転は local_offset の Quat で制御する。
};

using ShapeVariant = std::variant<AABB, OBB, Capsule>;

// ---- PhysMaterial -----------------------------------------------------------
// 衝突応答パラメータ。isTrigger = true のときは無視される。
struct PhysMaterial {
    float restitution = 0.0f;  // 反発係数 [0=吸収, 1=完全弾性]
    float friction    = 0.5f;  // 動摩擦係数
    float density     = 1.0f;  // 質量計算用
};

// ---- ColliderFlags ----------------------------------------------------------
struct ColliderFlags {
    bool isTrigger : 1;  // true → 押し戻しなし、イベントのみ発火
    bool isDynamic : 1;  // false → 静的
    bool generateContacts : 1; // false → broad-phase のみ

    ColliderFlags() : isTrigger(false), isDynamic(true), generateContacts(true) {}
};
// SAT 計算用の OBB 表現（ワールド空間）
struct OBBData
{
    DirectX::SimpleMath::Vector3 center;
    DirectX::SimpleMath::Vector3 halfExtents;
    std::array<DirectX::SimpleMath::Vector3, 3> axes; // [0]=X [1]=Y [2]=Z
};

// ---- Collider -----------------------------------------------------------
// 純粋データ。ロジックなし。
// コライダーをつけるならRigidbodyCompもつけること
//
// 使い方:
//   auto& col = world.AddComponent<ColliderComp>(eid);
//   col.shape  = Capsule{ .radius=0.4f, .half_height=0.9f };
//   col.layer  = CollisionLayer::PLAYER;
//   col.mask   = LayerPreset::PLAYER_MASK;
// 
//   auto& rb = world.AddComponent<RigidbodyComp>(eid);
// ----------------------------------------------------------------------------
struct ColliderComp {
    ShapeVariant    shape  = AABB{};
    CollisionLayer  layer  = CollisionLayer::NONE;
    CollisionLayer  mask   = CollisionLayer::NONE;
    ColliderFlags   flags  = {};
    PhysMaterial    material;

    OBBData cachedOBB{};

    // ローカル空間でのコライダーオフセット（モデル原点からのズレ補正）
    DirectX::SimpleMath::Vector3 localOffset{ 0.f, 0.f, 0.f };
    DirectX::SimpleMath::Quaternion localRotation;   // デフォルト = 単位クォータニオン

    // --- ヘルパー ---
    bool IsTrigger()  const noexcept { return flags.isTrigger; }
    bool IsDynamic()  const noexcept { return flags.isDynamic; }
    bool CanCollideWith(const ColliderComp& other) const noexcept {
        return Any(layer & other.mask) && Any(other.layer & mask);
    }

    // よく使う形状チェック
    bool IsAABB()    const noexcept { return std::holds_alternative<AABB>(shape); }
    bool IsOBB()     const noexcept { return std::holds_alternative<OBB>(shape); }
    bool IsCapsule() const noexcept { return std::holds_alternative<Capsule>(shape); }

    DirectX::SimpleMath::Vector3 GetHalfExtents() const noexcept {
        if (IsAABB())    return std::get<AABB>(shape).halfExtents;
        if (IsOBB())     return std::get<OBB>(shape).halfExtents;
        if (IsCapsule()) return {std::get<Capsule>(shape).radius, std::get<Capsule>(shape).halfHeight, std::get<Capsule>(shape).radius};
        return {0.f, 0.f, 0.f};
    }
};

} // namespace ECS
