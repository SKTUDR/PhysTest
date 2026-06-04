#pragma once
#include <any>
#include <vector>
#include <typeindex>
#include <functional>
#include <algorithm>

namespace ECS
{

    // ---- Event ------------------------------------------------------------------
    // 任意の型を std::any でラップした単一イベント。
    // typeIndex で型を識別し、型安全な取り出しを可能にする。
    // ----------------------------------------------------------------------------
    struct Event
    {
        std::type_index typeIndex; // イベントの型識別子
        std::any payload;          // 実際のイベントデータ

        template <typename T>
        explicit Event(T&& data) : typeIndex(typeid(std::decay_t<T>)), payload(std::forward<T>(data))
        {
        }

        // 型付きで取り出す（型が違う場合は nullptr を返す）
        template <typename T> const T* Get() const noexcept
        {
            if (typeIndex != std::type_index(typeid(T)))
                return nullptr;
            return std::any_cast<T>(&payload);
        }

        template <typename T> bool Is() const noexcept
        {
            return typeIndex == std::type_index(typeid(T));
        }
    };

    // ---- EventQueue -------------------------------------------------------------
    // World が持つ汎用イベントキュー。
    // コリジョン・アニメーション完了・ゲームイベントなど
    // 任意の型のイベントを1つのキューで管理できる。
    //
    // 使い方:
    //   // 積む側（System）
    //   world.GetEventQueue().Push(CollisionResult{ ... });
    //   world.GetEventQueue().Push(AnimationEndEvent{ eid, "walk" });
    //
    //   // 読む側（System）
    //   world.GetEventQueue().ForEach<CollisionResult>(
    //       [](const CollisionResult& r) { ... });
    // ----------------------------------------------------------------------------
    class EventQueue
    {
    public:
        // ---- イベントを積む -----------------------------------------------------
        template <typename T> void Push(T&& eventData)
        {
            m_events.emplace_back(std::forward<T>(eventData));
        }

        // ---- 特定型のイベントだけ走査 -------------------------------------------
        template <typename T, typename Fn> void ForEach(Fn&& fn) const
        {
            const std::type_index target(typeid(T));
            for (const Event& e : m_events)
            {
                if (e.typeIndex != target)
                    continue;
                if (const T* data = e.Get<T>())
                {
                    fn(*data);
                }
            }
        }

        // ---- 全イベントを走査（型を自分で判定する場合）-------------------------
        template <typename Fn> void ForEachRaw(Fn&& fn) const
        {
            for (const Event& e : m_events)
            {
                fn(e);
            }
        }

        // ---- フレーム先頭でクリア -----------------------------------------------
        void Clear() noexcept
        {
            m_events.clear();
        }

        bool Empty() const noexcept
        {
            return m_events.empty();
        }
        std::size_t Size() const noexcept
        {
            return m_events.size();
        }

    private:
        std::vector<Event> m_events;
    };

} // namespace ECS