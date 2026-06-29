#pragma once

namespace ECS
{
struct TransformComp
{
    DirectX::SimpleMath::Vector3 position = {0.f, 0.f, 0.f};
    DirectX::SimpleMath::Quaternion rotation = DirectX::SimpleMath::Quaternion::Identity;
    DirectX::SimpleMath::Vector3 scale = {1.f, 1.f, 1.f};

    // ---- ワールド行列生成 --------------------------------------------------
    // RenderSystem・CollisionSystem が毎フレーム呼ぶ。
    // SimpleMath::Matrix は row-major（HLSL の column-major と転置の関係）。
    DirectX::SimpleMath::Matrix ToWorldMatrix() const noexcept
    {
        return DirectX::SimpleMath::Matrix::CreateScale(scale) *
                DirectX::SimpleMath::Matrix::CreateFromQuaternion(rotation) *
                DirectX::SimpleMath::Matrix::CreateTranslation(position);
    }

    // ---- 向きベクトル ------------------------------------------------------
    // SimpleMath::Quaternion::Transform で基底ベクトルを回転させる。
    DirectX::SimpleMath::Vector3 Forward() const noexcept
    {
        return DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Forward, rotation);
    }
    DirectX::SimpleMath::Vector3 Right() const noexcept
    {
        return DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Right, rotation);
    }
    DirectX::SimpleMath::Vector3 Up() const noexcept
    {
        return DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Up, rotation);
    }

    // ---- 回転ヘルパー ------------------------------------------------------
    // Yaw(Y軸) / Pitch(X軸) / Roll(Z軸) を度数で受け取って rotation を上書き
    void SetRotationEuler(float yawDeg, float pitchDeg, float rollDeg) noexcept
    {
        rotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(DirectX::XMConvertToRadians(yawDeg),
                                                                                DirectX::XMConvertToRadians(pitchDeg),
                                                                                DirectX::XMConvertToRadians(rollDeg));
    }

    // 指定方向を向く（up が省略された場合は Y-up）
    void LookAt(const DirectX::SimpleMath::Vector3& target,
                const DirectX::SimpleMath::Vector3& up = DirectX::SimpleMath::Vector3::Up) noexcept
    {
        DirectX::SimpleMath::Vector3 forward = target - position;
        if (forward.LengthSquared() < 1e-8f)
            return;
        forward.Normalize();

        // forward × up で right を求め、直交基底からクォータニオンを生成
        DirectX::SimpleMath::Matrix lookMat = DirectX::SimpleMath::Matrix::CreateWorld(position, forward, up);
        rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(lookMat);
    }

    void SmoothLookAt(const DirectX::SimpleMath::Vector3& target, float deltaTime, float turnSpeed = 10.0f)
    {

        DirectX::SimpleMath::Vector3 forward = target - position;

        if (forward.LengthSquared() < 1e-8f)
            return;

        forward.Normalize();

        DirectX::SimpleMath::Matrix lookMat =
            DirectX::SimpleMath::Matrix::CreateWorld(position, forward, DirectX::SimpleMath::Vector3::Up);

        DirectX::SimpleMath::Quaternion targetRot = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(lookMat);

        rotation = DirectX::SimpleMath::Quaternion::Slerp(rotation, targetRot, turnSpeed * deltaTime);

        rotation.Normalize();
    }

};

// ---- LocalTransformComp -----------------------------------------------------
// 親エンティティ相対のローカル Transform。
// ユーザーが直接書き換える値。
//
// SceneGraphSystem が毎フレーム以下を計算して TransformComp に書き込む:
//   worldMatrix = localMatrix * parent.worldMatrix
//
// ルートエンティティ（HierarchyComp::parent.IsNull()）の場合:
//   worldMatrix = localMatrix（親がいないのでローカル = ワールド）
// ----------------------------------------------------------------------------
struct LocalTransformComp
{
    DirectX::SimpleMath::Vector3 localPosition = {0.f, 0.f, 0.f};
    DirectX::SimpleMath::Quaternion localRotation = DirectX::SimpleMath::Quaternion::Identity;
    DirectX::SimpleMath::Vector3 localScale = {1.f, 1.f, 1.f};

    // ローカル行列を生成
    DirectX::SimpleMath::Matrix ToLocalMatrix() const noexcept
    {
        return DirectX::SimpleMath::Matrix::CreateScale(localScale) *
                DirectX::SimpleMath::Matrix::CreateFromQuaternion(localRotation) *
                DirectX::SimpleMath::Matrix::CreateTranslation(localPosition);
    }

    // ---- 回転ヘルパー ------------------------------------------------------
    // Yaw(Y軸) / Pitch(X軸) / Roll(Z軸) を度数で受け取って rotation を上書き
    void SetRotationEuler(float yawDeg, float pitchDeg, float rollDeg) noexcept
    {
        localRotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(DirectX::XMConvertToRadians(yawDeg),
                                                                            DirectX::XMConvertToRadians(pitchDeg),
                                                                            DirectX::XMConvertToRadians(rollDeg));
    }

    // 指定方向を向く（up が省略された場合は Y-up）
    void LookAt(const DirectX::SimpleMath::Vector3& target,
                const DirectX::SimpleMath::Vector3& up = DirectX::SimpleMath::Vector3::Up) noexcept
    {
        DirectX::SimpleMath::Vector3 forward = target - localPosition;
        if (forward.LengthSquared() < 1e-8f)
            return;
        forward.Normalize();

        // forward × up で right を求め、直交基底からクォータニオンを生成
        DirectX::SimpleMath::Matrix lookMat = DirectX::SimpleMath::Matrix::CreateWorld(localPosition, forward, up);
        localRotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(lookMat);
    }

    void SmoothLookAt(const DirectX::SimpleMath::Vector3& target, float deltaTime, float turnSpeed = 10.0f)
    {

        DirectX::SimpleMath::Vector3 forward = target - localPosition;

        if (forward.LengthSquared() < 1e-8f)
            return;

        forward.Normalize();

        DirectX::SimpleMath::Matrix lookMat =
            DirectX::SimpleMath::Matrix::CreateWorld(localPosition, forward, DirectX::SimpleMath::Vector3::Up);

        DirectX::SimpleMath::Quaternion targetRot =
            DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(lookMat);

        localRotation = DirectX::SimpleMath::Quaternion::Slerp(localRotation, targetRot, turnSpeed * deltaTime);

        localRotation.Normalize();
    }
};

} // namespace ECS

