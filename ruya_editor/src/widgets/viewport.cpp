#include "viewport.h"

#include <imgui/imgui_impl_vulkan.h>

#include <engine.h>

bool editor::Viewport::isActive = false;

editor::Viewport::Viewport()
{
    for(uint32_t i = 0; i < engine->GetGraphics()->GetFrameBufferCount(); i++)
    {
        drawTextures.push_back(ImGui_ImplVulkan_AddTexture(engine->GetGraphics()->GetNearestSampler(), engine->GetGraphics()->GetFrameBufferWithIndex(i)->GetLightPassImage()->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }
}

editor::Viewport::~Viewport()
{

}

void editor::Viewport::Draw()
{
    const char8_t* utf8_viewportIcon = u8"\uEAA2 Viewport";
    const char* viewportIcon = reinterpret_cast<const char*>(utf8_viewportIcon);

    ImGui::Begin(viewportIcon);

    isActive = ImGui::IsWindowHovered();

    static ImVec2 offset = ImVec2(0.0f, 0.0f);
    static float zoom = 1.0f;

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 windowPos = ImGui::GetCursorScreenPos();

    if (availSize.x <= 0 || availSize.y <= 0) {
        ImGui::End();
        return;
    }

    ruya::VulkanImage* resultImage = engine->GetGraphics()->CurrentFrameBuffer()->GetLightPassImage();
    ImVec2 imageSizeRaw = ImVec2(
        static_cast<float>(resultImage->GetExtent().width),
        static_cast<float>(resultImage->GetExtent().height)
    );

    if (imageSizeRaw.x <= 0 || imageSizeRaw.y <= 0) {
        ImGui::End();
        return;
    }

    float aspectRatio = imageSizeRaw.x / imageSizeRaw.y;
    float availAspectRatio = availSize.x / availSize.y;

    ImVec2 baseImageSize;
    if (availAspectRatio > aspectRatio) {
        baseImageSize.y = availSize.y;
        baseImageSize.x = availSize.y * aspectRatio;
    }
    else {
        baseImageSize.x = availSize.x;
        baseImageSize.y = availSize.x / aspectRatio;
    }

    ImVec2 imageSize = ImVec2(baseImageSize.x * zoom, baseImageSize.y * zoom);

    if (ImGui::IsWindowHovered() && availSize.x > 0 && availSize.y > 0)
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            float zoomSpeed = 0.1f;
            float prevZoom = zoom;
            zoom += wheel * zoomSpeed;
            zoom = std::clamp(zoom, 0.1f, 10.0f);

            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 viewportCenter = ImVec2(windowPos.x + availSize.x * 0.5f, windowPos.y + availSize.y * 0.5f);
            ImVec2 mouseRelativeToCenter = ImVec2(mousePos.x - viewportCenter.x, mousePos.y - viewportCenter.y);

            float zoomFactor = zoom / prevZoom;
            offset.x = offset.x * zoomFactor + mouseRelativeToCenter.x * (1.0f - zoomFactor);
            offset.y = offset.y * zoomFactor + mouseRelativeToCenter.y * (1.0f - zoomFactor);

            imageSize = ImVec2(baseImageSize.x * zoom, baseImageSize.y * zoom);
        }
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        ImVec2 dragDelta = ImGui::GetIO().MouseDelta;
        offset.x += dragDelta.x;
        offset.y += dragDelta.y;
    }

    ImVec2 imagePos = ImVec2(
        windowPos.x + (availSize.x - imageSize.x) * 0.5f + offset.x,
        windowPos.y + (availSize.y - imageSize.y) * 0.5f + offset.y
    );

    ImGui::SetCursorScreenPos(imagePos);
    ImGui::Image(drawTextures[engine->GetGraphics()->GetCurrentFrameIndex()], imageSize);

    if (ImGui::IsWindowHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        zoom = 1.0f;
        offset = ImVec2(0.0f, 0.0f);
    }

    ImGui::End();
}

bool editor::Viewport::IsActive()
{
    return isActive;
}
