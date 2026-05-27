import common;

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

[[vk::binding(0, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> albedoDepthHi;
[[vk::binding(1, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> normalHi;

[[vk::binding(2, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> albedoDepthLo;
[[vk::binding(3, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> normalLo;

[[vk::binding(4, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> signalIn;
[[vk::binding(5, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> signalOut;

static const int   FOOTPRINT_RADIUS = 1;
static const float SPATIAL_SIGMA = 1.0;
static const float PLANE_SIGMA = 0.05;
static const float NORMAL_POWER = 32.0;

float3 reconstructViewPos(float2 uv, float depthNDC, float4x4 invViewProj, float3 cameraWorldPos)
{
    float2 ndcXY = uv * 2.0 - 1.0;
    ndcXY.y = 1 - ndcXY.y;
    float4 clip  = float4(ndcXY, depthNDC, 1.0);
    float4 world = mul(invViewProj, clip);
    world /= world.w;
    return world.xyz - cameraWorldPos;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    uint2 hiSize;
    signalOut.GetDimensions(hiSize.x, hiSize.y);

    uint2 loSize;
    signalIn.GetDimensions(loSize.x, loSize.y);

    int2 hiCoord = int2(globalID.xy);
    if (hiCoord.x >= int(hiSize.x) || hiCoord.y >= int(hiSize.y)) return;

    Camera   cam        = cameraData.camera;
    float4x4 invVP      = cam.invViewproj;
    float3   camPos     = cam.viewPos.xyz;

    float  centerDepth  = albedoDepthHi[hiCoord].a;
    float3 centerNormal = normalize(normalHi[hiCoord].rgb);

    float2 loCoordF = (float2(hiCoord) + 0.5) * 0.5 - 0.5;
    if (centerDepth > 0.99999)
    {
        int2 nearest = clamp(int2(round(loCoordF)),
                             int2(0, 0), int2(loSize) - int2(1, 1));
        signalOut[hiCoord] = signalIn[nearest];
        return;
    }

    float2 hiUV     = (float2(hiCoord) + 0.5) / float2(hiSize);
    float3 centerP  = reconstructViewPos(hiUV, centerDepth, invVP, camPos);

    int2  baseLo = int2(floor(loCoordF));
    float2 frac  = loCoordF - float2(baseLo);

    float4 totalSignal = float4(0, 0, 0, 0);
    float  totalWeight = 0.0;

    const float invTwoSigmaSq = 1.0 / (2.0 * SPATIAL_SIGMA * SPATIAL_SIGMA);

    [unroll]
    for (int dy = -FOOTPRINT_RADIUS; dy <= FOOTPRINT_RADIUS + 1; ++dy)
    {
        [unroll]
        for (int dx = -FOOTPRINT_RADIUS; dx <= FOOTPRINT_RADIUS + 1; ++dx)
        {
            int2 loCoord = clamp(baseLo + int2(dx, dy),
                                 int2(0, 0), int2(loSize) - int2(1, 1));

            float2 delta = float2(dx, dy) - frac;
            float  d2    = dot(delta, delta);
            float  wSpatial = exp(-d2 * invTwoSigmaSq);

            float  sDepth  = albedoDepthLo[loCoord].a;
            float3 sNormal = normalize(normalLo[loCoord].rgb);
            float4 sSignal = signalIn[loCoord];

            if (sDepth > 0.99999) continue;

            float2 sUV = (float2(loCoord) + 0.5) / float2(loSize);
            float3 sP  = reconstructViewPos(sUV, sDepth, invVP, camPos);

            float planeDist = abs(dot(centerNormal, sP - centerP));
            float wPlane    = exp(-planeDist / max(PLANE_SIGMA, 1e-4));

            float nDot    = max(dot(centerNormal, sNormal), 0.0);
            float wNormal = pow(nDot, NORMAL_POWER);

            float w = wSpatial * wPlane * wNormal;

            totalSignal += sSignal * w;
            totalWeight += w;
        }
    }

    if (totalWeight < 1e-4)
    {
        int2 nearest = clamp(int2(round(loCoordF)),
                             int2(0, 0), int2(loSize) - int2(1, 1));
        signalOut[hiCoord] = signalIn[nearest];
        return;
    }

    signalOut[hiCoord] = totalSignal / totalWeight;
}