//--------------------------------------------------------------------------------------
// File: SceneA.cpp
//
// 新規シーン作成時の元にするファイル
//
// Date: 2026.4.13
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "SceneA.h"

using namespace DirectX;

// 更新
void SceneA::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
	Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;

	if (gameContext.keyboardTracker.pressed.Space)
	{
        sceneController.RequestSwitch(SceneId::SceneB);
	}

	debugRenderer.DrawText({ 0.0f, 0.0f }, L"Title");
}

// 描画
void SceneA::Render(GameContext& gameContext)
{
	gameContext;
}

// シーン切り替え時に呼び出される関数
void SceneA::OnEnter(GameContext& gameContext)
{
	gameContext;
}
