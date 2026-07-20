#include "vulkanApp.h"
#include <vk_enum_string_helper.h>

namespace VulkanApp
{
bool App::isHDRInstanceSupported()
{
  // Windows version
  std::vector<const char *> hdrInstanceExtensions;
  // Fullscreen exclusive
  hdrInstanceExtensions.push_back(vk::KHRGetPhysicalDeviceProperties2ExtensionName); // instance // or vulkan 1.1
  hdrInstanceExtensions.push_back(vk::KHRGetSurfaceCapabilities2ExtensionName);      // instance

  // HDR swapchain
  hdrInstanceExtensions.push_back(vk::KHRSurfaceExtensionName);             // instance
  hdrInstanceExtensions.push_back(vk::EXTSwapchainColorSpaceExtensionName); // instance

  auto availableInstanceExtensions = context.enumerateInstanceExtensionProperties();
  for (uint32_t i = 0; i < hdrInstanceExtensions.size(); i++)
  {
    if (std::ranges::none_of(availableInstanceExtensions,
                             [hdrExtension = hdrInstanceExtensions[i]](auto const &extensionProperty) {
                               return strcmp(extensionProperty.extensionName, hdrExtension) == 0;
                             }))
    {
      return false;
    }
  }
  return true;
}

bool App::isHDRDeviceSupported()
{
  // full-screen exclusive
  std::vector<const char *> hdrDeviceExtensions;

  hdrDeviceExtensions.push_back(vk::KHRSwapchainExtensionName); // device
#if defined(_WIN32)
  hdrDeviceExtensions.push_back(vk::EXTFullScreenExclusiveExtensionName); // device
#endif
  auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
  for (uint32_t i = 0; i < hdrDeviceExtensions.size(); i++)
  {
    if (std::ranges::none_of(availableDeviceExtensions,
                             [hdrExtension = hdrDeviceExtensions[i]](auto const &extensionProperty) {
                               return strcmp(extensionProperty.extensionName, hdrExtension) == 0;
                             }))
    {
      printf("%s\n", hdrDeviceExtensions[i]);
      return false;
    }
  }
  return true;
}

void App::updateHDRVariables(vk::Format format, vk::ColorSpaceKHR csp)
{

  hdrManager->colorInfo.formatName = string_VkFormat(VkFormat(format));

  if (format == vk::Format::eR16G16B16A16Sfloat)
  {
    hdrManager->colorInfo.formatEnum = 16;
    hdrManager->colorInfo.formatSrgb = false;
    hdrManager->colorInfo.formatBits = 16;
  }
  else if (format == vk::Format::eB8G8R8A8Unorm)
  {
    hdrManager->colorInfo.formatEnum = 0;
    hdrManager->colorInfo.formatSrgb = false;
    hdrManager->colorInfo.formatBits = 8;
  }
  else if (format == vk::Format::eB8G8R8A8Srgb)
  {
    hdrManager->colorInfo.formatEnum = 0;
    hdrManager->colorInfo.formatSrgb = true;
    hdrManager->colorInfo.formatBits = 8;
  }
  else if (format == vk::Format::eA2B10G10R10UnormPack32)
  {
    hdrManager->colorInfo.formatEnum = 10;
    hdrManager->colorInfo.formatSrgb = false;
    hdrManager->colorInfo.formatBits = 10;
  }
  else
  {
    throw std::runtime_error("could update format application variables");
  }

  if (csp == vk::ColorSpaceKHR::eExtendedSrgbLinearEXT) // scrgb
  {

    hdrManager->colorInfo.colorspaceName = string_VkColorSpaceKHR(VkColorSpaceKHR(csp));
    hdrManager->colorInfo.colorspaceEnum = 16;
  }
  else if (csp == vk::ColorSpaceKHR::eSrgbNonlinear) // srgb
  {

    hdrManager->colorInfo.colorspaceName = string_VkColorSpaceKHR(VkColorSpaceKHR(csp));
    hdrManager->colorInfo.colorspaceEnum = 0;
  }
  else if (csp == vk::ColorSpaceKHR::eHdr10St2084EXT) // hdr10 ST2084
  {

    hdrManager->colorInfo.colorspaceName = string_VkColorSpaceKHR(VkColorSpaceKHR(csp));
    hdrManager->colorInfo.colorspaceEnum = 10;
  }
  else if (csp == vk::ColorSpaceKHR::ePassThroughEXT)
  {
    // determined by wayland
  }
  else
  {
    throw std::runtime_error("could update format application variables");
  }
  hdrManager->updateHDRVariables();
};

} // namespace VulkanApp