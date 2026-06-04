#pragma once

#include <any>
#include <functional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <algorithm>

// ============================================================
//  EventBus – スレッドセーフ・型安全・RAII サブスクリプション
// ============================================================

class EventBus
{
public:
    // --------------------------------------------------------
    //  型定義
    // --------------------------------------------------------
    using Callback = std::function<void(const std::any&)>;
    using SubscriptionId = uint64_t;
    using ErrorHandler = std::function<void(const std::string& event, const std::exception&)>;

    // --------------------------------------------------------
    //  RAII スコープ付きサブスクリプション
    //
    //  スコープを外れると自動で Unsubscribe される。
    //  コピー不可、ムーブのみ可。
    // --------------------------------------------------------
    class Subscription
    {
    public:
        Subscription() = default;

        Subscription(EventBus* bus, std::string event, SubscriptionId id) : bus_(bus), event_(std::move(event)), id_(id)
        {
        }

        // コピー禁止
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        // ムーブ可
        Subscription(Subscription&& other) noexcept : bus_(other.bus_), event_(std::move(other.event_)), id_(other.id_)
        {
            other.bus_ = nullptr;
        }

        Subscription& operator=(Subscription&& other) noexcept
        {
            if (this != &other)
            {
                release();
                bus_ = other.bus_;
                event_ = std::move(other.event_);
                id_ = other.id_;
                other.bus_ = nullptr;
            }
            return *this;
        }

        ~Subscription()
        {
            release();
        }

        // 明示的に解除したい場合
        void Unsubscribe()
        {
            release();
            bus_ = nullptr;
        }

        SubscriptionId Id() const
        {
            return id_;
        }

    private:
        void release()
        {
            if (bus_)
                bus_->Unsubscribe(event_, id_);
        }

        EventBus* bus_ = nullptr;
        std::string event_;
        SubscriptionId id_ = 0;
    };

    // --------------------------------------------------------
    //  コンストラクタ / デストラクタ
    // --------------------------------------------------------
    EventBus() = default;
    ~EventBus() = default;

    // コピー・ムーブ禁止（mutex を持つため）
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    // --------------------------------------------------------
    //  エラーハンドラの設定
    //
    //  設定しない場合、コールバック内の例外は握り潰される。
    // --------------------------------------------------------
    void SetErrorHandler(ErrorHandler handler)
    {
        std::unique_lock lock(m_mutex);
        m_errorHandler = std::move(handler);
    }

    // --------------------------------------------------------
    //  Subscribe – コールバックを登録し SubscriptionId を返す
    // --------------------------------------------------------
    SubscriptionId Subscribe(const std::string& eventName, Callback cb)
    {
        const SubscriptionId id = m_nextId.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock lock(m_mutex);
        m_listeners[eventName].push_back({id, std::move(cb)});
        return id;
    }

    // --------------------------------------------------------
    //  SubscribeScoped – RAII Subscription を返す
    // --------------------------------------------------------
    Subscription SubscribeScoped(const std::string& eventName, Callback cb)
    {
        const SubscriptionId id = Subscribe(eventName, std::move(cb));
        return Subscription(this, eventName, id);
    }

    // --------------------------------------------------------
    //  Unsubscribe – SubscriptionId で特定のコールバックを解除
    // --------------------------------------------------------
    void Unsubscribe(const std::string& eventName, SubscriptionId id)
    {
        std::unique_lock lock(m_mutex);

        auto it = m_listeners.find(eventName);
        if (it == m_listeners.end())
            return;

        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const Entry& e) { return e.id == id; }), vec.end());

        // エントリが空になったらキーごと削除
        if (vec.empty())
            m_listeners.erase(it);
    }

    // --------------------------------------------------------
    //  Emit – イベントを発火する（オプションでデータを渡せる）
    // --------------------------------------------------------
    void Emit(const std::string& eventName, std::any data = {})
    {
        // イテレーション中の Subscribe/Unsubscribe に対応するため
        // コールバックリストをコピーしてからロックを解放する
        std::vector<Entry> snapshot;
        ErrorHandler errorHandler;

        {
            std::shared_lock lock(m_mutex);

            auto it = m_listeners.find(eventName);
            if (it == m_listeners.end())
                return;

            snapshot = it->second;        // コピー
            errorHandler = m_errorHandler; // コピー
        }

        for (const auto& entry : snapshot)
        {
            if (!entry.cb)
                continue;

            try
            {
                entry.cb(data);
            }
            catch (const std::exception& e)
            {
                if (errorHandler)
                    errorHandler(eventName, e);
                // errorHandler が未設定なら握り潰す（従来の挙動を維持）
            }
            catch (...)
            {
                // std::exception 以外は常に握り潰す
            }
        }
    }

    // --------------------------------------------------------
    //  ユーティリティ
    // --------------------------------------------------------

    // 特定イベントのリスナー数を返す
    std::size_t ListenerCount(const std::string& eventName) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_listeners.find(eventName);
        return (it != m_listeners.end()) ? it->second.size() : 0;
    }

    // 特定イベントのリスナーを全解除
    void Clear(const std::string& eventName)
    {
        std::unique_lock lock(m_mutex);
        m_listeners.erase(eventName);
    }

    // 全リスナーを解除
    void ClearAll()
    {
        std::unique_lock lock(m_mutex);
        m_listeners.clear();
    }

private:
    struct Entry
    {
        SubscriptionId id;
        Callback cb;
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::vector<Entry>> m_listeners;
    std::atomic<SubscriptionId> m_nextId{1};
    ErrorHandler m_errorHandler;
};

// ============================================================
//  使用例
// ============================================================
#ifdef EVENTBUS_EXAMPLE

#include <iostream>

int main()
{
    EventBus bus;

    // エラーハンドラの設定
    bus.SetErrorHandler([](const std::string& event, const std::exception& e)
                        { std::cerr << "[EventBus] Error in event '" << event << "': " << e.what() << "\n"; });

    // --- 1. 基本的な Subscribe / Emit ---
    auto id = bus.Subscribe("onScore",
                            [](const std::any& data)
                            {
                                int score = std::any_cast<int>(data);
                                std::cout << "Score updated: " << score << "\n";
                            });

    bus.Emit("onScore", 42); // => Score updated: 42

    // --- 2. Unsubscribe ---
    bus.Unsubscribe("onScore", id);
    bus.Emit("onScore", 99); // => (出力なし)

    // --- 3. RAII スコープ付きサブスクリプション ---
    {
        auto sub = bus.SubscribeScoped("onDamage",
                                       [](const std::any& data)
                                       {
                                           int dmg = std::any_cast<int>(data);
                                           std::cout << "Damage: " << dmg << "\n";
                                       });

        bus.Emit("onDamage", 10); // => Damage: 10
    } // ここで自動 Unsubscribe

    bus.Emit("onDamage", 20); // => (出力なし)

    // --- 4. 例外ハンドリング ---
    bus.Subscribe("onEvent", [](const std::any&) { throw std::runtime_error("something went wrong"); });
    bus.Subscribe("onEvent", [](const std::any&) { std::cout << "Second listener still runs\n"; });

    bus.Emit("onEvent");
    // => [EventBus] Error in event 'onEvent': something went wrong
    // => Second listener still runs

    return 0;
}

#endif // EVENTBUS_EXAMPLE