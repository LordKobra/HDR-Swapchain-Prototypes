#include "dxApp.h"
#include "hdrManager.h"
#include <stdexcept>

namespace DXApp
{

void App::updateHDRVariables(DXGI_FORMAT format, DXGI_COLOR_SPACE_TYPE csp)
{
  if (format == DXGI_FORMAT_R16G16B16A16_FLOAT)
  {
    hdrManager->colorInfo.formatName = "DXGI_FORMAT_R16G16B16A16_FLOAT";
    hdrManager->colorInfo.formatEnum = 16;
    hdrManager->colorInfo.formatSrgb = false;
    hdrManager->colorInfo.formatBits = 16;
  }
  else if (format == DXGI_FORMAT_R8G8B8A8_UNORM)
  {
    hdrManager->colorInfo.formatName = "DXGI_FORMAT_R8G8B8A8_UNORM";
    hdrManager->colorInfo.formatEnum = 0;
    hdrManager->colorInfo.formatSrgb = false;
    hdrManager->colorInfo.formatBits = 8;
  }
  else if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
  {
    hdrManager->colorInfo.formatName = "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
    hdrManager->colorInfo.formatEnum = 0;
    hdrManager->colorInfo.formatSrgb = true;
    hdrManager->colorInfo.formatBits = 8;
  }
  else if (format == DXGI_FORMAT_R10G10B10A2_UNORM)
  {
    hdrManager->colorInfo.formatName = "DXGI_FORMAT_R10G10B10A2_UNORM";
    hdrManager->colorInfo.formatEnum = 10;
    hdrManager->colorInfo.formatSrgb = false;
    hdrManager->colorInfo.formatBits = 10;
  }
  else
  {
    throw std::runtime_error("could update format application variables");
  }

  if (csp == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709) // scrgb
  {
    hdrManager->colorInfo.colorspaceName = "DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709";
    hdrManager->colorInfo.colorspaceEnum = 16;
  }
  else if (csp == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) // srgb
  {
    hdrManager->colorInfo.colorspaceName = "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709";
    hdrManager->colorInfo.colorspaceEnum = 0;
  }
  else if (csp == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) // hdr10 ST2084
  {
    hdrManager->colorInfo.colorspaceName = "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020";
    hdrManager->colorInfo.colorspaceEnum = 10;
  }
  else
  {
    throw std::runtime_error("could update format application variables");
  }

  hdrManager->updateHDRVariables();
}
} // namespace DXApp