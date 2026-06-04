//--------------------------------------------------------------------------------------
// File: Query.h
//
// Archetype を絞り込む QueryDesc と、その結果を走査する QueryResult の定義。
//
// Date: 2026.4.18
// Author: Ryosuke Koike
//--------------------------------------------------------------------------------------
#pragma once
#include "ComponentType.h"
#include "Archetype.h"

#include <vector>
#include <functional>

namespace ECS {

// ---- QueryDesc --------------------------------------------------------------
// 「all / any / none」の3種マスクでアーキタイプを絞り込む。
//   all  : これらコンポーネントをすべて持つ
//   any  : これらのうち少なくとも1つを持つ
//   none : これらをひとつも持たない
// ----------------------------------------------------------------------------
struct QueryDesc {
    ComponentMask all;   // 必須
    ComponentMask any;   // 任意（空なら判定スキップ）
    ComponentMask none;  // 除外

    bool Matches(const ComponentMask& archMask) const noexcept {
        if ((archMask & all) != all)           return false;
        if (none.any() && (archMask & none).any()) return false;
        if (any.any()  && (archMask & any).none()) return false;
        return true;
    }
};

// ---- QueryBuilder -----------------------------------------------------------
// QueryDesc を組み立てるヘルパー。
// 使い方:
//   auto desc = QueryBuilder{}
//       .all<TransformComp, VelocityComp>()
//       .none<StaticBlockComp>()
//       .build();
// ----------------------------------------------------------------------------
struct QueryBuilder {
    QueryDesc desc;

    template <typename... Ts>
    QueryBuilder& All() {
        desc.all |= MakeMask<Ts...>();
        return *this;
    }
    template <typename... Ts>
    QueryBuilder& Any() {
        desc.any |= MakeMask<Ts...>();
        return *this;
    }
    template <typename... Ts>
    QueryBuilder& None() {
        desc.none |= MakeMask<Ts...>();
        return *this;
    }

    QueryDesc Build() const noexcept { return desc; }
};

// ---- QueryResult ------------------------------------------------------------
// マッチした Archetype* のリストと、その中を走査するイテレータを提供する。
// World::query() が返す軽量ビュー（ポインタのみ保持・コピー可）。
// ----------------------------------------------------------------------------
class QueryResult {
public:
    explicit QueryResult(std::vector<Archetype*> archetypes)
        : m_archetypes(std::move(archetypes)) {}

    // 各エンティティに対してコールバックを呼ぶ
    // callback(EntityID, Archetype&, std::size_t row)
    template <typename Fn>
    void Each(Fn&& fn) const {
        for (Archetype* arch : m_archetypes) {
            const std::size_t n = arch->Size();
            for (std::size_t row = 0; row < n; ++row) {
                fn(arch->EntityAt(row), *arch, row);
            }
        }
    }

    // 型付き Each: コールバックにコンポーネント参照を直接渡す
    // 使い方:
    //   world.Each<TransformComp, VelocityComp>(
    //       [](EntityID id, TransformComp& t, VelocityComp& v){ ... });
    template <typename... Ts, typename Fn>
    void Each(Fn&& fn) const {
        for (Archetype* arch : m_archetypes) {
            const std::size_t n = arch->Size();
            for (std::size_t row = 0; row < n; ++row) {
                fn(arch->EntityAt(row), arch->Get<Ts>(row)...);
            }
        }
    }

    std::size_t ArchetypeCount() const noexcept { return m_archetypes.size(); }
    const std::vector<Archetype*>& Archetypes() const noexcept { return m_archetypes; }

private:
    std::vector<Archetype*> m_archetypes;
};

} // namespace ECS
