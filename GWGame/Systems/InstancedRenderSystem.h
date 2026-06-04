#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "Components/Transform.h"
#include "Components/Render.h"
#include "../Graphics/MeshRegistry.h"
#include "../Graphics/MeshData.h"

// RenderSystem の Frustum を再利用
#include "../Systems/RenderSystem.h"

#include <d3d11.h>
#include <SimpleMath.h>
#include <unordered_map>
#include <vector>

namespace Graphics
{

    // ---- InstancedRenderSystem --------------------------------------------------
    // 処理フロー（毎フレーム）:
    //   ① ECS Query: TransformComp + RenderComp を持つ全エンティティを走査
    //   ② フラスタムカリング: 視野外をスキップ
    //   ③ meshId でグループ化: map<MeshID, vector<InstanceData>>
    //   ④ メッシュごとにインスタンスバッファ更新 + DrawIndexedInstanced
    //
    // ドローコール数 = 画面内に映る meshId の種別数（最悪でも MeshRegistry::Count()）
    // ----------------------------------------------------------------------------
    class InstancedRenderSystem
    {
    public:
        explicit InstancedRenderSystem(MeshRegistry& registry) : m_registry(registry)
        {
        }

        void Update(ECS::World& world, ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& view,
                    const DirectX::SimpleMath::Matrix& proj, ID3D11InputLayout* inputLayout, ID3D11VertexShader* vs,
                    ID3D11PixelShader* ps,
                    ID3D11Buffer* sceneCB) // VS 定数バッファ
        {
            // ---- シェーダーをバインド -------------------------------------------
            context->IASetInputLayout(inputLayout);
            context->VSSetShader(vs, nullptr, 0);
            context->PSSetShader(ps, nullptr, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // ---- フラスタム構築 -------------------------------------------------
            const ECS::Frustum frustum = ECS::Frustum::FromViewProj(view * proj);

            // ---- ① Query + カリング + グループ化 --------------------------------
            m_instanceGroups.clear();

            auto desc = ECS::QueryBuilder{}.All<ECS::TransformComp, ECS::RenderComp>().Build();

            world.Query(desc).Each<ECS::TransformComp, ECS::RenderComp>(
                [&](ECS::EntityID, const ECS::TransformComp& tr, const ECS::RenderComp& ren)
                {
                    if (!ren.visible)
                        return;

                    // フラスタムカリング
                    const DirectX::SimpleMath::Vector3 scaledHalf = {
                        ren.cullHalfExtents.x * tr.scale.x,
                        ren.cullHalfExtents.y * tr.scale.y,
                        ren.cullHalfExtents.z * tr.scale.z,
                    };
                    if (!frustum.IsAABBVisible(tr.position, scaledHalf))
                        return;

                    // InstanceData（ワールド行列）を MeshID グループに追加
                    InstanceData inst;
                    DirectX::XMStoreFloat4x4(&inst.world, DirectX::XMMatrixTranspose(tr.ToWorldMatrix()));

                    m_instanceGroups[ren.meshId].push_back(inst);
                });

            // ---- ② インスタンスバッファ更新 + ③ 描画 ---------------------------
            // View/Proj 定数バッファを VS スロット b0 に設定（meshごとに共通）
            UpdateViewProjCB(context, sceneCB, view, proj);
            context->VSSetConstantBuffers(0, 1, &sceneCB);

            m_lastDrawCallCount = 0;
            m_lastInstanceCount = 0;
            m_lastCulledMeshCount = 0;

            for (auto& [meshId, instances] : m_instanceGroups)
            {
                MeshData* mesh = m_registry.Get(meshId);
                if (!mesh || !mesh->IsValid())
                    continue;

                // インスタンスバッファ更新
                const uint32_t instanceCount = m_registry.UpdateInstanceBuffer(context, meshId, instances);
                if (instanceCount == 0)
                    continue;

                // 頂点バッファ（スロット 0）とインスタンスバッファ（スロット 1）
                ID3D11Buffer* vbs[2] = {mesh->vertexBuffer.Get(), mesh->instanceBuffer.Get()};
                UINT strides[2] = {sizeof(Vertex), sizeof(InstanceData)};
                UINT offsets[2] = {0, 0};
                context->IASetVertexBuffers(0, 2, vbs, strides, offsets);
                context->IASetIndexBuffer(mesh->indexBuffer.Get(), mesh->indexFormat, 0);

                // SubMesh ごとに DrawIndexedInstanced
                for (const auto& sub : mesh->subMeshes)
                {
                    context->DrawIndexedInstanced(sub.indexCount,  // インデックス数
                                                  instanceCount,   // インスタンス数
                                                  sub.indexOffset, // インデックス開始位置
                                                  0,               // 頂点オフセット
                                                  0);              // インスタンス開始位置
                    ++m_lastDrawCallCount;
                }

                m_lastInstanceCount += instanceCount;
            }
        }

        // ---- デバッグ統計 -------------------------------------------------------
        int LastDrawCallCount() const noexcept
        {
            return m_lastDrawCallCount;
        }
        int LastInstanceCount() const noexcept
        {
            return m_lastInstanceCount;
        }
        int LastCulledMeshCount() const noexcept
        {
            return m_lastCulledMeshCount;
        }

    private:
        MeshRegistry& m_registry;

        // meshId → インスタンスデータリスト（毎フレームクリア）
        std::unordered_map<MeshID, std::vector<InstanceData>> m_instanceGroups;

        int m_lastDrawCallCount = 0;
        int m_lastInstanceCount = 0;
        int m_lastCulledMeshCount = 0;

        // View / Proj を定数バッファに書き込む
        // HLSL: cbuffer SceneCB : register(b0) { float4x4 view; float4x4 proj; }
        struct alignas(16) ViewProjCB
        {
            DirectX::XMFLOAT4X4 view;
            DirectX::XMFLOAT4X4 proj;
        };
        static_assert(sizeof(ViewProjCB) % 16 == 0);

        static void UpdateViewProjCB(ID3D11DeviceContext* context, ID3D11Buffer* cb,
                                     const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
        {
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(context->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return;

            ViewProjCB data;
            DirectX::XMStoreFloat4x4(&data.view, DirectX::XMMatrixTranspose(view));
            DirectX::XMStoreFloat4x4(&data.proj, DirectX::XMMatrixTranspose(proj));
            std::memcpy(mapped.pData, &data, sizeof(data));
            context->Unmap(cb, 0);
        }
    };

} // namespace Graphics