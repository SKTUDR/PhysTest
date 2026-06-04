#pragma once
#include <cstdint>

namespace ECS {

// ---- CollisionLayer ---------------------------------------------------------
// エンティティが「何者か」を表すビット (layer) と
// 「何と衝突するか」を表すビット (mask) に使う。
//
// 判定条件: (A.layer & B.mask) != 0  かつ  (B.layer & A.mask) != 0
// 片方向だけ立てれば片方向のみ通知も可能。
// ----------------------------------------------------------------------------
enum class CollisionLayer : uint32_t {
    NONE        = 0,
    WORLD       = 1u << 0,   // 静的地形・床・壁
    PLAYER      = 1u << 1,   // プレイヤー
    ENEMY       = 1u << 2,   // 敵

    ALL         = ~0u,
};

inline constexpr CollisionLayer operator|(CollisionLayer a, CollisionLayer b) noexcept {
    return static_cast<CollisionLayer>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline constexpr CollisionLayer operator&(CollisionLayer a, CollisionLayer b) noexcept {
    return static_cast<CollisionLayer>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline constexpr bool Any(CollisionLayer v) noexcept {
    return static_cast<uint32_t>(v) != 0u;
}

// よく使うフィルタプリセット
namespace LayerPreset {
    // プレイヤー: 地形・敵と衝突、グレネードは受け取らない
    inline constexpr CollisionLayer PLAYER_MASK =
        CollisionLayer::WORLD | CollisionLayer::ENEMY;

    // 敵: 地形・プレイヤー・爆風を受ける
    inline constexpr CollisionLayer ENEMY_MASK =
        CollisionLayer::WORLD | CollisionLayer::PLAYER;

    // 地形: プレイヤー・敵と衝突、爆風は受け取らない
    inline constexpr CollisionLayer WORLD_MASK = 
        CollisionLayer::PLAYER | CollisionLayer::ENEMY;

} // namespace LayerPreset

} // namespace ECS
