//--------------------------------------------------------------------------------------
// File: FollowCameraSystem.h
//
// FollowCameraComp を持つカメラエンティティの TransformComp を毎フレーム更新する。
//
// 処理内容:
//   1. target の TransformComp から注視位置を取得
//   2. azimuth / elevation からカメラ理想位置を計算
//   3. positionSmoothing / rotationSmoothing で補間しながら TransformComp に書き込む
//   4. マウス入力で azimuth / elevation を更新（InputProviderで抽象化）
//
// 呼び出し順:
//   InputSystem::Update()
//   FollowCameraSystem::Update()   ← ここ
//   CameraSystem::Update()         ← TransformComp → view/proj 行列を計算
//   RenderSystem::Update()
//
// Date: 2026.5.11
//--------------------------------------------------------------------------------------
#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Camera.h"

#include <SimpleMath.h>
#include <algorithm>
#include <cmath>

namespace ECS
{

class FollowCameraSystem
{
public:
    // ---- マウス入力の抽象化 -------------------------------------------------
    // InputSystem との結合を避けるため、外部から差分を注入する方式にする。
    // Game::Update() で
    //   followCameraSystem.AddMouseDelta(mouse.GetDeltaX(), mouse.GetDeltaY());
    // のように呼ぶ。
    void AddMouseDelta(float dx, float dy) noexcept
    {
        m_mouseDeltaX += dx;
        m_mouseDeltaY += dy;
    }

    // ---- メインアップデート -------------------------------------------------
    void Update(World& world, float dt)
    {
        auto desc = QueryBuilder{}
            .All<TransformComp, FollowCameraComp>()
            .Build();

        world.Query(desc).Each<TransformComp, FollowCameraComp>(
            [&](EntityID eid, TransformComp& camTr, FollowCameraComp& follow)
            {
                // target が無効なら何もしない
                if (follow.target.IsNull() || !world.IsAlive(follow.target))
                    return;

                const TransformComp& targetTr = world.GetComponent<TransformComp>(follow.target);

                // ---- 1. マウス入力で azimuth / elevation を更新 -------------
                // CameraComp から感度を取得したいが結合を避けるため固定値を使う。
                // 感度が必要なら FollowCameraComp に mouseSensitivity を追加する。
                follow.azimuth   -= m_mouseDeltaX * m_sensitivity;
                follow.elevation -= m_mouseDeltaY * m_sensitivity;

                // elevation クランプ
                follow.elevation = std::clamp(follow.elevation,
                                              follow.elevationMin,
                                              follow.elevationMax);

                // azimuth は 0〜2π に正規化
                constexpr float TwoPI = DirectX::XM_2PI;
                follow.azimuth = std::fmod(follow.azimuth, TwoPI);
                if (follow.azimuth < 0.f)
                    follow.azimuth += TwoPI;

                // ---- 2. カメラの理想位置を計算 ------------------------------
                // 球面座標系で target からの相対位置を求める。
                //
                //   x = -sin(azimuth) * cos(elevation) * distance
                //   y =  sin(elevation)                * distance + height
                //   z = -cos(azimuth) * cos(elevation) * distance
                //
                // azimuth=0 のとき +Z 方向がカメラ後方（target の正面を向く）。
                const float cosElev = std::cos(follow.elevation);
                const float sinElev = std::sin(follow.elevation);
                const float cosAzim = std::cos(follow.azimuth);
                const float sinAzim = std::sin(follow.azimuth);

                // shoulderOffset は target のローカル空間で定義されていると仮定する
                const DirectX::SimpleMath::Vector3 worldShoulder =
                    follow.shoulderOffset;
                    //DirectX::SimpleMath::Vector3::Transform(follow.shoulderOffset, targetTr.rotation);

                // 注視点 = target 位置 + 肩オフセット + 高さ
                const DirectX::SimpleMath::Vector3 pivotPos =
                    targetTr.position
                    + worldShoulder
                    + DirectX::SimpleMath::Vector3(0.f, follow.height, 0.f);

                // 球面座標でのオフセット
                const DirectX::SimpleMath::Vector3 offset(
                    -sinAzim * cosElev * follow.distance,
                    -sinElev           * follow.distance,
                    -cosAzim * cosElev * follow.distance
                );

                const DirectX::SimpleMath::Vector3 idealPos = pivotPos + offset;

                // ---- 3. 位置補間（指数減衰スムージング） --------------------
                // smoothing が大きいほど速く追従する。
                // dt 依存の補間: factor = 1 - exp(-smoothing * dt)
                const float posFactor  = 1.f - std::exp(-follow.positionSmoothing  * dt);
                const float rotFactor  = 1.f - std::exp(-follow.rotationSmoothing  * dt);

                camTr.position = DirectX::SimpleMath::Vector3::Lerp(
                    camTr.position, idealPos, posFactor);

                // ---- 4. 回転補間（注視点を向く） ----------------------------
                // 理想回転: カメラ位置から pivotPos を向くクォータニオン
                DirectX::SimpleMath::Vector3 lookDir = pivotPos - camTr.position;
                if (lookDir.LengthSquared() > 1e-6f)
                {
                    lookDir.Normalize();

                    // LookAt 行列からクォータニオンを生成
                    // SimpleMath の CreateWorld は position / forward / up を受け取る
                    const DirectX::SimpleMath::Matrix lookMat =
                        DirectX::SimpleMath::Matrix::CreateWorld(
                            DirectX::SimpleMath::Vector3::Zero,
                            -lookDir,
                            DirectX::SimpleMath::Vector3::Up);
                    const DirectX::SimpleMath::Quaternion idealRot =
                        DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(lookMat);

                    camTr.rotation = DirectX::SimpleMath::Quaternion::Slerp(
                        camTr.rotation, idealRot, rotFactor);
                    camTr.rotation.Normalize();
                }
            });

        // フレーム末尾でデルタをリセット
        m_mouseDeltaX = 0.f;
        m_mouseDeltaY = 0.f;
    }

    void SetSensitivity(float s) noexcept { m_sensitivity = s; }

private:
    float m_mouseDeltaX = 0.f;
    float m_mouseDeltaY = 0.f;
    float m_sensitivity = 0.002f; // radian/pixel
};

} // namespace ECS