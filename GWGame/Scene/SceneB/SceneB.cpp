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
    m_world.ClearEvents();

    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());

	Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;
	Imase::DebugRenderer& debugRenderer2 = gameContext.debugRenderer;
    Imase::DebugRenderer& debugRenderer3 = gameContext.debugRenderer;
    Imase::DebugRenderer& debugRenderer4 = gameContext.debugRenderer;

	
    debugRenderer.DrawText(DirectX::SimpleMath::Vector2{100.0f, 0.0f}, std::to_wstring(gameContext.timer.GetFramesPerSecond()));
    
    //debugRenderer3.DrawText(DirectX::SimpleMath::Vector2{100.0f, 40.0f}, std::to_wstring(m_enemyIds.size()));

    m_debugCamera->Update();

    m_input.Update(gameContext);

    m_playerMoveSys.Update(m_world, m_input, gameContext);

    m_enemySys.Update(m_world, dt, gameContext);
    
    m_simpleMoveSys.Update(m_world, dt);

    m_fullMoveSys.Update(m_world, dt);  

    m_physicsSys.IntegrateVelocity(m_world, dt);

    m_collisionSys.Update(m_world, dt);

    m_physicsSys.ResolveCollisions(m_world, dt);

    m_physicsSys.IntegrateTransform(m_world, dt);

    m_playerSys.Update(sceneController, m_world, gameContext);

    auto& GRTR = m_world.GetComponent<ECS::TransformComp>(m_floorId);

    debugRenderer2.DrawText(DirectX::SimpleMath::Vector2{100.0f, 20.0f},
                            std::to_wstring(GRTR.position.x) + L", " +
                            std::to_wstring(GRTR.position.y) + L", " +
                            std::to_wstring(GRTR.position.z));

    //auto& ENRB = m_world.GetComponent<ECS::RigidbodyComp>(m_enemyIds[0]);

    //debugRenderer4.DrawText(DirectX::SimpleMath::Vector2{100.0f, 100.0f}, std::to_wstring(ENRB.velocity.y));

    m_gameDirector.Update(sceneController, m_world, gameContext);
}

// 描画
void SceneB::Render(GameContext& gameContext)
{
	gameContext;

    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    m_aliveTimer += dt;

    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    auto view = m_debugCamera->GetCameraMatrix();

    //context->VSSetShader(m_shadowShader->vs.Get(), nullptr, 0);
    //context->PSSetShader(nullptr, nullptr, 0); // PS 不要
    //context->IASetInputLayout(m_shadowShader->inputLayout.Get());

    //// 1. ECS からライト情報を収集
    //m_shadowRenderer->CollectLights(m_world);

    //// 2. 各ライト視点でシャドウパス
    ////    （CastShadowComp を持つエンティティのみ描画）
    //m_shadowRenderer->ShadowPass(m_world, context);

    //// 3. バックバッファに戻してライティングパス
    ////    （シャドウマップを SRV としてバインドした状態で通常描画）
    //ID3D11RenderTargetView* rtv = gameContext.backBufferRTV.Get();
    //ID3D11DepthStencilView* dsv = gameContext.depthStencilView.Get();
    //context->OMSetRenderTargets(1, &rtv, dsv);

    //context->RSSetViewports(1, &gameContext.screenViewport);
    //m_shadowRenderer->LightingPass(m_world, context, view, m_projection, gameContext.commonStates);
    
#ifdef _DEBUG
    m_gridFloor->Render(context,view, m_projection);
#endif

    //context->VSSetShader(m_sceneShader->vs.Get(), nullptr, 0);
    //context->PSSetShader(m_sceneShader->ps.Get(), nullptr, 0);
    //context->IASetInputLayout(m_sceneShader->inputLayout.Get());
    //
    //// シャドウマップ SRV をバインド
    //auto* shadowSRV = m_shadowRenderer->GetShadowMapSRV(0);
    //context->PSSetShaderResources(4, 1, &shadowSRV);

    m_renderSystem->Update(m_world, context, view, m_projection, gameContext.commonStates);

#ifdef _DEBUG
    m_colliderDebug.Render(m_world, context, view, m_projection);
#endif

    

    //m_shadowRenderer->CollectLights(m_world);
    //
    //// ======== シャドウパス ========================================
    //// シェーダーをシャドウ用に切り替える
    //context->VSSetShader(m_shadowShader->vs.Get(), nullptr, 0);
    //context->PSSetShader(nullptr, nullptr, 0);  // PS 不要
    //context->IASetInputLayout(m_shadowShader->inputLayout.Get());
    //
    //// CastShadowComp を持つエンティティをライト視点で描画
    //for (int i = 0; i < m_shadowRenderer->ActiveLightCount(); ++i) {
    //    // シャドウマップの DSV をバインド・クリア
    //    // （ShadowRenderer::ShadowPass 内で行う）
    //
    //    // エンティティごとに定数バッファを更新して描画
    //    auto desc = ECS::QueryBuilder{}
    //        .All<ECS::TransformComp, ECS::ModelRenderComp, ECS::CastShadowComp>()
    //        .Build();
    //
    //    m_world.Query(desc).Each<ECS::TransformComp, ECS::ModelRenderComp>(
    //        [&](ECS::EntityID, const ECS::TransformComp& tr,
    //                        const ECS::ModelRenderComp& ren)
    //    {
    //        ResourceManager::ShadowPassCB cb;
    //        cb.world         = XMMatrixTranspose(tr.ToWorldMatrix());
    //        cb.lightViewProj = XMMatrixTranspose(
    //            m_shadowRenderer->GetLightView(i));
    //        ResourceManager::ShaderManager::UpdateConstantBuffer(context, m_shadowPassCB.Get(), cb);
    //        context->VSSetConstantBuffers(0, 1, m_shadowPassCB.GetAddressOf());
    //
    //        auto* model = m_modelRegistry.Get(ren.modelId);
    //        if (model) model->Draw(context, gameContext.commonStates,
    //            tr.ToWorldMatrix(),
    //            m_shadowRenderer->GetLightView(i),
    //            m_shadowRenderer->GetLightProj(i));


    //    });
    //}
    //
    //// ======== 本描画パス ==========================================
    //// バックバッファに戻す
    //context->OMSetRenderTargets(1, &gameContext.backBufferRTV, gameContext.depthStencilView.Get());
    //context->RSSetViewports(1, &gameContext.screenViewport);
    //
    //// シェーダーを本描画用に切り替える
    //context->VSSetShader(m_sceneShader->vs.Get(), nullptr, 0);
    //context->PSSetShader(m_sceneShader->ps.Get(), nullptr, 0);
    //context->IASetInputLayout(m_sceneShader->inputLayout.Get());
    //
    //// シャドウマップ SRV をバインド
    //auto* shadowSRV = m_shadowRenderer->GetShadowMapSRV(0);
    //context->PSSetShaderResources(4, 1, &shadowSRV);
    //
    //// ライト定数バッファを更新
    //ResourceManager::LightCB lightData;
    //lightData.lightDir         = { 0.5f, 1.f, 0.5f };  // ライト方向
    //lightData.lightIntensity   = 1.f;
    //lightData.lightColor       = { 1.f, 0.95f, 0.8f };
    //lightData.ambientIntensity = 0.2f;
    //lightData.cameraPos = m_debugCamera->GetEyePosition();
    //lightData.shadowBias       = 0.001f;
    //ResourceManager::ShaderManager::UpdateConstantBuffer(context, m_lightCB.Get(), lightData);
    //context->PSSetConstantBuffers(1, 1, m_lightCB.GetAddressOf());
    //
    //// エンティティごとに描画
    //auto desc = ECS::QueryBuilder{}
    //    .All<ECS::TransformComp, ECS::ModelRenderComp>()
    //    .Build();
    //
    //m_world.Query(desc).Each<ECS::TransformComp, ECS::ModelRenderComp>(
    //    [&](ECS::EntityID, const ECS::TransformComp& tr,
    //                    const ECS::ModelRenderComp& ren)
    //{
    //
    //    ResourceManager::SceneCB sceneCB;
    //    sceneCB.world         = XMMatrixTranspose(tr.ToWorldMatrix());
    //    sceneCB.view          = XMMatrixTranspose(view);
    //    sceneCB.proj          = XMMatrixTranspose(m_projection);
    //    sceneCB.lightViewProj = XMMatrixTranspose(
    //        m_shadowRenderer->GetLightViewProj(0));
    //    ResourceManager::ShaderManager::UpdateConstantBuffer(context, m_sceneCB.Get(), sceneCB);
    //    context->VSSetConstantBuffers(0, 1, m_sceneCB.GetAddressOf());
    //
    //    m_renderSystem->Update(m_world, context, view, m_projection, gameContext.commonStates);
    //});
    //
    //// SRV をアンバインド
    //ID3D11ShaderResourceView* nullSRV = nullptr;
    //context->PSSetShaderResources(4, 1, &nullSRV);


}

// シーン切り替え時に呼び出される関数
void SceneB::OnEnter(GameContext& gameContext)
{
    gameContext;

    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    // シェーダーの設定
    // ShadowVS 用入力レイアウト
    D3D11_INPUT_ELEMENT_DESC shadowLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
        D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    gameContext.shaderManager.Load("shadow", L"Resources/Shaders/ShadowVS.hlsl", L"Resources/Shaders/ScenePS.hlsl",  shadowLayout, 1);

     D3D11_INPUT_ELEMENT_DESC sceneLayout[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,
           D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
           D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24,
           D3D11_INPUT_PER_VERTEX_DATA, 0 },
     };
    gameContext.shaderManager.Load("scene", L"Resources/Shaders/SceneVS.hlsl", L"Resources/Shaders/ScenePS.hlsl",
                                   sceneLayout, 3);

    m_shadowShader = gameContext.shaderManager.Get("shadow");
    m_sceneShader = gameContext.shaderManager.Get("scene");

    // 定数バッファ作成
    m_shadowPassCB = gameContext.shaderManager.CreateConstantBuffer<ResourceManager::ShadowPassCB>(device);
    m_sceneCB      = gameContext.shaderManager.CreateConstantBuffer<ResourceManager::SceneCB>(device);
    m_lightCB      = gameContext.shaderManager.CreateConstantBuffer<ResourceManager::LightCB>(device);

    // シャドウマップ用サンプラー（境界値 1.0 = 影なし）
    D3D11_SAMPLER_DESC shadowSamplerDesc = {};
    shadowSamplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    shadowSamplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.BorderColor[0] = 1.f; // 境界 = 1.0（影なし）
    shadowSamplerDesc.BorderColor[1] = 1.f;
    shadowSamplerDesc.BorderColor[2] = 1.f;
    shadowSamplerDesc.BorderColor[3] = 1.f;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    shadowSamplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&shadowSamplerDesc, m_shadowSampler.GetAddressOf());
     
    // シャドウサンプラーをスロット s1 にバインド（一度だけでよい）
    context->PSSetSamplers(1, 1, m_shadowSampler.GetAddressOf());


    // 画面サイズを取得
    RECT rect = gameContext.deviceResources.GetOutputSize();

    // プロジェクション行列を生成
    m_projection = CreateProjectionMatrix(gameContext);

    // --- デバッグ用 -----------------------------------------
    m_debugCamera = std::make_unique<Imase::DebugCamera>(rect.right, rect.bottom);  
    m_gridFloor.reset(new Imase::GridFloor(device, context, &gameContext.commonStates));

    // ---- システムの構築 --------------------------------------
    m_renderSystem = std::make_unique<ECS::RenderSystem>(m_modelRegistry);
    m_factory = std::make_unique<ECS::EntityFactory>(m_world, m_modelRegistry);
    m_shadowRenderer = std::make_unique<Graphics::ShadowRenderer>(device, m_modelRegistry);
    m_colliderDebug.Initialize(device, context);

    // ---- モデルを Registry に登録 --------------------------------
    m_blockModelId =  m_modelRegistry.LoadCMO(device, L"Resources/Models/Ground.cmo");
    m_enemyModelId = m_modelRegistry.LoadCMO(device, L"Resources/Models/bone.cmo");
    m_playerModelId = m_modelRegistry.LoadCMO(device, L"Resources/Models/Monkey.cmo");
    m_itemModelId = m_modelRegistry.LoadCMO(device, L"Resources/Models/Cube.cmo");

    // ---- Entity を生成して ECS ワールドに登録 ------------------------------
    // 90度回転
    constexpr float angle = DirectX::XMConvertToRadians(-90.0f);

    // X軸ベクトル
    SimpleMath::Vector3 axis = SimpleMath::Vector3::Right;

    m_floorId = m_factory->CreateGround(m_blockModelId, DirectX::SimpleMath::Vector3{0.f, -2.f, 3.f},
                                          DirectX::SimpleMath::Quaternion{SimpleMath::Quaternion::CreateFromAxisAngle(axis, angle)},
                                          DirectX::SimpleMath::Vector3{.05f, .5f, .1f});

    m_playerId = m_factory->CreatePlayer(m_itemModelId, DirectX::SimpleMath::Vector3{0.f, 1.f, 0.f},
        SimpleMath::Quaternion::CreateFromAxisAngle(axis, angle),
        DirectX::SimpleMath::Vector3{.5f, .5f, .5f});

    m_sunId = m_factory->CreateSun( DirectX::SimpleMath::Vector3{10.f, 20.f, 10.f},
                                    DirectX::SimpleMath::Vector3{1.f, 0.95f, 0.8f}, 1.f,
                                    DirectX::SimpleMath::Vector3{-0.5f, -1.f, -0.5f});

    SpawnEnemies(100);
    
    m_shadowRenderer->SetCommonStates(&gameContext.commonStates);

    m_aliveTimer = 0.f;
}

DirectX::SimpleMath::Matrix SceneB::CreateProjectionMatrix(GameContext& gameContext)
{
    SimpleMath::Matrix m;

    // 画面比率
    RECT rect = gameContext.deviceResources.GetOutputSize();
    float aspectRatio = static_cast<float>(rect.right) / static_cast<float>(rect.bottom);

    m = SimpleMath::Matrix::CreatePerspectiveFieldOfView(
        XMConvertToRadians(m_fov),  // 視野角
        aspectRatio,                // 画面比率
        m_nearClip,                 // 近い距離のクリップ
        m_farClip                   // 遠い距離のクリップ
    );

    return m;
}

ECS::EntityID SceneB::SpawnEnemy(DirectX::SimpleMath::Vector3 pos, float speed)
{
    ECS::EntityID eid = m_factory->CreateEnemy(m_itemModelId, pos, SimpleMath::Vector3{.5f,.5f,.5f}, RandomGenerator::GetInstance().RandFloat(0.0f, 1.0f));

    speed;
    //m_world.GetComponent<ECS::RigidbodyComp>(eid).AddForce(DirectX::SimpleMath::Vector3{0.f, 0.f, speed});

    return eid;
}

void SceneB::SpawnEnemies(int count)
{
    m_enemyIds.reserve(count);

    for (int i = 0; i < count / 3; ++i)
    {
        for (int j = 0; j < 3; j++)
        {
            ECS::EntityID eid = SpawnEnemy({RandomGenerator::GetInstance().RandFloat(-5.f, 5.f), static_cast<float>(i * 5), 30 + RandomGenerator::GetInstance().RandFloat(-5.f, 5.f)},
                                           RandomGenerator::GetInstance().RandFloat(-14.f, -8.f));
            m_enemyIds.push_back(eid);
        }
        

        
    }
}