#pragma once

#include <cstdint>
#include <string>

/*typedef struct VkHdrMetadataEXT {
    VkStructureType    sType;
    const void*        pNext;
    VkXYColorEXT       displayPrimaryRed;
    VkXYColorEXT       displayPrimaryGreen;
    VkXYColorEXT       displayPrimaryBlue;
    VkXYColorEXT       whitePoint;
    float              maxLuminance;
    float              minLuminance;
    float              maxContentLightLevel;
    float              maxFrameAverageLightLevel;
} VkHdrMetadataEXT;
typedef struct DXGI_HDR_METADATA_HDR10 {
  UINT16 RedPrimary[2];
  UINT16 GreenPrimary[2];
  UINT16 BluePrimary[2];
  UINT16 WhitePoint[2];
  UINT   MaxMasteringLuminance;
  UINT   MinMasteringLuminance;
  UINT16 MaxContentLightLevel;
  UINT16 MaxFrameAverageLightLevel;
} DXGI_HDR_METADATA_HDR10;
*/
namespace HDR
{
struct ColorInfo
{
  // system controlled
  bool     hdrWindowSupported = false;
  bool     hdrMonitorActive   = false;
  uint32_t monitorColorspace  = 0; //@todo unify between windows and linux with e.g. enum
  float    monitorMinCLL      = 0.0f;
  float    monitorMaxCLL      = 1000.0f;
  float    monitorMaxFALL     = 250.0f;
  float    osWhitepoint       = 203.0f;

  // Graphics API or system controlled
  std::string colorspaceName = "None";
  uint32_t    colorspaceEnum = 0;
  // @todo this needs to be split up into primaries and encoding and format bit depth
  // note 2: it should just enforce either scrgb, hdr10 or srgb support in wayland for and
  // keep it like this for now

  // Graphics API controlled
  bool        hdrGraphicsSupported = false;
  bool        hdrGameActive        = false;
  std::string formatName           = "None";
  uint32_t    formatEnum           = 0; // seems to be unused
  uint32_t    formatBits           = 8;
  bool        formatSrgb           = false;

  // user controlled
  float userMaxCLL     = 1000.0f;
  float userWhitepoint = 203.0f;
  bool  useOSValues    = true;
  bool  forceSdr       = false;

  // HDR Manager controlled
  bool  swapChainInvalid    = false;
  bool  constantsNeedUpdate = false;
  bool  hdrSupported        = false;
  bool  hdrPossible         = false;
  float appWhitepoint       = 203.0f;
  float appMaxCLL           = 1000.0f;
};

class HDRManager
{
public:
  ColorInfo colorInfo{};
  void      updateHDRVariables();
};
} // namespace HDR