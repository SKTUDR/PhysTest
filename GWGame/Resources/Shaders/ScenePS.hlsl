// ScenePS.hlsl
// 本描画用ピクセルシェーダー。
// シャドウマップを参照して影を計算し、Lambert 拡散光と合成する。

// ---- テクスチャ・サンプラー -------------------------------------------------
Texture2D    albedoMap   : register(t0);  // モデルのアルベドテクスチャ
Texture2D    shadowMap   : register(t4);  // シャドウマップ（ShadowRenderer がバインド）

SamplerState linearSampler : register(s0); // 通常描画用（Wrap）
SamplerState shadowSampler : register(s1); // シャドウマップ用（Clamp・Border=1）

// ---- 定数バッファ -----------------------------------------------------------
cbuffer LightCB : register(b1)
{
    float3 lightDir;       // ライト方向（ワールド空間・正規化済み）
    float  lightIntensity;
    float3 lightColor;
    float  ambientIntensity;
    float3 cameraPos;      // カメラ位置（スペキュラ用・今回は未使用）
    float  shadowBias;     // シャドウアクネ防止オフセット（0.001 程度）
};

// ---- 入力 ------------------------------------------------------------------
struct PS_INPUT
{
    float4 position   : SV_POSITION;
    float3 worldPos   : TEXCOORD0;
    float3 normal     : TEXCOORD1;
    float2 texCoord   : TEXCOORD2;
    float4 shadowCoord: TEXCOORD3;
};

// ---- PCF シャドウ計算 -------------------------------------------------------
// 3×3 のサンプルを平均してソフトシャドウにする（Percentage Closer Filtering）
float CalcShadowFactor(float4 shadowCoord)
{
    // 透視除算（Directional ライトは不要だが Spot/Point のために行う）
    float3 projCoord = shadowCoord.xyz / shadowCoord.w;

    // クリップ範囲外（ライトの視野外）は影なしとして扱う
    if (projCoord.x < -1.f || projCoord.x > 1.f ||
        projCoord.y < -1.f || projCoord.y > 1.f ||
        projCoord.z <  0.f || projCoord.z > 1.f)
    {
        return 1.f;
    }

    // NDC → テクスチャ座標に変換
    // DX11 は Y 軸が NDC と UV で反転している
    float2 shadowUV;
    shadowUV.x =  projCoord.x * 0.5f + 0.5f;
    shadowUV.y = -projCoord.y * 0.5f + 0.5f;

    float currentDepth = projCoord.z - shadowBias;

    // シャドウマップのテクセルサイズ（1024 解像度想定）
    float2 texelSize = float2(1.f / 1024.f, 1.f / 1024.f);

    // 3×3 PCF サンプリング
    float shadow = 0.f;
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset     = float2(x, y) * texelSize;
            float  shadowDepth = shadowMap.Sample(shadowSampler, shadowUV + offset).r;
            shadow += (currentDepth > shadowDepth) ? 0.f : 1.f;
        }
    }
    shadow /= 9.f;  // 3×3 の平均

    return shadow;
}

// ---- メイン ----------------------------------------------------------------
float4 main(PS_INPUT input) : SV_TARGET
{
    // アルベド
    float4 albedo = albedoMap.Sample(linearSampler, input.texCoord);

    // Lambert 拡散光
    float3 N         = normalize(input.normal);
    float3 L         = normalize(-lightDir);
    float  NdotL     = max(dot(N, L), 0.f);
    float3 diffuse   = lightColor * lightIntensity * NdotL;

    // アンビエント
    float3 ambient   = lightColor * ambientIntensity;

    // シャドウ係数（1.0=明るい, 0.0=完全に影）
    float shadowFactor = CalcShadowFactor(input.shadowCoord);

    // 合成: アンビエントは影響を受けない、拡散光のみシャドウで減衰
    float3 lighting = ambient + diffuse * shadowFactor;
    float4 finalColor = float4(albedo.rgb * lighting, albedo.a);

    return finalColor;
}
