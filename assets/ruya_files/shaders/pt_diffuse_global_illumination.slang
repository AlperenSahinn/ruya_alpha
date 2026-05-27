import common;

struct RayPayload
{
    float3 color;
    float3 throughput;
    float3 nextOrigin;
    float3 nextDirection;
    uint   rngState;
    uint   bounceIndex;
    bool   terminated;
    float4 hitNormal;
};

struct ShadowRayPayload
{
    bool occluded;
};

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

struct AtmosphericLightData { AtmosphericLight atmosphericLight; };
[[vk::binding(0, 1)]] ConstantBuffer<AtmosphericLightData> atmosphericLightData;

[[vk::binding(0, 2)]] RaytracingAccelerationStructure topLevelAS;

[[vk::binding(0, 3)]] Sampler2D image2Ds[];

struct MaterialEntry { Material material; };
[[vk::binding(0, 4)]] StructuredBuffer<MaterialEntry> materials[];

struct RenderGeometryEntry { RenderGeometry renderGeometry; };
[[vk::binding(0, 5)]] StructuredBuffer<RenderGeometryEntry> renderGeometries[];

[[vk::binding(0, 6)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> positionImage;
[[vk::binding(1, 6)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> normalImage;

[[vk::binding(2, 6)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giResultImage;
[[vk::binding(3, 6)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> giRayDir;

struct VertexBuffer { Vertex vertices[1]; };
struct IndexBuffer  { uint   indices[1];  };

static const int bounceCount = 3;

static const float GROUND_RADIUS = 6360000.0;
static const float ATMO_RADIUS   = 6420000.0;

static const float3 RAYLEIGH_BETA = float3(5.5e-6, 13.0e-6, 22.4e-6);

static const int VIEW_SAMPLES  = 16;
static const int LIGHT_SAMPLES = 8;

bool raySphereIntersect(float3 ro, float3 rd, float radius, out float t0, out float t1)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float disc = b * b - c;
    t0 = 0.0;
    t1 = 0.0;
    if (disc < 0.0) return false;
    float sq = sqrt(disc);
    t0 = -b - sq;
    t1 = -b + sq;
    return true;
}

float rayleighPhase(float cosTheta)
{
    return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float hgPhase(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

float3 computeSunTransmittance(float3 sunDir, float mieBeta, float rayleighH, float mieH)
{
    float3 ro = float3(0.0, GROUND_RADIUS + 1.0, 0.0);

    float tA0, tA1;
    if (!raySphereIntersect(ro, sunDir, ATMO_RADIUS, tA0, tA1))
    {
        return float3(1.0, 1.0, 1.0);
    }

    float tGnd0, tGnd1;
    bool blocked = raySphereIntersect(ro, sunDir, GROUND_RADIUS, tGnd0, tGnd1) && tGnd0 > 0.0;

    float tEnd = max(tA1, 0.0);
    const int SUN_T_SAMPLES = 8;
    float segLen = tEnd / float(SUN_T_SAMPLES);

    float odR = 0.0;
    float odM = 0.0;

    for (int i = 0; i < SUN_T_SAMPLES; ++i)
    {
        float t = (float(i) + 0.5) * segLen;
        float3 p = ro + sunDir * t;
        float alt = length(p) - GROUND_RADIUS;
        if (alt < 0.0) { blocked = true; break; }
        odR += exp(-alt / rayleighH) * segLen;
        odM += exp(-alt / mieH)      * segLen;
    }

    float3 tau = RAYLEIGH_BETA * odR + mieBeta * 1.1 * odM;
    float3 T   = exp(-tau);

    if (blocked) T *= 0.0;
    return T;
}

float3 computeVisibleSunColor(float3 baseSunColor, float3 sunDir,
                              float mieBeta, float rayleighH, float mieH)
{
    return baseSunColor * computeSunTransmittance(sunDir, mieBeta, rayleighH, mieH);
}

float sunElevationFactor(float3 sunDir)
{
    return smoothstep(-0.10, 0.10, sunDir.y);
}

float goldenHourFactor(float3 sunDir)
{
    float y = saturate(sunDir.y);
    return saturate(1.0 - smoothstep(0.0, 0.30, y));
}

float3 computeAtmosphere(float3 ro, float3 rd, float3 sunDir, float mieBeta, float mieG, float sunIntensity)
{
    float tAtmo0, tAtmo1;
    if (!raySphereIntersect(ro, rd, ATMO_RADIUS, tAtmo0, tAtmo1))
    {
        return float3(0.0, 0.0, 0.0);
    }

    float3 up = normalize(ro);
    float viewDotUp = dot(rd, up);

    float tGnd0, tGnd1;
    bool hitGround = raySphereIntersect(ro, rd, GROUND_RADIUS, tGnd0, tGnd1);

    float3 marchDir = rd;
    float marchTEnd = tAtmo1;
    if (hitGround && tGnd0 > 0.0)
    {
        marchDir = normalize(rd - viewDotUp * up);
        float tH0, tH1;
        raySphereIntersect(ro, marchDir, ATMO_RADIUS, tH0, tH1);
        marchTEnd = tH1;
    }

    float tStart = max(tAtmo0, 0.0);
    float tEnd = marchTEnd;
    if (tStart >= tEnd) return float3(0.0, 0.0, 0.0);

    float segmentLength = (tEnd - tStart) / float(VIEW_SAMPLES);
    float cosTheta = dot(marchDir, sunDir);

    float phaseR = rayleighPhase(cosTheta);
    float phaseM = hgPhase(cosTheta, mieG);

    float optDepthR = 0.0;
    float optDepthM = 0.0;

    float3 totalR = float3(0.0, 0.0, 0.0);
    float3 totalM = float3(0.0, 0.0, 0.0);

    float rayleighHeight = atmosphericLightData.atmosphericLight.scatterParams.z;
    float mieHeight = atmosphericLightData.atmosphericLight.scatterParams.w;

    for (int i = 0; i < VIEW_SAMPLES; i++)
    {
        float t = tStart + (float(i) + 0.5) * segmentLength;
        float3 pos = ro + marchDir * t;
        float altitude = length(pos) - GROUND_RADIUS;

        float hr = exp(-altitude / rayleighHeight) * segmentLength;
        float hm = exp(-altitude / mieHeight) * segmentLength;

        optDepthR += hr;
        optDepthM += hm;

        float tSun0, tSun1;
        raySphereIntersect(pos, sunDir, ATMO_RADIUS, tSun0, tSun1);
        float sunSegLen = tSun1 / float(LIGHT_SAMPLES);

        float sunOptR = 0.0;
        float sunOptM = 0.0;
        bool blocked = false;

        for (int j = 0; j < LIGHT_SAMPLES; j++)
        {
            float ts = (float(j) + 0.5) * sunSegLen;
            float3 sunPos = pos + sunDir * ts;
            float sunAlt = length(sunPos) - GROUND_RADIUS;

            if (sunAlt < 0.0)
            {
                blocked = true;
                break;
            }

            sunOptR += exp(-sunAlt / rayleighHeight) * sunSegLen;
            sunOptM += exp(-sunAlt / mieHeight) * sunSegLen;
        }

        if (!blocked)
        {
            float3 tau = RAYLEIGH_BETA * (optDepthR + sunOptR)
                       + mieBeta * 1.1 * (optDepthM + sunOptM);
            float3 attenuation = exp(-tau);

            totalR += hr * attenuation;
            totalM += hm * attenuation;
        }
    }

    float3 sky = sunIntensity * (totalR * RAYLEIGH_BETA * phaseR + totalM * mieBeta * phaseM);

    float3 groundCol = atmosphericLightData.atmosphericLight.groundAlbedo.rgb;
    float groundRefl = atmosphericLightData.atmosphericLight.groundAlbedo.a;
    float sunUp = max(dot(sunDir, up), 0.0);

    float3 visibleSunCol = computeSunTransmittance(sunDir, mieBeta, rayleighHeight, mieHeight);
    float3 groundBounce = groundCol * groundRefl * sunUp * RAYLEIGH_BETA * 0.5 * visibleSunCol;
    sky += groundBounce * sunIntensity * 0.1;

    if (hitGround && tGnd0 > 0.0)
    {
        float belowAmount = clamp(-viewDotUp * 3.0, 0.0, 1.0);

        float elev = sunElevationFactor(sunDir);
        float golden = goldenHourFactor(sunDir);

        float3 earthBase = groundCol;
        if (dot(earthBase, float3(1.0)) < 0.05)
        {
            earthBase = float3(0.10, 0.06, 0.035);
        }

        float3 lit = earthBase * visibleSunCol * (0.05 + 0.35 * elev);
        float3 twilight = earthBase * float3(0.30, 0.12, 0.05) * (0.03 + 0.12 * golden);
        float3 nightCol = earthBase * 0.002;

        float3 belowCol = lerp(nightCol, lit, elev);
        belowCol = lerp(belowCol, belowCol + twilight, golden * 0.6);

        float depthDarken = lerp(1.0, 0.35, belowAmount);
        belowCol *= depthDarken;

        sky = lerp(sky, belowCol, belowAmount * belowAmount);
    }

    return sky;
}

int rand(inout uint s)
{
    s = s * 214013u + 2531011u;
    return (int)((s >> 16) & 32767);
}

float frand(inout uint s)         { return float(rand(s)) / 32767.0; }
float2 frand2(inout uint s)       { return float2(frand(s), frand(s)); }
float3 frand3(inout uint s)       { return float3(frand(s), frand(s), frand(s)); }

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

float3 cosineSampleHemisphere(float3 n, inout uint s)
{
    float2 rnd = frand2(s);
    float a = PI * 2.0 * rnd.x;
    float b = 2.0 * rnd.y - 1.0;
    float3 dir = float3(sqrt(1.0 - b * b) * float2(cos(a), sin(a)), b);
    return normalize(n + dir);
}

[shader("miss")]
void missMain(inout RayPayload payload)
{
    float3 rayOrigin = WorldRayOrigin();
    float3 rayDirection = WorldRayDirection();

    float3 sunDir = normalize(-atmosphericLightData.atmosphericLight.sunDirection.xyz);
    float sunIntensity = atmosphericLightData.atmosphericLight.sunColor.a;
    float mieBeta = atmosphericLightData.atmosphericLight.scatterParams.x;
    float mieG = atmosphericLightData.atmosphericLight.scatterParams.y;

    float3 planetPos = rayOrigin + float3(0.0, GROUND_RADIUS, 0.0);
    float planetAlt = length(planetPos) - GROUND_RADIUS;
    if (planetAlt < 1.0)
    {
        planetPos = normalize(planetPos) * (GROUND_RADIUS + 1.0);
    }

    float3 sampleDir = normalize(rayDirection);

    float3 skyColor = computeAtmosphere(planetPos, sampleDir, sunDir, mieBeta, mieG, sunIntensity);

    float skyExposure = atmosphericLightData.atmosphericLight.skyParams.z;
    skyColor *= skyExposure;

    payload.color += payload.throughput * skyColor;
    payload.terminated = true;

    payload.nextOrigin = rayOrigin + 10.0f * rayDirection;
    
    payload.hitNormal = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

[shader("miss")]
void shadowMissMain(inout ShadowRayPayload payload)
{
    payload.occluded = false;
}

[shader("closesthit")]
void closestHitMain(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint geomIndex = InstanceID();
    RenderGeometry renderGeometry = renderGeometries[geomIndex][0].renderGeometry;
    Material material = materials[renderGeometry.materialDescriptorIndex][0].material;

    VertexBuffer* vb = (VertexBuffer*)renderGeometry.vertexBufferAddress;
    IndexBuffer* ib = (IndexBuffer*)renderGeometry.indexBufferAddress;

    uint primIdx = PrimitiveIndex();
    uint i0 = ib->indices[primIdx * 3 + 0];
    uint i1 = ib->indices[primIdx * 3 + 1];
    uint i2 = ib->indices[primIdx * 3 + 2];

    Vertex v0 = vb->vertices[i0];
    Vertex v1 = vb->vertices[i1];
    Vertex v2 = vb->vertices[i2];

    float3 bary = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    float2 texCoord = v0.texCoord * bary.x + v1.texCoord * bary.y + v2.texCoord * bary.z;

    float3 nObj = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);

    float3x4 o2w = ObjectToWorld3x4();
    float3x3 o2wRot = float3x3(o2w[0].xyz, o2w[1].xyz, o2w[2].xyz);

    float3 N = normalize(mul(o2wRot, nObj));

    float3 geomNormal = N;

    float3 albedo = image2Ds[material.albedoImageDescriptorIndex].SampleLevel(texCoord, 0.0).rgb;

    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 v = -normalize(WorldRayDirection());

    if (dot(geomNormal, v) < 0.0)
    {
        geomNormal = -geomNormal;
    }

    const float originBias = 0.0001;

    float3 sunDir = normalize(-atmosphericLightData.atmosphericLight.sunDirection.xyz);
    float NoLg = dot(geomNormal, sunDir);

    float mieBeta = atmosphericLightData.atmosphericLight.scatterParams.x;
    float rayleighH = atmosphericLightData.atmosphericLight.scatterParams.z;
    float mieH = atmosphericLightData.atmosphericLight.scatterParams.w;

    float3 baseSunColor = atmosphericLightData.atmosphericLight.sunColor.rgb;
    float  sunIntensityBR = atmosphericLightData.atmosphericLight.sunColor.a;
    float3 visibleSunCol = computeVisibleSunColor(baseSunColor, sunDir, mieBeta, rayleighH, mieH);
    float3 sunRadiance = visibleSunCol * sunIntensityBR;

    if (NoLg > 0.0)
    {
        RayDesc shadowRay;
        shadowRay.Origin = hitPos + geomNormal * originBias;
        shadowRay.Direction = sunDir;
        shadowRay.TMin = 0.01;
        shadowRay.TMax = 1000.0;

        ShadowRayPayload shadowPayload;
        shadowPayload.occluded = true;

        const uint shadowFlags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;

        TraceRay(
            topLevelAS, 
            shadowFlags, 
            0xFF, 
            0, 
            1, 
            1, 
            shadowRay, 
            shadowPayload);

        if (!shadowPayload.occluded)
        {

            float3 directContrib = (albedo / PI) * sunRadiance * NoLg;
            payload.color += payload.throughput * directContrib;
        }
    }

    float3 l = cosineSampleHemisphere(geomNormal, payload.rngState);

    if (dot(geomNormal, l) <= 0.0)
    {
        payload.terminated = true;
        return;
    }

    payload.throughput *= albedo;
    payload.nextOrigin = hitPos + geomNormal * originBias;
    payload.nextDirection = normalize(l);
    payload.bounceIndex += 1;
    payload.terminated = false;
    payload.hitNormal = float4(geomNormal, 1.0f);
}

[shader("raygeneration")]
void rayGenMain()
{
    uint2 pixelCoord = DispatchRaysIndex().xy;

    float4 posMetallic = positionImage[pixelCoord];
    float4 normalRough = normalImage[pixelCoord];

    float3 worldPos = posMetallic.xyz;
    float3 normal = normalize(normalRough.xyz);

    uint rngState;
    srand(int2(pixelCoord), int(cameraData.camera.frameNumber), rngState);

    const float originBias = 0.01;
    const uint rayFlags = RAY_FLAG_FORCE_OPAQUE;

    float3 accumulatedColor = float3(0.0, 0.0, 0.0);

    float3 firstDir = cosineSampleHemisphere(normal, rngState);
    bool validDir = dot(normal, firstDir) > 0.0;

    for (int s = 0; s < int(cameraData.camera.pathTracerSampleCount); ++s)
    {
        RayDesc ray;
        ray.Origin = worldPos + normal * originBias;
        ray.Direction = normalize(firstDir);
        ray.TMin = 0.01;
        ray.TMax = 1000.0;

        RayPayload payload;
        payload.color = float3(0.0, 0.0, 0.0);
        payload.throughput = float3(1.0, 1.0, 1.0);
        payload.nextOrigin = ray.Origin;
        payload.nextDirection = ray.Direction;
        payload.rngState = rngState;
        payload.bounceIndex = 0;
        payload.terminated = !validDir;
        payload.hitNormal = float4(0.0, 0.0, 0.0, 0.0);

        for (int i = 0; i < bounceCount; ++i)
        {
            if (payload.terminated) break;

            TraceRay(topLevelAS, rayFlags, 0xFF, 0, 1, 0, ray, payload);

            if (payload.terminated) break;

            ray.Origin = payload.nextOrigin;
            ray.Direction = payload.nextDirection;
            ray.TMin = 0.01;
            ray.TMax = 1000.0;
        }

        rngState = payload.rngState;

        accumulatedColor += payload.color;
    }

    accumulatedColor /= float(cameraData.camera.pathTracerSampleCount);

    giResultImage[pixelCoord] = float4(accumulatedColor, 1.0);
    giRayDir[pixelCoord] = float4(firstDir, 1.0);
}