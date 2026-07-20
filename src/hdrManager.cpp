#include "hdrManager.h"

namespace HDR
{

void HDRManager::updateHDRVariables()
{
  colorInfo.swapChainInvalid = false;
  // for now we expect push constants to update each frame
  colorInfo.constantsNeedUpdate = true;

  // user setting dependent updates
  colorInfo.hdrSupported = colorInfo.hdrWindowSupported && colorInfo.hdrGraphicsSupported;
  colorInfo.hdrPossible  = colorInfo.hdrSupported && colorInfo.hdrMonitorActive && !colorInfo.forceSdr;

  // going from hdr to sdr or the other way
  if ((!colorInfo.hdrPossible && colorInfo.hdrGameActive) || (colorInfo.hdrPossible && !colorInfo.hdrGameActive))
  {
    printf("swapchain needs update\n");
    colorInfo.swapChainInvalid = true;
  }

  colorInfo.appWhitepoint = colorInfo.useOSValues ? colorInfo.osWhitepoint : colorInfo.userWhitepoint;
  colorInfo.appMaxCLL     = colorInfo.useOSValues ? colorInfo.monitorMaxCLL : colorInfo.userMaxCLL;

  // @todo add appropriate tests and values for user settings
  // Some tests cap maxcll to monitormaxcll? -> monitorvalue could be wrong some ppl suggest
  colorInfo.appMaxCLL     = std::max(colorInfo.appMaxCLL, 5.0f);
  colorInfo.appWhitepoint = std::min(colorInfo.appWhitepoint, colorInfo.appMaxCLL);
}

} // namespace HDR