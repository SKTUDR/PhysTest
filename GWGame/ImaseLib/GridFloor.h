//--------------------------------------------------------------------------------------
// File: GridFloor.h
//
// �O���b�h�̏���`�悷��N���X
//
// Date: 2023.5.6
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#pragma once

namespace Imase
{
	// �O���b�h�̏���\������^�X�N
	class GridFloor
	{
	public:

		// �R���X�g���N�^
		GridFloor(
			ID3D11Device* pDevice,
			ID3D11DeviceContext* pContext,
			DirectX::CommonStates* pStates,
			DirectX::FXMVECTOR color = DirectX::Colors::LightGray,
			float size = FLOOR_SIZE,
			size_t divs = FLOOR_DIVS
		);

		// �`��
		void Render(
			ID3D11DeviceContext* pContext,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);

	private:

		// ���ʃX�e�[�g�ւ̃|�C���^
		DirectX::CommonStates* m_pStates;

		// �x�[�V�b�N�G�t�F�N�g�ւ̃|�C���^
		std::unique_ptr<DirectX::BasicEffect> m_basicEffect;

		// �v���~�e�B�u�o�b�`�ւ̃|�C���^
		std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;

		// ���̓��C�A�E�g
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

		// 大きさ
		float m_size;

		// 分割数
		size_t m_divs;

		// 色
		DirectX::SimpleMath::Color m_color;

	public:

		// ����1�ӂ̃T�C�Y
		static const float FLOOR_SIZE;

		// フロアの深度
		static const size_t FLOOR_DIVS = 10;

		// 大きさを設定する関数
		void SetSize(float size) { m_size = size; }

		// グリッドの分割数を設定する関数
		void SetDivs(size_t divs) { m_divs = divs; }

		// 色を設定する関数
		void SetColor(DirectX::FXMVECTOR color) { m_color = color; }

	};
}
