#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"

#include "../Components/Transform.h"
#include "../Components/Camera.h"

namespace ECS
{
    class CameraSystem
    {
    public:
        void Update(ECS::World& world, float dt)
        {
            // priorityが最大のCameraCompにタグを付ける
            // 既存タグをいったん全削除
            auto tagDesc = ECS::QueryBuilder{}.All<ECS::ActiveCameraTagComp>().Build();
            world.Query(tagDesc).Each<ECS::ActiveCameraTagComp>([&](ECS::EntityID eid, ECS::ActiveCameraTagComp /*ac*/)
                                      { world.RemoveComponent<ECS::ActiveCameraTagComp>(eid); });

            // priority最大を探してタグ付与
            ECS::EntityID bestEid = ECS::EntityID::Null();
            int bestPriority = INT_MIN;
            auto priDesc = ECS::QueryBuilder{}.All<ECS::CameraComp>().Build();
            world.Query(priDesc).Each<ECS::CameraComp>(
                [&](ECS::EntityID eid, ECS::CameraComp& cam)
                {
                    if (cam.isActive && cam.priority > bestPriority)
                    {
                        bestPriority = cam.priority;
                        bestEid = eid;
                    }
                });

            if (!bestEid.IsNull())
                world.AddComponent<ECS::ActiveCameraTagComp>(bestEid);

            // アスペクト比を毎フレーム更新
            const float aspect = static_cast<float>(m_screenW) / m_screenH;

            auto desc = ECS::QueryBuilder{}.All<ECS::TransformComp, ECS::CameraComp>().Build();
            world.Query(desc).Each<ECS::TransformComp, ECS::CameraComp>(
                [&](ECS::EntityID eid, ECS::TransformComp& tr, ECS::CameraComp& cam)
                {
                    if (!cam.isActive)
                        return;

                    cam.aspectRatio = aspect;

                    // ビュー行列: TransformCompから計算
                    const auto forward =
                        DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitZ, tr.rotation);

                    const auto up =
                        DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitY, tr.rotation);
                    cam.view = DirectX::SimpleMath::Matrix::CreateLookAt(tr.position, tr.position + forward, up);

                    // 投影行列
                    if (cam.isPerspective)
                        cam.projection = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
                            cam.fov, cam.aspectRatio, cam.nearZ, cam.farZ);
                    else
                        cam.projection = DirectX::SimpleMath::Matrix::CreateOrthographic(
                            cam.orthoHeight * cam.aspectRatio, cam.orthoHeight, cam.nearZ, cam.farZ);
                });
        }

        void SetScreenSize(int w, int h)
        {
            m_screenW = w;
            m_screenH = h;
        }

    private:
        int m_screenW = 1280;
        int m_screenH = 720;
    };
}
