//
// Game.h
//

#pragma once

#include "Common/DeviceResources.h"
#include "Common/StepTimer.h"
#include "PostProcess.h"

#include <memory>
#include <optional>

#include "ImaseLib/DebugRenderer.h"

#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "Scene/SceneId.h"
#include "ResourceManager/ShaderManager.h"
#include "KUtil/EventBus.h"

// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game() = default;

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // --------------------------------------------------------------------- //

private:

    // キーボードトラッカー
    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;

    // マウスボタントラッカー
    DirectX::Mouse::ButtonStateTracker m_mouseButtonTracker;

    // コモンステート
    std::unique_ptr<DirectX::CommonStates> m_states;

    // デバッグ用の描画セット
    std::unique_ptr<Imase::DebugRenderer> m_debugRenderer;

    std::unique_ptr<EventBus> m_eventBus;

    // ゲームコンテキスト
    std::optional<GameContext> m_gameContext;   

    // シーンマネージャー
    Imase::SceneManager<SceneId, GameContext> m_sceneManager;

    // シェーダーマネージャー
    std::unique_ptr<ResourceManager::ShaderManager> m_shaderManager;

    // プロジェクション行列
    DirectX::SimpleMath::Matrix m_projection;

    // バックバッファの RTV
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_backBufferRTV;

    // 画面解像度の深度バッファ
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    // 画面全体のビューポート
    D3D11_VIEWPORT m_screenViewport;

    IDXGISwapChain* m_swapChain;

    

    std::unique_ptr<DirectX::BasicPostProcess> m_postProcess;

};
