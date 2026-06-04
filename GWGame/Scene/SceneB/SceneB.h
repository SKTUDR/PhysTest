//--------------------------------------------------------------------------------------
// File: SceneB.h
//
// 新規シーン作成時の元にするファイル
//
// Date: 2026.4.13
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#pragma once

#include "../../ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"

#include "../../Graphics/ModelRegistry.h"
#include "../../Graphics/ShadowRenderer.h"
#include "../../Graphics/ColliderDebugRenderer.h"

#include "../../Components/Light.h"
#include "../../Components/CastShadow.h"

#include "../../ECS/World.h"
#include "../../ECS/Query.h"

#include "../../Systems/RenderSystem.h"
#include "../../Systems/Physics.h"
#include "../../Systems/PhysicsSystem.h"
#include "../../Systems/InputState.h"
#include "../../Systems/PlayerMovementSystem.h"
#include "../../Systems/EnemySystem.h"
#include "../../Systems/CollisionSystem.h"
#include "../../Systems/PlayerSystem.h"
#include "../../Systems/GameDirector.h"

#include "SetupEntities.h"

class SceneB : public Imase::SceneBase<SceneId, GameContext>
{
public:

	// 更新
	void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

	// 描画
	void Render(GameContext& gameContext) override;

	// シーン切り替え時に呼び出される関数
	void OnEnter(GameContext& gameContext) override;

private:

	// 正しい順番で宣言する
	Graphics::ModelRegistry m_modelRegistry; 
	ECS::World m_world;                      
	std::unique_ptr<ECS::RenderSystem> m_renderSystem;  
	std::unique_ptr<ECS::EntityFactory> m_factory;   
	std::unique_ptr<Graphics::ShadowRenderer> m_shadowRenderer;
    Graphics::ColliderDebugRenderer m_colliderDebug;

	// ロード済み ModelID
	Graphics::ModelID m_blockModelId = Graphics::INVALID_MODEL_ID;
	Graphics::ModelID m_enemyModelId = Graphics::INVALID_MODEL_ID;
    Graphics::ModelID m_playerModelId = Graphics::INVALID_MODEL_ID;
    Graphics::ModelID m_itemModelId = Graphics::INVALID_MODEL_ID;

	// 生成済み EntityID
	ECS::EntityID m_floorId = ECS::EntityID::Null()	;
	ECS::EntityID m_playerId = ECS::EntityID::Null();

    ECS::EntityID m_sunId = ECS::EntityID::Null(); 

    //  ---- エネミー EntityID ------------------------------------------
    //  100体を vector で管理。
    //  ここでは「特定エネミーを番号で引きたい」ユースケースのために保持する。
    std::vector<ECS::EntityID> m_enemyIds;

	std::unique_ptr<Imase::DebugCamera> m_debugCamera;
    std::unique_ptr<Imase::GridFloor>   m_gridFloor;
		
	// プロジェクション行列
	DirectX::SimpleMath::Matrix m_projection;

	// --- Systems ---------------------------------
    ECS::PlayerMovementSystem m_playerMoveSys;
    ECS::EnemySystem m_enemySys;
    ECS::SimpleMovementSystem m_simpleMoveSys;
    ECS::FullMovementSystem m_fullMoveSys;
    ECS::CollisionSystem m_collisionSys;
    ECS::PhysicsSystem m_physicsSys;
    ECS::PlayerSystem m_playerSys;
    ECS::GameDirector m_gameDirector;

	Input::InputState m_input;

	// --- シェーダー --------------------------------
    const ResourceManager::ShaderSet* m_sceneShader = nullptr;
    const ResourceManager::ShaderSet* m_shadowShader = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_sceneCB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_shadowPassCB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_lightCB;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_shadowSampler;


	// --- ヘルパ関数 -----------------------------

	// プロジェクション行列を設定する関数
    DirectX::SimpleMath::Matrix CreateProjectionMatrix(GameContext& gameContext);

	// =================================================================
    //  SpawnEnemy - 1体分の生成
    // =================================================================
    ECS::EntityID SpawnEnemy(DirectX::SimpleMath::Vector3 pos, float speed);

	// =================================================================
    //  SpawnEnemies - 格子状に配置
    // =================================================================
    void SpawnEnemies(int count);
    
	// プロジェクション行列設定用
    float m_fov = 45.f;
    float m_nearClip = 0.1f;
    float m_farClip = 1000.f;

	float m_aliveTimer = 0.f;
};

