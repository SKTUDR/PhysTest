#pragma once

namespace ECS
{
    struct CameraComp
    {
        // カメラ位置、回転は TransformComp で管理する。
        // ---- 投影パラメータ --------------------------------------------------------
        float fov = DirectX::XMConvertToRadians(45.0f); // 垂直方向の視野角（radian）
        float aspectRatio = 16.f / 9.f;                 // アスペクト比（画面の幅÷高さ）
        float nearZ = 0.1f;                             // ニアクリップ距離
        float farZ = 1000.f;                            // ファークリップ距離

        // 投影モード
        bool isPerspective = true; // true: パースペクティブ投影、false: 平行投影
        float orthoHeight = 10.f;  // 平行投影の高さ（幅は orthoHeight * aspectRatio）

        // 描画対象
        bool isActive = true;       // カメラが有効かどうか。複数あっても有効なカメラは1つだけにすること。
        int priority = 0;           // カメラの描画の優先度の高さ。CameraSystem が毎フレーム優先度最大のカメラに ActiveCameraTagComp を付与する。
        int renderTargetId = -1;    // 描画対象の RenderTargetComp の ID。-1 ならスクリーンに描画

        // ---- ビュー行列・射影行列 ------------------------------------------------
        // 毎フレームシステムでキャッシュする
        DirectX::SimpleMath::Matrix view = DirectX::SimpleMath::Matrix::Identity;
        DirectX::SimpleMath::Matrix projection = DirectX::SimpleMath::Matrix::Identity;
    };

    struct FreeLockCameraComp
    {
        float moveSpeed = 10.f; // カメラの移動速度
        float mouseSensitivity = 0.2f; // マウス感度
    };

    struct FollowCameraComp
    {
        EntityID target; // 追従対象エンティティ

        float distance = 30.f; // ターゲットからカメラまでの距離
        float height = 0.f;  // ターゲット基準の高さオフセット

        // ターゲットのローカル座標系での視点中心オフセット
        // (例: 肩越しカメラ用の左右オフセット)
        DirectX::SimpleMath::Vector3 shoulderOffset = {0.f, 0.f, 20.f};

        float azimuth = 0.f;    // 水平回転角（Yaw, ラジアン）
        float elevation = 0.3f; // 垂直回転角（Pitch, ラジアン）

        float elevationMin = -0.5f; // 垂直回転角の下限（ラジアン）
        float elevationMax = 1.2f;  // 垂直回転角の上限（ラジアン）

        float positionSmoothing = 10.f; // 位置補間の追従速度
        float rotationSmoothing = 30.f; // 回転補間の追従速度

        float inputSmoothing = 20.f;
    };

    struct ActiveCameraTagComp
    {
        // タグコンポーネント。アクティブなカメラに付与する。
        // CameraSystem は ActiveCameraTagComp を持つエンティティを1つだけ許容する。
    };
}
