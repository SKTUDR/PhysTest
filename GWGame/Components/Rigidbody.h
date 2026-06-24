#pragma once
#include <SimpleMath.h>

namespace ECS
{

    struct RigidbodyComp
    {
        // ---- 質量 ---------------------------------------------------------------
        float mass = 1.f;
        float invMass = 1.f;

        // ---- 慣性テンソル（ローカル空間・対角成分のみ）--------------------------
        DirectX::SimpleMath::Vector3 localInvInertia = {1.f, 1.f, 1.f};

        // ---- 速度 ---------------------------------------------------------------
        DirectX::SimpleMath::Vector3 velocity = {};
        DirectX::SimpleMath::Vector3 angularVelocity = {};

        // ---- 力の蓄積（フレームごとにリセット）---------------------------------
        DirectX::SimpleMath::Vector3 forceAccum = {};
        DirectX::SimpleMath::Vector3 torqueAccum = {};

        // ---- マテリアル ---------------------------------------------------------
        float restitution = 0.3f;
        float friction = 0.5f;

        float staticFriction = 0.6f;  // 静止摩擦係数（μs）
        float kineticFriction = 0.4f; // 動摩擦係数（μk）
        // ---- 制約 ---------------------------------------------------------------
        float linearDamping = 0.01f;
        float angularDamping = 0.5f;
        bool isKinematic = false;
        bool useGravity = true;

        // ---- 回転フリーズ（軸ごと）----------------------------------------------
        // true にした軸まわりの回転を完全に無効化する。
        // 旧 freezeRotation の代わりに使用。後方互換のため FreezeRotation() getter を提供。
        bool freezeRotationX = false; // ワールド X 軸まわりの回転を固定
        bool freezeRotationY = false; // ワールド Y 軸まわりの回転を固定
        bool freezeRotationZ = false; // ワールド Z 軸まわりの回転を固定

        // 旧 freezeRotation の後方互換 getter（3軸すべてフリーズ中なら true）
        bool FreezeRotation() const noexcept
        {
            return freezeRotationX && freezeRotationY && freezeRotationZ;
        }
        // 全軸を一括フリーズ／解除する便利関数
        void SetFreezeRotation(bool freeze) noexcept
        {
            freezeRotationX = freezeRotationY = freezeRotationZ = freeze;
        }

        // ---- 後方互換 -----------------------------------------------------------
        float InvMass() const noexcept
        {
            return invMass;
        }

        // =========================================================================
        //  ヘルパー
        // =========================================================================

        // フリーズマスクをワールド空間ベクトルとして返す（フリーズ軸 = 0、自由軸 = 1）
        // CalcWorldInvInertia や角速度クランプに共通利用する。
        DirectX::SimpleMath::Vector3 AngularFreezeAxisMask() const noexcept
        {
            return {freezeRotationX ? 0.f : 1.f, freezeRotationY ? 0.f : 1.f, freezeRotationZ ? 0.f : 1.f};
        }

        void SetMassAndInertia(float m, const DirectX::SimpleMath::Vector3& halfExtents) noexcept
        {
            mass = (m > 0.f) ? m : 1e-6f;
            invMass = 1.f / mass;

            const float lx = halfExtents.x * 2.f;
            const float ly = halfExtents.y * 2.f;
            const float lz = halfExtents.z * 2.f;
            const float k = mass / 12.f;
            localInvInertia.x = 1.f / (k * (ly * ly + lz * lz));
            localInvInertia.y = 1.f / (k * (lx * lx + lz * lz));
            localInvInertia.z = 1.f / (k * (lx * lx + ly * ly));
        }

        // ワールド空間の慣性テンソル逆行列を計算して返す
        //
        //   I_world_inv = R * diag(maskedLocalInvInertia) * R^T
        //
        // フリーズ軸に対応するローカル慣性テンソルの成分を 0 にしてから変換することで、
        // その軸まわりの角加速度を物理的に生じさせない。
        DirectX::SimpleMath::Matrix CalcWorldInvInertia(const DirectX::SimpleMath::Quaternion& rot) const noexcept
        {
            // 全軸フリーズなら単位行列（× 0 相当の代わりに呼び出し側で早期リターン可）
            if (FreezeRotation())
            {
                DirectX::SimpleMath::Matrix m{};
                return m;
            }

            const DirectX::SimpleMath::Matrix R = DirectX::SimpleMath::Matrix::CreateFromQuaternion(rot);

            // フリーズ軸の局所慣性成分を 0 にマスク
            const DirectX::SimpleMath::Vector3 mask = AngularFreezeAxisMask();

            DirectX::SimpleMath::Matrix I_local_inv = DirectX::SimpleMath::Matrix::Identity;
            I_local_inv._11 = localInvInertia.x * mask.x;
            I_local_inv._22 = localInvInertia.y * mask.y;
            I_local_inv._33 = localInvInertia.z * mask.z;
            I_local_inv._44 = 1.f;

            return R * I_local_inv * R.Transpose();
        }

        // ---- インパルス適用 -----------------------------------------------------

        void ApplyImpulse(const DirectX::SimpleMath::Vector3& impulse) noexcept
        {
            if (isKinematic)
                return;
            velocity += impulse * invMass;
        }

        void ApplyAngularImpulse(const DirectX::SimpleMath::Vector3& angularImpulse,
                                 const DirectX::SimpleMath::Quaternion& rot) noexcept
        {
            if (isKinematic)
                return;

            const DirectX::SimpleMath::Matrix iWorldInv = CalcWorldInvInertia(rot);
            DirectX::SimpleMath::Vector3 delta = DirectX::SimpleMath::Vector3::Transform(angularImpulse, iWorldInv);

            // 念のためフリーズ軸をクランプ（数値誤差対策）
            const DirectX::SimpleMath::Vector3 mask = AngularFreezeAxisMask();
            delta.x *= mask.x;
            delta.y *= mask.y;
            delta.z *= mask.z;

            angularVelocity += delta;
            ClampFrozenAngularVelocity();
        }

        void IntegrateAngularVelocity(const DirectX::SimpleMath::Quaternion& rot, float dt) noexcept
        {
            const DirectX::SimpleMath::Matrix iWorldInv = CalcWorldInvInertia(rot);
            DirectX::SimpleMath::Vector3 dw = DirectX::SimpleMath::Vector3::Transform(torqueAccum, iWorldInv) * dt;

            const DirectX::SimpleMath::Vector3 mask = AngularFreezeAxisMask();
            dw.x *= mask.x;
            dw.y *= mask.y;
            dw.z *= mask.z;

            angularVelocity += dw;
            ClampFrozenAngularVelocity();
        }

        // ---- 力 -----------------------------------------------------------------

        void AddForce(const DirectX::SimpleMath::Vector3& force) noexcept
        {
            forceAccum += force;
        }

        void AddForceAtPoint(const DirectX::SimpleMath::Vector3& force, const DirectX::SimpleMath::Vector3& worldPoint,
                             const DirectX::SimpleMath::Vector3& centerOfMass) noexcept
        {
            forceAccum += force;
            torqueAccum += (worldPoint - centerOfMass).Cross(force);
        }

        void ClearForces() noexcept
        {
            forceAccum = DirectX::SimpleMath::Vector3::Zero;
            torqueAccum = DirectX::SimpleMath::Vector3::Zero;
        }

    private:
        // フリーズ軸の angularVelocity 成分を強制ゼロにする
        // IntegrateAngularVelocity / ApplyAngularImpulse の末尾で呼ぶ
        void ClampFrozenAngularVelocity() noexcept
        {
            if (freezeRotationX)
                angularVelocity.x = 0.f;
            if (freezeRotationY)
                angularVelocity.y = 0.f;
            if (freezeRotationZ)
                angularVelocity.z = 0.f;
        }   
    };

} // namespace ECS  