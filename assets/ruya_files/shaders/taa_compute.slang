import common;

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

[[vk::binding(0, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> albedoDepth;
[[vk::binding(1, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> motionVectors;

[[vk::binding(2, 1)]] Sampler2D previousLightPassImage;
[[vk::binding(3, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> lightPassImage;

[[vk::binding(4, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> taaImage;

static const float FLT_EPS              = 1e-6f;
static const float BASE_SOURCE_WEIGHT   = 0.0625f;
static const float MAX_SOURCE_WEIGHT    = 0.40f;
static const float MOTION_FULL_PIXELS   = 12.0f;
static const float VARIANCE_GAMMA       = 1.25f;
static const float ANTI_FLICKER_BOOST   = 0.65f;

static const float2 haltonSequence[16] =
{
    float2(0.500000, 0.333333), float2(0.250000, 0.666667),
    float2(0.750000, 0.111111), float2(0.125000, 0.444444),
    float2(0.625000, 0.777778), float2(0.375000, 0.222222),
    float2(0.875000, 0.555556), float2(0.062500, 0.888889),
    float2(0.562500, 0.037037), float2(0.312500, 0.370370),
    float2(0.812500, 0.703704), float2(0.187500, 0.148148),
    float2(0.687500, 0.481481), float2(0.437500, 0.814815),
    float2(0.937500, 0.259259), float2(0.031250, 0.592593)
};

float2 ComputeJitterUV(int frameIdx)
{
    int idx = ((frameIdx % 16) + 16) % 16;
    float2 hs = haltonSequence[idx];
    return float2((hs.x - 0.5) / cameraData.camera.width,
                 -(hs.y - 0.5) / cameraData.camera.height);
}

float3 RGB_to_YCoCg(float3 c)
{
    return float3(
         0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
         0.5  * c.r              - 0.5  * c.b,
        -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

float3 YCoCg_to_RGB(float3 c)
{
    float tmp = c.x - c.z;
    return float3(tmp + c.y, c.x + c.z, tmp - c.y);
}

float3 Tonemap(float3 ycocg)    { return ycocg * rcp(1.0f + ycocg.x); }
float3 InverseTonemap(float3 ycocg) { return ycocg * rcp(max(1.0f - ycocg.x, FLT_EPS)); }

float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 p, float3 q)
{
    float3 center  = 0.5 * (aabbMax + aabbMin);
    float3 extents = 0.5 * (aabbMax - aabbMin) + FLT_EPS;
    float3 offset  = q - center;
    float3 ts      = abs(extents) / max(abs(offset), FLT_EPS);
    float  t       = saturate(min(ts.x, min(ts.y, ts.z)));
    return center + offset * t;
}

float3 SampleHistoryCatmullRom(Sampler2D tex, float2 uv, float2 texSize)
{
    float2 samplePos = uv * texSize;
    float2 texPos1   = floor(samplePos - 0.5) + 0.5;
    float2 f         = samplePos - texPos1;

    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12     = w1 + w2;
    float2 offset12 = w2 / w12;

    float2 invTex = 1.0 / texSize;
    float2 p0  = (texPos1 - 1.0)        * invTex;
    float2 p3  = (texPos1 + 2.0)        * invTex;
    float2 p12 = (texPos1 + offset12)   * invTex;

    float3 result = 0.0;
    result += tex.SampleLevel(float2(p12.x, p0.y),  0.0).rgb * (w12.x * w0.y);
    result += tex.SampleLevel(float2(p0.x,  p12.y), 0.0).rgb * (w0.x  * w12.y);
    result += tex.SampleLevel(float2(p12.x, p12.y), 0.0).rgb * (w12.x * w12.y);
    result += tex.SampleLevel(float2(p3.x,  p12.y), 0.0).rgb * (w3.x  * w12.y);
    result += tex.SampleLevel(float2(p12.x, p3.y),  0.0).rgb * (w12.x * w3.y);

    float wSum = (w12.x * w0.y) + (w0.x * w12.y) + (w12.x * w12.y)
               + (w3.x  * w12.y) + (w12.x * w3.y);
    return max(0.0, result / max(wSum, FLT_EPS));
}

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    int2 texCoord = int2(globalID.xy);
    uint2 texSizeU;
    lightPassImage.GetDimensions(texSizeU.x, texSizeU.y);
    int2  texSize  = int2(texSizeU);
    if (any(texCoord >= texSize)) return;

    float2 texSizeF  = float2(texSize);
    float2 invTexSize = 1.0 / texSizeF;

    int curFrame = int(cameraData.camera.frameNumber);
    float2 jitterUV     = ComputeJitterUV(curFrame);
    float2 prevJitterUV = ComputeJitterUV(curFrame - 1);

    float3 m1 = 0.0;
    float3 m2 = 0.0;
    float3 centerYCoCg = 0.0;

    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
    {
        int2 p = clamp(texCoord + int2(x, y), 0, texSize - 1);
        float3 hdr     = max(0.0, lightPassImage[p].rgb);
        float3 ycocg   = RGB_to_YCoCg(hdr);
        float3 mapped  = Tonemap(ycocg);

        m1 += mapped;
        m2 += mapped * mapped;
        if (x == 0 && y == 0) centerYCoCg = mapped;
    }

    const float invN = 1.0 / 9.0;
    float3 mu    = m1 * invN;
    float3 sigma = sqrt(max(0.0, m2 * invN - mu * mu));

    float fluxBoost = 1.0 + ANTI_FLICKER_BOOST * saturate(mu.x);
    float3 minC = mu - VARIANCE_GAMMA * fluxBoost * sigma;
    float3 maxC = mu + VARIANCE_GAMMA * fluxBoost * sigma;

    float depth = albedoDepth[texCoord].a;
    bool  isSky = (depth >= 1.0 - FLT_EPS);

    float2 currentUV    = (float2(texCoord) + 0.5) * invTexSize;
    float2 motionVector = isSky ? float2(0.0) : motionVectors[texCoord].rg;
    float2 historyUV    = currentUV - (motionVector - jitterUV + prevJitterUV);
    float  motionPx     = length(motionVector * texSizeF);

    if (any(historyUV < 0.0) || any(historyUV > 1.0))
    {
        taaImage[texCoord] = float4(YCoCg_to_RGB(InverseTonemap(centerYCoCg)), 1.0);
        return;
    }

    float3 histRGB   = SampleHistoryCatmullRom(previousLightPassImage, historyUV, texSizeF);
    float3 histYCoCg = Tonemap(RGB_to_YCoCg(histRGB));
    histYCoCg = ClipAABB(minC, maxC, centerYCoCg, histYCoCg);

    float motionFactor = saturate(motionPx / MOTION_FULL_PIXELS);
    float sourceW = lerp(BASE_SOURCE_WEIGHT, MAX_SOURCE_WEIGHT, motionFactor);

    float wS = sourceW       * rcp(1.0 + centerYCoCg.x);
    float wH = (1.0 - sourceW) * rcp(1.0 + histYCoCg.x);

    float3 blendedYCoCg = (centerYCoCg * wS + histYCoCg * wH) / max(wS + wH, FLT_EPS);

    float3 result = max(0.0, YCoCg_to_RGB(InverseTonemap(blendedYCoCg)));
    taaImage[texCoord] = float4(result, 1.0);
}
