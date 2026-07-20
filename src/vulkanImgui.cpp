#include "vulkanImgui.h"
#include "vulkanApp.h"
#include <cstdio>
#include <stdlib.h>
#if defined(_WIN32)
#include "imgui_impl_win32.h"
#elif defined(__linux__)
#include "linux_imgui_impl_wayland.h"
#endif
#include "imgui_impl_vulkan.h"

static void check_vk_result(VkResult err)
{
  if (err == 0)
    return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0)
    abort();
}

namespace VulkanApp
{
void ImguiInstance::initImgui(VulkanApp::App *appPointer)
{
  if (appPointer != nullptr)
  {
    vulkanApp = appPointer;
  }

  ctx = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctx);
  IMGUI_CHECKVERSION();

  // set imgui style here
#if defined(_WIN32)
  ImGui_ImplWin32_Init(vulkanApp->getWindowAPI()->getHwnd());
#elif defined(__linux__)
  ImGui_ImplWayland_Init(vulkanApp->getWindowAPI());
#endif
  vk::PipelineRenderingCreateInfo renderingInfo{
      .colorAttachmentCount    = 1,
      .pColorAttachmentFormats = &vulkanApp->swapChainImageFormat,
  };
  ImGui_ImplVulkan_InitInfo init_info{
      .Instance            = *vulkanApp->getInstance(),
      .PhysicalDevice      = *vulkanApp->getPhysicalDevice(),
      .Device              = *vulkanApp->getDevice(),
      .QueueFamily         = vulkanApp->queueFamilies.graphicsIndex,
      .Queue               = *vulkanApp->getGraphicsQueue(),
      .DescriptorPool      = nullptr,
      .DescriptorPoolSize  = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE,
      .MinImageCount       = vulkanApp->minImageCountSwapchain,
      .ImageCount          = static_cast<uint32_t>(vulkanApp->getSwapChainImages().size()),
      .PipelineInfoMain    = {.PipelineRenderingCreateInfo = renderingInfo},
      .UseDynamicRendering = true,
      .Allocator           = nullptr,
      .CheckVkResultFn     = check_vk_result
  };
  ImGui_ImplVulkan_Init(&init_info);

  ImGuiStyle &style  = ImGui::GetStyle();
  ImVec4     *colors = style.Colors;

  // Backgrounds
  colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.07f, 0.07f, 1.00f); // Deep charcoal
}

void ImguiInstance::imguiStartFrame()
{
  // Start the Dear ImGui frame
  ImGui_ImplVulkan_NewFrame();
#if defined(_WIN32)
  ImGui_ImplWin32_NewFrame();
#elif defined(__linux__)
  ImGui_ImplWayland_NewFrame();
#endif
  ImGui::NewFrame();

  // ImGui::ShowDemoWindow(); // Show demo window! :)
  ImGui::Checkbox("fullscreen", &vulkanApp->getWindowAPI()->getState().want_fullscreen);

  ImGui::Checkbox("force sdr", &vulkanApp->getColorInfo().forceSdr);
  ImGui::Checkbox("useOSValues", &vulkanApp->getColorInfo().useOSValues);
  ImGui::SliderFloat("userMaxCLL", &vulkanApp->getColorInfo().userMaxCLL, 5, 1000);
  ImGui::SliderFloat("userWhitepoint", &vulkanApp->getColorInfo().userWhitepoint, 5, 1000);

  ImGui::Text("Colorspace: %s", vulkanApp->getColorInfo().colorspaceName.c_str());
  ImGui::Text("Format: %s", vulkanApp->getColorInfo().formatName.c_str());
  ImGui::Text("formatSrgb: %i", vulkanApp->getColorInfo().formatSrgb);
  ImGui::Text("appMaxCLL: %f", vulkanApp->getColorInfo().appMaxCLL);
  ImGui::Text("appWhitepoint: %f", vulkanApp->getColorInfo().appWhitepoint);
  ImGui::Text("monitorMinCLL: %f", vulkanApp->getColorInfo().monitorMinCLL);
  ImGui::Text("buffer width: %i", vulkanApp->getSwapChainExtent().width);
  ImGui::Text("buffer height: %i", vulkanApp->getSwapChainExtent().height);
}

void ImguiInstance::imguiRender()
{
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *vulkanApp->getCommandBuffers()[vulkanApp->frameIndex]);
}

void ImguiInstance::updateImgui()
{
  cleanupImgui();
  initImgui(nullptr);
}

void ImguiInstance::cleanupImgui()
{
  ImGui_ImplVulkan_Shutdown();
#if defined(_WIN32)
  ImGui_ImplWin32_Shutdown();
#elif defined(__linux__)
  ImGui_ImplWayland_Shutdown();
#endif
  ImGui::DestroyContext();
}

} // namespace VulkanApp