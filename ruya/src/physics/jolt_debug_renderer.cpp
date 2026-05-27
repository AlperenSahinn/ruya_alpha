#include "jolt_debug_renderer.h"

#include <engine.h>

ruya::JoltDebugRenderer::JoltDebugRenderer()
{
    Initialize();
}

void ruya::JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    float r = inColor.r / 255.0f;
    float g = inColor.g / 255.0f;
    float b = inColor.b / 255.0f;
    float a = inColor.a / 255.0f;

    engine->GetRenderDataWriteBuffer()->debugVertexLines.push_back({ glm::vec4(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ(), 1.0f), glm::vec4(r, g, b, a) });
    engine->GetRenderDataWriteBuffer()->debugVertexLines.push_back({ glm::vec4(inTo.GetX(), inTo.GetY(), inTo.GetZ(), 1.0f), glm::vec4(r, g, b, a) });
}

void ruya::JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, JPH::DebugRenderer::ECastShadow inCastShadow)
{
    DrawLine(inV1, inV2, inColor);
    DrawLine(inV2, inV3, inColor);
    DrawLine(inV3, inV1, inColor);
}
