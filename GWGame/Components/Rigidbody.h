#pragma once
#include <SimpleMath.h>

namespace ECS
{

    // ---- RigidbodyComp ----------------------------------------------------------
    // 物理シミュレーション対象のエンティティが持つコンポーネント。
    //
    // 慣性テンソルの扱い:
    //   m_localInvInertia : ローカル空間の慣性テンソル逆行列（対角3x3）
    //                       SetMassAndInertia() で自動計算。形状が変わらない限り不変。
    //
    //   CalcWorldInvInertia(rot) : ワールド空間の慣性テンソル逆行列を返す
    //                       I_world_inv = R * I_local_inv * R^T
    //                       PhysicsSystem が毎フレーム呼ぶ。
    //                       物体が回転するとワールド空間での慣性も変わるため毎フレーム計算が必要。
    // ----------------------------------------------------------------------------
    struct RigidbodyComp
    {
        // ---- 質量 ---------------------------------------------------------------
        float mass = 1.f;
        float invMass = 1.f;

        // ---- 慣性テンソル（ローカル空間・対角成分のみ）--------------------------
        // 均一密度の直方体を仮定。SetMassAndInertia() で設定する。
        // ワールド空間への変換は CalcWorldInvInertia() で行う。
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

        // ---- 制約 ---------------------------------------------------------------
        float linearDamping = 0.01f;
        float angularDamping = 0.5f;
        bool isKinematic = false;
        bool freezeRotation = false;

        // ---- 後方互換 -----------------------------------------------------------
        // 新規コードは CalcWorldInvInertia() を使うこと
        float InvMass() const noexcept
        {
            return invMass;
        }

        // =========================================================================
        //  ヘルパー
        // =========================================================================

        // 質量と形状の半幅からローカル慣性テンソルを計算・設定する
        // halfExtents: コライダーの半幅（AABB / OBB の halfExtents と合わせる）
        void SetMassAndInertia(float m, const DirectX::SimpleMath::Vector3& halfExtents) noexcept
        {
            mass = (m > 0.f) ? m : 1e-6f;
            invMass = 1.f / mass;

            // 均一密度直方体の慣性テンソル（対角成分）
            //   Ix = m/12 * (hy^2 + hz^2)
            //   Iy = m/12 * (hx^2 + hz^2)
            //   Iz = m/12 * (hx^2 + hy^2)
            // ※ halfExtents なので辺長は 2*halfExtents
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
        //   I_world_inv = R * I_local_inv * R^T
        //
        // rot: TransformComp::rotation（現在のワールド回転）
        // 戻り値: ワールド空間の 3x3 慣性テンソル逆行列（SimpleMath::Matrix の左上3x3を使用）
        DirectX::SimpleMath::Matrix CalcWorldInvInertia(const DirectX::SimpleMath::Quaternion& rot) const noexcept
        {
            if (freezeRotation)
                return DirectX::SimpleMath::Matrix::Identity;
            

            // クォータニオンから回転行列を生成
            const DirectX::SimpleMath::Matrix R = DirectX::SimpleMath::Matrix::CreateFromQuaternion(rot);

            // ローカル慣性テンソル逆行列（対角 3x3）を 4x4 行列に埋め込む
            DirectX::SimpleMath::Matrix I_local_inv = DirectX::SimpleMath::Matrix::Identity;
            
            I_local_inv._11 = localInvInertia.x;
            I_local_inv._22 = localInvInertia.y;
            I_local_inv._33 = localInvInertia.z;
            I_local_inv._44 = 1.f; // 4行4列は変換に影響しないよう1にする

            // I_world_inv = R * I_local_inv * R^T
            // SimpleMath::Matrix は row-major なので Transpose() で R^T を取得
            return R * I_local_inv * R.Transpose();
        }

        // ---- インパルス適用 -----------------------------------------------------

        void ApplyImpulse(const DirectX::SimpleMath::Vector3& impulse) noexcept
        {
            if (isKinematic)
                return;
            velocity += impulse * invMass;
        }

        // angularImpulse: ワールド空間のトルクインパルス（r × j）
        // rot           : 現在の TransformComp::rotation
        void ApplyAngularImpulse(const DirectX::SimpleMath::Vector3& angularImpulse,
                                 const DirectX::SimpleMath::Quaternion& rot) noexcept
        {
            if (isKinematic || freezeRotation)
                return;

            // I_world_inv * angularImpulse
            const DirectX::SimpleMath::Matrix iWorldInv = CalcWorldInvInertia(rot);
            const DirectX::SimpleMath::Vector3 delta =
                DirectX::SimpleMath::Vector3::Transform(angularImpulse, iWorldInv); 

            angularVelocity += delta;
        }

        // ---- 角速度の積分（IntegrateVelocity で使う）---------------------------
        // トルクをワールド空間の慣性テンソルで角加速度に変換して角速度に加算する
        void IntegrateAngularVelocity(const DirectX::SimpleMath::Quaternion& rot, float dt) noexcept
        {
            if (freezeRotation)
                return;
            const DirectX::SimpleMath::Matrix iWorldInv = CalcWorldInvInertia(rot);
            angularVelocity += DirectX::SimpleMath::Vector3::Transform(torqueAccum, iWorldInv) * dt;
        }

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
    };

} // namespace ECS