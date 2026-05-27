import common;

[[vk::binding(0, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferNormals;

[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH0;
[[vk::binding(2, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH1;
[[vk::binding(3, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH2;
[[vk::binding(4, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGISH3;

[[vk::binding(5, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGIResolve;

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    int2 pixelCoord = int2(globalID.xy);
    
    uint2 dispatchSizeU;
    diffuseGIResolve.GetDimensions(dispatchSizeU.x, dispatchSizeU.y);
    if (any(pixelCoord >= int2(dispatchSizeU))) return;

    float3 normal = gbufferNormals[pixelCoord].rgb;
    
    if (dot(normal, normal) < 0.1f)
    {
        diffuseGIResolve[pixelCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    normal = normalize(normal);

    float3 sh0 = diffuseGISH0[pixelCoord].rgb;
    float3 sh1 = diffuseGISH1[pixelCoord].rgb;
    float3 sh2 = diffuseGISH2[pixelCoord].rgb;
    float3 sh3 = diffuseGISH3[pixelCoord].rgb;

    const float C0 = 0.8862269f;
    const float C1 = 1.0233267f;

    float3 irradiance = sh0 * C0 + sh1 * (C1 * normal.y) + sh2 * (C1 * normal.z) + sh3 * (C1 * normal.x);

    irradiance = max(irradiance, float3(0.0f, 0.0f, 0.0f));

    float3 diffuseColor = irradiance;

    diffuseGIResolve[pixelCoord] = float4(diffuseColor, 1.0f);
}