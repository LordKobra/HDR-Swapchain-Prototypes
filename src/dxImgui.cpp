#include "dxImgui.h"
#include "dxApp.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

namespace DXApp
{
void ImguiInstance::initImgui(DXApp::App *appPointer)
{
  if (appPointer != nullptr)
  {
    dxApp = appPointer;
  }

  ctx = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctx);
  IMGUI_CHECKVERSION();

  // @todo check dpi stuff in future?

  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Cont

  // set imgui style here
  ImGui_ImplWin32_Init(dxApp->getWindowAPI()->getHwnd());
  ImGui_ImplDX12_InitInfo init_info = {};
  init_info.Device                  = dxApp->getDevice().Get();
  init_info.CommandQueue            = dxApp->getCommandQueue().Get();
  init_info.NumFramesInFlight       = dxApp->getFrameCount();
  init_info.RTVFormat               = dxApp->getCurrentFormat();
  init_info.DSVFormat               = DXGI_FORMAT_UNKNOWN;
  init_info.SrvDescriptorHeap       = dxApp->getSrvDescriptorHeap().Get();

  static ExampleDescriptorHeapAllocator *g_descHeapAlloc = nullptr;
  g_descHeapAlloc                                        = &dxApp->descHeapAlloc;
  init_info.SrvDescriptorAllocFn                         = [](ImGui_ImplDX12_InitInfo *,
                                                              D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle,
                                                              D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    g_descHeapAlloc->Alloc(out_cpu_handle, out_gpu_handle);
  };
  init_info.SrvDescriptorFreeFn =
      [](ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
        g_descHeapAlloc->Free(cpu_handle, gpu_handle);
      };
  ImGui_ImplDX12_Init(&init_info);

  ImGuiStyle &style  = ImGui::GetStyle();
  ImVec4     *colors = style.Colors;

  // Backgrounds
  colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.07f, 0.07f, 1.00f); // Deep charcoal
}

void ImguiInstance::imguiStartFrame()
{
  // Start the Dear ImGui frame
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  // ImGui::ShowDemoWindow(); // Show demo window! :)

  ImGui::Checkbox("fullscreen", &dxApp->getWindowAPI()->getState().want_fullscreen);

  ImGui::Checkbox("forceSdr", &dxApp->getColorInfo().forceSdr);
  ImGui::Checkbox("useOSValues", &dxApp->getColorInfo().useOSValues);
  ImGui::SliderFloat("userMaxCLL", &dxApp->getColorInfo().userMaxCLL, 5, 1000);
  ImGui::SliderFloat("userWhitepoint", &dxApp->getColorInfo().userWhitepoint, 5, 1000);

  ImGui::Text("Colorspace: %s", dxApp->getColorInfo().colorspaceName.c_str());
  ImGui::Text("Format: %s", dxApp->getColorInfo().formatName.c_str());
  ImGui::Text("formatSrgb: %i", dxApp->getColorInfo().formatSrgb);
  ImGui::Text("appMaxCLL: %f", dxApp->getColorInfo().appMaxCLL);
  ImGui::Text("appWhitepoint: %f", dxApp->getColorInfo().appWhitepoint);
  ImGui::Text("monitorMinCLL: %f", dxApp->getColorInfo().monitorMinCLL);
  ImGui::Text("buffer width: %i", dxApp->getSwapChainWidth());
  ImGui::Text("buffer height: %i", dxApp->getSwapChainHeight());
}

void ImguiInstance::imguiRender()
{
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxApp->getCommandList().Get());
}

void ImguiInstance::updateImgui()
{
  cleanupImgui();
  initImgui(nullptr);
}

void ImguiInstance::cleanupImgui()
{
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

} // namespace DXApp