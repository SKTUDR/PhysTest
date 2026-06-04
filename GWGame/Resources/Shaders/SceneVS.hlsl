// SceneVS.hlsl
// 本描画用頂点シェーダー。
// ワールド座標・法線・シャドウ座標をピクセルシェーダーへ渡す。

cbuffer SceneCB : register(b0)
{
    matrix world;          // エンティティのワールド行列
    matrix view;           // カメラ View 行列
    matrix proj;           // カメラ Proj 行列
    matrix lightViewProj;  // ライト視点の View * Proj（シャドウ座標計算用）
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 position   : SV_POSITION;  // クリップ座標
    float3 worldPos   : TEXCOORD0;    // ワールド座標（拡散光計算用）
    float3 normal     : TEXCOORD1;    // ワールド法線
    float2 texCoord   : TEXCOORD2;    // UV
    float4 shadowCoord: TEXCOORD3;    // ライト視点でのクリップ座標
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 worldPos   = mul(float4(input.position, 1.f), world);
    output.worldPos   = worldPos.xyz;
    output.position   = mul(mul(worldPos, view), proj);
    output.normal     = normalize(mul(input.normal, (float3x3)world));
    output.texCoord   = input.texCoord;

    // シャドウ座標: ライト視点のクリップ空間へ変換
    output.shadowCoord = mul(worldPos, lightViewProj);

    return output;
}
