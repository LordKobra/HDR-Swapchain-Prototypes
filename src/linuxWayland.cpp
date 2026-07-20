#include "linuxWayland.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <stdint.h>
#include <stdio.h>

#include <linux/input-event-codes.h>

#define IO_WL_STUB_QUIET(Name) .Name = [](auto...) {}

namespace
{
extern const struct wl_pointer_listener                   pointer_listener;
extern const struct wl_callback_listener                  callback_listener;
extern const struct wp_image_description_info_v1_listener image_description_info_listener;
extern const struct wp_image_description_v1_listener      image_description_listener;
} // namespace

static void wl_seat_capabilities(void *data, struct wl_seat *seat, uint capabilities)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) > 0)
    localState->has_keyboard = true;
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) > 0)
  {
    localState->has_pointer = true;
    localState->pointer     = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(localState->pointer, &pointer_listener, data);
    printf("haspointer\n");
  }
}

static void wl_pointer_enter(void *data, struct wl_pointer *pointer, uint serial, struct wl_surface *surface,
                             wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->pointer_x           = surface_x;
  localState->pointer_y           = surface_y;
  localState->surface_focused     = true;
}

static void wl_pointer_leave(void *data, struct wl_pointer *pointer, uint serial, struct wl_surface *surface)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->surface_focused     = false;
}

static void wl_pointer_motion(void *data, struct wl_pointer *pointer, uint time, wl_fixed_t surface_x,
                              wl_fixed_t surface_y)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->pointer_x           = surface_x;
  localState->pointer_y           = surface_y;
}

static void wl_pointer_button(void *data, struct wl_pointer *pointer, uint serial, uint time, uint button, uint state)
{
  /* The button is a button code as defined in the Linux kernel's linux/input-event-codes.h header file, e.g. BTN_LEFT
   */
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
    localState->left_button_clicked = true;
  if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_RELEASED)
    localState->left_button_clicked = false;
}

static void wl_pointer_frame(void *data, struct wl_pointer *pointer)
{
  // it really depends how a proper event processing state looks like
}

static void wl_surface_preferred_buffer_scale(void *data, struct wl_surface *surface, int32_t scale)
{
  WaylandAPI::wlState *localState    = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->preferred_buffer_scale = scale;
}

static void wl_surface_preferred_buffer_transform(void *data, struct wl_surface *surface, unsigned int transform)
{
  WaylandAPI::wlState *localState        = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->preferred_buffer_transform = transform;
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
  xdg_wm_base_pong(wm_base, serial);
}

static void xdg_surface_configure(void *data, struct xdg_surface *surfaceXdg, uint32_t serial)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  xdg_surface_ack_configure(surfaceXdg, serial);
}

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int width, int height,
                                   struct wl_array *states)
{
  /* in window geometry

If the width or height arguments are zero, it means the client should decide its own window dimension. This may happen
when the compositor needs to configure the state of the surface but doesn't have any information about any previous or
expected dimension.
*/

  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if (!(width == 0 || height == 0))
  {
    localState->sWidth  = width;
    localState->sHeight = height;
  }
  if (states && states->data)
  {
    uint32_t *converted = static_cast<uint32_t *>(states->data);
    size_t    count     = states->size / sizeof(uint32_t);
    localState->surface_states.insert(localState->surface_states.end(), converted, converted + count);
  }
  // no fallback scale for now - also we do not update at fractional scale change here, but who cares, it should still
  // work
  float final_scale   = ((float)localState->preferred_scale / 120.0f);
  uint  new_fb_width  = (uint)((float)localState->sWidth * final_scale);
  uint  new_fb_height = (uint)((float)localState->sHeight * final_scale);
  if (new_fb_width != localState->fbWidth || new_fb_height != localState->fbHeight)
  {
    localState->swapChainInvalid = true;
    localState->fbWidth          = new_fb_width;
    localState->fbHeight         = new_fb_height;
  }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->window_closed       = true;
  printf("Window closed\n");
}

static void zxdg_toplevel_decoration_v1_configure(void *data, struct zxdg_toplevel_decoration_v1 *toplevel_decoration,
                                                  uint mode)
{
  if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE)
  {
    printf("Mode %d means cient-side decorations, but we need server-side decorations!", mode);
  }
}

static void wp_fractional_scale_v1_preferred_scale(void *data, struct wp_fractional_scale_v1 *fractional_scale,
                                                   uint scale)
{
  // note: scale is uint and becomes fractional by dividing by 120
  // float real_scale = (float) scale / 120.0f;
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->preferred_scale     = scale;
}

static void frame_done(void *data, struct wl_callback *callback, uint32_t time)
{
  wl_callback_destroy(callback);
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->frame_done          = true;
  localState->frame_cb            = nullptr;
  localState->elapsed_frame_time  = time - localState->frame_time;
  localState->frame_time          = time; // current time, in milliseconds, with an undefined base
}

static void wp_color_manager_v1_supported_feature(void *data, struct wp_color_manager_v1 *color_manager, uint feature)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if (feature == WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES)
    localState->supported_color_features.feature_set_luminances = true;
  if (feature == WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES)
    localState->supported_color_features.feature_set_mastering_display_primaries = true;
  if (feature == WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB)
    localState->supported_color_features.feature_windows_scrgb = true;
}

static void wp_color_manager_v1_supported_tf_named(void *data, struct wp_color_manager_v1 *color_manager, uint tf)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if (tf == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22)
    localState->supported_color_features.tf_gamma22 = true;
  if (tf == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB)
    localState->supported_color_features.tf_srgb = true;
  if (tf == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR)
    localState->supported_color_features.tf_linear = true;
  if (tf == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ)
    localState->supported_color_features.tf_pq = true;
}

static void wp_color_manager_v1_supported_primaries_named(void *data, wp_color_manager_v1 *color_manager,
                                                          uint primaries)
{
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if (primaries == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB)
    localState->supported_color_features.primary_bt709 = true;
  if (primaries == WP_COLOR_MANAGER_V1_PRIMARIES_BT2020)
    localState->supported_color_features.primary_bt2020 = true;
}

static void wp_color_manager_v1_done(void *data, wp_color_manager_v1 *color_manger)
{
  WaylandAPI::wlState *localState           = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->supported_color_features.done = true; // stays static for the app runtime
}

static void wp_color_management_surface_feedback_v1_preferred_changed(
    void *data, struct wp_color_management_surface_feedback_v1 *surface_feedback, uint32_t identity)
{
  // uint                 description_id = identity; // we could cache that but not now
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if (localState->preferred_description != nullptr)
  {
    wp_image_description_v1_destroy(localState->preferred_description);
    localState->preferred_description = nullptr;
  }
  localState->preferred_description =
      wp_color_management_surface_feedback_v1_get_preferred(localState->surface_feedback);
  // we do not need to add a listener here, because the preferred description can be treated as ready
  localState->preferred_info = wp_image_description_v1_get_information(localState->preferred_description);
  wp_image_description_info_v1_add_listener(localState->preferred_info, &image_description_info_listener, localState);
}

static void wp_image_description_v1_ready(void *data, struct wp_image_description_v1 *image_description_buffer,
                                          uint32_t identity)
{
  // created image description becomes ready and should be sent to compositor
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  wp_color_management_surface_v1_set_image_description(localState->color_management_surface,
                                                       localState->final_description,
                                                       WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
  printf("Image Description sent and accepted!\n");
}

static void wp_image_description_info_v1_primaries_named(void                         *data,
                                                         wp_image_description_info_v1 *image_description_info,
                                                         uint                          primaries)
{
  WaylandAPI::wlState *localState                                  = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->image_description_buffer.recommended_primaries_named = primaries;
}

static void wp_image_description_info_v1_tf_named(void *data, wp_image_description_info_v1 *image_description_info,
                                                  uint tf)
{
  WaylandAPI::wlState *localState                           = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->image_description_buffer.recommended_tf_named = tf;
}

static void wp_image_description_info_v1_luminances(void *data, wp_image_description_info_v1 *image_description_info,
                                                    uint min_lum, uint max_lum, uint reference_lum)
{
  WaylandAPI::wlState *localState                    = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->image_description_buffer.reference_lum = reference_lum;
}

static void wp_image_description_info_v1_target_primaries(void                         *data,
                                                          wp_image_description_info_v1 *image_description_info, int r_x,
                                                          int r_y, int g_x, int g_y, int b_x, int b_y, int w_x, int w_y)
{
  WaylandAPI::wlState *localState                = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->image_description_buffer.target_rx = r_x;
  localState->image_description_buffer.target_ry = r_y;
  localState->image_description_buffer.target_gx = g_x;
  localState->image_description_buffer.target_gy = g_y;
  localState->image_description_buffer.target_bx = b_x;
  localState->image_description_buffer.target_by = b_y;
  localState->image_description_buffer.target_wx = w_x;
  localState->image_description_buffer.target_wy = w_y;
}

static void wp_image_description_info_v1_target_luminance(void                         *data,
                                                          wp_image_description_info_v1 *image_description_info,
                                                          uint min_lum, uint max_lum)
{
  WaylandAPI::wlState *localState                     = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->image_description_buffer.target_min_lum = min_lum;
  localState->image_description_buffer.target_max_lum = max_lum;
}
static void wp_image_description_info_v1_target_max_fall(void                         *data,
                                                         wp_image_description_info_v1 *image_description_info,
                                                         uint                          max_fall)
{
  WaylandAPI::wlState *localState                      = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->image_description_buffer.target_max_fall = max_fall;
}

static void wp_image_description_info_v1_done(void *data, wp_image_description_info_v1 *image_description_info)
{
  WaylandAPI::wlState *localState      = reinterpret_cast<WaylandAPI::wlState *>(data);
  localState->image_description_final  = localState->image_description_buffer;
  localState->image_description_buffer = {};
  // @todo "destroys the object." -> do we need to do manual cleanup?
  // check if monitor in hdr
  bool no_luminance_headroom =
      localState->image_description_final.reference_lum == localState->image_description_final.target_max_lum;
  bool has_srgb_primaries =
      localState->image_description_final.recommended_primaries_named == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB;
  bool has_srgb_encoding =
      (localState->image_description_final.recommended_tf_named == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB) ||
      (localState->image_description_final.recommended_tf_named == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
  bool isMonitorSDR = no_luminance_headroom && has_srgb_primaries && has_srgb_encoding;

  localState->hdrManager->colorInfo.hdrMonitorActive = !isMonitorSDR;

  localState->hdrManager->colorInfo.monitorColorspace = localState->image_description_final.recommended_primaries_named;
  localState->hdrManager->colorInfo.monitorMaxFALL    = localState->image_description_final.target_max_fall;
  localState->requestedState.requestedMonitorMinCLL =
      ((float)localState->image_description_final.target_min_lum) / 10000.0;
  localState->requestedState.requestedMonitorMaxCLL = localState->image_description_final.target_max_lum;
  localState->requestedState.requestedOsWhitepoint  = localState->image_description_final.reference_lum;

  // we need to respect the forceSDR parameter here
  localState->hdrManager->updateHDRVariables(); // monitor feedback
  printf("Wayland Image Desc: Monitor in HDR: %i\n", localState->hdrManager->colorInfo.hdrMonitorActive);
}

// Listeners

// listen to capabilites(keyboard mouse) 1, name 2
static const struct wl_seat_listener seat_listener{.capabilities = wl_seat_capabilities, IO_WL_STUB_QUIET(name)};

namespace
{
// listen to enter 1, leave 1, motion 1, button 1, axis 1, frame 5, axis_source 5, axist_stop 5, axis_discrete 5-7,
// axis_value120 8, axis_relative_direction 9 events
const struct wl_pointer_listener pointer_listener = {
    .enter  = wl_pointer_enter,
    .leave  = wl_pointer_leave,
    .motion = wl_pointer_motion,
    .button = wl_pointer_button,
    IO_WL_STUB_QUIET(axis),
    .frame = wl_pointer_frame,
    IO_WL_STUB_QUIET(axis_source),
    IO_WL_STUB_QUIET(axis_stop),
    IO_WL_STUB_QUIET(axis_discrete)
};
} // namespace

// listen to enter, leave, preferred_buffer_scale, preferred_buffer_transform
static const struct wl_surface_listener surface_listener_base = {
    IO_WL_STUB_QUIET(enter),
    IO_WL_STUB_QUIET(leave),
    .preferred_buffer_scale     = wl_surface_preferred_buffer_scale,
    .preferred_buffer_transform = wl_surface_preferred_buffer_transform
};

// listen to configure
static const struct xdg_surface_listener surface_listener_xdg = {.configure = xdg_surface_configure};

// listen to ping
static const struct xdg_wm_base_listener wm_base_listener = {.ping = xdg_wm_base_ping};

// listen to configure(resize, change state), close, (configure_bounds, wm_capabilites only at newer version than 1)
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = xdg_toplevel_configure, .close = xdg_toplevel_close
};

// listen to configure (acknowledge decoration mode)
static const struct zxdg_toplevel_decoration_v1_listener toplevel_decoration_listener = {
    .configure = zxdg_toplevel_decoration_v1_configure
};

// listen to preferred_scale
static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = wp_fractional_scale_v1_preferred_scale
};

// listen to supported intent, features, transfer functions, primaries, done
static const struct wp_color_manager_v1_listener color_manager_listener = {
    IO_WL_STUB_QUIET(supported_intent),
    .supported_feature         = wp_color_manager_v1_supported_feature,
    .supported_tf_named        = wp_color_manager_v1_supported_tf_named,
    .supported_primaries_named = wp_color_manager_v1_supported_primaries_named,
    .done                      = wp_color_manager_v1_done
};

// listen to preferred_changed (v1)
static const struct wp_color_management_surface_feedback_v1_listener color_management_surface_feedback_listener = {
    .preferred_changed = wp_color_management_surface_feedback_v1_preferred_changed
};

namespace
{
const struct wp_image_description_v1_listener image_description_listener = {
    IO_WL_STUB_QUIET(failed), .ready = wp_image_description_v1_ready
};

const struct wp_image_description_info_v1_listener image_description_info_listener = {
    .done = wp_image_description_info_v1_done,
    IO_WL_STUB_QUIET(icc_file),
    IO_WL_STUB_QUIET(primaries),
    .primaries_named = wp_image_description_info_v1_primaries_named,
    IO_WL_STUB_QUIET(tf_power),
    .tf_named         = wp_image_description_info_v1_tf_named,
    .luminances       = wp_image_description_info_v1_luminances,
    .target_primaries = wp_image_description_info_v1_target_primaries,
    .target_luminance = wp_image_description_info_v1_target_luminance,
    IO_WL_STUB_QUIET(target_max_cll),
    IO_WL_STUB_QUIET(target_max_fall)

};

// listen to done frame request
const struct wl_callback_listener callback_listener = {.done = frame_done};
} // namespace

static void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                                   uint32_t version)
{
  // printf("interface: '%s', version: %d, name: %d\n", interface, version, name);
  WaylandAPI::wlState *localState = reinterpret_cast<WaylandAPI::wlState *>(data);
  if (strcmp(interface, wl_compositor_interface.name) == 0)
  {
    localState->compositor =
        reinterpret_cast<struct wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, 6));
  }
  if (strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    localState->wm_base =
        reinterpret_cast<struct xdg_wm_base *>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    xdg_wm_base_add_listener(localState->wm_base, &wm_base_listener, localState);
  }
  if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
  {
    localState->decoration_manager = reinterpret_cast<struct zxdg_decoration_manager_v1 *>(
        wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
  }
  if (strcmp(interface, wp_viewporter_interface.name) == 0)
  {
    localState->viewporter =
        reinterpret_cast<struct wp_viewporter *>(wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
  }
  if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0)
  {
    localState->scale_manager = reinterpret_cast<struct wp_fractional_scale_manager_v1 *>(
        wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1));
  }
  if (strcmp(interface, wp_color_manager_v1_interface.name) == 0)
  {
    localState->color_manager = reinterpret_cast<struct wp_color_manager_v1 *>(
        wl_registry_bind(registry, name, &wp_color_manager_v1_interface, 1));
    wp_color_manager_v1_add_listener(localState->color_manager, &color_manager_listener, localState);
  }
  if (strcmp(interface, wl_seat_interface.name) == 0)
  {
    localState->seat = reinterpret_cast<struct wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
    wl_seat_add_listener(localState->seat, &seat_listener, localState);
  }
}

// listen to global, global_remove
static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global, IO_WL_STUB_QUIET(global_remove)
};

namespace WaylandAPI
{
void WaylandAPI::init(HDR::HDRManager *hdrManagerInput)
{
  state.hdrManager = hdrManagerInput;

  state.display = wl_display_connect(NULL);
  if (!state.display)
  {
    fprintf(stderr, "Failed to connect to Wayland display.\n");
    return;
  }

  fprintf(stderr, "Connection established!\n");
  state.registry = wl_display_get_registry(state.display);
  wl_registry_add_listener(state.registry, &registry_listener, &state);

  wl_display_roundtrip(state.display);
  assert(state.compositor != nullptr);
  assert(state.seat != nullptr);
  assert(state.wm_base != nullptr);
  assert(state.decoration_manager != nullptr);
  assert(state.viewporter != nullptr);
  assert(state.scale_manager != nullptr);
  if (state.color_manager != nullptr)
  {
    hdrWindowSupported = true;
  }
  setHDRWindowSupport();

  state.surface = wl_compositor_create_surface(state.compositor);
  wl_surface_add_listener(state.surface, &surface_listener_base, &state);

  state.surfaceXdg = xdg_wm_base_get_xdg_surface(state.wm_base, state.surface);
  xdg_surface_add_listener(state.surfaceXdg, &surface_listener_xdg, &state);
  state.toplevel = xdg_surface_get_toplevel(state.surfaceXdg);
  xdg_toplevel_set_title(state.toplevel, "Wayland Vulkan Client");
  xdg_toplevel_add_listener(state.toplevel, &toplevel_listener, &state);

  state.toplevel_decoration =
      zxdg_decoration_manager_v1_get_toplevel_decoration(state.decoration_manager, state.toplevel);
  zxdg_toplevel_decoration_v1_set_mode(state.toplevel_decoration, 2);
  zxdg_toplevel_decoration_v1_add_listener(state.toplevel_decoration, &toplevel_decoration_listener, &state);

  state.viewport = wp_viewporter_get_viewport(state.viewporter, state.surface);

  state.fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(state.scale_manager, state.surface);
  wp_fractional_scale_v1_add_listener(state.fractional_scale, &fractional_scale_listener, &state);

  if (hdrWindowSupported)
  {
    state.color_management_surface = wp_color_manager_v1_get_surface(state.color_manager, state.surface);
    state.surface_feedback         = wp_color_manager_v1_get_surface_feedback(state.color_manager, state.surface);
    wp_color_management_surface_feedback_v1_add_listener(
        state.surface_feedback, &color_management_surface_feedback_listener, &state);
    if (state.preferred_description != nullptr)
    {
      wp_image_description_v1_destroy(state.preferred_description);
      state.preferred_description = nullptr;
    }
    state.preferred_description = wp_color_management_surface_feedback_v1_get_preferred(state.surface_feedback);
    // we do not need to add a listener here, because the preferred description can be treated as ready
    state.preferred_info = wp_image_description_v1_get_information(state.preferred_description);
    wp_image_description_info_v1_add_listener(state.preferred_info, &image_description_info_listener, &state);
  }
  state.frame_done = true; // we need to get the first frame started easy

  wl_surface_commit(state.surface);
  getTime(); // init time static member for imgui
  fprintf(stderr, "Surface established!3\n");

  return;
}

void WaylandAPI::createSurface(vk::raii::Instance &instance, vk::raii::SurfaceKHR &vulkanSurface)
{
  vk::WaylandSurfaceCreateInfoKHR surfaceCreateInfo{.flags = {}, .display = state.display, .surface = state.surface};
  vulkanSurface = instance.createWaylandSurfaceKHR(surfaceCreateInfo);
};

void WaylandAPI::requestFrame()
{
  state.frame_cb = wl_surface_frame(state.surface);
  wl_callback_add_listener(state.frame_cb, &callback_listener, &state);
  state.frame_done = false;
}

void WaylandAPI::setFullscreen()
{
  xdg_toplevel_set_fullscreen(state.toplevel, NULL);
}

void WaylandAPI::unsetFullscreen()
{
  xdg_toplevel_unset_fullscreen(state.toplevel);
}

void WaylandAPI::updateState()
{
  bool windowNeedsUpdate = state.requestedState.requestedMonitorMinCLL != state.hdrManager->colorInfo.monitorMinCLL ||
                           state.requestedState.requestedMonitorMaxCLL != state.hdrManager->colorInfo.monitorMaxCLL ||
                           state.requestedState.requestedOsWhitepoint != state.hdrManager->colorInfo.osWhitepoint ||
                           state.hdrManager->colorInfo.swapChainInvalid;
  if (windowNeedsUpdate)
  {
    updateSwapchain();
  }
}

void WaylandAPI::preparePresent() // needs guarantee to chain to a wl_surface_commit call from vkqueuepresent
{
  requestFrame();
  uint32_t surface_state;
  if (!state.surface_states.empty())
  {
    for (size_t i = 0; i < state.surface_states.size(); ++i)
    {
      surface_state = state.surface_states[i];
      if ((surface_state != XDG_TOPLEVEL_STATE_FULLSCREEN) && state.want_fullscreen)
      {
        setFullscreen();
        break;
      }
      if ((surface_state == XDG_TOPLEVEL_STATE_FULLSCREEN) && !state.want_fullscreen)
      {
        unsetFullscreen();
        break;
      }
    }
  }
  if (state.swapChainInvalid)
  {
    state.swapChainInvalid = false;
    state.buffer_transform = state.preferred_buffer_transform;
    wl_surface_set_buffer_transform(state.surface, state.buffer_transform);

    wl_surface_set_buffer_scale(state.surface, 1);
    wp_viewport_set_source(state.viewport, 0, 0, wl_fixed_from_int(state.fbWidth), wl_fixed_from_int(state.fbHeight));
    wp_viewport_set_destination(state.viewport, state.sWidth, state.sHeight);

    // set window opaque. it uses surface-local coordinates / logical
    if (state.region != nullptr)
    {
      wl_region_destroy(state.region);
      state.region = nullptr;
    }
    state.region = wl_compositor_create_region(state.compositor);
    wl_region_add(state.region, 0, 0, state.sWidth, state.sHeight);
    wl_surface_set_opaque_region(state.surface, state.region);
  }
}

bool WaylandAPI::dispatch()
{
  return (wl_display_dispatch(state.display) != -1);
}

void WaylandAPI::terminate()
{
  if (!state.display)
    return;

  if (hdrWindowSupported)
  {
    if (state.final_description)
      wp_image_description_v1_destroy(state.final_description);
    if (state.preferred_description)
      wp_image_description_v1_destroy(state.preferred_description);
    if (state.surface_feedback)
      wp_color_management_surface_feedback_v1_destroy(state.surface_feedback);
    if (state.color_management_surface)
      wp_color_management_surface_v1_destroy(state.color_management_surface);
    if (state.color_manager)
      wp_color_manager_v1_destroy(state.color_manager);
  }

  if (state.fractional_scale)
    wp_fractional_scale_v1_destroy(state.fractional_scale);
  if (state.scale_manager)
    wp_fractional_scale_manager_v1_destroy(state.scale_manager);

  if (state.viewport)
    wp_viewport_destroy(state.viewport);
  if (state.viewporter)
    wp_viewporter_destroy(state.viewporter);

  if (state.toplevel_decoration)
    zxdg_toplevel_decoration_v1_destroy(state.toplevel_decoration);
  if (state.decoration_manager)
    zxdg_decoration_manager_v1_destroy(state.decoration_manager);

  if (state.toplevel)
    xdg_toplevel_destroy(state.toplevel);
  if (state.surfaceXdg)
    xdg_surface_destroy(state.surfaceXdg);
  if (state.wm_base)
    xdg_wm_base_destroy(state.wm_base);

  if (state.keyboard)
    wl_keyboard_release(state.keyboard);
  if (state.pointer)
    wl_pointer_release(state.pointer);
  if (state.seat)
    wl_seat_release(state.seat);

  if (state.surface)
    wl_surface_destroy(state.surface);
  if (state.compositor)
  {
    wl_compositor_destroy(state.compositor);
  }
  wl_display_flush(state.display);
  wl_display_roundtrip(state.display);

  // check errors
  int error_code = wl_display_get_error(state.display);
  if (error_code)
    fprintf(stderr, "Wayland error during shutdown: %d\n", error_code);

  wl_display_disconnect(state.display);
}

void WaylandAPI::getFramebufferSize(int &width, int &height)
{
  width  = state.fbWidth;
  height = state.fbHeight;
}

void WaylandAPI::getSurfaceSize(int &width, int &height)
{
  width  = state.sWidth;
  height = state.sHeight;
}

double WaylandAPI::getTime()
{
  static const auto start    = std::chrono::high_resolution_clock::now();
  auto              now      = std::chrono::high_resolution_clock::now();
  double            duration = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
  return duration;
}

void WaylandAPI::getPointerPos(double &pointer_x, double &pointer_y)
{
  pointer_x = wl_fixed_to_double(state.pointer_x);
  pointer_y = wl_fixed_to_double(state.pointer_y);
}

bool WaylandAPI::windowShouldClose()
{
  return state.window_closed;
}

void WaylandAPI::setHDRWindowSupport()
{
  state.hdrManager->colorInfo.hdrWindowSupported = hdrWindowSupported;
}

void WaylandAPI::updateSwapchain()
{
  // add tf_named and primaries_named
  if (state.supported_color_features.primary_bt709)
  {
    state.image_description_final.primaries_named = WP_COLOR_MANAGER_V1_PRIMARIES_SRGB;
  }
  else if (state.supported_color_features.primary_bt2020 && state.hdrManager->colorInfo.hdrPossible)
  {
    state.image_description_final.primaries_named = WP_COLOR_MANAGER_V1_PRIMARIES_BT2020;
  }
  else
  {
    fprintf(stderr, "Wayland surface can't find supported HDR primaries!\n");
  }

  // Note: For compliance with the shader, we allow only SCRGB (rec709+ linear), HDR10 (rec2020+pq) and srgb
  if (state.supported_color_features.tf_linear &&
      state.image_description_final.primaries_named == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB &&
      state.hdrManager->colorInfo.hdrPossible)
  { // scrgb
    state.image_description_final.tf_named = WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
  }
  else if (state.supported_color_features.tf_pq &&
           state.image_description_final.primaries_named == WP_COLOR_MANAGER_V1_PRIMARIES_BT2020 &&
           state.hdrManager->colorInfo.hdrPossible)
  { // hdr10 ST2084
    state.image_description_final.tf_named = WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
  }
  else if (state.supported_color_features.tf_srgb &&
           state.image_description_final.primaries_named == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB &&
           !state.hdrManager->colorInfo.hdrPossible)
  { // srgb
    // in sdr we need to set vk::ColorSpaceKHR::eSrgbNonlinear for compliance with the VkApp
    // technically we should use WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB, but it is discouraged
    // by the protocol (deprecated in v2) so i feel like this wont be future proof to use,
    // my monitor however seems to assume SRGB rather than GAMMA22 for decoding.
    // @todo shader expects piecewise but compositor only supports gamma22 -> prob needs tf split in shader :/
    state.image_description_final.tf_named = WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
  }
  else if (state.supported_color_features.tf_gamma22 &&
           state.image_description_final.primaries_named == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB &&
           !state.hdrManager->colorInfo.hdrPossible)
  { // srgb
    state.image_description_final.tf_named = WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
  }
  else
  {
    fprintf(stderr,
            "Wayland surface can't find supported HDR transfer function! %i %i %i \n",
            state.image_description_final.primaries_named,
            state.supported_color_features.tf_srgb,
            state.supported_color_features.tf_gamma22);
  }

  if (state.hdrManager->colorInfo.hdrPossible)
  {
    uint csp = state.image_description_final.primaries_named;
    if (csp == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB) // scrgb
    {
      state.hdrManager->colorInfo.colorspaceName = "WAYLAND_SCRGB";
      state.hdrManager->colorInfo.colorspaceEnum = 16;
    }
    else if (csp == WP_COLOR_MANAGER_V1_PRIMARIES_BT2020) // hdr10 ST2084
    {
      state.hdrManager->colorInfo.colorspaceName = "WAYLAND_HDR10_ST2084";
      state.hdrManager->colorInfo.colorspaceEnum = 10;
    }
  }

  // actual swapchain update
  // creator needs to be recreated each time
  state.description_creator = wp_color_manager_v1_create_parametric_creator(state.color_manager);
  wp_image_description_creator_params_v1_set_tf_named(state.description_creator,
                                                      state.image_description_final.tf_named);
  wp_image_description_creator_params_v1_set_primaries_named(state.description_creator,
                                                             state.image_description_final.primaries_named);
  if (state.supported_color_features.feature_set_luminances)
  { // we compensate reference_lum for the shader math in hdr mode
    // Basically max_lum defines what is seen at 1.0, and reference_lum is the "anchor" and should be the system
    // reference to avoid further scaling
    uint32_t max_lum = state.hdrManager->colorInfo.hdrPossible ? 80 : state.image_description_final.reference_lum;
    wp_image_description_creator_params_v1_set_luminances(state.description_creator,
                                                          state.image_description_final.target_min_lum,
                                                          max_lum,
                                                          state.image_description_final.reference_lum);
  }
  if (state.supported_color_features.feature_set_mastering_display_primaries)
  {
    // set_mastering_display_primaries to avoid tonemapping from compositor
    wp_image_description_creator_params_v1_set_mastering_display_primaries(state.description_creator,
                                                                           state.image_description_final.target_rx,
                                                                           state.image_description_final.target_ry,
                                                                           state.image_description_final.target_gx,
                                                                           state.image_description_final.target_gy,
                                                                           state.image_description_final.target_bx,
                                                                           state.image_description_final.target_by,
                                                                           state.image_description_final.target_wx,
                                                                           state.image_description_final.target_wy);
    wp_image_description_creator_params_v1_set_mastering_luminance(state.description_creator,
                                                                   state.image_description_final.target_min_lum,
                                                                   state.image_description_final.target_max_lum);
  }
  state.hdrManager->colorInfo.monitorMinCLL = ((float)state.image_description_final.target_min_lum) / 10000.0;
  state.hdrManager->colorInfo.monitorMaxCLL = state.image_description_final.target_max_lum;
  state.hdrManager->colorInfo.osWhitepoint  = state.image_description_final.reference_lum;

  if (state.final_description != nullptr)
  {
    wp_image_description_v1_destroy(state.final_description);
    state.final_description = nullptr;
  }
  // This request destroys the wp_image_description_creator_params_v1 object
  state.final_description   = wp_image_description_creator_params_v1_create(state.description_creator);
  state.description_creator = nullptr;
  wp_image_description_v1_add_listener(state.final_description, &image_description_listener, &state);
}

} // namespace WaylandAPI

/*
@todo find cause of flickering when transition with forceSDR

//INFO: wayland needs server side decorations (SSD) as a requirement, or we have to manually draw them.
// GNOME does not support this (RIP)
//INFO OsRequirementsSupported() no libdecor, needs wayland and wayland protocols

*/