// ColliderDebugRenderer.h
#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Collider.h"

#include <PrimitiveBatch.h>
#include <Effects.h>
#include <VertexTypes.h>
#include <SimpleMath.h>
#include <memory>

namespace Graphics
{

    class ColliderDebugRenderer
    {
    public:
        void Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
        {
            m_batch = std::make_unique<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(context);
            m_effect = std::make_unique<DirectX::BasicEffect>(device);
            m_effect->SetVertexColorEnabled(true);

            // 入力レイアウト作成
            void const* shaderByteCode;
            size_t byteCodeLength;
            m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
            device->CreateInputLayout(DirectX::VertexPositionColor::InputElements,
                                      DirectX::VertexPositionColor::InputElementCount, shaderByteCode, byteCodeLength,
                                      m_inputLayout.GetAddressOf());
        }

        // デバッグビルドのみ描画
        void Render(ECS::World& world, ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& view,
                    const DirectX::SimpleMath::Matrix& proj)
        {
#ifdef _DEBUG
            m_effect->SetView(view);
            m_effect->SetProjection(proj);
            m_effect->Apply(context);
            context->IASetInputLayout(m_inputLayout.Get());

            m_batch->Begin();

            auto desc = ECS::QueryBuilder{}.All<ECS::TransformComp, ECS::ColliderComp>().Build();

            world.Query(desc).Each<ECS::TransformComp, ECS::ColliderComp>(
                [&](ECS::EntityID, const ECS::TransformComp& tr, const ECS::ColliderComp& col)
                {
                    // コライダーの種別ごとに色を変える
                    DirectX::XMVECTORF32 color;
                    if (col.IsTrigger())
                        color = DirectX::Colors::Yellow;
                    else if (col.IsAABB())
                        color = DirectX::Colors::Lime;
                    else if (col.IsCapsule())
                        color = DirectX::Colors::Cyan;
                    else
                        color = DirectX::Colors::Magenta;

                    // ワールド位置 = TransformComp::position + local_offset
                    DirectX::SimpleMath::Vector3 worldPos = {
                        tr.position.x + col.localOffset.x,
                        tr.position.y + col.localOffset.y,
                        tr.position.z + col.localOffset.z,
                    };

                    std::visit(
                        [&](auto&& shape)
                        {
                            using T = std::decay_t<decltype(shape)>;
                            if constexpr (std::is_same_v<T, ECS::AABB>)
                            {
                                DrawAABB(worldPos, shape.halfExtents, color);
                            }
                            else if constexpr (std::is_same_v<T, ECS::Capsule>)
                            {
                                DrawCapsule(worldPos, shape.radius, shape.halfHeight, color);
                            }
                            else if constexpr (std::is_same_v<T, ECS::OBB>)
                            {
                                DrawOBB(worldPos, shape.halfExtents, tr.rotation, color);
                            }
                        },
                        col.shape);
                });

            m_batch->End();
#endif
        }

    private:
        std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_batch;
        std::unique_ptr<DirectX::BasicEffect> m_effect;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

        using VPC = DirectX::VertexPositionColor;

        void DrawLine(const DirectX::SimpleMath::Vector3& a, const DirectX::SimpleMath::Vector3& b,
                      DirectX::XMVECTORF32 color)
        {
            m_batch->DrawLine(VPC(a, color), VPC(b, color));
        }

        // ---- AABB ワイヤーフレーム ------------------------------------------
        void DrawAABB(const DirectX::SimpleMath::Vector3& center, const DirectX::SimpleMath::Vector3& half,
                      DirectX::XMVECTORF32 color)
        {
            // 8頂点
            DirectX::SimpleMath::Vector3 v[8] = {
                {center.x - half.x, center.y - half.y, center.z - half.z},
                {center.x + half.x, center.y - half.y, center.z - half.z},
                {center.x + half.x, center.y - half.y, center.z + half.z},
                {center.x - half.x, center.y - half.y, center.z + half.z},
                {center.x - half.x, center.y + half.y, center.z - half.z},
                {center.x + half.x, center.y + half.y, center.z - half.z},
                {center.x + half.x, center.y + half.y, center.z + half.z},
                {center.x - half.x, center.y + half.y, center.z + half.z},
            };
            // 下面
            DrawLine(v[0], v[1], color);
            DrawLine(v[1], v[2], color);
            DrawLine(v[2], v[3], color);
            DrawLine(v[3], v[0], color);
            // 上面
            DrawLine(v[4], v[5], color);
            DrawLine(v[5], v[6], color);
            DrawLine(v[6], v[7], color);
            DrawLine(v[7], v[4], color);
            // 柱
            DrawLine(v[0], v[4], color);
            DrawLine(v[1], v[5], color);
            DrawLine(v[2], v[6], color);
            DrawLine(v[3], v[7], color);
        }

        // ---- Capsule ワイヤーフレーム（Y 軸）----------------------------------
        void DrawCapsule(const DirectX::SimpleMath::Vector3& center, float radius, float halfHeight,
                         DirectX::XMVECTORF32 color)
        {
            const int SEG = 16;
            const float step = DirectX::XM_2PI / SEG;

            // 上下の円
            for (int i = 0; i < SEG; ++i)
            {
                float a0 = step * i, a1 = step * (i + 1);
                // 上円
                DrawLine({center.x + radius * cosf(a0), center.y + halfHeight, center.z + radius * sinf(a0)},
                         {center.x + radius * cosf(a1), center.y + halfHeight, center.z + radius * sinf(a1)}, color);
                // 下円
                DrawLine({center.x + radius * cosf(a0), center.y - halfHeight, center.z + radius * sinf(a0)},
                         {center.x + radius * cosf(a1), center.y - halfHeight, center.z + radius * sinf(a1)}, color);
            }
            // 4本の縦線
            for (int i = 0; i < 4; ++i)
            {
                float a = step * (SEG / 4) * i;
                DrawLine({center.x + radius * cosf(a), center.y + halfHeight, center.z + radius * sinf(a)},
                         {center.x + radius * cosf(a), center.y - halfHeight, center.z + radius * sinf(a)}, color);
            }
        }

        // ---- OBB ワイヤーフレーム ----------------------------------------------
        void DrawOBB(const DirectX::SimpleMath::Vector3& center, const DirectX::SimpleMath::Vector3& half,
                     const DirectX::SimpleMath::Quaternion& rot, DirectX::XMVECTORF32 color)
        {
            auto rotate = [&](DirectX::SimpleMath::Vector3 v)
            { return DirectX::SimpleMath::Vector3::Transform(v, rot) + center; };
            DirectX::SimpleMath::Vector3 v[8] = {
                rotate({-half.x, -half.y, -half.z}), rotate({half.x, -half.y, -half.z}),
                rotate({half.x, -half.y, half.z}),   rotate({-half.x, -half.y, half.z}),
                rotate({-half.x, half.y, -half.z}),  rotate({half.x, half.y, -half.z}),
                rotate({half.x, half.y, half.z}),    rotate({-half.x, half.y, half.z}),
            };
            DrawLine(v[0], v[1], color);
            DrawLine(v[1], v[2], color);
            DrawLine(v[2], v[3], color);
            DrawLine(v[3], v[0], color);
            DrawLine(v[4], v[5], color);
            DrawLine(v[5], v[6], color);
            DrawLine(v[6], v[7], color);
            DrawLine(v[7], v[4], color);
            DrawLine(v[0], v[4], color);
            DrawLine(v[1], v[5], color);
            DrawLine(v[2], v[6], color);
            DrawLine(v[3], v[7], color);
        }
    };

} // namespace Graphics