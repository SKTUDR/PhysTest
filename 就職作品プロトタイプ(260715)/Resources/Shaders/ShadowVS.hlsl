// ShadowVS.hlsl
// シャドウパス専用の頂点シェーダー。
// ライト視点の ViewProj 行列でクリップ座標に変換するだけ。
// ピクセルシェーダーは不要（深度バッファへの書き込みのみ）。

cbuffer ShadowPassCB : register(b0)
{
    matrix world;          // エンティティのワールド行列
    matrix lightViewProj;  // ライト視点の View * Proj
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 worldPos  = mul(float4(input.position, 1.f), world);
    output.position  = mul(worldPos, lightViewProj);
    return output;
}
