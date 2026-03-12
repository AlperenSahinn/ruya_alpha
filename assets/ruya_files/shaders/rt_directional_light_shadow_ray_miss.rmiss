#version 460
#extension GL_EXT_ray_tracing : require

struct Payload
{
    float shadow;
};

layout(location = 0) rayPayloadInEXT Payload payLoad;

void main()
{
    payLoad.shadow = 1.0;
}