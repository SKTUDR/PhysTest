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

    // --- 固定タイムステップ設定 ---------------------------------------
    m_runner.SetFixedDeltaTime(1.0f / 60.f);
    m_runner.SetMaxSteps(3);

    // ---- システムの構築 --------------------------------------
    InitializeSystems(device, context);

    // ---- モデルを Registry に登録 --------------------------------
    LoadModels(device);

    // ---- オーディオを Registry に登録 -------------------------------
    LoadAudios();

    // ---- Entity を生成して ECS ワールドに登録 ------------------------------
    CreateSceneObject();
}


ECS::EntityID SceneB::SpawnRubble(DirectX::SimpleMath::Vector3 pos)
{
    ECS::EntityID eid = m_factory->CreateRubble(m_itemModelId, pos, SimpleMath::Vector3{1.f, 1.f, 1.f},
                                                RandomGenerator::GetInstance().RandFloat(0.0f, 1.0f));

    return eid;
}

void SceneB::InitializeSystems(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_audioRegistry.Initialize();
    m_audioSystem.Initialize(m_audioRegistry);

    m_renderSystem = std::make_unique<ECS::RenderSystem>(m_modelRegistry);
    m_factory = std::make_unique<ECS::EntityFactory>(m_world, m_modelRegistry, m_audioRegistry);
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

void SceneB::LoadAudios()
{
    m_bgmClipID = m_audioRegistry.Load(L"Resources/Audio/BGM/Title2.wav");
    m_footClipID = m_audioRegistry.Load(L"Resources/Audio/SE/PlayerFoot.mp3");
}

void SceneB::CreateSceneObject()
{
    m_playerId = m_factory->CreatePlayer(m_itemModelId, m_footClipID, DirectX::SimpleMath::Vector3{0.f, 5.f, 0.f},
                                         SimpleMath::Quaternion::Identity, DirectX::SimpleMath::Vector3{1.f, 1.f, 1.f});

    //m_enemyId = m_factory->CreateEnemy(m_enemyModelId, DirectX::SimpleMath::Vector3{0.f, 5.f, -60.f},
    //                                   DirectX::SimpleMath::Vector3{1.f, 1.f, 1.f}, 60.f);

    m_floorId = m_factory->CreateGround(m_itemModelId, DirectX::SimpleMath::Vector3{0.f, -0.5f, 0.f},
                                        DirectX::SimpleMath::Quaternion::Identity,
                                        DirectX::SimpleMath::Vector3{100.f, 3.f, 100.f});

    constexpr int kSpawnCount = 30 * 20;
    SpawnRubbles(kSpawnCount);

    m_cameraId = m_factory->CreatePlayerFollowCamera();

    m_bgmId = m_factory->CreateBGM(m_bgmClipID, 2.0f);
}

void SceneB::UpdateGame(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    auto& input = gameContext.m_inputSystem;

    if (input.GetAction(Input::InputAction::AltAttack).pressed)
    {
        m_world.GetComponent<ECS::AudioSourceComp>(m_bgmId).requestPause = true;
    }

    m_input.Update(gameContext);

    // Rigidbody、Colliderがついていない動体
    //m_simpleMoveSys.Update(m_world, dt);
    //m_fullMoveSys.Update(m_world, dt);

    m_playerMoveSys.Update(m_world, gameContext, dt);
    
    // m_enemySys.Update(m_world, fixedDt);

    m_collisionSys.Update(m_world, dt);
    m_physicsSys.IntegrateVelocity(m_world, dt);
    m_physicsSys.ResolveCollisions(m_world, dt);
    m_physicsSys.IntegrateTransform(m_world, dt);

    //m_runner.Tick(dt,
    //              [&](float fixedDt)
    //              {
    //                  m_playerMoveSys.Update(m_world, gameContext, fixedDt);
    //                  m_rubbleController.Update(m_world, fixedDt, gameContext);
    //                  // m_enemySys.Update(m_world, fixedDt);

    //                  m_collisionSys.Update(m_world, fixedDt);

    //                  m_physicsSys.ResolveCollisions(m_world, fixedDt);
    //                  m_physicsSys.IntegrateVelocity(m_world, fixedDt);
    //                  m_physicsSys.IntegrateTransform(m_world, fixedDt);
    //              });
    //
    m_playerSys.Update(sceneController, m_world, gameContext);

    // SceneGraphSystem の更新（親子関係の Transform 更新）
    
    m_sceneGraphSys.Update(m_world);

    m_rubbleController.Update(m_world, dt, gameContext);

    // 追従カメラのアップデート（エンティティが移動するシステムの後）
    auto x = input.GetValue(Input::InputAction::LookX);
    auto y = input.GetValue(Input::InputAction::LookY);

    m_folCameraSys.AddMouseDelta(x, y);
    m_folCameraSys.Update(m_world, dt);

    // カメラアップデート
    m_cameraSys.Update(m_world, dt);

    // ゲーム全体のアップデート
    m_gameDirector.Update(sceneController, m_world, gameContext);

    m_audioSystem.Update(m_world);
}

void SceneB::SpawnRubbles(int count)
{
    constexpr int kColumns = 30;        // 幅
    constexpr float kRowSpacing = 2.0f; // 縦の間隔
    constexpr float kColSpacing = 2.0f; // 横の間隔
    constexpr float kStartY = 4.0f;
    constexpr float kBaseZ = -30.0f;

    m_rubbleIds.clear();
    m_rubbleIds.reserve(count);

    for (int index = 0; index < count; ++index)
    {
        const int row = index / kColumns; // 何段目か
        const int col = index % kColumns; // 横の何番目か

        const float x = (col - kColumns * 0.5f) * kColSpacing;
        const float y = kStartY + row * kRowSpacing;
        const float z = kBaseZ; // 固定

        ECS::EntityID eid = SpawnRubble({x, y, z});
        m_rubbleIds.push_back(eid);
    }
}
