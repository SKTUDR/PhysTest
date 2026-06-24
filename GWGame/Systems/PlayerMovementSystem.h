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
            float moveAcceleration = 4000.f; // 入力時に加算する加速度 (m/s^2)
            float dashAcceleration = 8000.f;

            // 左クリック インパルス
            float impulseZ = 0.f; // +Z 方向の初速 (m/s)
            float impulseX = 0.f;  // +X 方向の初速 (m/s)
            float impulseY = 1.f; // +Y 方向の初速 (m/s)（放物線の山の高さを決める）
        };

        explicit PlayerMovementSystem(Params params = {}) : m_params(params)
        {
        }

        // ---- Update -------------------------------------------------------------
        void Update(World& world, const Input::InputState& input, GameContext &gameContext, float dt)
        {
            DirectX::SimpleMath::Vector3 moveForward  = DirectX::SimpleMath::Vector3::Zero;
            DirectX::SimpleMath::Vector3 moveRight    =  DirectX::SimpleMath::Vector3::Zero;

            auto camDesc = QueryBuilder{}.All<TransformComp, ActiveCameraTagComp>().Build();

            world.Query(camDesc).Each<TransformComp>(
                [&](EntityID, TransformComp& camTr)
                {
                    // カメラのforward/rightをそのまま使う
                    const auto camForward = camTr.Forward();
                    const auto camRight = camTr.Right();

                    // XZ平面に投影して正規化（Y成分を無視）
                    moveForward = {camForward.x, 0.f, camForward.z};
                    moveRight = {camRight.x, 0.f, camRight.z};
                    // 以降、moveForward/moveRightで移動方向を決定
                });

             const bool applyImpulse =
                 gameContext.mouseButtonTracker.leftButton & gameContext.mouseButtonTracker.PRESSED;

             // PlayerTagComp を持つエンティティだけを対象にする
              auto desc = QueryBuilder{}
                  .All<TransformComp, RigidbodyComp, PlayerTagComp>()
                  .Build();

              const Params& p = m_params;

              world.Query(desc).Each<TransformComp, RigidbodyComp>(
                  [&](EntityID, TransformComp& tr, RigidbodyComp& rb)
                  {
                      // ---- WASD: 加速度をセット ------------------------------------
                      // 入力がある方向に加速、入力がなければ加速度ゼロ
                      DirectX::SimpleMath::Vector3 moveInput = DirectX::SimpleMath::Vector3::Zero;
                      if (input.MoveForward())
                      {
                          moveInput -= moveForward;
                      }
                          
                      if (input.MoveBackward())
                      {
                          moveInput += moveForward;
                      }
                          
                      if (input.MoveLeft())
                      {
                          moveInput += moveRight;
                      }
                          
                      if (input.MoveRight())
                      {
                          moveInput -= moveRight;
                      }

                      if (moveInput.LengthSquared() > 0.0f)
                      {
                          moveInput.Normalize();
                      }

                      if (input.MoveDash())
                      {
                          rb.AddForce(moveInput * p.dashAcceleration);
                      }
                      else
                      {
                          rb.AddForce(moveInput * p.moveAcceleration);
                      }
                      
                      tr.SmoothLookAt(tr.position + moveInput, dt);
                  });
        }

    private:
        Params m_params;
    };

} // namespace ECS