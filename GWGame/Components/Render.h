#pragma once

#include "../Graphics/ModelRegistry.h"
#include "../Graphics/MeshRegistry.h"

using namespace DirectX;

namespace ECS
{
enum class MeshType : uint8_t
{
    Box = 0,
    Sphere,
    Torus,
    Teapot,
};

// --------------------------------------------------------
//  MeshRendererComponent
//  DirectX11 描画に必要なリソースへの参照
// --------------------------------------------------------
struct MeshRendererComp
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    UINT indexCount = 0;
    UINT vertexStride = 0;
    XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    bool visible = true;
};

// -------------------------------------------------------
//  ModelRenderComponent
//  モデル描画用。
// -------------------------------------------------------
//struct ModelRenderComp
//{
//    std::unique_ptr<DirectX::Model> model;
//    bool visible = true;
//};

struct SpriteRenderComp
{
    std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
    bool visible = true;
};

// --------------------------------------------------------
// TextureComponent
// テクスチャを持つエンティティに付与
// --------------------------------------------------------
struct TextureComp
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
};

struct MeshTypeComp
{
    MeshType type = MeshType::Box;
};

// --------------------------------------------------------
//  ColorComponent
//  シンプルな色情報 (アニメーションや状態変化に使う)
// --------------------------------------------------------
struct ColorComp
{
    XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    XMFLOAT4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float pulseSpeed = 1.0f;
};

// ---- ModelRenderComp -------------------------------------------------------------
// ECS SoA に収まる純粋データ。unique_ptr は一切持たない。
// ModelID は ModelRegistry への外部キー。
// RenderSystem が毎フレーム registry.Get(modelId) で Model* を引く。
// ----------------------------------------------------------------------------
struct ModelRenderComp
{
    Graphics::ModelID modelId = Graphics::INVALID_MODEL_ID;
    bool visible = true;
    float alpha = 1.0f;

    // ワールド行列のオーバーライド（省略時は TransformComp から自動生成）
    bool overrideWorld = false;
    DirectX::SimpleMath::Matrix worldMatrix = DirectX::SimpleMath::Matrix::Identity;

    // ---- フラスタムカリング用 AABB 半幅（ローカル空間）--------------------
    // モデルのサイズに合わせて調整する
    DirectX::SimpleMath::Vector3 cullHalfExtents = {10.f, 10.f, 10.f};
};

struct RenderComp
{
    Graphics::MeshID meshId = Graphics::INVALID_MESH_ID;
    bool visible = true;

    // フラスタムカリング用 AABB 半幅（ローカル空間）
    // メッシュのサイズに合わせて調整する
    DirectX::SimpleMath::Vector3 cullHalfExtents = {10.f, 10.f, 10.f};
};
} // namespace ECS

