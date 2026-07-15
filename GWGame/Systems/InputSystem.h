#pragma once
#include "InputAction.h"
#include "InputBinding.h"
#include <Keyboard.h>
#include <Mouse.h>
#include <GamePad.h>
#include <vector>
#include <array>

namespace Input
{

    // ---- InputSystem ------------------------------------------------------------
    // 毎フレーム Update() を呼ぶと DirectXTK デバイスを読んで
    // InputAction ごとの状態を更新する。
    //
    // 各 System は GetAction() で状態を参照するだけでよく、
    // デバイス名・キー名を知る必要がない。
    //
    // 使い方:
    //   // Game::Initialize()
    //   m_inputSystem.AddBindings(Input::MakeDefaultBindings());
    //
    //   // Game::Update()
    //   m_inputSystem.Update(keyboard, mouse, gamePad);
    //
    //   // 各 System 内
    //   if (m_inputSystem.GetAction(InputAction::Jump).pressed) { ... }
    //   float lookX = m_inputSystem.GetAction(InputAction::LookRight).value;
    // ----------------------------------------------------------------------------
    class InputSystem
    {
    public:
        InputSystem()
        {
            m_bindings.reserve(64);
        }

        // ---- バインド登録 -------------------------------------------------------

        void AddBinding(const InputBinding& b)
        {
            m_bindings.push_back(b);
        }

        template <std::size_t N> void AddBindings(const std::array<InputBinding, N>& arr)
        {
            for (const auto& b : arr)
                m_bindings.push_back(b);
        }

        void ClearBindings()
        {
            m_bindings.clear();
        }

        // ---- 毎フレーム呼ぶ -----------------------------------------------------
        void Update(DirectX::Keyboard::State& keyboard, DirectX::Mouse::State& mouse, DirectX::GamePad::State& gamePad)
        {
            // デバイス状態取得
            //const auto kbState = keyboard.GetState();
            //const auto mouseState = mouse.GetState();
            //const auto gpState = gamePad.GetState(0);

            m_kbTracker.Update(keyboard);
            m_mouseTracker.Update(mouse);
            m_gpTracker.Update(gamePad);

            // 全アクションをリセット
            m_actions.fill(InputActionState{});

            // 全バインドを評価
            for (const auto& binding : m_bindings)
            {
                const int idx = static_cast<int>(binding.action);
                auto& state = m_actions[idx];

                std::visit(
                    [&](const auto& b)
                    {
                        EvaluateBinding(b, state, keyboard, mouse, gamePad, m_kbTracker, m_mouseTracker,
                                        m_gpTracker);
                    },
                    binding.binding);
            }
        }

        // ---- アクション状態参照 -------------------------------------------------

        const InputActionState& GetAction(InputAction action) const noexcept
        {
            return m_actions[static_cast<int>(action)];
        }

        bool IsPressed(InputAction a) const noexcept
        {
            return GetAction(a).pressed;
        }
        bool IsHeld(InputAction a) const noexcept
        {
            return GetAction(a).held;
        }
        bool IsReleased(InputAction a) const noexcept
        {
            return GetAction(a).released;
        }
        float GetValue(InputAction a) const noexcept
        {
            return GetAction(a).value;
        }

    private:
        std::vector<InputBinding> m_bindings;
        std::array<InputActionState, ACTION_COUNT> m_actions{};

        DirectX::Keyboard::KeyboardStateTracker m_kbTracker;
        DirectX::Mouse::ButtonStateTracker m_mouseTracker;
        DirectX::GamePad::ButtonStateTracker m_gpTracker;

        // ---- バインド評価（std::visit で型ごとに処理）--------------------------

        static void EvaluateBinding(const KeyboardBinding& b, InputActionState& s, const DirectX::Keyboard::State& kb,
                                    const DirectX::Mouse::State&, const DirectX::GamePad::State&,
                                    const DirectX::Keyboard::KeyboardStateTracker& kbT,
                                    const DirectX::Mouse::ButtonStateTracker&,
                                    const DirectX::GamePad::ButtonStateTracker&)
        {
            using T = DirectX::Keyboard::KeyboardStateTracker;
            const bool isHeld = kb.IsKeyDown(b.key);
            if (isHeld)
            {
                s.held = true;
                s.value += b.scale;
            }
            if (kbT.IsKeyPressed(b.key))
                s.pressed = true;
            if (kbT.IsKeyReleased(b.key))
                s.released = true;
        }

        static void EvaluateBinding(const MouseButtonBinding& b, InputActionState& s, const DirectX::Keyboard::State&,
                                    const DirectX::Mouse::State& mouse, const DirectX::GamePad::State&,
                                    const DirectX::Keyboard::KeyboardStateTracker&,
                                    const DirectX::Mouse::ButtonStateTracker& mT,
                                    const DirectX::GamePad::ButtonStateTracker&)
        {
            using B = MouseButtonBinding::Button;
            using TK = DirectX::Mouse::ButtonStateTracker;

            bool held = false;
            bool pressed = false;
            bool released = false;

            switch (b.button)
            {
                case B::Left:
                    held = mouse.leftButton;
                    pressed = (mT.leftButton == TK::PRESSED);
                    released = (mT.leftButton == TK::RELEASED);
                    break;
                case B::Right:
                    held = mouse.rightButton;
                    pressed = (mT.rightButton == TK::PRESSED);
                    released = (mT.rightButton == TK::RELEASED);
                    break;
                case B::Middle:
                    held = mouse.middleButton;
                    pressed = (mT.middleButton == TK::PRESSED);
                    released = (mT.middleButton == TK::RELEASED);
                    break;
                case B::X1:
                    held = mouse.xButton1;
                    pressed = (mT.xButton1 == TK::PRESSED);
                    released = (mT.xButton1 == TK::RELEASED);
                    break;
                case B::X2:
                    held = mouse.xButton2;
                    pressed = (mT.xButton2 == TK::PRESSED);
                    released = (mT.xButton2 == TK::RELEASED);
                    break;
            }

            if (held)
            {
                s.held = true;
                s.value = 1.f;
            }
            if (pressed)
                s.pressed = true;
            if (released)
                s.released = true;
        }

        static void EvaluateBinding(const MouseAxisBinding& b, InputActionState& s, const DirectX::Keyboard::State&,
                                    const DirectX::Mouse::State& mouse, const DirectX::GamePad::State&,
                                    const DirectX::Keyboard::KeyboardStateTracker&,
                                    const DirectX::Mouse::ButtonStateTracker&,
                                    const DirectX::GamePad::ButtonStateTracker&)
        {
            using A = MouseAxisBinding::Axis;
            float raw = 0.f;
            switch (b.axis)
            {
                case A::X:
                    raw = static_cast<float>(mouse.x);
                    break;
                case A::Y:
                    raw = static_cast<float>(mouse.y);
                    break;
                case A::ScrollWheel:
                    raw = static_cast<float>(mouse.scrollWheelValue);
                    break;
            }
            const float v = raw * b.scale;
            if (std::abs(v) > 0.f)
            {
                s.value += v; // 同 Action の複数バインドを加算
                s.held = true;
            }
        }

        static void EvaluateBinding(const GamePadButtonBinding& b, InputActionState& s, const DirectX::Keyboard::State&,
                                    const DirectX::Mouse::State&, const DirectX::GamePad::State& gp,
                                    const DirectX::Keyboard::KeyboardStateTracker&,
                                    const DirectX::Mouse::ButtonStateTracker&,
                                    const DirectX::GamePad::ButtonStateTracker& gpT)
        {
            using B = GamePadButtonBinding::Button;
            using TK = DirectX::GamePad::ButtonStateTracker;

            if (!gp.IsConnected())
                return;

            bool held = false;
            bool pressed = false;
            bool released = false;

            // ゲームパッドの各ボタン状態を評価
            auto Check = [&](bool gpHeld, TK::ButtonState trState)
            {
                held = gpHeld;
                pressed = (trState == TK::PRESSED);
                released = (trState == TK::RELEASED);
            };

            switch (b.button)
            {
                case B::A:
                    Check(gp.buttons.a, gpT.a);
                    break;
                case B::B:
                    Check(gp.buttons.b, gpT.b);
                    break;
                case B::X:
                    Check(gp.buttons.x, gpT.x);
                    break;
                case B::Y:
                    Check(gp.buttons.y, gpT.y);
                    break;
                case B::LeftShoulder:
                    Check(gp.buttons.leftShoulder, gpT.leftShoulder);
                    break;
                case B::RightShoulder:
                    Check(gp.buttons.rightShoulder, gpT.rightShoulder);
                    break;
                case B::LeftTrigger:
                    held = gp.IsLeftTriggerPressed();
                    pressed = (gpT.leftTrigger == TK::PRESSED);
                    released = (gpT.leftTrigger == TK::RELEASED);
                    break;
                case B::RightTrigger:
                    held = gp.IsRightTriggerPressed();
                    pressed = (gpT.rightTrigger == TK::PRESSED);
                    released = (gpT.rightTrigger == TK::RELEASED);
                    break;
                case B::DPadUp:
                    Check(gp.dpad.up, gpT.dpadUp);
                    break;
                case B::DPadDown:
                    Check(gp.dpad.down, gpT.dpadDown);
                    break;
                case B::DPadLeft:
                    Check(gp.dpad.left, gpT.dpadLeft);
                    break;
                case B::DPadRight:
                    Check(gp.dpad.right, gpT.dpadRight);
                    break;
                case B::Start:
                    Check(gp.buttons.start, gpT.start);
                    break;
                case B::Back:
                    Check(gp.buttons.back, gpT.back);
                    break;
                case B::LeftStick:
                    Check(gp.buttons.leftStick, gpT.leftStick);
                    break;
                case B::RightStick:
                    Check(gp.buttons.rightStick, gpT.rightStick);
                    break;
            }

            if (held)
            {
                s.held = true;
                s.value = 1.f;
            }
            if (pressed)
                s.pressed = true;
            if (released)
                s.released = true;
        }

        static void EvaluateBinding(const GamePadAxisBinding& b, InputActionState& s, const DirectX::Keyboard::State&,
                                    const DirectX::Mouse::State&, const DirectX::GamePad::State& gp,
                                    const DirectX::Keyboard::KeyboardStateTracker&,
                                    const DirectX::Mouse::ButtonStateTracker&,
                                    const DirectX::GamePad::ButtonStateTracker&)
        {
            if (!gp.IsConnected())
                return;

            using A = GamePadAxisBinding::Axis;
            float raw = 0.f;
            switch (b.axis)
            {
                case A::LeftStickX:
                    raw = gp.thumbSticks.leftX;
                    break;
                case A::LeftStickY:
                    raw = gp.thumbSticks.leftY;
                    break;
                case A::RightStickX:
                    raw = gp.thumbSticks.rightX;
                    break;
                case A::RightStickY:
                    raw = gp.thumbSticks.rightY;
                    break;
                case A::LeftTrigger:
                    raw = gp.triggers.left;
                    break;
                case A::RightTrigger:
                    raw = gp.triggers.right;
                    break;
            }

            if (std::abs(raw) < b.deadZone)
                return;
            const float v = raw * b.scale;
            if (v > 0.f)
            {
                s.value += v;
                s.held = true;
            }
        }
    };

} // namespace Input