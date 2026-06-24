//--------------------------------------------------------------------------------------
// File: World.h
//
// ECS の中心クラス World の定義
//
// Date: 2026.4.20
// Author: Ryosuke Koike
//--------------------------------------------------------------------------------------
#pragma once
#include "Entity.h"
#include "ComponentType.h"
#include "Archetype.h"
#include "Query.h"
#include "EventQueue.h"
#include "../Physics/PhysicsWorld.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <new>

namespace ECS
{

    // ---- World ------------------------------------------------------------------
    // ECS の中心クラス。
    //   * エンティティの生成 / 破棄
    //   * コンポーネントの追加 / 削除 / 取得
    //   * Archetype 間のエンティティ移動
    //   * Query によるアーキタイプ列挙
    // ----------------------------------------------------------------------------
    class World
    {
    public:
        // ---- エンティティ -------------------------------------------------------

        // エンティティ生成関数
        EntityID Create()
        {
            uint32_t index;
            if (!m_freeList.empty())
            {
                index = m_freeList.back();
                m_freeList.pop_back();
            }
            else
            {
                index = static_cast<uint32_t>(m_records.size());
                m_records.push_back({});
                m_generations.push_back(0);
            }
            const uint8_t gen = m_generations[index];
            EntityID eid = EntityID::Make(index, gen);

            Archetype* emptyArch = GetOrCreateArchetype(ComponentMask{});
            std::size_t row = emptyArch->AddEntity(eid);
            m_records[index] = {emptyArch, row};
            return eid;
        }

        // エンティティ破棄関数
        void RequestDestroy(EntityID eid)
        {
            if (!IsAlive(eid))
                return;
            
            m_destroyQueue.push_back(eid);
        }

        void DestroyEntitiesProcess()
        {
            for (auto id : m_destroyQueue)
            {
                if (!IsAlive(id))
                    continue;
                const uint32_t idx = id.Index();
                EntityRecord& rec = m_records[idx];

                EntityID moved = rec.m_archetype->RemoveEntity(rec.row);
                if (!moved.IsNull())
                {
                    m_records[moved.Index()].row = rec.row;
                }
                rec = {};

                ++m_generations[idx];
                m_freeList.push_back(idx);
            }

            m_destroyQueue.clear();
        }

        // 渡されたEntityID のエンティティが存在しているか
        bool IsAlive(EntityID eid) const noexcept
        {
            const uint32_t idx = eid.Index();
            if (idx >= m_generations.size())
                return false;
            return m_generations[idx] == eid.Generation();
        }

        // ---- コンポーネント -----------------------------------------------------

        template <typename T> T& AddComponent(EntityID eid)
        {
            assert(IsAlive(eid));
            assert(!HasComponent<T>(eid) && "Component already exists");

            const ComponentID cid = ComponentType<T>::ID();
            EntityRecord& rec = m_records[eid.Index()];

            ComponentMask newMask = rec.m_archetype->Mask();
            newMask.set(cid);

            Archetype* newArch = GetOrCreateArchetype(newMask);
            EnsureColumn<T>(*newArch);

            // MigrateEntity の前に新コンポーネントのカラムに
            // 未初期化領域を確保しておく（MigrateEntity内のAddEntityと行数を合わせる）
            std::size_t newRow = MigrateEntity(eid, rec, newArch);

            // newRow は MigrateEntity 内の AddEntity が確保した行
            // 新コンポーネントの列だけはムーブされていないので、
            // その行のスロットに placement new する
            ComponentStorage* newCol = GetColumnStorage(*newArch, cid);
            assert(newCol);
           
            //void* slot = newCol->PushUninitialized();

            void* slot = newCol->At(newRow);
            return *new (slot) T{};
        }

        template <typename T> void RemoveComponent(EntityID eid)
        {
            assert(IsAlive(eid));
            assert(HasComponent<T>(eid) && "Component does not exist");

            const ComponentID cid = ComponentType<T>::ID();
            EntityRecord& rec = m_records[eid.Index()];

            ComponentMask newMask = rec.m_archetype->Mask();
            newMask.reset(cid);

            Archetype* newArch = GetOrCreateArchetype(newMask);
            MigrateEntity(eid, rec, newArch);
        }

        template <typename T> bool HasComponent(EntityID eid) const noexcept
        {
            assert(IsAlive(eid));
            return m_records[eid.Index()].m_archetype->Mask().test(ComponentType<T>::ID());
        }

        template <typename T> T& GetComponent(EntityID eid)
        {
            assert(IsAlive(eid));
            assert(HasComponent<T>(eid));
            const EntityRecord& rec = m_records[eid.Index()];
            return rec.m_archetype->Get<T>(rec.row);
        }

        template <typename T> const T& GetComponent(EntityID eid) const
        {
            assert(IsAlive(eid));
            assert(HasComponent<T>(eid));
            const EntityRecord& rec = m_records[eid.Index()];
            return rec.m_archetype->Get<T>(rec.row);
        }

        // ---- Query --------------------------------------------------------------

        QueryResult Query(const QueryDesc& desc)
        {
            std::vector<Archetype*> result;
            for (auto& [mask, archPtr] : m_archetypes)
            {
                if (desc.Matches(archPtr->Mask()))
                {
                    result.push_back(archPtr.get());
                }
            }
            return QueryResult(std::move(result));
        }

        QueryResult Query(const QueryBuilder& builder)
        {
            return Query(builder.Build());
        }

        // ---- 汎用イベントキュー -------------------------------------------------
        // 任意の型のイベントを Push できる。World は具体的なイベント型を知らない。
        //
        // 積む側（CollisionSystem など）:
        //   world.GetEventQueue().Push(CollisionResult{ ... });
        //   world.GetEventQueue().Push(AnimationEndEvent{ eid });
        //
        // 読む側（PlayerCollisionSystem など）:
        //   world.GetEventQueue().ForEach<CollisionResult>(
        //       [](const CollisionResult& r) { ... });
        // フレーム先頭で ClearEvents() を呼ぶこと。
        EventQueue& GetEventQueue() noexcept
        {
            return m_eventQueue;
        }
        const EventQueue& GetEventQueue() const noexcept
        {
            return m_eventQueue;
        }
        void ClearOldFrameEvents() noexcept
        {
            m_eventQueue.Clear();
        }
        void FlushEvents() noexcept
        {
            m_eventQueue.Flush();
        }

        // ---- 物理空間API -------------------------------------------------------
        PhysicsWorld& GetPhysicsWorld() noexcept
        {
            return m_physicsWorld;
        }

        const PhysicsWorld& GetPhysicsWorld() const noexcept
        {
            return m_physicsWorld;
        }

        // ---- 統計 ---------------------------------------------------------------
        std::size_t EntityCount() const noexcept
        {
            return m_records.size() - m_freeList.size();
        }
        std::size_t ArchetypeCount() const noexcept
        {
            return m_archetypes.size();
        }

    private:
        struct EntityRecord
        {
            Archetype* m_archetype = nullptr;
            std::size_t row = 0;
        };

        struct ComponentMaskHash
        {
            std::size_t operator()(const ComponentMask& mask) const noexcept
            {
                return std::hash<uint64_t>{}(mask.to_ullong());
            }
        };

        // ArchetypeMap の定義を変更
        using ArchetypeMap = std::unordered_map<
            ComponentMask,
            std::unique_ptr<Archetype>,
            ComponentMaskHash
        > ;

        std::vector<EntityRecord>   m_records;
        std::vector<uint8_t>        m_generations;
        std::vector<uint32_t>       m_freeList;
        std::vector<EntityID>       m_destroyQueue;

        ArchetypeMap                m_archetypes;

        EventQueue m_eventQueue;
        PhysicsWorld m_physicsWorld;

        // ---- 内部ヘルパー -------------------------------------------------------

        static std::string MaskKey(const ComponentMask& mask)
        {
            return mask.to_string();
        }

        Archetype* GetOrCreateArchetype(const ComponentMask& mask)
        {
            auto it = m_archetypes.find(mask);
            if (it != m_archetypes.end())
                return it->second.get();

            auto arch = std::make_unique<Archetype>(mask);
            Archetype* ptr = arch.get();
            m_archetypes.emplace(mask, std::move(arch));
            return ptr;
        }

        template <typename T> void EnsureColumn(Archetype& arch)
        {
            const ComponentID cid = ComponentType<T>::ID();
            if (arch.HasColumn(cid))
                return;

            arch.RegisterColumn(
                cid, sizeof(T),
                [](void* dst, void* src)
                {
                    new (dst) T(std::move(*static_cast<T*>(src)));
                    static_cast<T*>(src)->~T();
                },
                [](void* ptr) { static_cast<T*>(ptr)->~T(); });
        }

        // src が持つ全カラムのメタ情報を dst へ転写する（未登録カラムのみ）。
        // MigrateEntity の前に呼ぶことで、新 Archetype に共通カラムが必ず存在することを保証する。
        static void EnsureColumnsFrom(Archetype& dst, const Archetype& src)
        {
            for (const auto& col : src.Columns())
            {
                if (dst.HasColumn(col.id))
                    continue;
                dst.RegisterColumn(col.id, col.storage.elementSize, col.storage.moveFn, col.storage.destroyFn);
            }
        }

        // eid を現 Archetype から newArch へ移動する。
        // 共通カラムはムーブ、旧のみカラムは SwapRemove で破棄、
        // 新規カラムは呼び出し元（AddComponent）が placement new で初期化する。
        std::size_t MigrateEntity(EntityID eid, EntityRecord& rec, Archetype* newArch)
        {
            Archetype* oldArch = rec.m_archetype;
            const std::size_t oldRow = rec.row;

            // 新 Archetype に旧 Archetype の共通カラムを事前登録
            EnsureColumnsFrom(*newArch, *oldArch);

            // 新 Archetype に行を確保
            std::size_t newRow = newArch->AddEntity(eid);

            // 共通コンポーネントをムーブ
            const ComponentMask common = oldArch->Mask() & newArch->Mask();
            for (std::size_t i = 0; i < MAX_COMPONENTS; ++i)
            {
                if (!common.test(i))
                    continue;
                auto cid = static_cast<ComponentID>(i);

                ComponentStorage* dstCol = GetColumnStorage(*newArch, cid);
                ComponentStorage* srcCol = GetColumnStorage(*oldArch, cid);
                assert(dstCol && srcCol);

                ComponentStorage::MoveElement(*dstCol, *srcCol, oldRow, newRow);
            }

            // 旧 Archetype から削除（SwapRemove）
            EntityID moved = oldArch->RemoveEntity(oldRow);
            if (!moved.IsNull())
            {
                m_records[moved.Index()].row = oldRow;
            }

            // レコード更新
            rec.m_archetype = newArch;
            rec.row = newRow;
            return newRow;
        }

        static ComponentStorage* GetColumnStorage(Archetype& arch, ComponentID cid)
        {
            for (auto& col : const_cast<std::vector<Archetype::ColumnInfo>&>(arch.Columns()))
            {
                if (col.id == cid)
                    return &col.storage;
            }
            return nullptr;
        }
    };

} // namespace ECS