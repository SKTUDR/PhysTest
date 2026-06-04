// ShaderManager.h
#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <unordered_map>
#include <string>

namespace ResourceManager
{
    struct ShaderSet
    {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    };

    class ShaderManager
    {
    public:
        void Initialize(ID3D11Device* device);
        void Finalize();

        // シェーダーをロード（既にあればキャッシュを返す）
        const ShaderSet* Load(const std::string& name, // キー名 ("basic", "ui" など)
                              const wchar_t* vsFile, const wchar_t* psFile, const D3D11_INPUT_ELEMENT_DESC* layout,
                              UINT layoutCount);

        // キー名で取得
        const ShaderSet* Get(const std::string& name) const;

         // ---- 定数バッファ作成 ---------------------------------------------------
        // T: 定数バッファの構造体（16バイトアライン必須）
        template <typename T> static Microsoft::WRL::ComPtr<ID3D11Buffer> CreateConstantBuffer(ID3D11Device* device)
        {
            static_assert(sizeof(T) % 16 == 0, "定数バッファは16バイトアラインが必要です");

            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = sizeof(T);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
            DX::ThrowIfFailed(device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf()));
            return buffer;
        }

        // ---- 定数バッファ更新 ---------------------------------------------------
        template <typename T>
        static void UpdateConstantBuffer(ID3D11DeviceContext* context, ID3D11Buffer* buffer, const T& data)
        {
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            memcpy(mapped.pData, &data, sizeof(T));
            context->Unmap(buffer, 0);
        }

    private:
        ID3D11Device* m_device = nullptr;
        std::unordered_map<std::string, ShaderSet> m_cache;

        bool CompileShader(const wchar_t* file, const char* entry, const char* target, ID3DBlob** blobOut);

        
    };

// ShadowVS.hlsl の cbuffer ShadowPassCB : register(b0)
struct alignas(16) ShadowPassCB
{
    DirectX::XMMATRIX world;
    DirectX::XMMATRIX lightViewProj;
};
static_assert(sizeof(ShadowPassCB) % 16 == 0);

// SceneVS.hlsl の cbuffer SceneCB : register(b0)
struct alignas(16) SceneCB
{
    DirectX::XMMATRIX world;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX proj;
    DirectX::XMMATRIX lightViewProj;
};
static_assert(sizeof(SceneCB) % 16 == 0);

// ScenePS.hlsl の cbuffer LightCB : register(b1)
struct alignas(16) LightCB
{
    DirectX::XMFLOAT3 lightDir;
    float lightIntensity;
    DirectX::XMFLOAT3 lightColor;
    float ambientIntensity;
    DirectX::XMFLOAT3 cameraPos;
    float shadowBias; // 推奨値: 0.001f
};
static_assert(sizeof(LightCB) % 16 == 0);

}