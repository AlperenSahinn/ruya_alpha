import common;

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

[[vk::binding(0, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferAlbedoDepth;
[[vk::binding(1, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferPositionMetallic;
[[vk::binding(2, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferNormals;

[[vk::binding(3, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> temporalReuseDiffuseGISH0;
[[vk::binding(4, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> temporalReuseDiffuseGISH1;
[[vk::binding(5, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> temporalReuseDiffuseGISH2;
[[vk::binding(6, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> temporalReuseDiffuseGISH3;

[[vk::binding(7, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH0;
[[vk::binding(8, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH1;
[[vk::binding(9, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH2;
[[vk::binding(10, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH3;

struct SH4Color 
{
    float3 c[4];
};

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

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    int2 pixelCoord = int2(globalID.xy);
    uint2 dispatchSizeU;
    
    temporalReuseDiffuseGISH0.GetDimensions(dispatchSizeU.x, dispatchSizeU.y);
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

    float3 centerNormal = gbufferNormals[pixelCoord].rgb;
    float3 centerPos = gbufferPositionMetallic[pixelCoord].rgb;
    
    float centerHistoryLength = temporalReuseDiffuseGISH0[pixelCoord].a;

    SH4Color sumSH = InitSH4();
    float sumWeight = 0.0f;

    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            int2 neighborCoord = pixelCoord + int2(x, y);

            neighborCoord = clamp(neighborCoord, int2(0, 0), dispatchSize - int2(1, 1));

            float3 neighborPos = gbufferPositionMetallic[neighborCoord].rgb;

            float spatialWeight = exp(-float(x * x + y * y) / 4.0f);

            float3 posDifference = neighborPos - centerPos;
            float planeDistance = abs(dot(centerNormal, posDifference));
            float depthWeight = exp(-planeDistance * 1.0f);

            float w = spatialWeight * depthWeight;

            if (w > 0.01f)
            {
                float4 sh0_hist = temporalReuseDiffuseGISH0[neighborCoord];
                float3 sh1 = temporalReuseDiffuseGISH1[neighborCoord].rgb;
                float3 sh2 = temporalReuseDiffuseGISH2[neighborCoord].rgb;
                float3 sh3 = temporalReuseDiffuseGISH3[neighborCoord].rgb;

                sumSH.c[0] += sh0_hist.rgb * w;
                sumSH.c[1] += sh1 * w;
                sumSH.c[2] += sh2 * w;
                sumSH.c[3] += sh3 * w;

                sumWeight += w;
            }
        }
    }

    if (sumWeight > 0.0001f)
    {
        float invWeight = 1.0f / sumWeight;
        sumSH.c[0] *= invWeight;
        sumSH.c[1] *= invWeight;
        sumSH.c[2] *= invWeight;
        sumSH.c[3] *= invWeight;
    }

    diffuseGISH0[pixelCoord] = float4(sumSH.c[0], centerHistoryLength); 
    diffuseGISH1[pixelCoord] = float4(sumSH.c[1], 1.0f);
    diffuseGISH2[pixelCoord] = float4(sumSH.c[2], 1.0f);
    diffuseGISH3[pixelCoord] = float4(sumSH.c[3], 1.0f);
}