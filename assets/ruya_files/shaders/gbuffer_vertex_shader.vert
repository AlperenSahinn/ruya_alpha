#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include "glsl_common.glsl"

layout(location = 0) out uint renderGeometryIndex;
layout(location = 1) out vec3 position;
layout(location = 2) out vec2 texCoord;
layout(location = 3) out vec3 vertexNormal;
layout(location = 4) out mat3 tbn;

layout(set = 2, binding = 0, std430, scalar) readonly buffer RenderGeometryBuffer 
{
    RenderGeometry renderGeometry;
} renderGeometries[];

layout(buffer_reference, scalar) buffer VertexBuffer 
{
    Vertex vertices[];
};

layout(buffer_reference, scalar) buffer IndexBuffer 
{
    uint indices[];
};

layout(set = 3, binding = 0, std140) uniform CameraData {
    Camera camera;
} cameraData;

void main() 
{
    RenderGeometry renderGeometry = renderGeometries[gl_BaseInstanceARB].renderGeometry;
    
    VertexBuffer vertexBuffer = VertexBuffer(renderGeometry.vertexBufferAddress);
    IndexBuffer indexBuffer = IndexBuffer(renderGeometry.indexBufferAddress);
    
    uint index = indexBuffer.indices[gl_VertexIndex];

    Vertex vertex = vertexBuffer.vertices[index];
    
    texCoord = vertex.texCoord;

    vec4 worldPos = renderGeometry.transform * vec4(vertex.position, 1.0);
    position = worldPos.xyz;
    gl_Position = cameraData.camera.viewproj * worldPos;

    vec3 t = normalize(vec3(renderGeometry.transform * vec4(vertex.tangent, 0.0)));
	vec3 b = normalize(vec3(renderGeometry.transform * vec4(vertex.bitangent, 0.0)));
	vec3 n = normalize(mat3(transpose(inverse(renderGeometry.transform))) * vertex.normal);
	tbn = mat3(t, b, n);

    vertexNormal = n;

    renderGeometryIndex = gl_BaseInstanceARB;
}