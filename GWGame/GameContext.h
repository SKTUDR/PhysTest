//--------------------------------------------------------------------------------------
// File: GameContext.h
//
// ゲーム全体に受け渡す情報群
//
// Date: 2026.3.3
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#pragma once

#include "Common/StepTimer.h"
#include "Common/DeviceResources.h"
#include "ImaseLib/DebugRenderer.h"
#include "ImaseLib/DebugCamera.h"
#include "ImaseLib/GridFloor.h"
#include "ResourceManager/ShaderManager.h"
#include "KUtil/EventBus.h"

// 　ゲーム全体で共有するコンテキスト
struct GameContext
{
	// タイマー
	DX::StepTimer& timer;

	// デバイスリソース
	DX::DeviceResources& deviceResources;

	// キーボードトラッカー
	DirectX::Keyboard::KeyboardStateTracker& keyboardTracker;

	// マウストラッカー
	DirectX::Mouse::ButtonStateTracker& mouseButtonTracker;

	// コモンステート
	DirectX::CommonStates& commonStates;

	// デバッグ描画用
	Imase::DebugRenderer& debugRenderer;

	// イベントバス
    EventBus& eventBus;

	// シェーダーマネージャー
    ResourceManager::ShaderManager& shaderManager;

	// プロジェクション行列
    DirectX::SimpleMath::Matrix& projection;

	// バックバッファの RTV
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;

    // 画面解像度の深度バッファ
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;

	// 画面全体のビューポート
    D3D11_VIEWPORT screenViewport;
};

