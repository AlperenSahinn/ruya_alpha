#include "Editor.h"
#include <functional>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_vulkan.h>

#include <SDL3/SDL.h>

#include <engine.h>
#include <core/log.h>
#include <graphics/graphics.h>

#include "editor_style.h"

#include "Widgets/widget.h"
#include "Widgets/dock_space.h"
#include "Widgets/menu_bar.h"
#include "Widgets/toolbar.h"
#include "Widgets/status_bar.h"
#include "Widgets/scene_outliner.h"
#include "Widgets/asset_browser.h"
#include "Widgets/viewport.h"
#include "Widgets/properties.h"
#include "Widgets/atmospheric_light_properties.h"
#include "Widgets/material_editor.h"

using namespace ruya;
using namespace editor;

ImFont* g_FontDefault = nullptr;
ImFont* g_FontBold = nullptr;
ImFont* g_FontLarge = nullptr;

editor::Editor::Editor()
{
    InitEditor();

    widgets.push_back(std::make_unique<MenuBar>());
    widgets.push_back(std::make_unique<DockSpace>());
    widgets.push_back(std::make_unique<Viewport>());
    widgets.push_back(std::make_unique<SceneOutliner>());
    widgets.push_back(std::make_unique<Properties>());
    widgets.push_back(std::make_unique<AssetBrowser>());
    widgets.push_back(std::make_unique<AtmosphericLightProperties>());
    widgets.push_back(std::make_unique<MaterialEditor>());

    SetThemeModernDark();

    ImGui::GetStyle().ScaleAllSizes(currentDpiScale);

    editorCameraSystem = std::make_unique<EditorCameraSystem>();
}

editor::Editor::~Editor()
{
    widgets.clear();
    ImGui_ImplVulkan_Shutdown();
}

std::vector<std::unique_ptr<Widget>>& editor::Editor::GetWidgets()
{
    return widgets;
}

void editor::Editor::SetThemeModernDark()
{
    ApplyModernDarkTheme();
}

void editor::Editor::UpdateEditorCamera()
{
    editorCameraSystem->OnEngineUpdate();
}

float editor::Editor::GetCurrentDpiScale() const
{
    float scale = SDL_GetWindowDisplayScale(engine->GetWindow()->GetSDLWindow());
    if (scale <= 0.0f) scale = 1.0f;
    return scale;
}

void editor::Editor::UpdateDpiScale()
{
    float newScale = GetCurrentDpiScale();

    if (std::abs(newScale - currentDpiScale) < 0.01f)
        return;

    ImGui::GetStyle() = ImGuiStyle();
    ApplyModernDarkTheme();
    ImGui::GetStyle().ScaleAllSizes(newScale);

    currentDpiScale = newScale;

    RUYA_LOG_INFO("[Editor] DPI scale updated to {:.2f}", newScale);
}

void editor::Editor::InitEditor()
{
    ImGui::CreateContext();

    ImGui_ImplSDL3_InitForVulkan(engine->GetWindow()->GetSDLWindow());

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    currentDpiScale = GetCurrentDpiScale();

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = engine->GetGraphics()->GetVulkanContext()->GetInstance();
    init_info.PhysicalDevice = engine->GetGraphics()->GetVulkanContext()->GetPhysicalDevice();
    init_info.Device = engine->GetGraphics()->GetVulkanContext()->GetDevice();
    init_info.Queue = engine->GetGraphics()->GetVulkanContext()->GetDeviceQueue();
    init_info.QueueFamily = engine->GetGraphics()->GetVulkanContext()->GetDeviceQueueFamilyIndex();
    init_info.DescriptorPoolSize = 1024;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    std::vector<VkFormat> formats = { engine->GetGraphics()->GetVulkanContext()->GetSwapchainImageFormat() };
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = formats.data();
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.ApiVersion = VK_API_VERSION_1_3;

    std::string notoSansRegular = ASSETS_DIR + "ruya_files/fonts/NotoSans-Regular.ttf";
    std::string notoSansBold = ASSETS_DIR + "ruya_files/fonts/NotoSans-Bold.ttf";
    std::string phosphorFill = ASSETS_DIR + "ruya_files/fonts/Phosphor-Fill.ttf";

    g_FontDefault = io.Fonts->AddFontFromFileTTF(
        notoSansRegular.c_str(), 12.0f * currentDpiScale);

    {
        ImFontConfig iconCfg;
        iconCfg.MergeMode = true;
        iconCfg.PixelSnapH = true;
        iconCfg.GlyphOffset.y = 2.0f * currentDpiScale;
        io.Fonts->AddFontFromFileTTF(
            phosphorFill.c_str(), 13.0f * currentDpiScale,
            &iconCfg, io.Fonts->GetGlyphRangesDefault());
    }

    g_FontBold = io.Fonts->AddFontFromFileTTF(
        notoSansBold.c_str(), 12.0f * currentDpiScale);

    if (g_FontBold)
    {
        ImFontConfig boldIconCfg;
        boldIconCfg.MergeMode = true;
        boldIconCfg.PixelSnapH = true;
        boldIconCfg.GlyphOffset.y = 2.0f * currentDpiScale;
        io.Fonts->AddFontFromFileTTF(
            phosphorFill.c_str(), 15.0f * currentDpiScale,
            &boldIconCfg, io.Fonts->GetGlyphRangesDefault());
    }
    else
    {
        g_FontBold = g_FontDefault;
    }

    g_FontLarge = io.Fonts->AddFontFromFileTTF(
        notoSansRegular.c_str(), 11.0f * currentDpiScale);

    if (g_FontLarge)
    {
        ImFontConfig largeIconCfg;
        largeIconCfg.MergeMode = true;
        largeIconCfg.PixelSnapH = true;
        largeIconCfg.GlyphOffset.y = 12.0f * currentDpiScale;
        io.Fonts->AddFontFromFileTTF(
            phosphorFill.c_str(), 36.0f * currentDpiScale,
            &largeIconCfg, io.Fonts->GetGlyphRangesDefault());
    }
    else
    {
        g_FontLarge = g_FontDefault;
    }

    ImGui_ImplVulkan_Init(&init_info);

    RUYA_LOG_INFO("[Editor] ImGui initialized (DPI scale: {:.2f})", currentDpiScale);
}

void editor::Editor::ShutdownEditor()
{
    ImGui_ImplVulkan_Shutdown();
}

void editor::EditorUpdate(Editor* pEditor)
{
    pEditor->UpdateEditorCamera();
    pEditor->UpdateDpiScale();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    for (std::unique_ptr<Widget>& widget : pEditor->GetWidgets())
        widget->Draw();

    ImGui::Render();
}

void editor::EditorDraw()
{
    if (ImGui::GetDrawData())
        ImGui_ImplVulkan_RenderDrawData(
            ImGui::GetDrawData(),
            engine->GetGraphics()->GetVulkanContext()
            ->GetCurrentFrameResource()->GetCommandBuffer()->GetDeviceHandle());
}