import common;

struct AtmosphericLightData { AtmosphericLight atmosphericLight; };
[[vk::binding(0, 0)]] ConstantBuffer<AtmosphericLightData> atmosphericLightData;

[[vk::binding(0, 1)]] RaytracingAccelerationStructure TLAS;

[[vk::binding(0, 2)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferPositionMetallic;

[[vk::binding(1, 2)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferVertexNormals;

[[vk::binding(2, 2)]] [[vk::image_format("r8")]] RWTexture2D<float> shadowMap;

struct ShadowPayload
{
    float shadow;
};

static const float SHADOW_BIAS = 0.0001f;
static const uint RAY_FLAGS = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE;

[shader("raygeneration")]
void rayGenMain()
{
    uint2 pixelCoord = DispatchRaysIndex().xy;

    float3 worldPos = gbufferPositionMetallic[pixelCoord].rgb;
    float3 normal = gbufferVertexNormals[pixelCoord].rgb;

    float3 lightDirection = normalize(-atmosphericLightData.atmosphericLight.sunDirection.xyz);

    float3 rayOrigin = worldPos + normal * SHADOW_BIAS;
    float3 rayDirection = lightDirection;

    RayDesc ray;
    ray.Origin    = rayOrigin;
    ray.TMin      = 0.01;
    ray.Direction = rayDirection;
    ray.TMax      = 1000.0;

    ShadowPayload payload;
    payload.shadow = 0.0;

    TraceRay(
        TLAS,
        RAY_FLAGS,
        0xFF,   
        0,      
        1,      
        0,      
        ray,
        payload);

    shadowMap[pixelCoord] = payload.shadow;
}

[shader("miss")]
void missMain(inout ShadowPayload payload)
{
    payload.shadow = 1.0;
}
