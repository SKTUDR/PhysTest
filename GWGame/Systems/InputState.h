#pragma once

#include <Keyboard.h>
#include <Mouse.h>

#include "../GameContext.h"

namespace Input
{

    // ---- InputState -------------------------------------------------------------
    // 1フレーム分のキーボード・マウス入力をまとめた純粋データ。
    // Game::Update() で毎フレーム更新し、各 System に const 参照で渡す。
    //
    // DirectXTK の Keyboard::State / Mouse::State をラップして
    // 「今フレームに押された（トリガー）」判定を追加している。
    // ----------------------------------------------------------------------------
    struct InputState
    {
        DirectX::Keyboard::State keyboard;
        
        DirectX::Mouse::State mouse;
     
        // ---- WASD ---------------------------------------------------------------
        bool MoveForward() const noexcept
        {
            return Keyboard::Get().GetState().W;
        }
        bool MoveBackward() const noexcept
        {
            return Keyboard::Get().GetState().S;
        }
        bool MoveLeft() const noexcept
        {
            return Keyboard::Get().GetState().A;
        }
        bool MoveRight() const noexcept
        {
            return Keyboard::Get().GetState().D;
        }

        // ---- マウスボタン -------------------------------------------------------
        // 「今フレームに押した瞬間」だけ true（ホールド中は false）
        DirectX::Mouse::ButtonStateTracker::ButtonState LeftClickTriggered(GameContext &gameContext) const noexcept
        {
            return gameContext.mouseButtonTracker.PRESSED;
        }

        // ---- フレーム更新 -------------------------------------------------------
        // Game::Update() の先頭で必ず呼ぶ
        void Update(GameContext &gameContext) noexcept
        {
            gameContext;
        }

        private:

    };

} // namespace Input