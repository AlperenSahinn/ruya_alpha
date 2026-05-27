import common;

struct CameraData { Camera camera; };
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> cameraData;

struct AtmosphericLightData { AtmosphericLight atmosphericLight; };
[[vk::binding(0, 1)]] ConstantBuffer<AtmosphericLightData> atmosphericLightData;

[[vk::binding(0, 2)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> albedoDepth;
[[vk::binding(1, 2)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> positionMetallic;
[[vk::binding(2, 2)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> normalRoughness;

[[vk::binding(3, 2)]] [[vk::image_format("r8")]] RWTexture2D<float>  directionalLightShadowMap;

[[vk::binding(4, 2)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> gi;

[[vk::binding(5, 2)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> lightPassImage;

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
    bool blocked = raySphereIntersect(ro, sunDir, GROUND_RADIUS, tGnd0, tGnd1)
                   && tGnd0 > 0.0;

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

    if (blocked)
    {
        T *= 0.0;
    }
    return T;
}

float3 computeVisibleSunColor(float3 baseSunColor, float3 sunDir,
                              float mieBeta, float rayleighH, float mieH)
{
    float3 T = computeSunTransmittance(sunDir, mieBeta, rayleighH, mieH);
    return baseSunColor * T;
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
    float mieHeight      = atmosphericLightData.atmosphericLight.scatterParams.w;

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
    float  groundRefl = atmosphericLightData.atmosphericLight.groundAlbedo.a;
    float  sunUp = max(dot(sunDir, up), 0.0);

    float3 visibleSunCol = computeSunTransmittance(sunDir, mieBeta, rayleighHeight, mieHeight);
    float3 groundBounce  = groundCol * groundRefl * sunUp * RAYLEIGH_BETA * 0.5 * visibleSunCol;
    sky += groundBounce * sunIntensity * 0.1;

    if (hitGround && tGnd0 > 0.0)
    {
        float belowAmount = clamp(-viewDotUp * 3.0, 0.0, 1.0);

        float elev   = sunElevationFactor(sunDir);
        float golden = goldenHourFactor(sunDir);

        float3 earthBase = groundCol;
        if (dot(earthBase, float3(1.0)) < 0.05)
        {
            earthBase = float3(0.10, 0.06, 0.035);
        }

        float3 lit       = earthBase * visibleSunCol * (0.05 + 0.35 * elev);
        float3 twilight  = earthBase * float3(0.30, 0.12, 0.05) * (0.03 + 0.12 * golden);
        float3 nightCol  = earthBase * 0.002;

        float3 belowCol = lerp(nightCol, lit, elev);
        belowCol = lerp(belowCol, belowCol + twilight, golden * 0.6);

        float depthDarken = lerp(1.0, 0.35, belowAmount);
        belowCol *= depthDarken;

        sky = lerp(sky, belowCol, belowAmount * belowAmount);
    }

    return sky;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0)
              * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float d = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * d * d);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

float BurleyDiffuse(float NdotV, float NdotL, float LdotH, float roughness)
{
    float energyBias   = lerp(0.0, 0.5,  roughness);
    float energyFactor = lerp(1.0, 1.0 / 1.51, roughness);
    float FD90 = energyBias + 2.0 * LdotH * LdotH * roughness;
    float ls = 1.0 + (FD90 - 1.0) * pow(clamp(1.0 - NdotL, 0.0, 1.0), 5.0);
    float vs = 1.0 + (FD90 - 1.0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
    return ls * vs * energyFactor;
}

float3 multiscatterCompensation(float3 F0, float NdotV, float roughness)
{
    float Ess = 1.0 - (roughness * roughness) * 0.5;
    Ess = lerp(Ess, 1.0, pow(1.0 - NdotV, 5.0));
    float3 Favg = F0 + (1.0 - F0) / 21.0;
    float3 Fms = Favg * Favg * Ess / (1.0 - Favg * (1.0 - Ess));
    return float3(1.0, 1.0, 1.0) + Fms * (1.0 / max(Ess, 0.001) - 1.0);
}

float hgPhaseNormalized(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

float3 sunVisibilityAt(float3 worldPos, float3 sunDir, float mieBeta, float rayleighH, float mieH)
{
    float3 p = worldPos + float3(0.0, GROUND_RADIUS, 0.0);

    float3 up = normalize(p);
    float sunUp = dot(sunDir, up);
    if (sunUp < -0.05) return float3(0.0, 0.0, 0.0);

    float tG0, tG1;
    if (raySphereIntersect(p, sunDir, GROUND_RADIUS, tG0, tG1) && tG0 > 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    float tA0, tA1;
    if (!raySphereIntersect(p, sunDir, ATMO_RADIUS, tA0, tA1))
    {
        return float3(1.0, 1.0, 1.0);
    }
    float tEnd = max(tA1, 0.0);

    const int N = 4;
    float seg = tEnd / float(N);
    float odR = 0.0, odM = 0.0;
    [unroll]
    for (int i = 0; i < N; ++i)
    {
        float t = (float(i) + 0.5) * seg;
        float3 sp = p + sunDir * t;
        float alt = length(sp) - GROUND_RADIUS;
        if (alt < 0.0) return float3(0.0, 0.0, 0.0);
        odR += exp(-alt / rayleighH) * seg;
        odM += exp(-alt / mieH)      * seg;
    }
    float3 tau = RAYLEIGH_BETA * odR + mieBeta * 1.1 * odM;
    float3 T = exp(-tau);

    float horizonFade = smoothstep(-0.05, 0.05, sunUp);
    return T * horizonFade;
}

float3 computeFog(float3 color, float3 worldPos, float3 camPos, float3 sunDir, float3 sunColor,
                  float sunIntensity, float mieG, float surfaceShadow, float surfaceNdotL,
                  float mieBeta, float rayleighH, float mieH)
{
    float3 viewVec = worldPos - camPos;
    float dist = length(viewVec);
    if (dist < 0.001) return color;
    float3 viewDir = viewVec / dist;

    float fogDensity      = atmosphericLightData.atmosphericLight.skyParams.w;
    const float fogHeightFalloff = 0.02;
    const float fogBaseHeight    = 0.0;

    float k = fogHeightFalloff;
    float h0 = fogBaseHeight;
    float hC = camPos.y   - h0;
    float hW = worldPos.y - h0;
    float dh = hW - hC;

    float opticalDepth;
    if (abs(k * dh) > 1e-4)
    {
        opticalDepth = fogDensity * dist * exp(-k * hC) * (1.0 - exp(-k * dh)) / (k * dh);
    }
    else
    {
        opticalDepth = fogDensity * dist * exp(-k * hC);
    }
    float fogTransmittance = exp(-max(opticalDepth, 0.0));
    float fogExtinction = 1.0 - fogTransmittance;

    if (fogExtinction < 1e-3) return color;

    float sunAboveHorizon = smoothstep(-0.02, 0.08, sunDir.y);
    float surfaceFacingSun = smoothstep(-0.05, 0.15, surfaceNdotL);
    float directSunVis = surfaceShadow * sunAboveHorizon * surfaceFacingSun;

    float viewSunCos = dot(viewDir, sunDir);
    float phaseFwd = hgPhaseNormalized(viewSunCos,  mieG);
    float phaseBwd = hgPhaseNormalized(viewSunCos, -0.2);
    float phase = phaseFwd * 0.85 + phaseBwd * 0.15;

    float3 sunAtmoColor = computeVisibleSunColor(sunColor, sunDir, mieBeta, rayleighH, mieH);
    float3 directScatter = sunAtmoColor * sunIntensity * phase * directSunVis;

    float elev   = sunElevationFactor(sunDir);
    float golden = goldenHourFactor(sunDir);

    float3 dayFog     = float3(0.55, 0.60, 0.70);
    float3 horizonFog = float3(0.65, 0.55, 0.45);
    float3 nightFog   = float3(0.015, 0.020, 0.035);

    float3 ambientFog = lerp(nightFog, dayFog, elev);
    ambientFog = lerp(ambientFog, horizonFog, golden * 0.6);

    float ambientLevel = lerp(0.02, 0.12, elev);
    float3 ambientScatter = ambientFog * ambientLevel;

    float3 fogColor = directScatter + ambientScatter;
    return color * fogTransmittance + fogColor * fogExtinction;
}

float3 EvaluateSH4(float3 L0, float3 L1, float3 detailedNormal)
{
    float dirFactor = dot(L1, detailedNormal);
    float3 shResult = L0 + (dirFactor * 2.0 / 3.0);
    return max(shResult, 0.0);
}

[shader("compute")]
[numthreads(128, 1, 1)]
void computeMain(uint3 globalID : SV_DispatchThreadID)
{
    int2 texCoord = int2(globalID.xy);
    uint2 texSizeU;
    lightPassImage.GetDimensions(texSizeU.x, texSizeU.y);
    int2 texSize = int2(texSizeU);

    if (texCoord.x >= texSize.x || texCoord.y >= texSize.y) return;

    float4 albedoDepthSample  = albedoDepth[texCoord];
    float4 posMetallicSample  = positionMetallic[texCoord];
    float4 normalRoughSample  = normalRoughness[texCoord];

    float3 albedo    = albedoDepthSample.rgb;
    float  depth     = albedoDepthSample.a;
    float3 position  = posMetallicSample.rgb;
    float  metallic  = posMetallicSample.w;
    float3 normalRaw = normalRoughSample.rgb;
    float roughness  = normalRoughSample.w;
    float shadow     = directionalLightShadowMap[texCoord];

    float3 sunDir       = normalize(-atmosphericLightData.atmosphericLight.sunDirection.xyz);
    float3 sunColor     = atmosphericLightData.atmosphericLight.sunColor.rgb;
    float  sunIntensity = atmosphericLightData.atmosphericLight.sunColor.a;
    float  mieBeta      = atmosphericLightData.atmosphericLight.scatterParams.x;
    float  mieG         = atmosphericLightData.atmosphericLight.scatterParams.y;

    if (depth > 0.99999)
    {
        float2 screenUV = (float2(texCoord) + 0.5) / float2(texSize);
        float4 clip = float4(screenUV * 2.0 - 1.0, 1.0, 1.0);
        clip.y = -clip.y;

        float4 worldPt = mul(cameraData.camera.invViewproj, clip);
        float3 rd = normalize(worldPt.xyz / worldPt.w - cameraData.camera.viewPos.xyz);

        float3 camPlanet = cameraData.camera.viewPos.xyz + float3(0.0, GROUND_RADIUS, 0.0);
        float camAlt = length(camPlanet) - GROUND_RADIUS;
        if (camAlt < 1.0)
        {
            camPlanet = normalize(camPlanet) * (GROUND_RADIUS + 1.0);
        }

        float3 skyColor = computeAtmosphere(camPlanet, rd, sunDir, mieBeta, mieG, sunIntensity);

        float sunDiskSize  = atmosphericLightData.atmosphericLight.skyParams.x;
        float sunDiskSharp = atmosphericLightData.atmosphericLight.skyParams.y;

        float sunCos = dot(rd, sunDir);
        float sunDisk = smoothstep(sunDiskSize, sunDiskSharp, sunCos);

        float limbDarkening = 1.0 - 0.4 * pow(1.0 - clamp((sunCos - sunDiskSize)
                                         / (sunDiskSharp - sunDiskSize + 0.0001), 0.0, 1.0), 0.5);

        float rayleighH = atmosphericLightData.atmosphericLight.scatterParams.z;
        float mieH      = atmosphericLightData.atmosphericLight.scatterParams.w;
        float3 visibleSunCol = computeVisibleSunColor(sunColor, sunDir, mieBeta, rayleighH, mieH);

        skyColor += sunDisk * visibleSunCol * sunIntensity * 2.0 * limbDarkening;

        float skyExposure = atmosphericLightData.atmosphericLight.skyParams.z;
        skyColor *= skyExposure;

        lightPassImage[texCoord] = float4(skyColor, 1.0);
        return;
    }

    float3 N = normalize(normalRaw);
    float3 V = normalize(cameraData.camera.viewPos.xyz - position);
    float3 L = sunDir;
    float3 H = normalize(V + L);

    float NdotL_raw = dot(N, L);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotL = max(NdotL_raw, 0.0);
    float LdotH = max(dot(L, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    roughness = clamp(roughness, 0.04, 1.0);

    float rayleighH = atmosphericLightData.atmosphericLight.scatterParams.z;
    float mieH      = atmosphericLightData.atmosphericLight.scatterParams.w;
    float3 visibleSunCol = computeVisibleSunColor(sunColor, sunDir, mieBeta, rayleighH, mieH);

    float3 radiance = visibleSunCol * sunIntensity;
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 directLight = float3(0.0, 0.0, 0.0);

    if (NdotL_raw > 0.0)
    {
        float3 F   = FresnelSchlick(HdotV, F0);
        float  NDF = DistributionGGX(N, H, roughness);
        float  G   = GeometrySmith(N, V, L, roughness);

        float3 specular = (NDF * G * F) / (4.0 * NdotV + 1e-4);
        specular *= multiscatterCompensation(F0, NdotV, roughness);

        specular = min(specular, float3(64.0, 64.0, 64.0));

        float3 kD = (1.0 - F) * (1.0 - metallic);
        float3 diffuse = kD * (albedo / PI) * BurleyDiffuse(NdotV, NdotL, LdotH, roughness);

        directLight = (diffuse + specular) * radiance * NdotL;

        float shadowTerminator = smoothstep(0.0, 0.04, NdotL);
        directLight *= shadow * shadowTerminator;
    }

    directLight = clamp(directLight, float3(0.0, 0.0, 0.0), float3(1e6, 1e6, 1e6));

    float3 indirectDiffuse = albedo * gi[texCoord].rgb;
    float3 result = directLight + indirectDiffuse;

    result = computeFog(result, position, cameraData.camera.viewPos.xyz, sunDir, sunColor, sunIntensity, mieG, shadow, NdotL_raw, mieBeta, rayleighH, mieH);

    lightPassImage[texCoord] = float4(result, 1.0);
}