import common;

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

[[vk::binding(0, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferAlbedoDepth;
[[vk::binding(1, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferPositionMetallic;
[[vk::binding(2, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferVertexNormals;
[[vk::binding(3, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gbufferMotionVectors;

[[vk::binding(4, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevGbufferPositionMetallic;
[[vk::binding(5, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevGbufferVertexNormals;
[[vk::binding(6, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevGIReservoirWM;
[[vk::binding(7, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevGIReservoirF;
[[vk::binding(8, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevGIHitPos;
[[vk::binding(9, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> prevGIHitNormal;

[[vk::binding(10, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> diffuseGlobalIlluminationMap;
[[vk::binding(11, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirWM;
[[vk::binding(12, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giReservoirF;
[[vk::binding(13, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giHitPos;
[[vk::binding(14, 1)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giHitNormal;

static const float FLT_EPS = 1e-6f;
static const float M_MAX = 30.0f;
static const float NORMAL_THRESHOLD = 0.9f;
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
    diffuseGlobalIlluminationMap.GetDimensions(dispatchSizeU.x, dispatchSizeU.y);
    int2 dispatchSize = int2(dispatchSizeU);

    if (any(pixelCoord >= dispatchSize)) return;

    float depth = gbufferAlbedoDepth[pixelCoord].a;
    if (depth > 0.99999f)
    {
        giReservoirWM[pixelCoord]  = float4(0.0f, 0.0f, 0.0f, 0.0f);
        giReservoirF[pixelCoord]   = float4(0.0f, 0.0f, 0.0f, 0.0f);
        giHitPos[pixelCoord]       = float4(0.0f, 0.0f, 0.0f, 0.0f);
        giHitNormal[pixelCoord]    = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 fCurrent = Sanitize3(diffuseGlobalIlluminationMap[pixelCoord].rgb);
    float  pCurrent = Luminance(fCurrent);
    float  wCurrent = pCurrent;
    float  mCurrent = 1.0f;

    float3 y_c   = giHitPos[pixelCoord].rgb;
    float4 n_y_c = giHitNormal[pixelCoord];

    if (!IsFinite(wCurrent) || wCurrent < 0.0f)
    {
        wCurrent = 0.0f;
        pCurrent = 0.0f;
        fCurrent = float3(0.0f);
    }

    uint rngState;
    srand(pixelCoord, int(cameraData.camera.frameNumber), rngState);
    rand(rngState); rand(rngState);

    float2 invTexSize    = 1.0f / float2(dispatchSize);
    float2 currentUV     = (float2(pixelCoord) + 0.5f) * invTexSize;
    float2 currentJitter = ComputeJitterUV(int(cameraData.camera.frameNumber));
    float2 prevJitter    = ComputeJitterUV(int(cameraData.camera.frameNumber) - 1);
    float2 motionVector  = gbufferMotionVectors[pixelCoord].rg;
    float2 historyTexCoord = currentUV - (motionVector - currentJitter + prevJitter);
    bool   historyValid = all(historyTexCoord >= 0.0f) && all(historyTexCoord <= 1.0f);

    float3 currNormal = gbufferVertexNormals[pixelCoord].rgb;
    float3 currPos    = gbufferPositionMetallic[pixelCoord].rgb;

    float  wSum              = wCurrent;
    float  c_tot             = mCurrent;
    float3 selectedF         = fCurrent;
    float3 selectedHitPos    = y_c;
    float4 selectedHitNormal = n_y_c;
    float  selectedTargetPdf = pCurrent;

    if (historyValid)
    {
        float2 historyPixelF = historyTexCoord * float2(dispatchSize) - 0.5f;
        int2 historyPixelTL  = int2(floor(historyPixelF));
        float2 fracPart      = historyPixelF - float2(historyPixelTL);

        float2 r = frand2(rngState);
        int2 offset = int2(r.x < fracPart.x ? 1 : 0, r.y < fracPart.y ? 1 : 0);
        int2 historyPixel = historyPixelTL + offset;
        historyPixel = clamp(historyPixel, int2(0, 0), dispatchSize - int2(1, 1));

        float3 prevNormal = prevGbufferVertexNormals[historyPixel].rgb;
        float3 prevPos    = prevGbufferPositionMetallic[historyPixel].rgb;

        float expectedPrevDist = length(currPos - cameraData.camera.prevViewPos.xyz);
        float prevViewDist     = length(prevPos - cameraData.camera.prevViewPos.xyz);
        float depthRel = abs(expectedPrevDist - prevViewDist) / max(expectedPrevDist, 1e-4f);

        bool normalsClose = dot(currNormal, prevNormal) > NORMAL_THRESHOLD;
        bool planeClose   = depthRel < 0.05f;

        if (normalsClose && planeClose)
        {
            float4 prevWM = prevGIReservoirWM[historyPixel].rgba;
            float3 fHistory = Sanitize3(prevGIReservoirF[historyPixel].rgb);
            float  WHistory    = Sanitize(prevWM.r);
            float  mHistoryRaw = Sanitize(prevWM.g);
            if (WHistory    < 0.0f) WHistory    = 0.0f;
            if (mHistoryRaw < 0.0f) mHistoryRaw = 0.0f;

            float mHistory = min(mHistoryRaw, M_MAX);

            float3 y_n   = prevGIHitPos[historyPixel].rgb;
            float4 n_y_n = prevGIHitNormal[historyPixel];

            float3 currToY    = y_n - currPos;
            float  distCurrSq = dot(currToY, currToY);
            float  distCurr   = sqrt(max(distCurrSq, 1e-10f));
            float3 omega_c    = currToY / distCurr;

            float3 neighborToY = y_n - prevPos;
            float  distNeighSq = dot(neighborToY, neighborToY);
            float  distNeigh   = sqrt(max(distNeighSq, 1e-10f));
            float3 omega_n     = neighborToY / distNeigh;

            float cosTheta_c = dot(currNormal, omega_c);
            float cosTheta_n = dot(prevNormal, omega_n);

            bool  shiftValid = true;
            float shiftJacobian = 1.0f;

            bool isSkyHit = (n_y_n.w <= 0.001f);

            if (!isSkyHit)
            {
                float cosThetaY_c = max(0.0f, dot(n_y_n.xyz, -omega_c));
                float cosThetaY_n = max(0.0f, dot(n_y_n.xyz, -omega_n));

                if (cosTheta_c < 0.1f || cosTheta_n < 0.1f ||
                    cosThetaY_c < 0.1f || cosThetaY_n < 0.1f)
                {
                    shiftValid = false;
                }
                else
                {
                    float distanceRatio = distCurrSq / max(distNeighSq, 1e-5f);
                    if (distanceRatio > 2.0f || distanceRatio < 0.5f) shiftValid = false;
                    else if (distCurrSq < 0.001f || distNeighSq < 0.001f) shiftValid = false;
                    else
                    {
                        float G_c = (cosTheta_c * cosThetaY_c) / max(distCurrSq, 1e-4f);
                        float G_n = (cosTheta_n * cosThetaY_n) / max(distNeighSq, 1e-4f);
                        shiftJacobian = G_c / max(G_n, 1e-5f);
                        if (!IsFinite(shiftJacobian) || shiftJacobian <= 0.0f) shiftValid = false;
                        else if (shiftJacobian > 5.0f || shiftJacobian < 0.1f) shiftValid = false;
                    }
                }
            }

            if (shiftValid)
            {
                float pHistoryAtCurrent = Luminance(fHistory) * shiftJacobian;
                if (!IsFinite(pHistoryAtCurrent) || pHistoryAtCurrent < 0.0f)
                    pHistoryAtCurrent = 0.0f;

                float wHistory = mHistory * pHistoryAtCurrent * WHistory;
                if (!IsFinite(wHistory) || wHistory < 0.0f) wHistory = 0.0f;

                wSum  += wHistory;
                c_tot += mHistory;

                if (wSum > 0.0f && frand(rngState) < (wHistory / wSum))
                {
                    selectedF         = fHistory;
                    selectedHitPos    = y_n;
                    selectedHitNormal = n_y_n;
                    selectedTargetPdf = pHistoryAtCurrent;
                }
            }
        }
    }

    float outM = min(c_tot, M_MAX);

    float finalWeight = 0.0f;
    if (selectedTargetPdf > 0.0f && c_tot > 0.0f)
    {
        finalWeight = (1.0f / selectedTargetPdf) * (wSum / c_tot);
    }
    if (!IsFinite(finalWeight) || finalWeight < 0.0f) finalWeight = 0.0f;

    giReservoirWM[pixelCoord]  = float4(finalWeight, outM, 0.0f, 0.0f);
    giReservoirF[pixelCoord]   = float4(selectedF, 1.0f);
    giHitPos[pixelCoord]       = float4(selectedHitPos, 1.0f);
    giHitNormal[pixelCoord]    = selectedHitNormal;
}