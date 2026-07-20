#include "win32Window.h"
#include "utility.h"

#include <iostream>
#include <stdexcept>
#include <windows.h>

#include <imgui.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Win32API
{

LRESULT CALLBACK Win32Window::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_NCCREATE)
  {
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT *)lParam)->lpCreateParams);
    return TRUE;
  }

  return ((Win32Window *)GetWindowLongPtr(hWnd, GWLP_USERDATA))->_WindowProc(hWnd, message, wParam, lParam);
}
LRESULT Win32Window::_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    return true;
  switch (message)
  {
  case WM_DESTROY:
    PostQuitMessage(0); //@todo destroy only the window, not the process -> multithreading needed?
    return 0;
  case WM_SIZE:
    state.size_changed = true;
    return 0;
  case WM_DISPLAYCHANGE:
    state.display_changed = true;
    return 0;
  case WM_MOVE:
    state.window_moved = true;
    return 0;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

void Win32Window::init(std::wstring windowName, HDR::HDRManager *pHdrManager)
{
  m_hInstance                 = GetModuleHandle(nullptr);
  const wchar_t *WINDOW_NAME  = windowName.c_str();
  const wchar_t  CLASS_NAME[] = L"win32WindowClass";

  WNDCLASSEX wc    = {};
  wc.cbSize        = sizeof(WNDCLASSEX);
  wc.style         = CS_CLASSDC; //@todo style?
  wc.lpfnWndProc   = WindowProc;
  wc.hInstance     = m_hInstance;
  wc.lpszClassName = CLASS_NAME;
  SetProcessDPIAware();
  RegisterClassEx(&wc);

  m_hwnd = CreateWindow(wc.lpszClassName,
                        WINDOW_NAME,
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        WIDTH,
                        HEIGHT,
                        nullptr,
                        nullptr,
                        m_hInstance,
                        this);

  if (m_hwnd == NULL)
  {
    throw std::exception("failed to create Windows window!");
  }
  ShowWindow(m_hwnd, SW_SHOW);

  // HDR part

  hdrManager = pHdrManager;
  // Create a new factory
  if (CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pFactory)) != S_OK)
  {
    throw std::runtime_error("could not create DXGI Factory");
  }
  hdrManager->colorInfo.hdrWindowSupported = true; // @todo any conditions?
  // // @todo for now true, but should check windows 10 2004 update
  if (hdrManager->colorInfo.hdrWindowSupported)
  {
    isHDRMonitorActive();
  }

  state.frame_done = true;
}

void Win32Window::getFramebufferSize(int &width, int &height)
{
  RECT rect;
  if (GetClientRect(m_hwnd, &rect))
  {
    width  = rect.right;
    height = rect.bottom;
  }
}

void Win32Window::createSurface(vk::raii::Instance &instance, vk::raii::SurfaceKHR &vulkanSurface)
{
  vk::Win32SurfaceCreateInfoKHR surfaceCreateInfo{.flags = {}, .hinstance = m_hInstance, .hwnd = m_hwnd};
  vulkanSurface = instance.createWin32SurfaceKHR(surfaceCreateInfo);
};

bool Win32Window::dispatch()
{
  MSG msg = {};
  // Process any messages in the queue.
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // we should wait for wm_paint
  {
    if (msg.message == WM_QUIT)
    {
      state.window_closed = true;
      return false;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  state.frame_done = true;
  return true;
};

void Win32Window::preparePresent() // guaranteed frame draw
{
  state.frame_done       = false;
  state.swapChainInvalid = false;
}

void Win32Window::terminate()
{
  // @todo- nothing for now pog - hwnd and hinstance cleanup?
}

bool Win32Window::windowShouldClose()
{
  return state.window_closed;
}

void Win32Window::updateState()
{
  state.swapChainInvalid = state.swapChainInvalid || state.size_changed || state.display_changed;

  int width = 0, height = 0;
  getFramebufferSize(width, height);
  bool minimized = (width == 0 || height == 0); // avoid refresh on minimize
  if (hdrManager->colorInfo.hdrWindowSupported && !minimized &&
      (state.swapChainInvalid || state.window_moved || (m_pFactory->IsCurrent() == false)))
  {
    isHDRMonitorActive();
  }

  state.window_moved    = false;
  state.size_changed    = false;
  state.display_changed = false;
}

// @todo does this function really need to error out?
// @todo this function currently serves two purposes: if monitor is active and if hdr values update
bool Win32Window::isHDRMonitorActive()
{
  // Update if factory is outdated (something changed)
  if (m_pFactory->IsCurrent() == false)
  {
    if (CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pFactory)) != S_OK)
    {
      throw std::runtime_error("could not create DXGI Factory");
    }
  }

  // enumerate displays
  int                                   adapterIdx = 0;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
  if ((m_pFactory->EnumAdapters1(0, &pAdapter)) != S_OK)
  {
    throw std::runtime_error("could not enumerate DXGI adapter");
  }

  // get app window rect
  RECT appRect{};
  GetWindowRect(m_hwnd, &appRect);

  UINT                                i = 0;
  Microsoft::WRL::ComPtr<IDXGIOutput> currentOutput;
  Microsoft::WRL::ComPtr<IDXGIOutput> bestOutput;
  float                               bestIntersectArea = -1.0f;
  while (pAdapter->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
  {
    // current output rectangle
    DXGI_OUTPUT_DESC desc;
    if ((currentOutput->GetDesc(&desc)) != S_OK)
    {
      throw std::runtime_error("could not get DXGI output description");
    }
    RECT r = desc.DesktopCoordinates;

    // intersection, like in dx12 sample
    // https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HDR/src/D3D12HDR.cpp
    float intersectArea = fmax(0, fmin(appRect.right, r.right) - fmax(appRect.left, r.left)) *
                          fmax(0, fmin(appRect.bottom, r.bottom) - fmax(appRect.top, r.top));
    if (intersectArea > bestIntersectArea)
    {
      bestOutput        = currentOutput;
      bestIntersectArea = intersectArea;
    }

    i++;
  }

  // retrieve color space
  Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
  if ((bestOutput.As(&output6)) != S_OK)
  {
    throw std::runtime_error("could not convert DXGI display output");
  }

  DXGI_OUTPUT_DESC1 desc1;
  if ((output6->GetDesc1(&desc1)) != S_OK)
  {
    throw std::runtime_error("could not get main display DXGI info");
  }
  m_mainMonitor                           = std::wstring(desc1.DeviceName);
  hdrManager->colorInfo.monitorColorspace = desc1.ColorSpace;
  hdrManager->colorInfo.monitorMinCLL     = desc1.MinLuminance;
  hdrManager->colorInfo.monitorMaxCLL     = desc1.MaxLuminance;
  hdrManager->colorInfo.monitorMaxFALL    = desc1.MaxFullFrameLuminance;
  if (!obtainWhiteLevel())
  {
    hdrManager->colorInfo.osWhitepoint = 80.0f;
  }
  std::cout << "Display Colorspace:    " << desc1.ColorSpace << std::endl;
  std::cout << "Display Min Luminance: " << desc1.MinLuminance << std::endl;
  std::cout << "Display Max Luminance: " << desc1.MaxLuminance << std::endl;
  std::cout << "Display Max Full Frame Luminance: " << desc1.MaxFullFrameLuminance << std::endl;
  hdrManager->colorInfo.hdrMonitorActive = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) ||
                                           (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
  hdrManager->updateHDRVariables();
  return hdrManager->colorInfo.hdrMonitorActive;
}

bool Win32Window::obtainWhiteLevel()
{
  std::vector<DISPLAYCONFIG_PATH_INFO> paths;
  std::vector<DISPLAYCONFIG_MODE_INFO> modes;
  UINT32                               numPathElements = 0, numModeElements = 0;
  LONG                                 result = ERROR_SUCCESS;
  do
  {
    // get number of active display paths and modes
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathElements, &numModeElements) != ERROR_SUCCESS)
    {
      std::cerr << "could not get display config buffer sizes" << std::endl;
      return false;
    }
    paths.resize(numPathElements);
    modes.resize(numModeElements);
    result = QueryDisplayConfig(
        QDC_ONLY_ACTIVE_PATHS, &numPathElements, paths.data(), &numModeElements, modes.data(), nullptr);
    // It's possible that between the call to GetDisplayConfigBufferSizes and QueryDisplayConfig
    // that the display state changed, so loop on the case of ERROR_INSUFFICIENT_BUFFER.
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-querydisplayconfig#examples
  } while (result == ERROR_INSUFFICIENT_BUFFER);

  if (result != ERROR_SUCCESS)
  {
    std::cerr << "could not get display config buffer sizes" << std::endl;
    return false;
  }

  // find matching monitor
  for (auto &path : paths)
  {
    // Find the target (monitor) friendly name
    DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
    targetName.header.adapterId                 = path.targetInfo.adapterId;
    targetName.header.id                        = path.targetInfo.id;
    targetName.header.type                      = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    targetName.header.size                      = sizeof(targetName);
    result                                      = DisplayConfigGetDeviceInfo(&targetName.header);

    if (result != ERROR_SUCCESS)
    {
      continue;
    }

    /*/ Find the adapter device name
    DISPLAYCONFIG_ADAPTER_NAME adapterName = {};
    adapterName.header.adapterId           = path.targetInfo.adapterId;
    adapterName.header.type                = DISPLAYCONFIG_DEVICE_INFO_GET_ADAPTER_NAME;
    adapterName.header.size                = sizeof(adapterName);

    result = DisplayConfigGetDeviceInfo(&adapterName.header);

    // get gdi device name
    if (result != ERROR_SUCCESS)
    {
      continue;
    } */
    DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
    sourceName.header.type                      = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    sourceName.header.size                      = sizeof(sourceName);
    sourceName.header.adapterId                 = path.targetInfo.adapterId;
    sourceName.header.id                        = path.sourceInfo.id;

    result = DisplayConfigGetDeviceInfo(&sourceName.header);

    // also check if monitor is main monitor
    if (result != ERROR_SUCCESS || m_mainMonitor != std::wstring(sourceName.viewGdiDeviceName))
    {
      continue;
    }

    // Get SDR white level
    DISPLAYCONFIG_SDR_WHITE_LEVEL sdrWhiteLevel = {};
    sdrWhiteLevel.header.type                   = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
    sdrWhiteLevel.header.size                   = sizeof(sdrWhiteLevel);
    sdrWhiteLevel.header.adapterId              = path.targetInfo.adapterId;
    sdrWhiteLevel.header.id                     = path.targetInfo.id;

    if (DisplayConfigGetDeviceInfo(&sdrWhiteLevel.header) == ERROR_SUCCESS)
    {
      // SDR white level is in nits (80 nits in SDR mode)

      float sdrWhiteLevelNits            = float(sdrWhiteLevel.SDRWhiteLevel) / 1000.0f * 80.0f;
      hdrManager->colorInfo.osWhitepoint = sdrWhiteLevelNits;
      // "E.g. a value of 1000 would indicate that the SDR white level is 80 nits, while a value of 2000 would
      // indicate an SDR white level of 160 nits."
      //  https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level
      std::wcout << "Display: " << targetName.monitorFriendlyDeviceName << std::endl;
      // std::wcout << "Adapter: " << adapterName.adapterDevicePath << std::endl;
      // std::wcout << "source: " << sourceName.viewGdiDeviceName << std::endl;
      std::cout << "SDR White Level: " << sdrWhiteLevelNits << " nits\n";
      return true;
    }
  }
  return false;
}

// in dx12, the fullscreen transition is directed by the app, because it needs to respect fullscreen exclusive
bool Win32Window::setFullScreenState()
{
  if (state.want_fullscreen)
  {
    // change to fullscreen

    // Save the old window rect so we can restore it when exiting fullscreen
    // mode.
    GetWindowRect(m_hwnd, &m_windowRect);

    // Make the window borderless so that the client area can fill the screen.
    SetWindowLong(m_hwnd,
                  GWL_STYLE,
                  m_windowStyle & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

    // desktop coordinates
    // Get the desktop coordinates, thanks to https://github.com/TheRealMJP/EarlyZTest/
    POINT    point   = {};
    HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
    if (monitor == 0)
      return false;

    MONITORINFOEX info = {};
    info.cbSize        = sizeof(MONITORINFOEX);
    if (GetMonitorInfo(monitor, &info) == 0)
      return false;

    SetWindowPos(m_hwnd,
                 HWND_TOPMOST,
                 info.rcMonitor.left,
                 info.rcMonitor.top,
                 info.rcMonitor.right,
                 info.rcMonitor.bottom,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE);
    ShowWindow(m_hwnd, SW_MAXIMIZE);
    return true;
  }
  else // unset fullscreen
  {
    // Restore the window's attributes and size.
    SetWindowLong(m_hwnd, GWL_STYLE, m_windowStyle);

    SetWindowPos(m_hwnd,
                 HWND_NOTOPMOST,
                 m_windowRect.left,
                 m_windowRect.top,
                 m_windowRect.right - m_windowRect.left,
                 m_windowRect.bottom - m_windowRect.top,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE);

    ShowWindow(m_hwnd, SW_NORMAL);
  }
  return false;
}

} // namespace Win32API

// @todo When maximizing the window and then entering fullscreen, the maximized state forgot the dimensions of the
// original window and will stay maximized after unmaximzing
