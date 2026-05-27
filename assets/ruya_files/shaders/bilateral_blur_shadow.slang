// [[vk::binding(0, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> albedoDepth;
// [[vk::binding(3, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> normalTex;

// [[vk::binding(0, 1)]] [[vk::image_format("r8")]] RWTexture2D<float> shadowMapInput;
// [[vk::binding(0, 2)]] [[vk::image_format("r8")]] RWTexture2D<float> shadowMapOutput;

// struct PushConstants
// {
//     int2 direction;
// };
// [[vk::push_constant]] ConstantBuffer<PushConstants> pc;

// static const int KERNEL_RADIUS = 5;

// static const float gaussianWeights[11] = {
//     0.017, 0.045, 0.098, 0.177, 0.251, 0.282, 0.251, 0.177, 0.098, 0.045, 0.017
// };

// [shader("compute")]
// [numthreads(16, 16, 1)]
// void computeMain(
//     uint3 workGroupID       : SV_GroupID,
//     uint3 localInvocationID : SV_GroupThreadID,
//     uint3 globalID          : SV_DispatchThreadID)
// {
//     uint2 imgSize;
//     shadowMapInput.GetDimensions(imgSize.x, imgSize.y);

//     int2 pixelCoord;
//     if (pc.direction.x == 1)
//         pixelCoord = int2(int(workGroupID.x) * 128 + int(localInvocationID.x), int(workGroupID.y));
//     else
//         pixelCoord = int2(int(workGroupID.y), int(workGroupID.x) * 128 + int(localInvocationID.x));

//     if (pixelCoord.x >= int(imgSize.x) || pixelCoord.y >= int(imgSize.y)) return;

//     float  centerShadow = shadowMapInput[pixelCoord];
//     float  centerDepth  = albedoDepth[pixelCoord].a;
//     float3 centerNormal = normalTex[pixelCoord].rgb;

//     if (centerDepth > 0.99999)
//     {
//         shadowMapOutput[pixelCoord] = centerShadow;
//         return;
//     }

//     float totalWeight = 0.0;
//     float totalShadow = 0.0;

//     for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
//     {
//         int2 sampleCoord = clamp(pixelCoord + pc.direction * i, int2(0, 0), int2(imgSize) - int2(1, 1));

//         float  s = shadowMapInput[sampleCoord];
//         float  d = albedoDepth[sampleCoord].a;
//         float3 n = normalTex[sampleCoord].rgb;

//         float wSpatial = gaussianWeights[i + KERNEL_RADIUS];
//         float wDepth   = exp(-abs(centerDepth - d) * 1000.0);
//         float wNormal  = pow(max(dot(centerNormal, n), 0.0), 16.0);

//         float w = wSpatial * wDepth * wNormal;
//         totalShadow += s * w;
//         totalWeight += w;
//     }

//     shadowMapOutput[pixelCoord] = totalShadow / max(totalWeight, 0.00001);
// }

[[vk::binding(0, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> albedoDepth;
[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> normalTex;

[[vk::binding(2, 0)]] [[vk::image_format("r8")]] RWTexture2D<float> shadowMapInput;
[[vk::binding(3, 0)]] [[vk::image_format("r8")]] RWTexture2D<float> shadowMapOutput;

static const int KERNEL_RADIUS = 1;

static const float gaussianWeights[3] = {
    0.25, 0.50, 0.25
};

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(
    uint3 workGroupID       : SV_GroupID,
    uint3 localInvocationID : SV_GroupThreadID,
    uint3 globalID          : SV_DispatchThreadID)
{
    uint2 imgSize;
    shadowMapInput.GetDimensions(imgSize.x, imgSize.y);

    int2 pixelCoord = int2(globalID.xy);

    if (pixelCoord.x >= int(imgSize.x) || pixelCoord.y >= int(imgSize.y)) return;

    float  centerShadow = shadowMapInput[pixelCoord];
    float  centerDepth  = albedoDepth[pixelCoord].a;
    float3 centerNormal = normalTex[pixelCoord].rgb;

    if (centerDepth > 0.99999)
    {
        shadowMapOutput[pixelCoord] = centerShadow;
        return;
    }

    float totalWeight = 0.0;
    float totalShadow = 0.0;

    for (int y = -KERNEL_RADIUS; y <= KERNEL_RADIUS; ++y)
    {
        for (int x = -KERNEL_RADIUS; x <= KERNEL_RADIUS; ++x)
        {
            int2 sampleCoord = clamp(pixelCoord + int2(x, y), int2(0, 0), int2(imgSize) - int2(1, 1));

            float  s = shadowMapInput[sampleCoord];
            float  d = albedoDepth[sampleCoord].a;
            float3 n = normalTex[sampleCoord].rgb;

            float wSpatial = gaussianWeights[x + KERNEL_RADIUS] * gaussianWeights[y + KERNEL_RADIUS];
            float wDepth   = exp(-abs(centerDepth - d) * 1000.0);
            float wNormal  = pow(max(dot(centerNormal, n), 0.0), 16.0);

            float w = wSpatial * wDepth * wNormal;
            totalShadow += s * w;
            totalWeight += w;
        }
    }

    shadowMapOutput[pixelCoord] = totalShadow / max(totalWeight, 0.00001);
}