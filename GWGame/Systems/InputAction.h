#pragma once
#include <cstdint>

namespace Input
{

    // ---- InputAction ------------------------------------------------------------
    // ゲームプレイ上の抽象アクション。
    // デバイスやキー名を System 側に露出しない。
    // 追加するときはここに値を足して Bindings に登録するだけでよい。
    // ----------------------------------------------------------------------------
    enum class InputAction : uint32_t
    {
        // ---- 移動 ---------------------------------------------------------------
        MoveX,
        MoveY,
        Jump,
        Sprint,

        // ---- 戦闘 ---------------------------------------------------------------
        Attack,
        AltAttack,
        Interact,

        // ---- UI -----------------------------------------------------------------
        Pause,
        Confirm,
        Cancel,

        // ---- カメラ -------------------------------------------------------------
        LookX,
        LookY,

        Count // 番兵（配列サイズに使う）
    };

    static constexpr int ACTION_COUNT = static_cast<int>(InputAction::Count);

    // ---- InputActionState -------------------------------------------------------
    // 1 アクションの状態。毎フレーム InputSystem が更新する。
    // ----------------------------------------------------------------------------
    struct InputActionState
    {
        bool pressed = false;  // このフレームに押した瞬間
        bool held = false;     // 押し続けている
        bool released = false; // このフレームに離した瞬間
        float value = 0.f;     // アナログ値（スティック・マウス移動量など）

        bool IsActive() const noexcept
        {
            return pressed || held;
        }
    };

} // namespace Input