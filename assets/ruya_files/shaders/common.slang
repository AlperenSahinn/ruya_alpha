public static const float PI = 3.14159265359;

public struct Material
{
    public uint albedoImageDescriptorIndex;
    public uint normalImageDescriptorIndex;
    public uint metallicRoughnessImageDescriptorIndex;
    public uint pad;
};

public struct RenderGeometry
{
    public float4x4 transform;
    public float4x4 transformInverse;
    public float4x4 prevTransform;
    public uint64_t vertexBufferAddress;
    public uint64_t indexBufferAddress;
    public uint materialDescriptorIndex;
    public uint pad0;
    public uint pad1;
    public uint pad2;
};

public struct Vertex
{
    public float3 position;
    public float3 normal;
    public float3 tangent;
    public float3 bitangent;
    public float2 texCoord;
    public float2 pad;
};

public struct Camera
{
    public float4x4 view;
    public float4 viewPos;
    public float4x4 proj;
    public float4x4 viewproj;
    public float4x4 invViewproj;
    public float4x4 prevViewproj;
    public float4 prevViewPos;
    public float fov;
    public uint width;
    public uint height;
    public float near;
    public float far;
    public uint frameNumber;
    public float sampleCount;
    public int pathTracerSampleCount;
};

public struct AtmosphericLight
{
    public float4 sunDirection;
    public float4 sunColor;        // .a = sun intensity
    public float4 skyParams;       // .x = sun disk size, .y = sun disk sharpness, .z = sky exposure, .w = fog density
    public float4 groundAlbedo;    // .rgb = ground color, .a = ground reflectance
    public float4 scatterParams;   // .x = mie beta, .y = mie g, .z = rayleigh height, .w = mie height
};

public struct GIReservoir
{
    float w;
    float m;
    float3 f;
}
