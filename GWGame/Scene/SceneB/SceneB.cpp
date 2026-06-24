//--------------------------------------------------------------------------------------
// File: SceneB.cpp
//
// 新規シーン作成時の元にするファイル
//
// Date: 2026.4.13
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "SceneB.h"
#include "../../Components/Physics.h"

#include "../../KUtil/Random.h"

using namespace DirectX;

// 更新
void SceneB::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    m_world.ClearOldFrameEvents();
    m_world.FlushEvents();
    
	Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;
    debugRenderer.DrawText(DirectX::SimpleMath::Vector2{100.0f, 0.0f}, std::to_wstring(gameContext.timer.GetFramesPerSecond()));

    UpdateGame(sceneController, gameContext);

    m_world.DestroyEntitiesProcess();
}

// 描画
void SceneB::Render(GameContext& gameContext)
{
	gameContext;

    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    auto view = m_world.GetComponent<ECS::CameraComp>(m_cameraId).view;
    auto proj = m_world.GetComponent<ECS::CameraComp>(m_cameraId).projection;

    
#ifdef _DEBUG
    m_gridFloor->Render(context,view, proj);
#endif


    m_renderSystem->Update(m_world, context, gameContext.commonStates);

    m_colliderDebug.Render(m_world, context, view, proj);
}

// シーン切り替え時に呼び出される関数
void SceneB::OnEnter(GameContext& gameContext)
{
    gameContext;

    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    // 画面サイズを取得
    RECT rect = gameContext.deviceResources.GetOutputSize();

    // --- デバッグ用 -----------------------------------------
    m_gridFloor.reset(new Imase::GridFloor(device, context, &gameContext.commonStates));

    // ---- システムの構築 --------------------------------------
    InitializeSystems(device, context);

    // ---- モデルを Registry に登録 --------------------------------
    LoadModels(device);

    // ---- Entity を生成して ECS ワールドに登録 ------------------------------
    CreateSceneObject();
}


ECS::EntityID SceneB::SpawnEnemy(DirectX::SimpleMath::Vector3 pos)
{
    ECS::EntityID eid = m_factory->CreateEnemy(m_itemModelId, pos, SimpleMath::Vector3{.5f,.5f,.5f}, RandomGenerator::GetInstance().RandFloat(0.0f, 1.0f));

    return eid;
}

void SceneB::InitializeSystems(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_renderSystem = std::make_unique<ECS::RenderSystem>(m_modelRegistry);
    m_factory = std::make_unique<ECS::EntityFactory>(m_world, m_modelRegistry);
    m_shadowRenderer = std::make_unique<Graphics::ShadowRenderer>(device, m_modelRegistry);
    m_colliderDebug.Initialize(device, context);
}

void SceneB::LoadModels(ID3D11Device* device)
{
    m_blockModelId = m_modelRegistry.LoadCMO(device, L"Resources/Models/Ground.cmo");
    m_enemyModelId = m_modelRegistry.LoadCMO(device, L"Resources/Models/bone.cmo");
    m_playerModelId = m_modelRegistry.LoadCMO(device, L"Resources/Models/Monkey.cmo");
    m_itemModelId = m_modelRegistry.LoadCMO(device, L"Resources/Models/Cube.cmo");
}

void SceneB::CreateSceneObject()
{
    m_playerId = m_factory->CreatePlayer(m_itemModelId, DirectX::SimpleMath::Vector3{0.f, 5.f, 0.f},
                                         SimpleMath::Quaternion::Identity, DirectX::SimpleMath::Vector3{.5f, .5f, .5f});

    m_floorId = m_factory->CreateGround(m_itemModelId, DirectX::SimpleMath::Vector3{0.f, -0.5f, 0.f},
                                        DirectX::SimpleMath::Quaternion::Identity,
                                        DirectX::SimpleMath::Vector3{100.f, 3.f, 100.f});

    constexpr int kSpawnCount = 2000;
    SpawnEnemies(kSpawnCount);

    m_cameraId = m_factory->CreatePlayerFollowCamera();
}

void SceneB::UpdateGame(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    m_input.Update(gameContext);

    m_playerMoveSys.Update(m_world, m_input, gameContext, dt);
    m_playerSys.Update(sceneController, m_world, gameContext);
    m_enemySys.Update(m_world, dt, gameContext);

    // Rigidbody、Colliderがついていない動体
    m_simpleMoveSys.Update(m_world, dt);
    m_fullMoveSys.Update(m_world, dt);

    // 物理システム
    m_collisionSys.Update(m_world, dt);
    m_physicsSys.IntegrateVelocity(m_world, dt);
    m_physicsSys.ResolveCollisions(m_world, dt);
    m_physicsSys.IntegrateTransform(m_world, dt);

    // 追従カメラのアップデート（エンティティが移動するシステムの後）
    m_folCameraSys.AddMouseDelta(m_input.MouseDeltaX(), m_input.MouseDeltaY());
    m_folCameraSys.Update(m_world, dt);

    // カメラアップデート
    m_cameraSys.Update(m_world, dt);

    // ゲーム全体のアップデート
    m_gameDirector.Update(sceneController, m_world, gameContext);
}

void SceneB::SpawnEnemies(int count)
{
    constexpr int kColumns = 20;
    constexpr float kRowSpacing = 2.0f;
    constexpr float kStartY = 10.0f;
    constexpr float kBaseZ = 30.0f;

    auto& rng = RandomGenerator::GetInstance();

    m_enemyIds.clear();
    m_enemyIds.reserve(count);

    constexpr float kSpawnWidth = 75.f;
    constexpr float kSpawnDepth = 50.f;

    for (int index = 0; index < count; ++index)
    {
        const int row = index / kColumns;

        const DirectX::SimpleMath::Vector3 position{rng.RandFloat(-kSpawnWidth, kSpawnWidth), kStartY + row * kRowSpacing,
                                                    kBaseZ + rng.RandFloat(-kSpawnDepth, kSpawnDepth)};

        ECS::EntityID eid = SpawnEnemy(position);

        m_enemyIds.push_back(eid);
    }
}