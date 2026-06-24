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
    ROCK        = 1u << 3,   // 瓦礫
    STONE       = 1u << 4,   // 石

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
    // プレイヤー: 地形・敵と衝突
    inline constexpr CollisionLayer PLAYER_MASK =
        CollisionLayer::WORLD | CollisionLayer::ENEMY;

    // 敵: 地形・プレイヤーと衝突、投げたいた石とは衝突しない
    inline constexpr CollisionLayer ENEMY_MASK =
        CollisionLayer::WORLD | CollisionLayer::PLAYER;

    // 地形: プレイヤー・敵・投げた石と衝突
    inline constexpr CollisionLayer WORLD_MASK = 
        CollisionLayer::PLAYER | CollisionLayer::ENEMY | CollisionLayer::STONE;

    inline constexpr CollisionLayer ROCK_MASK =
        CollisionLayer::ENEMY | CollisionLayer::STONE;

} // namespace LayerPreset

} // namespace ECS
