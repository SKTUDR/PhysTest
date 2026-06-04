//--------------------------------------------------------------------------------------
// File: ComponentType.h
//
// コンポーネント型 → コンパイル時ID マッピング
//
// Date: 2026.4.18
// Author: Ryosuke Koike
//--------------------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <bitset>
#include <cassert>

namespace ECS {

// コンポーネントの最大種類数
static constexpr std::size_t MAX_COMPONENTS = 64;

using ComponentID      = uint8_t;
using ComponentMask    = std::bitset<MAX_COMPONENTS>;

// ---- 型→ID マッピング -------------------------------------------------------
// 各コンポーネント型に対してコンパイル時に一意のIDを付与する。
// 使い方:
//   ComponentType<TransformComp>::id()   → ComponentID
//   ComponentType<TransformComp>::mask() → ComponentMask (該当ビットのみ1)
// ---------------------------------------------------------------------------

namespace detail {

// グローバルカウンタ（翻訳単位をまたがるため inline変数を使用）
inline ComponentID g_nextComponentID = 0;

template <typename T>
inline ComponentID g_componentID = []() -> ComponentID {
    assert(g_nextComponentID < MAX_COMPONENTS && "MAX_COMPONENTS exceeded");
    return g_nextComponentID++;
}();

} // namespace detail

template <typename T>
struct ComponentType {
    static ComponentID ID() noexcept {
        return detail::g_componentID<T>;
    }

    static ComponentMask Mask() noexcept {
        ComponentMask m;
        m.set(ID());
        return m;
    }
};

// 複数コンポーネント型からまとめてマスクを作る便利関数
template <typename... Ts>
ComponentMask MakeMask() noexcept {
    ComponentMask m;
    (m.set(ComponentType<Ts>::ID()), ...);
    return m;
}

} // namespace ECS
