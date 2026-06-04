#pragma once
#include <d3d11.h>
#include <SimpleMath.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

namespace Graphics
{

    using MeshID = uint32_t;
    static constexpr MeshID INVALID_MESH_ID = UINT32_MAX;
    static constexpr uint32_t MAX_INSTANCES = 1024;

    // ---- Vertex -----------------------------------------------------------------
    // 頂点レイアウト。HLSL の VS_INPUT と完全に一致させる。
    // ----------------------------------------------------------------------------
    struct Vertex
    {
        DirectX::SimpleMath::Vector3 position;
        DirectX::SimpleMath::Vector3 normal;
        DirectX::SimpleMath::Vector2 texCoord;
    };

    // ---- InstanceData -----------------------------------------------------------
    // インスタンスバッファの 1 要素。
    // VS の SV_InstanceID でインデックスし、ワールド行列を取得する。
    // HLSL では row_major float4x4 として受け取る（転置不要）。
    // ----------------------------------------------------------------------------
    struct InstanceData
    {
        DirectX::XMFLOAT4X4 world;
    };

    // ---- SubMesh ----------------------------------------------------------------
    // 1 マテリアル分の描画範囲。
    // MeshData が複数の SubMesh を持つ（マテリアル切り替えが必要な場合）。
    // ----------------------------------------------------------------------------
    struct SubMesh
    {
        uint32_t indexOffset = 0;   // インデックスバッファ内の開始位置
        uint32_t indexCount = 0;    // 描画するインデックス数
        uint32_t materialIndex = 0; // 将来のマテリアル参照用（現状は未使用）
    };

    // ---- MeshData ---------------------------------------------------------------
    // 1モデル分の GPU リソース一式。MeshRegistry が所有する。
    // ECS の RenderComp は MeshID だけを持ち、MeshRegistry 経由で参照する。
    // ----------------------------------------------------------------------------
    struct MeshData
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer; // 動的・毎フレーム更新

        UINT vertexStride = sizeof(Vertex);
        UINT vertexOffset = 0;
        UINT indexCount = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;

        std::vector<SubMesh> subMeshes;

        // インスタンスバッファ作成（Initialize 時に1回だけ呼ぶ）
        static Microsoft::WRL::ComPtr<ID3D11Buffer> CreateInstanceBuffer(ID3D11Device* device,
                                                                         uint32_t maxInstances = MAX_INSTANCES)
        {
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = sizeof(InstanceData) * maxInstances;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            Microsoft::WRL::ComPtr<ID3D11Buffer> buf;
            device->CreateBuffer(&desc, nullptr, buf.GetAddressOf());
            return buf;
        }

        bool IsValid() const noexcept
        {
            return vertexBuffer != nullptr && indexBuffer != nullptr;
        }
    };

    // ---- 入力レイアウト記述子 ---------------------------------------------------
    // IASetInputLayout に渡す。MeshRegistry::Initialize() で使用する。
    // スロット 0: 頂点データ（Vertex 構造体）
    // スロット 1: インスタンスデータ（InstanceData 構造体、per-instance）
    // ----------------------------------------------------------------------------
    inline const D3D11_INPUT_ELEMENT_DESC INSTANCED_INPUT_LAYOUT[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        // ワールド行列を 4 行に分割してインスタンスバッファから受け取る
        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    inline constexpr UINT INSTANCED_INPUT_LAYOUT_COUNT = static_cast<UINT>(std::size(INSTANCED_INPUT_LAYOUT));

} // namespace Graphics