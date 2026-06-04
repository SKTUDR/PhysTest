//--------------------------------------------------------------------------------------
// File: Entity.h
//
// エンティティIDの定義
//
// Date: 2026.4.18
// Author: Ryosuke Koike
//--------------------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <limits>

namespace ECS {

// EntityID
// 世代付き32bitハンドル
// 上位8bit = 世代 (generation), 下位24bit = インデックス
using EntityRaw = uint32_t;

    struct EntityID {
        static constexpr uint32_t INDEX_BITS = 24;
        static constexpr uint32_t GEN_BITS   = 8;
        static constexpr uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1u; // 0x00FFFFFF
        static constexpr uint32_t GEN_MASK   = (1u << GEN_BITS)   - 1u; // 0xFF
        static constexpr uint32_t NULL_INDEX = INDEX_MASK;               // 無効値

        EntityRaw value = 0;

        static EntityID Make(uint32_t index, uint8_t gen) noexcept {
            return EntityID{ (static_cast<uint32_t>(gen) << INDEX_BITS) | (index & INDEX_MASK) };
        }

        static EntityID Null() noexcept { return EntityID{ NULL_INDEX }; }

        uint32_t Index()      const noexcept { return value & INDEX_MASK; }
        uint8_t  Generation() const noexcept { return static_cast<uint8_t>(value >> INDEX_BITS); }
        bool     IsNull()    const noexcept { return Index() == NULL_INDEX; }

        bool operator==(EntityID o) const noexcept { return value == o.value; }
        bool operator!=(EntityID o) const noexcept { return value != o.value; }
    };

} // namespace ECS
