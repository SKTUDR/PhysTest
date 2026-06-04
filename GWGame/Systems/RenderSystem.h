#pragma once
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/Render.h"
#include "../Graphics/ModelRegistry.h"

#include <array>

#include <SimpleMath.h>
#include <CommonStates.h>
#include <d3d11.h>

namespace ECS
{
    // ============================================================================
    //  Frustum
    //  View * Proj 行列から 6 平面を抽出し、AABB との交差判定を行う。
    //
    //  平面の法線はフラスタム内側方向を向いている。
    //  IsAABBVisible() が false → 完全に外側 → カリング対象
    // ============================================================================
    struct Frustum
    {
        // 平面: normal.Dot(point) + d >= 0 ならば内側
        struct Plane
        {
            DirectX::SimpleMath::Vector3 normal;
            float d = 0.f;

            float DistanceTo(const DirectX::SimpleMath::Vector3& p) const noexcept
            {
                return normal.Dot(p) + d;
            }
        };

        std::array<Plane, 6> planes; // Left, Right, Bottom, Top, Near, Far

        // ---- View * Proj 行列から平面を抽出（Gribb-Hartman 法）-----------------
        static Frustum FromViewProj(const DirectX::SimpleMath::Matrix& viewProj) noexcept
        {
            Frustum f;

            // DirectXMath は row-major なので行を m[row][col] で取得
            const auto& m = viewProj;

            // Left  : col3 + col0
            f.planes[0] = MakePlane(m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41);
            // Right : col3 - col0
            f.planes[1] = MakePlane(m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41);
            // Bottom: col3 + col1
            f.planes[2] = MakePlane(m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42);
            // Top   : col3 - col1
            f.planes[3] = MakePlane(m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42);
            // Near  : col2
            f.planes[4] = MakePlane(m._13, m._23, m._33, m._43);
            // Far   : col3 - col2
            f.planes[5] = MakePlane(m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43);
            return f;
        }

        // ---- AABB との交差判定 --------------------------------------------------
        // center: AABB の中心（ワールド空間）
        // halfExtents: 各軸の半幅
        // 戻り値: false → 完全に外側（カリング可）
        bool IsAABBVisible(const DirectX::SimpleMath::Vector3& center,
                           const DirectX::SimpleMath::Vector3& halfExtents) const noexcept
        {
            for (const auto& plane : planes)
            {
                // AABB の「最も平面に近い頂点」との距離を計算
                // 各軸で法線の符号に合わせて halfExtents を足す（p-vertex 法）
                const float r = std::abs(plane.normal.x) * halfExtents.x + std::abs(plane.normal.y) * halfExtents.y +
                                std::abs(plane.normal.z) * halfExtents.z;

                if (plane.DistanceTo(center) + r < 0.f)
                {
                    return false; // この平面の外側 → カリング
                }
            }
            return true;
        }

    private:
        static Plane MakePlane(float a, float b, float c, float d) noexcept
        {
            const float len = std::sqrt(a * a + b * b + c * c);
            if (len < 1e-6f)
                return {};
            return {{a / len, b / len, c / len}, d / len};
        }
    };


    // ---- RenderSystem -----------------------------------------------------------
    // TransformComp + RenderComp を持つ全エンティティを Query し、
    // TransformComp::ToWorldMatrix() で得たワールド行列の位置に
    // ModelRegistry 経由で DirectXTK (DX11) の Model::Draw を呼ぶ。
    // ----------------------------------------------------------------------------
    class RenderSystem
    {
    public:
        explicit RenderSystem(Graphics::ModelRegistry& registry) : m_registry(registry)
        {
        }

        void Update(World& world, ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& view,
                    const DirectX::SimpleMath::Matrix& proj, DirectX::CommonStates& states)
        {
            // ---- フラスタムを View * Proj から毎フレーム構築 ----------------
            const Frustum frustum = Frustum::FromViewProj(view * proj);

            // ---- カリング統計（デバッグ用・Release では最適化で消える）------
            m_lastTotalCount = 0;
            m_lastDrawnCount = 0;
            m_lastCulledCount = 0;


            auto desc = QueryBuilder{}.All<TransformComp, ModelRenderComp>().Build();

            world.Query(desc).Each<TransformComp, ModelRenderComp>(
                [&](EntityID /*id*/, const TransformComp& tr, const ModelRenderComp& ren)
                {
                    if (!ren.visible)
                        return;

                    // ---- フラスタムカリング -------------------------------------
                    // scale を考慮して AABB の半幅をスケーリングする
                    const DirectX::SimpleMath::Vector3 scaledHalf = {
                        ren.cullHalfExtents.x * tr.scale.x,
                        ren.cullHalfExtents.y * tr.scale.y,
                        ren.cullHalfExtents.z * tr.scale.z,
                    };
                    if (!frustum.IsAABBVisible(tr.position, scaledHalf))
                    {
                        ++m_lastCulledCount;
                        return; // 視野外 → Draw スキップ
                    }

                    // ---- 描画 ---------------------------------------------------
                    DirectX::Model* model = m_registry.Get(ren.modelId);
                    if (!model)
                        return;

                    // overrideWorld が true なら手動行列、それ以外は
                    // TransformComp::ToWorldMatrix() で position / rotation / scale から生成
                    const DirectX::SimpleMath::Matrix worldMat =
                        ren.overrideWorld ? ren.worldMatrix : tr.ToWorldMatrix();

                    model->Draw(context, states, worldMat, view, proj);
                    ++m_lastDrawnCount;
                });
        }

         // ---- デバッグ統計 -------------------------------------------------------
        int LastTotalCount()  const noexcept { return m_lastTotalCount; }
        int LastDrawnCount()  const noexcept { return m_lastDrawnCount; }
        int LastCulledCount() const noexcept { return m_lastCulledCount; }

    private:
        Graphics::ModelRegistry& m_registry;

         // デバッグ用カリング統計（前フレームの値）
        int m_lastTotalCount = 0;
        int m_lastDrawnCount = 0;
        int m_lastCulledCount = 0;
    };
}