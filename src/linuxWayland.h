#pragma once

#include "hdrManager.h"
#include "utility.h"

#include <vulkan/vulkan_hpp_macros.hpp> // optional: include Vulkan-Hpp configuration macros
#include <vulkan/vulkan_raii.hpp>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xdg-shell.h>

#include <color-management-v1.h>        // for query HDR display
#include <fractional-scale-v1.h>        // for mapping perfectly to screen pixel
#include <viewporter.h>                 // for mapping perfectly to screen pixel
#include <xdg-decoration-unstable-v1.h> // for serverside decorations

namespace WaylandAPI
{

struct wlSupportedColorFeatures
{
  bool feature_set_mastering_display_primaries = false;
  bool feature_set_luminances                  = false;
  bool feature_windows_scrgb                   = false; // ignore for now
  bool primary_bt709                           = false; // srgb
  bool primary_bt2020                          = false; // b2020
  bool tf_gamma22                              = false; // gamma22
  bool tf_srgb                                 = false; // srgb piecewise
  bool tf_pq                                   = false; // st2084_pq
  bool tf_linear                               = false; // ext_linear
  bool done                                    = false;
};

struct wlImageDesc
{
  int      target_rx                   = 0; // x * 1 000 000
  int      target_ry                   = 0; // y * 1 000 000
  int      target_gx                   = 0; // ...
  int      target_gy                   = 0;
  int      target_bx                   = 0;
  int      target_by                   = 0;
  int      target_wx                   = 0;
  int      target_wy                   = 0;
  uint32_t target_min_lum              = 0; // cd/m2 * 10000
  uint32_t target_max_lum              = 0; // cd/m2 * 1
  uint32_t target_max_fall             = 0; // cd/m2 * 1
  uint32_t reference_lum               = 0; // cd/m2 whitepoint
  uint32_t tf_named                    = 0;
  uint32_t recommended_tf_named        = 0;
  uint32_t primaries_named             = 0;
  uint32_t recommended_primaries_named = 0;
};

struct requestedHDRState
{
  float requestedMonitorMinCLL = 0.0f;
  float requestedMonitorMaxCLL = 1000.0f;
  float requestedOsWhitepoint  = 203.0f;
};

struct wlState
{
  // wayland base
  struct wl_display    *display                    = nullptr;
  struct wl_registry   *registry                   = nullptr;
  struct wl_compositor *compositor                 = nullptr;
  struct wl_surface    *surface                    = nullptr;
  struct wl_callback   *frame_cb                   = nullptr;
  struct wl_region     *region                     = nullptr;
  bool                  frame_done                 = false;
  uint                  frame_time                 = 0;
  uint                  elapsed_frame_time         = 0;
  float                 preferred_buffer_scale     = 1.0f;
  unsigned int          preferred_buffer_transform = 0;
  struct wl_seat       *seat                       = nullptr;
  struct wl_keyboard   *keyboard                   = nullptr;
  struct wl_pointer    *pointer                    = nullptr;
  bool                  has_keyboard               = false;
  bool                  has_pointer                = false;
  bool                  left_button_clicked        = false;
  wl_fixed_t            pointer_x                  = 0;
  wl_fixed_t            pointer_y                  = 0;
  bool                  surface_focused            = false;

  // xdg shell
  struct xdg_wm_base   *wm_base    = nullptr;
  struct xdg_surface   *surfaceXdg = nullptr;
  struct xdg_toplevel  *toplevel   = nullptr;
  int                   sWidth     = WIDTH;
  int                   sHeight    = HEIGHT;
  std::vector<uint32_t> surface_states{};
  bool                  want_fullscreen = false;
  bool                  window_closed   = false;

  // xdg decoration
  struct zxdg_decoration_manager_v1  *decoration_manager  = nullptr;
  struct zxdg_toplevel_decoration_v1 *toplevel_decoration = nullptr;

  // viewporter
  struct wp_viewporter *viewporter = nullptr;
  struct wp_viewport   *viewport   = nullptr;

  // fractional scale
  struct wp_fractional_scale_manager_v1 *scale_manager    = nullptr;
  struct wp_fractional_scale_v1         *fractional_scale = nullptr;
  uint                                   preferred_scale  = 120;

  // color management
  struct wp_color_manager_v1                     *color_manager            = nullptr;
  struct wp_color_management_surface_v1          *color_management_surface = nullptr;
  struct wp_color_management_surface_feedback_v1 *surface_feedback         = nullptr;
  struct wp_image_description_v1                 *preferred_description    = nullptr;
  struct wp_image_description_info_v1            *preferred_info           = nullptr;
  struct wp_image_description_creator_params_v1  *description_creator      = nullptr;
  struct wp_image_description_v1                 *final_description        = nullptr;
  wlSupportedColorFeatures                        supported_color_features{};
  wlImageDesc                                     image_description_buffer{};
  wlImageDesc                                     image_description_final{};
  bool                                            isMonitorHDR = false;
  requestedHDRState                               requestedState{};

  // general stuff
  unsigned int     buffer_transform = 0;
  int              fbWidth          = WIDTH;
  int              fbHeight         = HEIGHT;
  bool             swapChainInvalid = false;
  HDR::HDRManager *hdrManager       = nullptr;
};

class WaylandAPI
{
public:
  void init(HDR::HDRManager *hdrManagerInput);
  void createSurface(vk::raii::Instance &instance, vk::raii::SurfaceKHR &vulkanSurface);
  void updateState();
  void preparePresent();
  bool dispatch();
  void terminate();

  void   setFullscreen();
  void   unsetFullscreen();
  double getTime();
  void   getPointerPos(double &pointer_x, double &pointer_y);
  bool   windowShouldClose();
  // Getter
  void                       getFramebufferSize(int &width, int &height);
  void                       getSurfaceSize(int &width, int &height);
  std::vector<const char *> &getRequiredInstanceExtensions()
  {
    return instanceExtensions;
  }
  wlState &getState()
  {
    return state;
  }

private:
  std::vector<const char *> instanceExtensions = {vk::KHRSurfaceExtensionName, vk::KHRWaylandSurfaceExtensionName};
  wlState                   state;
  bool                      hdrWindowSupported = false;

  void requestFrame();
  void setHDRWindowSupport();
  void updateSwapchain();
};
} // namespace WaylandAPI