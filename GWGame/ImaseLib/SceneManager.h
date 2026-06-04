//--------------------------------------------------------------------------------------
// File: SceneManager.h
//
// シーンを管理するクラス
//
// Date: 2026.3.20
// Author: Suzuki Ryutaro
//--------------------------------------------------------------------------------------
#pragma once
#include <vector>
#include <memory>
#include <optional>
#include <cassert>
#include <functional>
#include <unordered_map>

namespace Imase
{
    // シーンマネージャーへのインターフェイス
    template <class TSceneKey> struct ISceneController
    {
        // デストラクタに=defaultを付ける理由
        // ムーブコンストラクタやコピーコンストラクタを明示的に定義した際、
        // デストラクタが暗黙的に生成されなくなる（または動作が変わる）のを防ぐ
        virtual ~ISceneController() = default;

        // シーン切り替え関数
        virtual void RequestSwitch(const TSceneKey& key) = 0;

        // シーンのプッシュ関数
        virtual void RequestPush(const TSceneKey& key) = 0;

        // シーンのポップ関数
        virtual void RequestPop() = 0;
    };

    // シーンの基底クラス
    template <class TSceneKey, class TContext> struct SceneBase
    {
        virtual ~SceneBase() = default;

        // 更新
        virtual void Update(ISceneController<TSceneKey>& sceneController, TContext& context) = 0;
        // 描画
        virtual void Render(TContext& gameContext) = 0;

        // 自シーンへ移行時に呼び出される関数
        virtual void OnEnter(TContext&)
        {
        }
        // 他シーンへ移行時に呼び出さる関数
        virtual void OnExit(TContext&)
        {
        }
        // 待機状態へ移行時に呼び出さる関数（Pushされた場合）
        virtual void OnSuspend(TContext&)
        {
        }
        // 待機状態から復帰時に呼び出さる関数（Popされた場合）
        virtual void OnResume(TContext&)
        {
        }

        // DirectXのデバイスロスト時に呼び出される関数
        virtual void OnDeviceLost(TContext&)
        {
        }
        // DirectXのデバイス復帰時に呼び出される関数
        virtual void OnDeviceRestored(TContext&)
        {
        }
        // ウインドウサイズ変更時に呼び出される関数
        virtual void OnWindowSizeChanged(TContext&)
        {
        }
    };

    template <class TSceneKey, class TContext> using ScenePtr = std::unique_ptr<SceneBase<TSceneKey, TContext>>;

    // シーンマネージャークラス
    template <class TSceneKey, class TContext> class SceneManager final : public ISceneController<TSceneKey>
    {
        // SceneFactoryは、対応するシーンをunique_ptrで返す関数
        using SceneFactory = std::function<ScenePtr<TSceneKey, TContext>()>;

        // シーンマネージャーへの命令
        enum class SceneCommand
        {
            None,   // なし
            Switch, // シーンの切り替え
            Push,   // シーンのプッシュ
            Pop     // シーンのポップ
        };

        // シーン生成テーブル
        std::unordered_map<TSceneKey, SceneFactory> m_sceneFactories;

        // 作成されたシーンのスタック
        std::vector<ScenePtr<TSceneKey, TContext>> m_sceneStack;

        // シーンマネージャーへの次の命令
        SceneCommand m_nextCommand = SceneCommand::None;

        // 次のシーンID
        std::optional<TSceneKey> m_nextSceneKey = std::nullopt;

    public:
        // コンストラクタ
        SceneManager() = default;

        // シーンの登録
        template <class TScene>
            requires std::derived_from<TScene, SceneBase<TSceneKey, TContext>> // <- SceneBaseを継承したクラスか？
        void RegisterScene(const TSceneKey& key)
        {
            auto [it, inserted] = m_sceneFactories.emplace(key, []() { return std::make_unique<TScene>(); });
            assert(inserted && "同じシーンキーが既に登録されています");
        }

        // 更新
        void Update(TContext& context)
        {
            assert(!m_sceneStack.empty() && "シーンスタックが空です");
            if (m_sceneStack.empty())
                return;

            // *this を ISceneController<TSceneKey>& にキャストして渡す
            auto& controller = static_cast<ISceneController<TSceneKey>&>(*this);
            m_sceneStack.back()->Update(controller, context);

            ApplyRequest(context);
        }

        // 描画
        void Render(TContext& context)
        {
            assert(!m_sceneStack.empty() && "シーンスタックが空です");
            if (m_sceneStack.empty())
                return;

            m_sceneStack.back()->Render(context);
        }

        // 起動シーンの設定
        void SetFirstScene(const TSceneKey& key, TContext& context)
        {
            assert(m_sceneStack.empty() && "SetFirstScene() は1回だけ呼び出すことができます");

            auto scene = CreateScene(key);
            assert(scene && "ファクトリがnullptrを返却しました");

            m_sceneStack.push_back(std::move(scene));
            m_sceneStack.back()->OnEnter(context);
        }

        // シーンの切り替え
        void RequestSwitch(const TSceneKey& key) override
        {
            ValidateRequest();
            m_nextCommand = SceneCommand::Switch;
            m_nextSceneKey = key;
        }

        // シーンのプッシュ
        void RequestPush(const TSceneKey& key) override
        {
            ValidateRequest();
            m_nextCommand = SceneCommand::Push;
            m_nextSceneKey = key;
        }

        // シーンのポップ
        void RequestPop() override
        {
            ValidateRequest();
            m_nextCommand = SceneCommand::Pop;
        }

        // DirectXのデバイスロスト時に呼び出される関数
        void OnDeviceLost(TContext& context)
        {
            for (auto& scene : m_sceneStack)
            {
                scene->OnDeviceLost(context);
            }
        }

        // DirectXのデバイス復帰時にに呼び出される関数
        void OnDeviceRestored(TContext& context)
        {
            for (auto& scene : m_sceneStack)
            {
                scene->OnDeviceRestored(context);
            }
        }

        // ウインドウサイズ変更時に呼び出される関数
        void OnWindowSizeChanged(TContext& context)
        {
            for (auto& scene : m_sceneStack)
            {
                scene->OnWindowSizeChanged(context);
            }
        }

    private:
        // シーンの生成関数
        ScenePtr<TSceneKey, TContext> CreateScene(const TSceneKey& key)
        {
            auto it = m_sceneFactories.find(key);
            if (it == m_sceneFactories.end())
            {
                assert(false && "未登録のシーンキーです");
                return nullptr;
            }
            return it->second();
        }

        // 同一フレーム内で遷移リクエストは１回だけなのでチェックする関数
        void ValidateRequest() const
        {
            assert(m_nextCommand == SceneCommand::None && "同一フレーム内での複数回の遷移リクエストは禁止されています");
        }

        // リクエストを適応する関数
        void ApplyRequest(TContext& context)
        {
            switch (m_nextCommand)
            {
                case SceneCommand::None: // なし
                    break;
                case SceneCommand::Switch: // シーンの切り替え
                    ApplySwitch(context);
                    break;
                case SceneCommand::Push: // シーンのプッシュ
                    ApplyPush(context);
                    break;
                case SceneCommand::Pop: // シーンのポップ
                    ApplyPop(context);
                    break;
            }

            m_nextCommand = SceneCommand::None;
            m_nextSceneKey = std::nullopt;
        }

        // シーンの切り替え処理
        void ApplySwitch(TContext& context)
        {
            assert(m_nextSceneKey.has_value() && "シーンキーが設定されていません");

            auto nextScene = CreateScene(*m_nextSceneKey);
            assert(nextScene && "ファクトリがnullptrを返却しました");
            m_sceneStack.back()->OnExit(context);
            m_sceneStack.back() = std::move(nextScene);
            m_sceneStack.back()->OnEnter(context);
        }

        // シーンのプッシュ処理
        void ApplyPush(TContext& context)
        {
            assert(m_nextSceneKey.has_value() && "シーンキーが設定されていません");

            auto nextScene = CreateScene(*m_nextSceneKey);
            assert(nextScene && "ファクトリがnullptrを返却しました");
            m_sceneStack.back()->OnSuspend(context);
            m_sceneStack.push_back(std::move(nextScene));
            m_sceneStack.back()->OnEnter(context);
        }

        // シーンのポップ処理
        void ApplyPop(TContext& context)
        {
            assert(m_sceneStack.size() > 1 && "シーンスタックを空にすることはできません");

            m_sceneStack.back()->OnExit(context);
            m_sceneStack.pop_back();
            m_sceneStack.back()->OnResume(context);
        }
    };
}
