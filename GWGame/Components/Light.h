#pragma once

namespace ECS
{

    // ---- LightComp --------------------------------------------------------------
    // ライトのパラメータを保持する純粋データ。
    // TransformComp::position をライト位置として使うため、
    // 位置はここに持たない。
    //
    // 動くライトは TransformComp + VelocityComp + LightComp を持たせれば
    // FullMovementSystem がそのまま動かしてくれる。
    //
    // ShadowRenderer::CollectLights() が毎フレーム Query して収集する。
    // ----------------------------------------------------------------------------
    struct LightComp
    {
        enum class Type
        {
            Directional, // 太陽光など・位置を持たない平行光
            Point,       // 点光源・全方位に照射
            Spot,        // スポットライト・円錐状に照射
        };

        Type type = Type::Directional;
        DirectX::SimpleMath::Vector3 color = {1.f, 1.f, 1.f}; // RGB 輝度
        float intensity = 1.f;                                // 輝度スケール
        float range = 10.f;                                   // 有効範囲 (m)  Point / Spot 用
        float spotAngle = 45.f;                               // 半頂角 (度)   Spot 用
        bool castShadow = true;                               // false にするとシャドウパスをスキップ

        // ---- Directional ライト用: 照射方向 ------------------------------------
        // Type::Directional の場合のみ有効。TransformComp の rotation からも
        // 計算できるが、よく使うため直接持つ。
        DirectX::SimpleMath::Vector3 direction = {0.f, -1.f, 0.f}; // 下向きがデフォルト

        // ---- ライト行列生成ヘルパー ---------------------------------------------
        // ShadowRenderer がシャドウパスで使うライト視点の View 行列を返す。
        // lightPos: TransformComp::position から渡す
        DirectX::SimpleMath::Matrix CalcLightView(const DirectX::SimpleMath::Vector3& lightPos) const noexcept
        {
            using namespace DirectX::SimpleMath;
            switch (type)
            {
                case Type::Directional:
                {
                    // 平行光はシーン全体を覆う位置から見下ろす
                    const Vector3 target = lightPos + direction;
                    return Matrix::CreateLookAt(lightPos, target, Vector3::Up);
                }
                case Type::Point:
                    // 点光源は6方向に別パスが必要（キューブシャドウマップ）
                    // 簡易実装では -Y 方向（真下）のみ
                    return Matrix::CreateLookAt(lightPos, lightPos + Vector3::Down, Vector3::Forward);
                case Type::Spot:
                {
                    const Vector3 target = lightPos + direction;
                    return Matrix::CreateLookAt(lightPos, target, Vector3::Up);
                }
                default:
                    return Matrix::Identity;
            }
        }

        // シャドウパスの Proj 行列を返す
        DirectX::SimpleMath::Matrix CalcLightProj() const noexcept
        {
            using namespace DirectX::SimpleMath;
            switch (type)
            {
                case Type::Directional:
                    // 平行光は正射影
                    return Matrix::CreateOrthographic(50.f, 50.f, // シーンに合わせて調整
                                                      0.1f, 500.f);
                case Type::Point:
                case Type::Spot:
                    // 点光源・スポットは透視投影
                    return Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(spotAngle * 2.f),
                                                                1.f, // シャドウマップは正方形
                                                                0.1f, range);
                default:
                    return Matrix::Identity;
            }
        }
    };

} // namespace ECS