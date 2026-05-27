import common;

[[vk::binding(0, 0)]] Sampler2D image2Ds[];

struct MaterialEntry { Material material; };
[[vk::binding(0, 1)]] StructuredBuffer<MaterialEntry> materials[];

struct RenderGeometryEntry { RenderGeometry renderGeometry; };
[[vk::binding(0, 2)]] StructuredBuffer<RenderGeometryEntry> renderGeometries[];

struct CameraData { Camera camera; };
[[vk::binding(0, 3)]] ConstantBuffer<CameraData> cameraData;

struct VertexBuffer
{
    Vertex vertices[1];
};

struct IndexBuffer
{
    uint indices[1];
};

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

float2 CalculateJitterOffset(uint frameNumber, uint frameWidth, uint frameHeight)
{
    float2 hs = haltonSequence[frameNumber % 16];
    float2 jitterOffset = float2((hs.x - 0.5f) * 2.0f / float(frameWidth), (hs.y - 0.5f) * 2.0f / float(frameHeight));
    return jitterOffset;
}

struct VSOutput
{
    float4 position : SV_Position;
    nointerpolation uint renderItemID : RGEOM_INDEX;
    float3 worldPos : WORLD_POS;
    float2 texCoord : TEXCOORD0;
    float3 vertexNormal : VNORMAL;
    float3 tbn0 : TBN0;
    float3 tbn1 : TBN1;
    float3 tbn2 : TBN2;
    float4 currClipPos : CLIP_POS_CURR;
    float4 prevClipPos : CLIP_POS_PREV;
};

[shader("vertex")]
VSOutput vertexMain(
    uint vertexID         : SV_VertexID,
    uint startInstance    : SV_StartInstanceLocation)
{
    VSOutput o;

    uint geomIndex = startInstance;

    RenderGeometry renderGeometry = renderGeometries[geomIndex][0].renderGeometry;

    VertexBuffer* vb = (VertexBuffer*)renderGeometry.vertexBufferAddress;
    IndexBuffer* ib = (IndexBuffer*)renderGeometry.indexBufferAddress;

    uint index = ib->indices[vertexID];
    Vertex v = vb->vertices[index];

    o.texCoord = v.texCoord;

    float4 worldPos = mul(renderGeometry.transform, float4(v.position, 1.0));
    o.worldPos = worldPos.xyz;

    float4 clipPos = mul(cameraData.camera.viewproj, worldPos);
    float2 jitterOffset = CalculateJitterOffset(cameraData.camera.frameNumber, cameraData.camera.width, cameraData.camera.height);
    float4 jitteredClipPos = clipPos;
    jitteredClipPos.xy += jitterOffset * jitteredClipPos.w;
    o.position = jitteredClipPos;
    o.currClipPos = jitteredClipPos;

    float4 prevWorldPos = mul(renderGeometry.prevTransform, float4(v.position, 1.0));
    float4 prevClipPos = mul(cameraData.camera.prevViewproj, prevWorldPos);
    float2 prevjitterOffset = CalculateJitterOffset(cameraData.camera.frameNumber - 1, cameraData.camera.width, cameraData.camera.height);
    float4 prevjitteredClipPos = prevClipPos;
    prevjitteredClipPos.xy += prevjitterOffset * prevjitteredClipPos.w;
    o.prevClipPos = prevjitteredClipPos;

    float4x4 normalMatrix = transpose(renderGeometry.transformInverse);
    float3 T = normalize(mul(normalMatrix, float4(v.tangent,   0.0)).xyz);
    float3 B = normalize(mul(normalMatrix, float4(v.bitangent, 0.0)).xyz);
    float3 N = normalize(mul(normalMatrix, float4(v.normal,    0.0)).xyz);

    o.tbn0 = T;
    o.tbn1 = B;
    o.tbn2 = N;
    o.vertexNormal = N;
    o.renderItemID = geomIndex;

    return o;
}

struct FSOutput
{
    float4 albedoDepth                  : SV_Target0; // .a = depth
    float4 positionMetallic             : SV_Target1; // .a = metallic
    float4 normalRoughness              : SV_Target2; // .a = roughness
    float4 vertexNormalsRenderItemID    : SV_Target3; // .a = render item id
    float4 motionVectors                : SV_Target4;
};

[shader("fragment")]
FSOutput fragmentMain(VSOutput i, float4 fragCoord : SV_Position)
{
    FSOutput o;

    RenderGeometry renderGeometry = renderGeometries[i.renderItemID][0].renderGeometry;
    Material material = materials[renderGeometry.materialDescriptorIndex][0].material;

    const int mipBias = 0;

    float3 albedo = image2Ds[material.albedoImageDescriptorIndex].Sample(i.texCoord, mipBias).rgb;
    float2 nxy = image2Ds[material.normalImageDescriptorIndex].Sample(i.texCoord, mipBias).rg;
    nxy = nxy * 2.0 - 1.0;
    float nz = sqrt(saturate(1.0 - dot(nxy, nxy)));
    float3 normal = float3(nxy, nz);
    normal.y = -normal.y;
    float3x3 tbn = float3x3(i.tbn0, i.tbn1, i.tbn2);
    normal = normalize(mul(transpose(tbn), normal));

    float3 mr = image2Ds[material.metallicRoughnessImageDescriptorIndex].Sample(i.texCoord, mipBias).rgb;

    float2 currentPosNDC = i.currClipPos.xy / i.currClipPos.w;
    float2 prevPosNDC = i.prevClipPos.xy / i.prevClipPos.w;
    float2 velocity = currentPosNDC - prevPosNDC;
    velocity = velocity * float2(0.5, -0.5);
    float prevDepthNDC = i.prevClipPos.z / i.prevClipPos.w;

    o.albedoDepth               = float4(albedo, fragCoord.z);
    o.positionMetallic          = float4(i.worldPos, mr.b);
    o.normalRoughness           = float4(normal, mr.g);
    o.vertexNormalsRenderItemID = float4(i.vertexNormal, 1.0f);
    //o.renderItemIDs = i.renderItemID;
    o.motionVectors = float4(velocity, prevDepthNDC, 1.0);
    return o;
}
