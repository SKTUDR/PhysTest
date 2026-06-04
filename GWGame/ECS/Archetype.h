//--------------------------------------------------------------------------------------
// File: Archetype.h
//
// ECS の Archetype クラスの定義
//
// Date: 2026.5.11
// Author: Ryosuke Koike
//--------------------------------------------------------------------------------------
#pragma once
#include "Entity.h"
#include "ComponentType.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <cassert>
#include <functional>

namespace ECS {

// ---- ComponentStorage -------------------------------------------------------
// 1種類のコンポーネントを連続メモリに保持する型消去コンテナ。
// 要素サイズは実行時に決まる。SoA配置のカラム1本に相当する。
// ----------------------------------------------------------------------------
struct ComponentStorage {
    std::size_t      elementSize = 0;
    std::vector<std::byte> data;

    // コンストラクタ/デストラクタ/ムーブを呼ぶための関数ポインタ
    using MoveFunc    = void(*)(void* dst, void* src);   // dst へ src をムーブ
    using DestroyFunc = void(*)(void* ptr);              // デストラクタ呼び出し

    MoveFunc    moveFn    = nullptr;
    DestroyFunc destroyFn = nullptr;

    ComponentStorage() = default;
    ComponentStorage(std::size_t elemSize, MoveFunc mv, DestroyFunc dstr)
        : elementSize(elemSize), moveFn(mv), destroyFn(dstr) {}

    std::size_t Size() const noexcept {
        return elementSize ? data.size() / elementSize : 0;
    }

    void* At(std::size_t row) noexcept {
        return data.data() + row * elementSize;
    }
    const void* At(std::size_t row) const noexcept {
        return data.data() + row * elementSize;
    }

    // 末尾に未初期化領域を1要素確保して返す
    void* PushUninitialized() {
        data.resize(data.size() + elementSize);
        return data.data() + data.size() - elementSize;
    }

    // row を末尾要素と swap して削除（O(1)）
    // 戻り値: 末尾にあったエンティティの旧インデックス（呼び出し元がEntityMap更新に使う）
    void SwapRemove(std::size_t row) {
        const std::size_t last = Size() - 1;
        if (row != last) {
            moveFn(At(row), At(last));
        }
        destroyFn(At(last));
        data.resize(data.size() - elementSize);
    }

    // src_storage の src_row から dst_storage の末尾へムーブ
    static void MoveElement(ComponentStorage& dst, ComponentStorage& src, std::size_t srcRow) {
        assert(dst.elementSize == src.elementSize);
        void* d = dst.PushUninitialized();
        src.moveFn(d, src.At(srcRow));
    }
};

// ---- Archetype --------------------------------------------------------------
// 同一 ComponentMask を持つエンティティ群の SoA ストレージ。
// 各カラムが ComponentStorage 1本で、行インデックスが揃っている。
// ----------------------------------------------------------------------------
class Archetype {
public:
    struct ColumnInfo {
        ComponentID       id;
        ComponentStorage  storage;
    };

    explicit Archetype(ComponentMask mask) : m_mask(mask) {}

    // コンポーネント列を登録（World が Archetype 生成時に呼ぶ）
    void RegisterColumn(ComponentID id, std::size_t elemSize,
                         ComponentStorage::MoveFunc mv,
                         ComponentStorage::DestroyFunc dstr) {
        assert(m_colIndex.find(id) == m_colIndex.end());
        m_colIndex[id] = m_columns.size();
        m_columns.push_back({ id, ComponentStorage(elemSize, mv, dstr) });
    }

    // エンティティを末尾行として追加
    // 戻り値: 行インデックス
    std::size_t AddEntity(EntityID eid) {
        m_entities.push_back(eid);
        return m_entities.size() - 1;
    }

    // 特定行のコンポーネントへの生ポインタを返す
    void* GetComponent(std::size_t row, ComponentID id) {
        auto it = m_colIndex.find(id);
        assert(it != m_colIndex.end());
        return m_columns[it->second].storage.At(row);
    }
    const void* GetComponent(std::size_t row, ComponentID id) const {
        auto it = m_colIndex.find(id);
        assert(it != m_colIndex.end());
        return m_columns[it->second].storage.At(row);
    }

    // 型付きアクセサ
    template <typename T>
    T& Get(std::size_t row) {
        return *static_cast<T*>(GetComponent(row, ComponentType<T>::ID()));
    }
    template <typename T>
    const T& Get(std::size_t row) const {
        return *static_cast<const T*>(GetComponent(row, ComponentType<T>::ID()));
    }

    // row を swap_remove で削除。末尾にいたエンティティの EntityID を返す
    // (末尾 == row のときは EntityID::null() を返す)
    EntityID RemoveEntity(std::size_t row) {
        const std::size_t last = m_entities.size() - 1;
        EntityID moved = (row != last) ? m_entities[last] : EntityID::Null();

        for (auto& col : m_columns) {
            col.storage.SwapRemove(row);
        }
        if (row != last) {
            m_entities[row] = m_entities[last];
        }
        m_entities.pop_back();
        return moved;
    }

    // 行数
    std::size_t Size()            const noexcept { return m_entities.size(); }
    bool        Empty()           const noexcept { return m_entities.empty(); }
    const ComponentMask& Mask()   const noexcept { return m_mask; }
    EntityID EntityAt(std::size_t row) const noexcept { return m_entities[row]; }
    const std::vector<EntityID>& Entities() const noexcept { return m_entities; }
    bool HasColumn(ComponentID id) const noexcept {
        return m_colIndex.count(id) > 0;
    }

    // 全カラムへのイテレーション
    const std::vector<ColumnInfo>& Columns() const noexcept { return m_columns; }
private:
    ComponentMask m_mask;
    std::vector<EntityID>   m_entities;
    std::vector<ColumnInfo> m_columns;
    std::unordered_map<ComponentID, std::size_t> m_colIndex;
};

} // namespace ECS
