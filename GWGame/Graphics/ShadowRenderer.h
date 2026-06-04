#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Render.h"
#include "../Components/Light.h"
#include "../Components/CastShadow.h"
#include "../Graphics/ModelRegistry.h"


namespace Graphics
{

    // ---- ShadowMap --------------------------------------------------------------
    // 1ライト分のシャドウマップリソース一式。
    // ShadowRenderer がライト数分を動的に確保する。
    // ----------------------------------------------------------------------------
    struct ShadowMap
    {
        ShadowMap() = default;

        static constexpr UINT RESOLUTION = 1024; // シャドウマップ解像度

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;   // 深度書き込み用
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; // シェーダー参照用

        // ライト行列（シャドウパス終了後に保持しておき本描画で使う）
        DirectX::SimpleMath::Matrix lightView;
        DirectX::SimpleMath::Matrix lightProj;

        // View * Proj（シェーダーに渡す行列）
        DirectX::SimpleMath::Matrix LightViewProj() const noexcept
        {
            return lightView * lightProj;
        }

        static ShadowMap Create(ID3D11Device* device)
        {
            ShadowMap sm;

            // ---- Texture2D（深度バッファ兼 SRV）--------------------------------
            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = RESOLUTION;
            texDesc.Height = RESOLUTION;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_R32_TYPELESS; // DSV と SRV で共有
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            device->CreateTexture2D(&texDesc, nullptr, sm.texture.GetAddressOf());

            // ---- DepthStencilView -----------------------------------------------
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
            device->CreateDepthStencilView(sm.texture.Get(), &dsvDesc, sm.dsv.GetAddressOf());

            // ---- ShaderResourceView ---------------------------------------------
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;
            device->CreateShaderResourceView(sm.texture.Get(), &srvDesc, sm.srv.GetAddressOf());

            return sm;
        }
    };

    // ---- ShadowRenderer ---------------------------------------------------------
    // 責務:
    //   1. CollectLights()  : ECS から LightComp + TransformComp を Query して収集
    //   2. ShadowPass()     : 各ライト視点で CastShadowComp を持つエンティティを描画
    //   3. LightingPass()   : シャドウマップを参照しながら通常描画
    // ----------------------------------------------------------------------------
    class ShadowRenderer
    {
    public:
        static constexpr int MAX_LIGHTS = 8; // 最大ライト数

        ShadowRenderer() = default;

        ShadowRenderer(ID3D11Device* device, ModelRegistry& registry) : m_registry(registry)
        {
            // シャドウマップをMAX_LIGHTS分あらかじめ確保
            m_shadowMaps.reserve(MAX_LIGHTS);
            for (int i = 0; i < MAX_LIGHTS; ++i)
            {
                m_shadowMaps.push_back(ShadowMap::Create(device));
            }

            // ビューポート（シャドウパス用）
            m_shadowViewport.Width = static_cast<float>(ShadowMap::RESOLUTION);
            m_shadowViewport.Height = static_cast<float>(ShadowMap::RESOLUTION);
            m_shadowViewport.MinDepth = 0.f;
            m_shadowViewport.MaxDepth = 1.f;
            m_shadowViewport.TopLeftX = 0.f;
            m_shadowViewport.TopLeftY = 0.f;
        }

        // ---- 1. ライト情報を ECS から収集 --------------------------------------
        // Game::Render() の先頭で呼ぶ。
        void CollectLights(ECS::World& world)
        {
            m_activeLights.clear();

            auto desc = ECS::QueryBuilder{}.All<ECS::TransformComp, ECS::LightComp>().Build();

            world.Query(desc).Each<ECS::TransformComp, ECS::LightComp>(
                [&](ECS::EntityID, const ECS::TransformComp& tr, const ECS::LightComp& lt)
                {
                    if (m_activeLights.size() >= MAX_LIGHTS)
                        return;

                    LightEntry entry;
                    entry.position = tr.position;
                    entry.light = lt;
                    entry.lightView = lt.CalcLightView(tr.position);
                    entry.lightProj = lt.CalcLightProj();
                    m_activeLights.push_back(entry);
                });
        }

        // ---- 2. シャドウパス ---------------------------------------------------
        // 各ライト視点で CastShadowComp を持つエンティティを深度バッファに描画。
        void ShadowPass(ECS::World& world, ID3D11DeviceContext* context)
        {
            // CastShadowComp を持つエンティティを収集
            auto desc = ECS::QueryBuilder{}.All<ECS::TransformComp, ECS::RenderComp, ECS::CastShadowComp>().Build();

            for (std::size_t i = 0; i < m_activeLights.size(); ++i)
            {
                const LightEntry& entry = m_activeLights[i];
                if (!entry.light.castShadow)
                    continue;

                ShadowMap& sm = m_shadowMaps[i];
                sm.lightView = entry.lightView;
                sm.lightProj = entry.lightProj;

                // レンダーターゲットを解除・深度バッファのみにバインド
                ID3D11RenderTargetView* nullRTV = nullptr;
                context->OMSetRenderTargets(1, &nullRTV, sm.dsv.Get());
                context->ClearDepthStencilView(sm.dsv.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
                context->RSSetViewports(1, &m_shadowViewport);

                // ライト視点の View * Proj を定数バッファに設定
                // （自前シェーダーを使う場合はここで CB を更新する）
                const DirectX::SimpleMath::Matrix lvp = sm.LightViewProj();

                // CastShadowComp を持つエンティティをライト視点で描画
                world.Query(desc).Each<ECS::TransformComp, ECS::ModelRenderComp>(
                    [&](ECS::EntityID, const ECS::TransformComp& tr, const ECS::ModelRenderComp& ren)
                    {
                        if (!ren.visible)
                            return;
                        DirectX::Model* model = m_registry.Get(ren.modelId);
                        if (!model)
                            return;

                        const DirectX::SimpleMath::Matrix worldMat =
                            ren.overrideWorld ? ren.worldMatrix : tr.ToWorldMatrix();

                        // シャドウパスは深度値のみ書き込む
                        // DirectXTK の Draw を流用（カラー出力は RTV がないので無効）
                        model->Draw(context, *m_states, worldMat, sm.lightView, sm.lightProj);
                    });
            }
        }

        // ---- 3. ライティングパス（通常描画）------------------------------------
        // シャドウマップを SRV としてシェーダーにバインドしてから通常描画する。
        // ここでは DirectXTK の Draw をそのまま呼ぶ簡易実装。
        // 本格的な PCF フィルタを使う場合は自前シェーダーが必要。
        void LightingPass(ECS::World& world, ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& view,
                          const DirectX::SimpleMath::Matrix& proj, DirectX::CommonStates& states)
        {
            // シャドウマップ SRV をシェーダーにバインド（スロット t4〜）
            // スロット番号は自前シェーダーの register(t4) に合わせる
            for (std::size_t i = 0; i < m_activeLights.size(); ++i)
            {
                ID3D11ShaderResourceView* srv = m_shadowMaps[i].srv.Get();
                context->PSSetShaderResources(static_cast<UINT>(4 + i), 1, &srv);
            }

            // 通常の描画（RenderSystem と同じ流れ）
            auto desc = ECS::QueryBuilder{}.All<ECS::TransformComp, ECS::ModelRenderComp>().Build();

            world.Query(desc).Each<ECS::TransformComp, ECS::ModelRenderComp>(
                [&](ECS::EntityID, const ECS::TransformComp& tr, const ECS::ModelRenderComp& ren)
                {
                    if (!ren.visible)
                        return;
                    DirectX::Model* model = m_registry.Get(ren.modelId);
                    if (!model)
                        return;

                    const DirectX::SimpleMath::Matrix worldMat =
                        ren.overrideWorld ? ren.worldMatrix : tr.ToWorldMatrix();

                    model->Draw(context, states, worldMat, view, proj);
                });

            // SRV をアンバインド（次フレームの DSV バインドと競合させないため）
            for (std::size_t i = 0; i < m_activeLights.size(); ++i)
            {
                ID3D11ShaderResourceView* nullSrv = nullptr;
                context->PSSetShaderResources(static_cast<UINT>(4 + i), 1, &nullSrv);
            }
        }

        // ---- アクセサ -----------------------------------------------------------
        // LightViewProj 行列をシェーダー定数バッファに渡す際に使う
        int ActiveLightCount() const noexcept
        {
            return static_cast<int>(m_activeLights.size());
        }

        const DirectX::SimpleMath::Matrix& GetLightView(int index) const noexcept
        {
            assert(index < static_cast<int>(m_shadowMaps.size()));
            return m_shadowMaps[index].lightView;
        }

        const DirectX::SimpleMath::Matrix& GetLightProj(int index) const noexcept
        {
            assert(index < static_cast<int>(m_shadowMaps.size()));
            return m_shadowMaps[index].lightProj;
        }

        const DirectX::SimpleMath::Matrix GetLightViewProj(int index) const noexcept
        {
            assert(index < static_cast<int>(m_shadowMaps.size()));
            return m_shadowMaps[index].LightViewProj();
        }

        ID3D11ShaderResourceView* GetShadowMapSRV(int index) const noexcept
        {
            assert(index < static_cast<int>(m_shadowMaps.size()));
            return m_shadowMaps[index].srv.Get();
        }

        void SetCommonStates(DirectX::CommonStates* states) noexcept
        {
            m_states = states;
        }

    private:
        struct LightEntry
        {
            DirectX::SimpleMath::Vector3 position;
            ECS::LightComp light;
            DirectX::SimpleMath::Matrix lightView;
            DirectX::SimpleMath::Matrix lightProj;
        };

        ModelRegistry& m_registry;
        std::vector<ShadowMap> m_shadowMaps;    // MAX_LIGHTS 分を事前確保
        std::vector<LightEntry> m_activeLights; // 毎フレーム CollectLights で更新
        D3D11_VIEWPORT m_shadowViewport = {};
        DirectX::CommonStates* m_states = nullptr;
    };

} // namespace Graphics