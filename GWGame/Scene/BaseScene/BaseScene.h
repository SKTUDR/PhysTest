//--------------------------------------------------------------------------------------
// File: BaseScene.h
//
// �V�K�V�[���쐬���̌��ɂ���t�@�C��
//
// Date: 2026.4.13
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#pragma once

#include "../../ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"

class BaseScene : public Imase::SceneBase<SceneId, GameContext>
{
public:

	// �X�V
	void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

	// �`��
	void Render(GameContext& gameContext) override;

	// �V�[���؂�ւ����ɌĂяo�����֐�
	void OnEnter(GameContext& gameContext) override;

};

