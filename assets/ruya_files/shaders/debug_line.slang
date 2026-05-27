struct PushConstants
{
    float4x4 viewProj;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct VSInput
{
    [[vk::location(0)]] float4 position : POSITION0;
    [[vk::location(1)]] float4 color    : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

[shader("vertex")]
VSOutput vertexMain(VSInput i)
{
    VSOutput o;
    o.position = mul(pc.viewProj, float4(i.position.xyz, 1.0));
    o.color    = i.color;
    return o;
}

[shader("fragment")]
float4 fragmentMain(VSOutput i) : SV_Target0
{
    return i.color;
}
