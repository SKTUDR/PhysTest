#pragma once
#include "../../ECS/World.h"
#include "../../Components/Transform.h"
#include "../../Components/Collider.h"
#include "../../Components/GamePlay.h"
#include "../../Components/Render.h"
#include "../../Graphics/ModelRegistry.h"

#include <SimpleMath.h>

namespace ECS
{

    // ---- EntityFactory ----------------------------------------------------------
    // TransformComp + RenderComp を持つエンティティの生成をまとめたファクトリ。
    // World と ModelRegistry への参照を保持し、生成関数を提供する。
    // ----------------------------------------------------------------------------
    class EntityFactory
    {
    public:
        EntityFactory(World& world, Graphics::ModelRegistry& registry) : m_world(world), m_registry(registry)
        {
        }

        // ---- 基本: Transform + RenderComp だけのエンティティ -------------------
        // 最もシンプルな可視オブジェクト。
        // 戻り値の EntityID を呼び出し元で保持しておくと後から Component を追加できる。
        EntityID CreateRenderableEntity(
            Graphics::ModelID modelId,
            const DirectX::SimpleMath::Vector3& position = {0.f, 0.f, 0.f},
            const DirectX::SimpleMath::Quaternion& rotation = DirectX::SimpleMath::Quaternion::Identity,
            const DirectX::SimpleMath::Vector3& scale = {1.f, 1.f, 1.f});
        

        // ---- 位置だけ指定する簡易版 --------------------------------------------
        //EntityID CreateRenderableEntity(Graphics::ModelID modelId, const DirectX::SimpleMath::Vector3& position);
        

        EntityID CreatePlayer(Graphics::ModelID modelId, const DirectX::SimpleMath::Vector3& position,
                     const DirectX::SimpleMath::Quaternion& rotation = DirectX::SimpleMath::Quaternion::Identity,
                     const DirectX::SimpleMath::Vector3& scale = {1.f, 1.f, 1.f});

        //EntityID CreateEnemy(Graphics::ModelID modelId, 
        //                     const DirectX::SimpleMath::Vector3& position = {0.f,0.f,0.f},
        //                     const DirectX::SimpleMath::Vector3& scale = {1.f, 1.f, 1.f});

        EntityID CreateEnemy(Graphics::ModelID modelId, const DirectX::SimpleMath::Vector3& position,
                             const DirectX::SimpleMath::Vector3& scale, float mass = 1.0f);

        EntityID CreateGround(Graphics::ModelID modelId, const DirectX::SimpleMath::Vector3& position,
                              const DirectX::SimpleMath::Quaternion& rotation,
                              const DirectX::SimpleMath::Vector3& scale);

        EntityID CreateSun(const DirectX::SimpleMath::Vector3& position = {10.f, 20.f, 10.f},
                           const DirectX::SimpleMath::Vector3& color = {1.f, 0.95f, 0.8f}, const float intensity = 1.f,
                           DirectX::SimpleMath::Vector3 direction = {-0.5f, -1.f, -0.5f}
        );

        EntityID CreateCamera(const DirectX::SimpleMath::Vector3& position = {0.f, 5.f, -10.f},
                     const DirectX::SimpleMath::Quaternion& rotation = DirectX::SimpleMath::Quaternion::Identity,
                     const int priority = 0,         
                     const float fov = 45.f,
                     const float nearClip = 0.1f,
                     const float farClip = 1000.f,
                     const bool isPerspective = true,
                     const bool isActive = true
                     );

        EntityID CreatePlayerFollowCamera(
            const DirectX::SimpleMath::Vector3& position = {0.f, 5.f, -10.f},
            const DirectX::SimpleMath::Quaternion& rotation = DirectX::SimpleMath::Quaternion::Identity,
            const int priority = 0, const float fov = DirectX::XMConvertToRadians(60.f),
            const float nearClip = 0.1f, const float farClip = 1000.f,
            const bool isPerspective = true, const bool isActive = true);


    private:
        World& m_world;
        Graphics::ModelRegistry& m_registry;
    };

    

} // namespace ECS