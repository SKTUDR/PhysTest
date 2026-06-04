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
        
        

        // Collider: Capsule + PLAYER レイヤー
        auto& col = m_world.AddComponent<ColliderComp>(eid);
        //col.shape = Capsule{.radius = 0.4f, .halfHeight = 0.4f};
        col.shape = OBB{.halfExtents = {0.5f, 0.5f, .5f}, .orientation = rotation};
        col.layer = CollisionLayer::PLAYER;
        col.mask = LayerPreset::PLAYER_MASK;
        col.localOffset = {0.f, 0.f, 0.f}; // 足元が原点になるよう Y オフセット

        auto& rb = m_world.AddComponent<RigidbodyComp>(eid); // デフォルトで質量1、重力有効、非運動体
        rb.isKinematic = false;
        rb.SetMassAndInertia(20.f, {col.GetHalfExtents()}); // コライダーサイズに合わせて質量と慣性を設定
        rb.freezeRotation = false;                           // プレイヤーは回転させない
        rb.restitution = 0.3f;
        rb.friction = 0.98f;

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

        // Velocity
        // m_world.AddComponent<VelocityComp>(eid); // ゼロ初期化

        

        // Collider: Capsule + ENEMY レイヤー
        //auto& coll = m_world.AddComponent<ColliderComp>(eid);
        //coll.shape = OBB{.halfExtents = {0.3f, 0.5f, 0.5f}, .orientation = DirectX::SimpleMath::Quaternion::Identity};
        //coll.layer = CollisionLayer::ENEMY;
        //coll.flags.isTrigger = false; // 当たり判定はイベントのみで、物理的な衝突反応はなし
        //coll.mask = LayerPreset::ENEMY_MASK;
        //coll.localOffset = {0.f, 0.f, 0.f}; // 足元が原点になるよう Y オフセット

        auto& col = m_world.AddComponent<ColliderComp>(eid);
        //col.shape = Capsule{.radius = 0.5f, .halfHeight = 0.5f};
        col.shape = OBB{.halfExtents = {0.5f, 0.5f, .5f}, .orientation = DirectX::SimpleMath::Quaternion::Identity};
        //col.shape = AABB{.halfExtents = {0.5f, 0.5f, 1.f}};
        col.layer = CollisionLayer::ENEMY;
        col.mask = CollisionLayer::ALL;
        col.localOffset = {0.f, 0.f, 0.f}; // 足元が原点になるよう Y オフセット

        auto& rb = m_world.AddComponent<RigidbodyComp>(eid); // デフォルトで質量1、重力有効、非運動体
        rb.SetMassAndInertia(20.f, {col.GetHalfExtents()}); // コライダーサイズに合わせて質量と慣性を設定
        rb.freezeRotation = false;
        rb.restitution = .3f;
        rb.friction = 0.999f;

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
        coll.shape = AABB{.halfExtents = {100, 3, 100}}; // モデルのサイズに合わせて調整
        coll.localOffset = {0.f, -2.f, 0.f};            // モデルの中心が地面から少し浮いている想定
        coll.flags.isDynamic = false;
        coll.layer = CollisionLayer::WORLD;
        coll.mask = CollisionLayer::ALL;

        auto& rb = m_world.AddComponent<RigidbodyComp>(eid);
        rb.isKinematic = true;
        rb.SetMassAndInertia(0.f, {coll.GetHalfExtents()}); // 静的オブジェクトなので質量0、慣性なし
        rb.restitution = .3f;

        auto& ren = m_world.GetComponent<ModelRenderComp>(eid);
        ren.visible = false;

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
}
