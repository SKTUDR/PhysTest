#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Physics.h"
#include "../Components/Rigidbody.h"
#include "../Components/GamePlay.h"
#include "InputState.h"

#include <SimpleMath.h>
#include <algorithm>

namespace ECS
{

    // ============================================================================
    //  PlayerMovementSystem
    //
    //  対象: TransformComp + VelocityComp + AccelerationComp + PlayerTagComp
    //
    //  処理:
    //    [WASD]       カメラ考慮なし・ワールド軸平行に移動入力を AccelerationComp に加算
    //    [左クリック] +Z +Y 方向の瞬間インパルスを VelocityComp.linear に直接加算
    //                 （Unity の Rigidbody.AddForce(..., ForceMode.Impulse) 相当）
    //
    //  FullMovementSystem との役割分担:
    //    PlayerMovementSystem  → 入力を読んで AccelerationComp / VelocityComp を書く
    //    FullMovementSystem    → AccelerationComp / VelocityComp を積分して
    //                            TransformComp::position を更新する
    //    ∴ 呼び出し順: PlayerMovementSystem → FullMovementSystem
    // ============================================================================
    class PlayerMovementSystem
    {
    public:
        // ---- パラメータ ---------------------------------------------------------
        struct Params
        {
            // WASD 移動
            float moveAcceleration = 10.f; // 入力時に加算する加速度 (m/s^2)

            // 左クリック インパルス
            float impulseZ = 0.f; // +Z 方向の初速 (m/s)
            float impulseX = 0.f;  // +X 方向の初速 (m/s)
            float impulseY = 1.f; // +Y 方向の初速 (m/s)（放物線の山の高さを決める）
        };

        explicit PlayerMovementSystem(Params params = {}) : m_params(params)
        {
        }

        // ---- Update -------------------------------------------------------------
        void Update(World& world, const Input::InputState& input, GameContext &gameContext)
        {
            //// WASD 入力をワールド軸に変換
            //// カメラ考慮なし: W=+Z, S=-Z, A=-X, D=+X
            //DirectX::SimpleMath::Vector3 moveInput = DirectX::SimpleMath::Vector3::Zero;

            //if (gameContext.keyboardTracker.GetLastState().IsKeyDown(Keyboard::Right))
            //    moveInput.x -= 1.f;
            //if (gameContext.keyboardTracker.GetLastState().IsKeyDown(Keyboard::Left))   
            //    moveInput.x += 1.f;

            //const bool applyImpulse =
            //    gameContext.mouseButtonTracker.leftButton & gameContext.mouseButtonTracker.PRESSED;

            //// PlayerTagComp を持つエンティティだけを対象にする
            //auto desc = QueryBuilder{}
            //    .All<TransformComp, VelocityComp, AccelerationComp, PlayerTagComp>()
            //    .Build();

            //const Params& p = m_params;

            //world.Query(desc).Each<TransformComp, VelocityComp, AccelerationComp>(
            //    [&moveInput, applyImpulse, &p](EntityID, TransformComp& tr, VelocityComp& vel,
            //                                   AccelerationComp& acc)
            //    {
            //        // ---- WASD: 加速度をセット ------------------------------------
            //        // 入力がある方向に加速、入力がなければ加速度ゼロ
            //        acc.linear.x = moveInput.x * p.moveAcceleration;

            //        // ---- 左クリック: 瞬間インパルスを速度に直接加算 --------------
            //        if (applyImpulse)
            //        {
            //            vel.linear.x += moveInput.x * p.impulseX;
            //        }

            //        if (tr.position.x < -5.f) {
            //            tr.position.x = -5.f;
            //            vel.linear.x = 0.f;
            //        }
            //        else if (tr.position.x > 5.f) {
            //            tr.position.x = 5.f;
            //            vel.linear.x = 0.f;
            //        }
            //    });

             const bool applyImpulse =
                 gameContext.mouseButtonTracker.leftButton & gameContext.mouseButtonTracker.PRESSED;

             // PlayerTagComp を持つエンティティだけを対象にする
              auto desc = QueryBuilder{}
                  .All<TransformComp, RigidbodyComp, PlayerTagComp>()
                  .Build();

              const Params& p = m_params;

              world.Query(desc).Each<TransformComp, RigidbodyComp>(
                  [&input, applyImpulse, &p](EntityID, TransformComp& tr, RigidbodyComp& rb)
                  {
                      // ---- WASD: 加速度をセット ------------------------------------
                      // 入力がある方向に加速、入力がなければ加速度ゼロ
                      DirectX::SimpleMath::Vector3 moveInput = DirectX::SimpleMath::Vector3::Zero;
                      if (input.MoveForward())
                          moveInput.z += 1.f;
                      if (input.MoveBackward())
                          moveInput.z -= 1.f;
                      if (input.MoveLeft())
                          moveInput.x += 1.f;
                      if (input.MoveRight())
                          moveInput.x -= 1.f;

                      rb.AddForce(moveInput * p.moveAcceleration);
                      // ---- 左クリック: 瞬間インパルスを速度に直接加算 --------------
                      if (applyImpulse)
                      {
                          DirectX::SimpleMath::Vector3 impulse = {moveInput.x * p.impulseX, p.impulseY, p.impulseZ};
                          rb.ApplyImpulse(impulse);
                          rb.ApplyAngularImpulse(DirectX::SimpleMath::Vector3(0.0f, 0.0f, 10.0f), tr.rotation);
                      }
                      if (tr.position.x < -5.f) {
                          tr.position.x = -5.f;
                          rb.velocity.x = 0.f;
                      }
                      else if (tr.position.x > 5.f) {
                          tr.position.x = 5.f;
                          rb.velocity.x = 0.f;
                      }
                  });
        }

    private:
        Params m_params;
    };

} // namespace ECS