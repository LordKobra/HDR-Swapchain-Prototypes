#include "imgui.h"
#include <cstdio>
#include <unistd.h>
#ifndef IMGUI_DISABLE
#include "linux_imgui_impl_wayland.h"

struct ImGui_ImplWayland_Data
{
  WaylandAPI::WaylandAPI *wlAPI = nullptr;

  ImGuiContext *Context                 = nullptr;
  ImVec2        LastValidMousePos       = {0, 0};
  double        time                    = 0;
  char          BackendPlatformName[32] = "Wayland";
};

struct ImGui_ImplWayland_SurfaceToContext
{
  struct wl_surface *surface = nullptr;
  ImGuiContext      *context = nullptr;
};

static ImGui_ImplWayland_SurfaceToContext g_ContextMap;

static void ImGui_ImplWayland_ContextMap_Add(struct wl_surface *surface, ImGuiContext *ctx)
{
  g_ContextMap.context = ctx;
  g_ContextMap.surface = surface;
}

static ImGuiContext *ImGui_ImplWayland_ContextMap_Get(struct wl_surface *surface)
{
  if (g_ContextMap.surface == surface)
    return g_ContextMap.context;
  return nullptr;
}

namespace ImGui
{
extern ImGuiIO &GetIO(ImGuiContext *);
}
static ImGui_ImplWayland_Data *ImGui_ImplWayland_GetBackendData()
{
  return ImGui::GetCurrentContext() ? (ImGui_ImplWayland_Data *)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

bool ImGui_ImplWayland_Init(WaylandAPI::WaylandAPI *wlAPI)
{
  ImGuiIO &io = ImGui::GetIO();
  IMGUI_CHECKVERSION();
  IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

  ImGui_ImplWayland_Data *bd = IM_NEW(ImGui_ImplWayland_Data)();
  snprintf(bd->BackendPlatformName, sizeof(bd->BackendPlatformName), "imgui_impl_wayland (%d)", 1);
  io.BackendPlatformUserData = (void *)bd;
  io.BackendPlatformName     = bd->BackendPlatformName;

  bd->Context = ImGui::GetCurrentContext();
  bd->wlAPI   = wlAPI;
  bd->time    = 0.0;
  ImGui_ImplWayland_ContextMap_Add(wlAPI->getState().surface, bd->Context);

  //@todo manually feed event processing data to imgui
  // Set platform dependent data in viewport
  ImGuiViewport *main_viewport  = ImGui::GetMainViewport();
  main_viewport->PlatformHandle = (void *)bd->wlAPI;
  IM_UNUSED(main_viewport);

  return true;
}

void ImGui_ImplWayland_Shutdown()
{
  ImGui_ImplWayland_Data *bd = ImGui_ImplWayland_GetBackendData();
  IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");

  ImGuiIO         &io          = ImGui::GetIO();
  ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();

  io.BackendPlatformName     = nullptr;
  io.BackendPlatformUserData = nullptr;
  // io.BackendFlags &=
  platform_io.ClearPlatformHandlers();
  IM_DELETE(bd);
}

float ImGui_ImplWayland_GetContentScaleForWindow()
{
  return 1.0f;
}

float ImGui_ImplWayland_GetContentScaleForMonitor()
{
  return 1.0f;
}

static void ImGui_ImplWayland_GetSurfaceSizeAndFramebufferScale(WaylandAPI::WaylandAPI *wlAPI, ImVec2 *out_size,
                                                                ImVec2 *out_framebuffer_scale)
{
  int sWidth = 1, sHeight = 1, fbWidth = 1, fbHeight = 1;
  wlAPI->getSurfaceSize(sWidth, sHeight);
  wlAPI->getFramebufferSize(fbWidth, fbHeight);
  float fb_scale_x = (sWidth > 0) ? (float)fbWidth / (float)sWidth : 1.0f;
  float fb_scale_y = (sHeight > 0) ? (float)fbHeight / (float)sHeight : 1.0f;
  if (out_size != nullptr)
  {
    *out_size = ImVec2((float)sWidth, (float)sHeight);
  }
  if (out_framebuffer_scale != nullptr)
  {
    *out_framebuffer_scale = ImVec2(fb_scale_x, fb_scale_y);
  }
}

static void ImGui_ImplWayland_UpdateMouseData()
{
  ImGuiIO                &io = ImGui::GetIO();
  ImGui_ImplWayland_Data *bd = ImGui_ImplWayland_GetBackendData();

  const bool is_window_focused = bd->wlAPI->getState().surface_focused; // always assume focus?
  if (is_window_focused)
  {
    double pointer_x, pointer_y;
    bd->wlAPI->getPointerPos(pointer_x, pointer_y);
    bd->LastValidMousePos = ImVec2((float)pointer_x, (float)pointer_y);
    io.AddMousePosEvent((float)pointer_x, (float)pointer_y); // todo check if callback is better
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, bd->wlAPI->getState().left_button_clicked);
  }
}

void ImGui_ImplWayland_NewFrame()
{
  ImGuiIO                &io = ImGui::GetIO();
  ImGui_ImplWayland_Data *bd = ImGui_ImplWayland_GetBackendData();
  IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplWayland_InitForXXX()?");

  ImGui_ImplWayland_GetSurfaceSizeAndFramebufferScale(bd->wlAPI, &io.DisplaySize, &io.DisplayFramebufferScale);

  double current_time = bd->wlAPI->getTime();
  if (current_time <= bd->time)
  {
    current_time = bd->time + 0.00001;
  }
  io.DeltaTime = bd->time > 0.0 ? (float)(current_time - bd->time) : (float)(1.0f / 60.0f);
  bd->time     = current_time;

  ImGui_ImplWayland_UpdateMouseData();
}

// GLFW doesn't provide a portable sleep function
void ImGui_ImplWayland_Sleep(int milliseconds)
{
  usleep(milliseconds * 1000);
}

#endif // #ifndef IMGUI_DISABLE