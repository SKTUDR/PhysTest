#pragma once
#include "InputAction.h"
#include <Keyboard.h>
#include <Mouse.h>
#include <GamePad.h>
#include <array>
#include <variant>
#include <functional>

namespace Input
{

    // ---- デバイス入力の種別 -----------------------------------------------------

    // キーボードキー
    struct KeyboardBinding
    {
        DirectX::Keyboard::Keys key;
        float scale = 1.0f;
    };

    // マウスボタン
    struct MouseButtonBinding
    {
        enum class Button
        {
            Left,
            Right,
            Middle,
            X1,
            X2
        } button;
    };

    // マウス軸（移動量）
    struct MouseAxisBinding
    {
        enum class Axis
        {
            X,
            Y,
            ScrollWheel
        } axis;
        float scale = 1.f; // 正負・スケール
    };

    // ゲームパッドボタン
    struct GamePadButtonBinding
    {
        enum class Button
        {
            A,
            B,
            X,
            Y,
            LeftShoulder,
            RightShoulder,
            LeftTrigger,
            RightTrigger,
            DPadUp,
            DPadDown,
            DPadLeft,
            DPadRight,
            Start,
            Back,
            LeftStick,
            RightStick
        } button;
        int playerIndex = 0;
    };

    // ゲームパッドスティック軸
    struct GamePadAxisBinding
    {
        enum class Axis
        {
            LeftStickX,
            LeftStickY,
            RightStickX,
            RightStickY,
            LeftTrigger,
            RightTrigger
        } axis;
        float scale = 1.f;
        float deadZone = 0.1f;
        int playerIndex = 0;
    };

    // ---- InputBinding -----------------------------------------------------------
    // 1 つのアクションに対応するデバイス入力。
    // 複数バインドを持てる（例: Jump = Space or GamePad A）。
    // ----------------------------------------------------------------------------
    using BindingVariant =
        std::variant<KeyboardBinding, MouseButtonBinding, MouseAxisBinding, GamePadButtonBinding, GamePadAxisBinding>;

    struct InputBinding
    {
        InputAction action;
        BindingVariant binding;
    };

    // ---- DefaultBindings --------------------------------------------------------
    // コードで固定のデフォルトバインド一覧。
    // 変更する場合はここを編集する。
    // ----------------------------------------------------------------------------
    inline std::array<InputBinding, 29> MakeDefaultBindings()
    {
        using KB = KeyboardBinding;
        using MB = MouseButtonBinding;
        using MA = MouseAxisBinding;
        using GPB = GamePadButtonBinding;
        using GPA = GamePadAxisBinding;

        return {{
            // ---- 移動（キーボード） ----
            {InputAction::MoveY, KB{DirectX::Keyboard::W, -1.0f}},
            {InputAction::MoveY, KB{DirectX::Keyboard::S,  1.0f}},
            {InputAction::MoveX, KB{DirectX::Keyboard::A,  1.0f}},
            {InputAction::MoveX, KB{DirectX::Keyboard::D, -1.0f}},
            {InputAction::Jump, KB{DirectX::Keyboard::Space}},
            {InputAction::Sprint, KB{DirectX::Keyboard::LeftShift}},

            // ---- 移動（ゲームパッド） ----
            {InputAction::MoveY, GPA{GPA::Axis::LeftStickY,  1.f, 0.1f}},
            {InputAction::MoveY, GPA{GPA::Axis::LeftStickY, -1.f, 0.1f}},
            {InputAction::MoveX, GPA{GPA::Axis::LeftStickX, -1.f, 0.1f}},
            {InputAction::MoveX, GPA{GPA::Axis::LeftStickX,  1.f, 0.1f}},
            {InputAction::Jump, GPB{GPB::Button::A}},
            {InputAction::Sprint, GPB{GPB::Button::LeftStick}},

            // ---- 戦闘 ----
            {InputAction::Attack, MB{MB::Button::Left}},
            {InputAction::AltAttack, MB{MB::Button::Right}},
            {InputAction::Interact, KB{DirectX::Keyboard::E}},
            {InputAction::Attack, GPB{GPB::Button::RightTrigger}},
            {InputAction::AltAttack, GPB{GPB::Button::RightShoulder}},
            {InputAction::Interact, GPB{GPB::Button::X}},

            // ---- UI ----
            {InputAction::Pause, KB{DirectX::Keyboard::Escape}},
            {InputAction::Confirm, KB{DirectX::Keyboard::Enter}},
            {InputAction::Cancel, KB{DirectX::Keyboard::Back}},
            {InputAction::Pause, GPB{GPB::Button::Start}},
            {InputAction::Confirm, GPB{GPB::Button::A}},
            {InputAction::Cancel, GPB{GPB::Button::B}},

            // ---- カメラ（マウス軸） ----
            {InputAction::LookX, MA{MA::Axis::X,  1.f}},
            {InputAction::LookY, MA{MA::Axis::Y,  1.f}},

            // ---- カメラ（ゲームパッド右スティック） ----
            {InputAction::LookX, GPA{GPA::Axis::RightStickX, 1.f, 0.1f}},
            // ↑ 配列サイズに合わせる
        }};
    }

} // namespace Input