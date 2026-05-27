import common;

[[vk::binding(0, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> inputImage;
[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> outputImage;

float3 uchimura(float3 x, float P, float a, float m, float l, float c, float b) {
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    float3 w0 = 1.0 - smoothstep(0.0, m, x);
    float3 w2 = step(m + l0, x);
    float3 w1 = 1.0 - w0 - w2;

    float3 T = m * pow(max(x / m, 0.0), float3(c, c, c)) + b;
    float3 S = P - (P - S1) * exp(CP * (x - S0));
    float3 L = m + a * (x - m);

    return T * w0 + L * w1 + S * w2;
}

float3 uchimura(float3 x) {
    const float P = 1.0;  // max display brightness
    const float a = 1.0;  // contrast
    const float m = 0.22; // linear section start
    const float l = 0.4;  // linear section length
    const float c = 1.33; // black
    const float b = 0.0;  // pedestal

    return uchimura(x, P, a, m, l, c, b);
}

float uchimura(float x, float P, float a, float m, float l, float c, float b) {
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    float w0 = 1.0 - smoothstep(0.0, m, x);
    float w2 = step(m + l0, x);
    float w1 = 1.0 - w0 - w2;

    float T = m * pow(max(x / m, 0.0), c) + b;
    float S = P - (P - S1) * exp(CP * (x - S0));
    float L = m + a * (x - m);

    return T * w0 + L * w1 + S * w2;
}

float uchimura(float x) {
    const float P = 1.0;  // max display brightness
    const float a = 1.0;  // contrast
    const float m = 0.22; // linear section start
    const float l = 0.4;  // linear section length
    const float c = 1.33; // black
    const float b = 0.0;  // pedestal

    return uchimura(x, P, a, m, l, c, b);
}

[shader("compute")]
[numthreads(128, 1, 1)]
void computeMain(uint3 globalID: SV_DispatchThreadID)
{
    int2 texCoord = int2(globalID.xy);
    uint2 texSizeU;
    inputImage.GetDimensions(texSizeU.x, texSizeU.y);
    int2 texSize = int2(texSizeU);

    if (texCoord.x >= texSize.x || texCoord.y >= texSize.y) return;

    float3 inputColor = inputImage[texCoord].rgb;

    float3 result = uchimura(inputColor);
    
    result = pow(max(result, 0.0), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    outputImage[texCoord] = float4(result, 1.0);
}