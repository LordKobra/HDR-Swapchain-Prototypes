#pragma once
#include "imgui.h"

namespace DXApp
{
class App;

class ImguiInstance
{

private:
  ImGuiContext *ctx   = nullptr;
  DXApp::App   *dxApp = nullptr;

public:
  void initImgui(DXApp::App *appPointer);
  void imguiStartFrame();
  void imguiRender();
  void updateImgui();
  void cleanupImgui();
};
} // namespace DXApp