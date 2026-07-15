#pragma once
#include <functional>
#include <algorithm>

// ---- FixedUpdateRunner ------------------------------------------------------
// Unity の FixedUpdate と同等の固定タイムステップ実行を管理する。
//
// 使い方:
//   FixedUpdateRunner runner;
//   runner.SetFixedDeltaTime(1.f / 60.f);  // 60Hz（デフォルト）
//
//   // Game::Update() 内
//   runner.Tick(dt, [&](float fixedDt) {
//       m_collisionSystem.Update(m_world, fixedDt);
//       m_physicsSystem.ResolveCollisions(m_world, fixedDt);
//       m_physicsSystem.IntegrateVelocity(m_world, fixedDt);
//       m_physicsSystem.IntegrateTransform(m_world, fixedDt);
//   });
//
// ----------------------------------------------------------------------------
class FixedUpdateRunner
{
public:
    // fixedDeltaTime: 固定タイムステップ（秒）デフォルト 1/60
    // maxSteps     : 1 フレームあたりの最大実行回数（スパイラルオブデス防止）
    explicit FixedUpdateRunner(float fixedDeltaTime = 1.f / 60.f, int maxSteps = 5)
        : m_fixedDt(fixedDeltaTime), m_maxSteps(maxSteps)
    {
    }

    void SetFixedDeltaTime(float dt) noexcept
    {
        m_fixedDt = dt;
    }
    void SetMaxSteps(int steps) noexcept
    {
        m_maxSteps = steps;
    }

    float GetFixedDeltaTime() const noexcept
    {
        return m_fixedDt;
    }

    // alpha: 前回の FixedUpdate からの経過割合 [0, 1]
    // レンダリング時に position の補間に使う
    //   renderPos = prevPos * (1 - alpha) + currentPos * alpha
    float GetAlpha() const noexcept
    {
        return m_alpha;
    }

    // ---- Tick ---------------------------------------------------------------
    // Game::Update(dt) から毎フレーム呼ぶ。
    // callback(fixedDt) が物理・衝突の固定ステップ処理。
    template <typename Fn> void Tick(float dt, Fn&& callback)
    {
        m_stepCount = 0;

        m_accumulator += dt;


        // 1 フレームあたりの最大ステップ数を超えないようにクランプ
        // （重い処理でフレームが落ちたとき accumulator が無限に増えるのを防ぐ）
        const float maxAccum = m_fixedDt * static_cast<float>(m_maxSteps);
        if (m_accumulator > maxAccum)
            m_accumulator = maxAccum;

        while (m_accumulator >= m_fixedDt)
        {
            callback(m_fixedDt);
            m_accumulator -= m_fixedDt;
            m_stepCount++;
        }

        // レンダリング補間係数（今フレームの残り割合）
        m_alpha = m_accumulator / m_fixedDt;
    }

    // 前フレームのステップ数（デバッグ用）
    int GetLastStepCount() const noexcept
    {
        return m_stepCount;
    }
    void ResetStepCount() noexcept
    {
        m_stepCount = 0;
    }

private:
    float m_fixedDt = 1.f / 60.f;
    int m_maxSteps = 5;
    float m_accumulator = 0.f;
    float m_alpha = 0.f;
    int m_stepCount = 0;
};