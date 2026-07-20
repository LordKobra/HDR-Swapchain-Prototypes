#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "hdrManager.h"

#include <vulkan/vulkan_hpp_macros.hpp> // optional: include Vulkan-Hpp configuration macros
#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

#include <dxgi1_6.h>
#include <wrl.h>

namespace Win32API
{

struct winState
{
  // Window events
  bool window_closed   = false;
  bool window_moved    = false;
  bool size_changed    = false;
  bool display_changed = false;

  // user settings
  bool want_fullscreen = false;
  // app controlled
  bool is_fullscreen = false;
  // state
  bool swapChainInvalid = false;
  bool frame_done       = true;
};

class Win32Window
{
public:
  void init(std::wstring windowName, HDR::HDRManager *hdrManager);
  bool dispatch();
  void updateState();
  void preparePresent();
  bool setFullScreenState();

  void  createSurface(vk::raii::Instance &instance, vk::raii::SurfaceKHR &vulkanSurface);
  HWND &getHwnd()
  {
    return m_hwnd;
  };
  winState &getState()
  {
    return state;
  }
  void getFramebufferSize(int &width, int &height);
  bool windowShouldClose();
  void terminate();

  std::vector<const char *> &getRequiredInstanceExtensions()
  {
    return instanceExtensions;
  }

private:
  std::vector<const char *> instanceExtensions = {vk::KHRSurfaceExtensionName, vk::KHRWin32SurfaceExtensionName};
  Microsoft::WRL::ComPtr<IDXGIFactory4> m_pFactory;
  std::wstring                          m_mainMonitor = L"";
  HWND                                  m_hwnd;
  HINSTANCE                             m_hInstance;
  RECT                                  m_windowRect;
  static const UINT                     m_windowStyle = WS_OVERLAPPEDWINDOW;
  winState                              state{};
  HDR::HDRManager                      *hdrManager = nullptr;

  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT                 _WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  bool                    isHDRMonitorActive();
  bool                    obtainWhiteLevel();
};
} // namespace Win32API
