#include "pch.h"
#include "SetupEntities.h"

#include "../../ECS/Entity.h"
#include "../../ECS/World.h"
#include "../../Components/Components.h"

#include "../../KUtil/Random.h"


namespace ECS
{   
    EntityID EntityFactory::CreateRenderableEntity(Graphics::ModelID modelId,
                                                   const DirectX::SimpleMath::Vector3& position,
                                                   const DirectX::SimpleMath::Quaternion& rotation,
                                                   const DirectX::SimpleMath::Vector3& scale)
    {
        EntityID eid = m_world.Create();

        auto& tr = m_world.AddComponent<TransformComp>(eid);
        tr.position = position;
        tr.rotation = rotation;
        tr.scale = scale;

        auto& ren = m_world.AddComponent<ModelRenderComp>(eid);
        ren.modelId = modelId;

        return eid;
    }

    EntityID ECS::EntityFactory::CreatePlayer(Graphics::ModelID modelId, const DirectX::SimpleMath::Vector3& position,
                                              const DirectX::SimpleMath::Quaternion& rotation,
                                              const DirectX::SimpleMath::Vector3& scale)
    {
        EntityID eid = CreateRenderableEntity(modelId, position, rotation, scale);


        auto& col = m_world.AddComponent<ColliderComp>(eid);

        col.shape = OBB{.halfExtents = {.5f, .5f, .5f}, .orientation = rotation};
        col.layer = CollisionLayer::PLAYER;
        col.mask = LayerPreset::PLAYER_MASK;
        col.localOffset = {0.f, 0.f, 0.f}; // 足元が原点になるよう Y オフセット

        auto& rb = m_world.AddComponent<RigidbodyComp>(eid); // デフォルトで質量1、重力有効、非運動体
        rb.isKinematic = false;
        rb.SetMassAndInertia(60.f, {col.GetHalfExtents()}); // コライダーサイズに合わせて質量と慣性を設定
        rb.SetFreezeRotation(true);                             // プレイヤーは回転させない
        rb.restitution = 0.3f;
        rb.staticFriction = 0.8f;
        rb.kineticFriction = 0.7f;
        rb.linearDamping = 4.f;

        auto& ren = m_world.GetComponent<ModelRenderComp>(eid);
        ren.visible = true;

        // Tag
        m_world.AddComponent<PlayerTagComp>(eid);
        m_world.AddComponent<CastShadowComp>(eid);

        return eid;
    }
    EntityID EntityFactory::CreateEnemy(Graphics::ModelID modelId, const DirectX::SimpleMath::Vector3& position,
                                        const DirectX::SimpleMath::Vector3& scale,
                                        float mass)
    {
        ECS::EntityID eid = CreateRenderableEntity(modelId, position, DirectX::SimpleMath::Quaternion::Identity, scale);

        auto& col = m_world.AddComponent<ColliderComp>(eid);

        col.shape = OBB{.halfExtents = {.5f, .5f, .5f}, .orientation = DirectX::SimpleMath::Quaternion::Identity};
        col.layer = CollisionLayer::ENEMY;
        col.mask = CollisionLayer::ALL;
        col.localOffset = {0.f, 0.f, 0.f};

        auto& rb = m_world.AddComponent<RigidbodyComp>(eid); // デフォルトで質量1、重力有効、非運動体
        rb.isKinematic = true;
        rb.SetMassAndInertia(60.f, {col.GetHalfExtents()}); // コライダーサイズに合わせて質量と慣性を設定
        rb.SetFreezeRotation(false);                        
        rb.restitution = 0.3f;
        rb.staticFriction = 0.8f;
        rb.kineticFriction = 0.9f;

        auto& ren = m_world.GetComponent<ModelRenderComp>(eid);
        ren.visible = true;

        // Tag
        m_world.AddComponent<EnemyTagComp>(eid);
        m_world.AddComponent<CastShadowComp>(eid);

        return eid;

    }

    EntityID EntityFactory::CreateGround(Graphics::ModelID modelId, const DirectX::SimpleMath::Vector3& position,
                                         const DirectX::SimpleMath::Quaternion& rotation = DirectX::SimpleMath::Quaternion::Identity,
                                         const DirectX::SimpleMath::Vector3& scale = {1.f, 1.f, .5f})
    {
        ECS::EntityID eid = CreateRenderableEntity(modelId, position, rotation, scale);

        auto& coll = m_world.AddComponent<ColliderComp>(eid);
        coll.shape = OBB{.halfExtents = {100, 3, 100}, .orientation = rotation}; // モデルのサイズに合わせて調整
        coll.localOffset = {0.f, 0.f, 0.f};            // モデルの中心が地面から少し浮いている想定
        coll.flags.isDynamic = true;
        coll.layer = CollisionLayer::WORLD;
        coll.mask = CollisionLayer::ALL;

        auto& rb = m_world.AddComponent<RigidbodyComp>(eid);
        rb.isKinematic = true;
        rb.SetMassAndInertia(0.f, {coll.GetHalfExtents()}); // 静的オブジェクトなので質量0、慣性なし
        rb.restitution = .3f;
        rb.SetFreezeRotation(true);

        auto& ren = m_world.GetComponent<ModelRenderComp>(eid);
        ren.visible = true;

        m_world.AddComponent<GroundTagComp>(eid);

        return eid;
    }
    
    EntityID EntityFactory::CreateSun(const DirectX::SimpleMath::Vector3& position,
                                      const DirectX::SimpleMath::Vector3& color, const float intensity,
                                      DirectX::SimpleMath::Vector3 direction)
    {
         // 太陽光（Directional）を生成
         ECS::EntityID sunId = m_world.Create();
         auto& sunTr = m_world.AddComponent<ECS::TransformComp>(sunId);
         sunTr.position = position;        // シーン外の高い位置
         auto& sunLt = m_world.AddComponent<ECS::LightComp>(sunId);
         sunLt.type       = ECS::LightComp::Type::Directional;
         sunLt.color      = color;      // 暖色の太陽光
         sunLt.intensity  = intensity;
         sunLt.direction  = direction;    // 斜め下方向に照射
         sunLt.castShadow = true;

         return sunId;
    }
    EntityID EntityFactory::CreateCamera(const DirectX::SimpleMath::Vector3& position,
                                         const DirectX::SimpleMath::Quaternion& rotation,
                                         const int priority, const float fov, const float nearClip, const float farClip,
                                         const bool isPerspective, const bool isActive )
    {
        ECS::EntityID camId = m_world.Create();
        auto& camTr = m_world.AddComponent<ECS::TransformComp>(camId);
        camTr.position = position;
        camTr.rotation = rotation;

        auto& camCam = m_world.AddComponent<ECS::CameraComp>(camId);
        camCam.priority = priority;
        camCam.fov = fov;
        camCam.nearZ = nearClip;
        camCam.farZ = farClip;
        camCam.isPerspective = isPerspective;
        camCam.isActive = isActive;

        return camId;
    }

    EntityID EntityFactory::CreatePlayerFollowCamera(const DirectX::SimpleMath::Vector3& position,
                                                     const DirectX::SimpleMath::Quaternion& rotation,
                                                     const int priority, const float fov, const float nearClip,
                                                     const float farClip, const bool isPerspective, const bool isActive)
    {
        ECS::EntityID camId =
            CreateCamera(position, rotation, priority, fov, nearClip, farClip, isPerspective, isActive);

        auto& folCam = m_world.AddComponent<FollowCameraComp>(camId);
         
        auto desc = QueryBuilder{}.All<PlayerTagComp>().Build();
        m_world.Query(desc).Each<PlayerTagComp>(
            [&](EntityID eid, const PlayerTagComp)
            {
                folCam.target = eid;
            });

        // 距離・位置
        folCam.distance = 5.f;
        folCam.height = 1.6f;
        folCam.shoulderOffset = {-.0f, 0.f, 0.f};

        // 角度
        folCam.azimuth = 0.f;
        folCam.elevation = 0.25f;
        folCam.elevationMin = -0.9f;
        folCam.elevationMax = 0.9f;

        // 追従の滑らかさ
        folCam.positionSmoothing = 25.f;    
        folCam.rotationSmoothing = 120.f;

        return camId;
    }


}
