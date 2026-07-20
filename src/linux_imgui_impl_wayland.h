#pragma once
#include "imgui.h" //IMGUI_IMPL_API

#ifndef IMGUI_DISABLE
#include <stdbool.h>

#include "linuxWayland.h"

IMGUI_IMPL_API bool ImGui_ImplWayland_Init(WaylandAPI::WaylandAPI *wlAPI);
IMGUI_IMPL_API void ImGui_ImplWayland_Shutdown();
IMGUI_IMPL_API void ImGui_ImplWayland_NewFrame();

// helpers
IMGUI_IMPL_API void  ImGui_ImplWayland_Sleep(int milliseconds);
IMGUI_IMPL_API float ImGui_ImplWayland_GetContentScaleForWindow();
IMGUI_IMPL_API float ImGui_ImplWayland_GetContentScaleForMonitor();
#endif // ifndef IMGUI_DISABLE