import common;

[[vk::binding(0, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirWMResolve;
[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirFResolve;
[[vk::binding(2, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> restirResolve;

[shader("compute")]
[numthreads(128, 1, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    int2 texCoord = int2(globalID.xy);
    uint2 texSizeU;
    restirResolve.GetDimensions(texSizeU.x, texSizeU.y);
    int2 texSize = int2(texSizeU);

    if (texCoord.x >= texSize.x || texCoord.y >= texSize.y) return;

    float3 irradiance = giReservoirFResolve[texCoord].rgb * giReservoirWMResolve[texCoord].r;

    restirResolve[texCoord] = float4(irradiance, 1.0);
}
