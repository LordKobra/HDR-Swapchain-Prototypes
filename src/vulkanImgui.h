#pragma once
#include "imgui.h"

namespace VulkanApp
{
class App;

class ImguiInstance
{

private:
  ImGuiContext   *ctx       = nullptr;
  VulkanApp::App *vulkanApp = nullptr;

public:
  void initImgui(VulkanApp::App *appPointer);
  void imguiStartFrame();
  void imguiRender();
  void updateImgui();
  void cleanupImgui();
};

} // namespace VulkanApp