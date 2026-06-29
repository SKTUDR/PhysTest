#pragma once
#include "../ECS/Entity.h"

namespace ECS
{

    // ---- HierarchyComp ----------------------------------------------------------
    // 親子関係を侵入型リンクリスト（firstChild + nextSibling）で保持する。
    //
    // メモリ効率と参照局所性を改善する。
    //
    // 構造例:
    //   parent
    //     firstChild → child0 → child1 → child2 → Null
    //                  (nextSibling chain)
    //
    // depth はルートからの深さをキャッシュする。
    // SceneGraphSystem::SetParent() / Detach() が更新する。
    // 必要になったら双方向リストに変更予定
    // ----------------------------------------------------------------------------
    struct HierarchyComp
    {
        EntityID parent = EntityID::Null();      // 親（ルートは Null）
        EntityID firstChild = EntityID::Null();  // 最初の子（子なしは Null）
        EntityID nextSibling = EntityID::Null(); // 次の兄弟（末っ子は Null）
        int depth = 0;                           // ルートからの深さ（キャッシュ）

        bool IsRoot() const noexcept
        {
            return parent.IsNull();
        }
        bool HasChild() const noexcept
        {
            return !firstChild.IsNull();
        }
        bool HasSibling() const noexcept
        {
            return !nextSibling.IsNull();
        }
    };

} // namespace ECS