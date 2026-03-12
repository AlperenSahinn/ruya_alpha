#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

const float PI = 3.14159265359;

struct Material 
{                        
    uint albedoImageDescriptorIndex;
    uint normalImageDescriptorIndex;
    uint metallicRoughnessImageDescriptorIndex;
    uint pad;
};

struct RenderGeometry
{
    mat4 transform;
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint materialDescriptorIndex;
    uint pad[3];
};

struct Vertex 
{
    vec3 position; 
    vec3 normal;   
    vec3 tangent;  
    vec3 bitangent;
    vec2 texCoord; 
    vec2 pad;      
};

struct Camera
{
    mat4 view;
    vec4 viewPos;
    mat4 proj;
    mat4 viewproj;
    float fov;
	float width;
	float height;
	float near;
	float far;
	float frameNumber;
	float sampleCount;
	float pad2;
};

struct DirectionalLight
{
    vec4 ambientColor; // ambientColor.a is ambient light intensity
    vec4 sunlightDirection;
    vec4 sunlightColor; // sunlightColor.a is directional light intensity
    vec4 pad;
};