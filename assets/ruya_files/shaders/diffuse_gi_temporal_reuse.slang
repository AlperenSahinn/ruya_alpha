import common;

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

[[vk::binding(0, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferAlbedoDepth;
[[vk::binding(1, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferPositionMetallic;
[[vk::binding(2, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferMotionVectors;

[[vk::binding(3, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevGbufferPositionMetallic;

[[vk::binding(4, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevDiffuseGISH0;
[[vk::binding(5, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevDiffuseGISH1;
[[vk::binding(6, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevDiffuseGISH2;
[[vk::binding(7, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevDiffuseGISH3;

[[vk::binding(8, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGI;
[[vk::binding(9, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGIRayDir;

[[vk::binding(10, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH0;
[[vk::binding(11, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH1;
[[vk::binding(12, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH2;
[[vk::binding(13, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH3;

static const float2 haltonSequence[] =
{
    float2(0.500000, 0.333333), 
    float2(0.250000, 0.666667),
    float2(0.750000, 0.111111), 
    float2(0.125000, 0.444444),
    float2(0.625000, 0.777778), 
    float2(0.375000, 0.222222),
    float2(0.875000, 0.555556), 
    float2(0.062500, 0.888889),
    float2(0.562500, 0.037037), 
    float2(0.312500, 0.370370),
    float2(0.812500, 0.703704), 
    float2(0.187500, 0.148148),
    float2(0.687500, 0.481481), 
    float2(0.437500, 0.814815),
    float2(0.937500, 0.259259), 
    float2(0.031250, 0.592593)
};

struct SH4Color 
{
    float3 c[4];
};

int rand(inout uint s) 
{ 
    s = s * 214013u + 2531011u; 
    return (int)((s >> 16) & 32767); 
}

float frand(inout uint s) 
{ 
    return float(rand(s)) / 32767.0; 
}

float2 frand2(inout uint s) 
{ 
    return float2(frand(s), frand(s)); 
}

float3 frand3(inout uint s) 
{ 
    return float3(frand(s), frand(s), frand(s)); 
}

void srand(int2 p, int frame, inout uint s)
{
    int n = frame;
    n = (n<<13)^n; n=n*(n*n*15731+789221)+1376312589;
    n += p.y;
    n = (n<<13)^n; n=n*(n*n*15731+789221)+1376312589;
    n += p.x;
    n = (n<<13)^n; n=n*(n*n*15731+789221)+1376312589;
    s = n;
}

float2 ComputeJitterUV(int frameIdx)
{
    int idx = ((frameIdx % 16) + 16) % 16;
    float2 hs = haltonSequence[idx];
    return float2((hs.x - 0.5) / cameraData.camera.width, -(hs.y - 0.5) / cameraData.camera.height);
}

float3 SanitizeRadiance(float3 radiance)
{
    if (any(isnan(radiance)) || any(isinf(radiance)))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }
    
    // Maksimum parlaklık sınırı (100.0f sahnenin pozlamasına göre artırılabilir)
    // Bu, beyaz lekelenmelerin (firefly) history'de kalıp parlamasını engeller.
    return min(radiance, float3(100.0f, 100.0f, 100.0f));
}

SH4Color InitSH4() 
{
    SH4Color sh;
    [unroll]
    for (int i = 0; i < 4; ++i) 
    {
        sh.c[i] = float3(0.0f, 0.0f, 0.0f);
    }
    return sh;
}

void EvaluateSH4Basis(float3 dir, out float sh[4]) 
{
    sh[0] = 0.282095f;
    sh[1] = 0.488603f * dir.y;
    sh[2] = 0.488603f * dir.z;
    sh[3] = 0.488603f * dir.x;
}

void AccumulateRadianceToSH4(inout SH4Color sh, float3 dir, float3 radiance)
{
    float shBasis[4];
    EvaluateSH4Basis(dir, shBasis);

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        sh.c[i] += radiance * shBasis[i];
    }
}

SH4Color TemporalBlendSH4(SH4Color shHistory, SH4Color shCurrent, float historyLength)
{
    float validHistory = min(historyLength, 64.0f);
    float alpha = 1.0f / max(validHistory, 1.0f);

    SH4Color shResult;
    [unroll]
    for (int i = 0; i < 4; ++i) 
    {
        shResult.c[i] = lerp(shHistory.c[i], shCurrent.c[i], alpha);
    }
    return shResult;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    int2 pixelCoord = int2(globalID.xy);
    uint2 dispatchSizeU;
    diffuseGI.GetDimensions(dispatchSizeU.x, dispatchSizeU.y);
    int2 dispatchSize = int2(dispatchSizeU);

    if (any(pixelCoord >= dispatchSize)) return;

    float depth = gbufferAlbedoDepth[pixelCoord].a;
    if (depth > 0.99999f)
    {
        diffuseGISH0[pixelCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        diffuseGISH1[pixelCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        diffuseGISH2[pixelCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        diffuseGISH3[pixelCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    SH4Color sh4Current = InitSH4();
    float3 rawRadiance = diffuseGI[pixelCoord].rgb;
    rawRadiance = SanitizeRadiance(rawRadiance); 
    AccumulateRadianceToSH4(sh4Current, diffuseGIRayDir[pixelCoord].rgb, rawRadiance);

    float3 currPos = gbufferPositionMetallic[pixelCoord].rgb;

    float2 invTexSize = 1.0f / float2(dispatchSize);
    float2 currentUV = (float2(pixelCoord) + 0.5f) * invTexSize;
    float2 currentJitter = ComputeJitterUV(int(cameraData.camera.frameNumber));
    float2 prevJitter = ComputeJitterUV(int(cameraData.camera.frameNumber) - 1);
    float2 motionVector = gbufferMotionVectors[pixelCoord].rg;
    float2 historyTexCoord = currentUV - (motionVector - currentJitter + prevJitter);
    
    bool historyValid = all(historyTexCoord >= 0.0f) && all(historyTexCoord <= 1.0f);

    SH4Color sh4Prev = InitSH4();
    float historyLength = 0.0f;
    bool useHistory = false;

    if (historyValid)
    {
        float2 historyPixelF = historyTexCoord * float2(dispatchSize) - 0.5f;
        int2 historyPixelTL = int2(floor(historyPixelF));
        float2 fracPart = historyPixelF - float2(historyPixelTL);

        uint rngState;
        srand(pixelCoord, int(cameraData.camera.frameNumber), rngState);
        rand(rngState); rand(rngState);

        float2 r = frand2(rngState);
        int2 offset = int2(r.x < fracPart.x ? 1 : 0, r.y < fracPart.y ? 1 : 0);
        int2 historyPixel = historyPixelTL + offset;
        historyPixel = clamp(historyPixel, int2(0, 0), dispatchSize - int2(1, 1));

        float3 prevPos = prevGbufferPositionMetallic[historyPixel].rgb;

        float expectedPrevDist = length(currPos - cameraData.camera.prevViewPos.xyz);
        float prevViewDist = length(prevPos - cameraData.camera.prevViewPos.xyz);
        float depthRel = abs(expectedPrevDist - prevViewDist) / max(expectedPrevDist, 1e-4f);

        bool planeClose = depthRel < 0.25f;

        if (planeClose)
        {
            float4 sh0_hist = prevDiffuseGISH0[historyPixel];
            float3 sh1      = prevDiffuseGISH1[historyPixel].rgb;
            float3 sh2      = prevDiffuseGISH2[historyPixel].rgb;
            float3 sh3      = prevDiffuseGISH3[historyPixel].rgb;

            sh4Prev.c[0] = sh0_hist.rgb;
            sh4Prev.c[1] = sh1;
            sh4Prev.c[2] = sh2;
            sh4Prev.c[3] = sh3;
            
            historyLength = sh0_hist.a;
            useHistory = true;
        }
    }

    if(useHistory)
    {
        historyLength = min(historyLength + 1.0f, 64.0f);
    }
    else
    {
        historyLength = 1.0f;
    }

    sh4Current = TemporalBlendSH4(sh4Prev, sh4Current, historyLength);

    diffuseGISH0[pixelCoord] = float4(sh4Current.c[0], historyLength); 
    diffuseGISH1[pixelCoord] = float4(sh4Current.c[1], 1.0f);
    diffuseGISH2[pixelCoord] = float4(sh4Current.c[2], 1.0f);
    diffuseGISH3[pixelCoord] = float4(sh4Current.c[3], 1.0f);
}