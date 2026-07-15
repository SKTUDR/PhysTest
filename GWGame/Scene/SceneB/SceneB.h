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

#include "../FixedUpdateRunner.h"

#include "../../Graphics/ModelRegistry.h"
#include "../../Graphics/ShadowRenderer.h"
#include "../../Graphics/ColliderDebugRenderer.h"

#include "../../Audio/AudioRegistry.h"
#include "../../Audio/AudioSystem.h"

#include "../../Components/Light.h"
#include "../../Components/CastShadow.h"

#include "../../ECS/World.h"
#include "../../ECS/Query.h"

#include "../../Systems/RenderSystem.h"
#include "../../Systems/SceneGraphSystem.h"
#include "../../Systems/Physics.h"
#include "../../Systems/PhysicsSystem.h"
#include "../../Systems/InputState.h"
#include "../../Systems/PlayerMovementSystem.h"
#include "../../Systems/EnemySystem.h"
#include "../../Systems/CollisionSystem.h"
#include "../../Systems/PlayerSystem.h"
#include "../../Systems/GameDirector.h"
#include "../../Systems/RubbleController.h"

#include "../../Systems/CameraSystem.h"
#include "../../Systems/FollowCameraSystem.h"

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

    // 固定タイムステップ
    FixedUpdateRunner m_runner;

	// 正しい順番で宣言する
	Graphics::ModelRegistry m_modelRegistry; 
	ECS::World m_world;                      
	std::unique_ptr<ECS::RenderSystem> m_renderSystem;  
	std::unique_ptr<ECS::EntityFactory> m_factory;   
	std::unique_ptr<Graphics::ShadowRenderer> m_shadowRenderer;
    Graphics::ColliderDebugRenderer m_colliderDebug;

    Audio::AudioRegistry m_audioRegistry;
    Audio::AudioSystem m_audioSystem;



	// ロード済み ModelID
	Graphics::ModelID m_blockModelId    = Graphics::INVALID_MODEL_ID;
	Graphics::ModelID m_enemyModelId    = Graphics::INVALID_MODEL_ID;
    Graphics::ModelID m_playerModelId   = Graphics::INVALID_MODEL_ID;
    Graphics::ModelID m_itemModelId     = Graphics::INVALID_MODEL_ID;

    // ロード済み AudioID
    Audio::ClipID m_bgmClipID    = Audio::INVALID_CLIP_ID;
    Audio::ClipID m_footClipID   = Audio::INVALID_CLIP_ID;


	// 生成済み EntityID
	ECS::EntityID m_floorId	 = ECS::EntityID::Null();
    ECS::EntityID m_enemyId = ECS::EntityID::Null();
	ECS::EntityID m_playerId = ECS::EntityID::Null();
    ECS::EntityID m_taihouId = ECS::EntityID::Null();


    ECS::EntityID m_cameraId = ECS::EntityID::Null();

    ECS::EntityID m_sunId	 = ECS::EntityID::Null(); 
    ECS::EntityID m_bgmId = ECS::EntityID::Null();

    float m_fixedAccumulator = 0.0f;

    //  ---- エネミー EntityID ------------------------------------------
    //  n体を vector で管理。
    std::vector<ECS::EntityID> m_rubbleIds;

    std::unique_ptr<Imase::GridFloor>   m_gridFloor;

	// --- Systems ---------------------------------
    ECS::PlayerMovementSystem   m_playerMoveSys;
    ECS::EnemySystem            m_enemySys;
    ECS::SimpleMovementSystem   m_simpleMoveSys;
    ECS::FullMovementSystem     m_fullMoveSys;
    ECS::CollisionSystem        m_collisionSys;
    ECS::PhysicsSystem          m_physicsSys;
    ECS::PlayerSystem           m_playerSys;
    ECS::GameDirector           m_gameDirector;
    ECS::RubbleController       m_rubbleController;
    ECS::SceneGraphSystem       m_sceneGraphSys;  

	ECS::CameraSystem       m_cameraSys;
    ECS::FollowCameraSystem m_folCameraSys;

	Input::InputState m_input;


	// --- ヘルパ関数 -----------------------------
	// =================================================================
    //  SpawnEnemy - 1体分の生成
    // =================================================================
    ECS::EntityID SpawnRubble(DirectX::SimpleMath::Vector3 pos);

    void InitializeSystems(ID3D11Device* device, ID3D11DeviceContext* context);
    void LoadModels(ID3D11Device* device);
    void LoadAudios();
    void CreateSceneObject();
    void UpdateGame(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext);

	// =================================================================
    //  SpawnEnemies - 格子状に配置
    // =================================================================
    void SpawnRubbles(int count);
};

