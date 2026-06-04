//--------------------------------------------------------------------------------------
// File: GridFloor.cpp
//
// �O���b�h�̏���`�悷��N���X
//
// Date: 2023.5.6
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "GridFloor.h"

using namespace DirectX;
using namespace Imase;

// ����1�ӂ̃T�C�Y
const  float GridFloor::FLOOR_SIZE = 10.0f;

GridFloor::GridFloor(
	ID3D11Device* pDevice,
	ID3D11DeviceContext* pContext,
	CommonStates* pStates,
	FXMVECTOR color,
	float size,
	size_t divs
)
	: m_pStates(pStates)
	, m_color(color)
	, m_size(size)
	, m_divs(divs)
{
	// �v���~�e�B�u�o�b�`�̍쐬
	m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(pContext);

	// �x�[�V�b�N�G�t�F�N�g�̍쐬
	m_basicEffect = std::make_unique<BasicEffect>(pDevice);
	m_basicEffect->SetVertexColorEnabled(true);
	m_basicEffect->SetLightingEnabled(false);
	m_basicEffect->SetTextureEnabled(false);

	// ���̓��C�A�E�g�̍쐬
	DX::ThrowIfFailed(
		CreateInputLayoutFromEffect<VertexPositionColor>(
			pDevice,
			m_basicEffect.get(),
			m_inputLayout.ReleaseAndGetAddressOf()
			)
	);
}

void GridFloor::Render(
	ID3D11DeviceContext* pContext,
	const SimpleMath::Matrix& view,
	const SimpleMath::Matrix& proj
)
{
	// �u�����h�X�e�[�g�̐ݒ�i�s�����j
	pContext->OMSetBlendState(m_pStates->Opaque(), nullptr, 0xFFFFFFFF);
	// �[�x�o�b�t�@�̐ݒ�i�ʏ�j
	pContext->OMSetDepthStencilState(m_pStates->DepthDefault(), 0);
	// �J�����O�̐ݒ�i�J�����O�Ȃ��j
	pContext->RSSetState(m_pStates->CullNone());

	// �e�s��̐ݒ�
	SimpleMath::Matrix world;
	m_basicEffect->SetWorld(world);
	m_basicEffect->SetView(view);
	m_basicEffect->SetProjection(proj);

	// �G�t�F�N�g��K�p����
	m_basicEffect->Apply(pContext);

	// ���̓��C�A�E�g��ݒ�
	pContext->IASetInputLayout(m_inputLayout.Get());

	// �O���b�h�̏���`��
	m_primitiveBatch->Begin();

	DX::DrawGrid(
		m_primitiveBatch.get(),
		SimpleMath::Vector3(m_size / 2.0f, 0.0f, 0.0f),
		SimpleMath::Vector3(0.0f, 0.0f, m_size / 2.0f),
		SimpleMath::Vector3(0.0f, 0.0f, 0.0f),
		m_divs, m_divs,
		m_color
	);

	m_primitiveBatch->End();
}
