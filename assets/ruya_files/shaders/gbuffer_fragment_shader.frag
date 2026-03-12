#version 460
#extension GL_GOOGLE_include_directive : require

#include "glsl_common.glsl"

layout(location = 0) flat in uint renderGeometryIndex;
layout(location = 1) in vec3 position;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 vertexNormal;
layout (location = 4) in mat3 tbn;

layout(location = 0) out vec4 outAlbedoDepth; //4th component of vector is depth
layout(location = 1) out vec4 outPositionMetallic; //th component of vector is metallic
layout(location = 2) out vec4 outNormalRoughness; //th component of vector is roughness
layout(location = 3) out vec4 vertexNormals;

layout(set = 0, binding = 0) uniform sampler2D image2Ds[];

layout(set = 1, binding = 0, std430, scalar) readonly buffer MaterialBuffer 
{
    Material material;
} materials[];

layout(set = 2, binding = 0, std430, scalar) readonly buffer RenderGeometryBuffer 
{
    RenderGeometry renderGeometry;
} renderGeometries[];

void main() 
{
    RenderGeometry renderGeometry = renderGeometries[renderGeometryIndex].renderGeometry;
    Material material = materials[renderGeometry.materialDescriptorIndex].material;

    vec3 albedo = texture(image2Ds[material.albedoImageDescriptorIndex], texCoord).rgb;
    vec3 normal = texture(image2Ds[material.normalImageDescriptorIndex], texCoord).rgb;
    normal = normal * 2.0 - 1.0;
    vec3 metallicRoughness = texture(image2Ds[material.metallicRoughnessImageDescriptorIndex], texCoord).rgb; //g: roughness, b: metallic

    outAlbedoDepth = vec4(albedo, gl_FragCoord.z);
    outPositionMetallic = vec4(position, metallicRoughness.b);
    outNormalRoughness = vec4(normalize(tbn * normal), metallicRoughness.g);
    vertexNormals = vec4(vertexNormal, 1.0);
}