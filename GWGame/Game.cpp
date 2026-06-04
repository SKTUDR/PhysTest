//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

#include <iostream>

#include "Scene/BaseScene/BaseScene.h"
#include "Scene/SceneA/SceneA.h"
#include "Scene/SceneB/SceneB.h"

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
    : m_keyboardTracker{}
    , m_mouseButtonTracker{}
    , m_states{}
    , m_debugRenderer{}
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */

    auto device = m_deviceResources->GetD3DDevice();

    ComPtr<IDXGISwapChain1> swapChain = m_deviceResources->GetSwapChain();
    if (!swapChain)
        throw std::runtime_error("SwapChain missing");

    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr))
    { /* エラーハンドリング */
    }

    swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_backBufferRTV.GetAddressOf());



    // 画面サイズの深度バッファを作成
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
    device->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf());
    device->CreateDepthStencilView(depthTex.Get(), nullptr, m_depthStencilView.GetAddressOf());

    m_screenViewport.Width = static_cast<float>(width);
    m_screenViewport.Height = static_cast<float>(height);
    m_screenViewport.MinDepth = 0.f;
    m_screenViewport.MaxDepth = 1.f;
    m_screenViewport.TopLeftX = 0.f;
    m_screenViewport.TopLeftY = 0.f;




    // シーンの登録
    m_sceneManager.RegisterScene<BaseScene>(SceneId::BaseScene);
    m_sceneManager.RegisterScene<SceneA>(SceneId::SceneA);
    m_sceneManager.RegisterScene<SceneB>(SceneId::SceneB);

    // イベントバスの作成
    m_eventBus = std::make_unique<EventBus>();

    // エラーハンドラの設定
    m_eventBus->SetErrorHandler([](const std::string&   event, const std::exception& e) { assert(false && "EventBus error"); });

    // シェーダーマネージャーの作成
    m_shaderManager = std::make_unique<ResourceManager::ShaderManager>();

    // シェーダーマネージャーの初期化
    m_shaderManager->Initialize(m_deviceResources->GetD3DDevice());

    // ゲームコンテキストの設定
    m_gameContext.emplace(
        m_timer,
        *m_deviceResources,
        m_keyboardTracker,
        m_mouseButtonTracker,
        *m_states,
        *m_debugRenderer,
        *m_eventBus,
        *m_shaderManager,
        m_projection,
        m_backBufferRTV,
        m_depthStencilView,
        m_screenViewport
    );

    // 起動シーンの設定
    m_sceneManager.SetFirstScene(SceneId::SceneA, *m_gameContext);

}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;

    // キーボードトラッカーの更新
    auto keyboard = Keyboard::Get().GetState();
    m_keyboardTracker.Update(keyboard);

    // マウスボタントラッカーの更新
    auto mouse = Mouse::Get().GetState();
    m_mouseButtonTracker.Update(mouse);

    // シーンマネージャーの更新
    m_sceneManager.Update(*m_gameContext);

}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();

    // TODO: Add your rendering code here.
    context;

    // シーンの描画
    m_sceneManager.Render(*m_gameContext);

    // デバッグ関連の文字列などを描画
    m_debugRenderer->Render(context, m_states.get());

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    const auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
    const auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Game window is being resized.


}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    // TODO: Initialize device dependent objects here (independent of window size).
    
    // デバッグ用の描画セットの作成
    m_debugRenderer = std::make_unique<Imase::DebugRenderer>(device, context, L"Resources/Font/SegoeUI_18.spritefont");

    // コモンステートの作成
    m_states = std::make_unique<CommonStates>(device);

     m_postProcess = std::make_unique<DirectX::BasicPostProcess>(device);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.

    auto device = m_deviceResources->GetD3DDevice();

    if (!m_gameContext)
    {
        return;
    }
    m_sceneManager.OnWindowSizeChanged(*m_gameContext);

    int w, h;
    GetDefaultSize(w, h);

     m_projection = SimpleMath::Matrix::CreatePerspectiveFieldOfView(
        XMConvertToRadians(45.0f), static_cast<float>(w) / static_cast<float>(h), 0.1f, 100.0f);

}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
    m_sceneManager.OnDeviceLost(*m_gameContext);
    m_debugRenderer.reset();
    m_states.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();

}
#pragma endregion
