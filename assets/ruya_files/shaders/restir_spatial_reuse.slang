import common;

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

[[vk::binding(0, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferAlbedoDepth;
[[vk::binding(1, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferPositionMetallic;
[[vk::binding(2, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferVertexNormals;

[[vk::binding(3, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirWM;
[[vk::binding(4, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirF;
[[vk::binding(5, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giHitPos;
[[vk::binding(6, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giHitNormal;

[[vk::binding(7, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirWMResolved;
[[vk::binding(8, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirFResolved;
[[vk::binding(9, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirHitPosResolved;
[[vk::binding(10, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirHitNormalResolved;

static const float FLT_EPS = 1e-6f;
static const float NORMAL_THRESHOLD = 0.9f;
static const int NUM_SPATIAL_SAMPLES = 10;
static const float SPATIAL_RADIUS = 30.0f;
static const float SPATIAL_M_MAX  = 30.0f;

int rand(inout uint s) 
{ 
    s = s * 214013u + 2531011u; 
    return (int)((s >> 16) & 32767); 
}

float frand(inout uint s) 
{ 
    return float(rand(s)) / 32767.0; 
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

float Luminance(float3 color) 
{ 
    return dot(color, float3(0.299, 0.587, 0.114)); 
}

bool IsFinite(float x) 
{ 
    return !(isnan(x) || isinf(x)); 

}
bool IsFinite3(float3 v) 
{ 
    return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z); 
}

float3 Sanitize3(float3 v) 
{ 
    return IsFinite3(v) ? v : float3(0.0f, 0.0f, 0.0f); 
}

float Sanitize(float x) 
{ 
    return IsFinite(x) ? x : 0.0f; 
}

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    int2 pixelCoord = int2(globalID.xy);
    uint2 dispatchSizeU;
    giReservoirF.GetDimensions(dispatchSizeU.x, dispatchSizeU.y);
    int2 dispatchSize = int2(dispatchSizeU);

    if (any(pixelCoord >= dispatchSize)) return;

    float depth = gbufferAlbedoDepth[pixelCoord].a;
    if (depth > 0.99999f)
    {
        giReservoirWMResolved[pixelCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        giReservoirFResolved[pixelCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    uint rngState;
    srand(pixelCoord, int(cameraData.camera.frameNumber) + 1234, rngState);
    rand(rngState); rand(rngState);
    float4 centerWM = giReservoirWM[pixelCoord];
    float3 centerF  = Sanitize3(giReservoirF[pixelCoord].rgb);

    float WCenter = Sanitize(centerWM.r);
    float mCenter = Sanitize(centerWM.g);
    if (WCenter < 0.0f) WCenter = 0.0f;
    if (mCenter < 0.0f) mCenter = 0.0f;

    float pCenter = Luminance(centerF);
    float wCenter = pCenter * WCenter * mCenter;
    if (!IsFinite(wCenter) || wCenter < 0.0f) wCenter = 0.0f;

    float3 currNormal = gbufferVertexNormals[pixelCoord].rgb;
    float3 currPos    = gbufferPositionMetallic[pixelCoord].rgb;

    float3 centerHitPos     = giHitPos[pixelCoord].rgb;
    float4 centerHitNormal  = giHitNormal[pixelCoord];

    float  wSum              = wCenter;
    float  c_tot             = mCenter;
    float3 selectedF         = centerF;
    float3 selectedHitPos    = centerHitPos;
    float4 selectedHitNormal = centerHitNormal;
    float  selectedTargetPdf = pCenter;

    for (int i = 0; i < NUM_SPATIAL_SAMPLES; ++i)
    {
        float angle  = frand(rngState) * 2.0f * PI;
        float radius = sqrt(frand(rngState)) * SPATIAL_RADIUS;
        int2 offset = int2(int(cos(angle) * radius), int(sin(angle) * radius));
        int2 neighborCoord = pixelCoord + offset;

        if (any(neighborCoord < 0) || any(neighborCoord >= dispatchSize) || all(offset == 0)) continue;

        float3 neighborNormal = gbufferVertexNormals[neighborCoord].rgb;
        float3 neighborPos    = gbufferPositionMetallic[neighborCoord].rgb;
        float3 delta  = neighborPos - currPos;
        float  distSq = dot(delta, delta);

        bool normalsClose = dot(currNormal, neighborNormal) > NORMAL_THRESHOLD;
        bool worldClose   = distSq < 0.05f;
        if (!(worldClose)) continue;

        float4 neighborWM = giReservoirWM[neighborCoord];
        float3 neighborF  = Sanitize3(giReservoirF[neighborCoord].rgb);
        float  WNeighbor    = Sanitize(neighborWM.r);
        float  mNeighborRaw = Sanitize(neighborWM.g);
        if (WNeighbor    < 0.0f) WNeighbor    = 0.0f;
        if (mNeighborRaw < 0.0f) mNeighborRaw = 0.0f;

        float mNeighbor = min(mNeighborRaw, SPATIAL_M_MAX);

        float3 y   = giHitPos[neighborCoord].rgb;
        float4 n_y = giHitNormal[neighborCoord];

        float3 currToY    = y - currPos;
        float  distCurrSq = dot(currToY, currToY);
        float  distCurr   = sqrt(max(distCurrSq, 1e-10f));
        float3 omega_c    = currToY / distCurr;

        float3 neighborToY = y - neighborPos;
        float  distNeighSq = dot(neighborToY, neighborToY);
        float  distNeigh   = sqrt(max(distNeighSq, 1e-10f));
        float3 omega_n     = neighborToY / distNeigh;

        bool  isSkyHit       = (n_y.w <= 0.001f);
        float shiftJacobian  = 1.0f;

        if (!isSkyHit)
        {
            float cosTheta_c = dot(currNormal,     omega_c);
            float cosTheta_n = dot(neighborNormal, omega_n);
            if (cosTheta_c < 0.01f || cosTheta_n < 0.01f) continue;

            float cosThetaY_c = max(0.0f, dot(n_y.xyz, -omega_c));
            float cosThetaY_n = max(0.0f, dot(n_y.xyz, -omega_n));
            if (cosThetaY_c < 0.01f || cosThetaY_n < 0.01f) continue;
            if (distCurrSq  < 0.001f || distNeighSq < 0.001f) continue;

            float G_c = (cosTheta_c * cosThetaY_c) / max(distCurrSq, 1e-4f);
            float G_n = (cosTheta_n * cosThetaY_n) / max(distNeighSq, 1e-4f);
            shiftJacobian = G_c / max(G_n, 1e-5f);

            if (!IsFinite(shiftJacobian) || shiftJacobian <= 0.0f) continue;
            if (shiftJacobian > 10.0f || shiftJacobian < 0.01f)    continue;
        }

        float pNeighborAtCenter = Luminance(neighborF) * shiftJacobian;
        if (!IsFinite(pNeighborAtCenter) || pNeighborAtCenter < 0.0f) pNeighborAtCenter = 0.0f;

        float wNeighbor = mNeighbor * pNeighborAtCenter * WNeighbor;

        c_tot += mNeighbor;

        if (!IsFinite(wNeighbor) || wNeighbor < 0.0f) wNeighbor = 0.0f;

        wSum  += wNeighbor;

        if (wSum > 0.0f && frand(rngState) < (wNeighbor / wSum))
        {
            selectedF         = neighborF;
            selectedHitPos    = y;
            selectedHitNormal = n_y;
            selectedTargetPdf = pNeighborAtCenter;
        }
    }

    float outM = min(c_tot, SPATIAL_M_MAX);

    float finalWeight = 0.0f;
    if (selectedTargetPdf > 0.0f && c_tot > 0.0f)
    {
        finalWeight = (1.0f / selectedTargetPdf) * (wSum / c_tot);
    }
    if (!IsFinite(finalWeight) || finalWeight < 0.0f) finalWeight = 0.0f;

    giReservoirWMResolved[pixelCoord]        = float4(finalWeight, outM, 0.0f, 0.0f);
    giReservoirFResolved[pixelCoord]         = float4(selectedF, 1.0f);
    giReservoirHitPosResolved[pixelCoord]    = float4(selectedHitPos, 1.0f);
    giReservoirHitNormalResolved[pixelCoord] = selectedHitNormal;
}