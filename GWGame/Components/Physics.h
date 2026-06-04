#pragma once

namespace ECS
{
    // ---- VelocityComp -----------------------------------------------------------
    // エンティティの速度を保持する純粋データ。
    // SimpleMovementSystem / FullMovementSystem が毎フレーム
    // TransformComp::position に加算する。
    // ----------------------------------------------------------------------------
    struct VelocityComp
    {
        DirectX::SimpleMath::Vector3 linear = {0.f, 0.f, 0.f};  // 並進速度 (m/s)
        DirectX::SimpleMath::Vector3 angular = {0.f, 0.f, 0.f}; // 角速度 (rad/s) Yaw/Pitch/Roll

        // 速度の大きさを maxSpeed 以下にクランプする
        void ClampSpeed(float maxSpeed) noexcept
        {
            const float len = linear.Length();
            if (len > maxSpeed && len > 1e-6f)
            {
                linear *= maxSpeed / len;
            }
        }

        void Reset() noexcept
        {
            linear = DirectX::SimpleMath::Vector3::Zero;
            angular = DirectX::SimpleMath::Vector3::Zero;
        }
    };

    // ---- AccelerationComp -------------------------------------------------------
    // エンティティの加速度・減速（ダンピング）を保持する純粋データ。
    // FullMovementSystem が毎フレーム VelocityComp に加算し、
    // ダンピングで速度を減衰させる。
    //
    // 使い分け:
    //   VelocityComp のみ       → SimpleMovementSystem（等速直線運動）
    //   VelocityComp +
    //   AccelerationComp        → FullMovementSystem（加速・摩擦あり）
    // ----------------------------------------------------------------------------
    struct AccelerationComp
    {
        DirectX::SimpleMath::Vector3 linear = {0.f, 0.f, 0.f};  // 加速度 (m/s^2)
        DirectX::SimpleMath::Vector3 angular = {0.f, 0.f, 0.f}; // 角加速度 (rad/s^2)

        // 線形ダンピング係数 [0=減衰なし, 1=即時停止]
        // velocity *= (1 - damping * dt) で毎フレーム速度を減衰させる
        float linearDamping = 0.0f;
        float angularDamping = 0.0f;

        // 速度の最大値（0 = 無制限）
        float maxLinearSpeed = 0.0f;
        float maxAngularSpeed = 0.0f;

        void ResetAcceleration() noexcept
        {
            linear = DirectX::SimpleMath::Vector3::Zero;
            angular = DirectX::SimpleMath::Vector3::Zero;
        }
    };
} // namespace ECS


