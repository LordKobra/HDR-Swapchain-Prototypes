#pragma once

#include "hdrManager.h"
#if defined(__linux__)
#include "linuxWayland.h"
#elif defined(_WIN32)
#include "win32Window.h"
#endif
#include <iostream>
#include <memory>

#include <vulkan/vulkan_hpp_macros.hpp> // optional: include Vulkan-Hpp configuration macros
#include <vulkan/vulkan_raii.hpp>

#include "vulkanImgui.h"

namespace VulkanApp
{

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                      vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                      void                                         *pUserData)
{
  std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
  return vk::False;
}
struct QueueFamilies
{
  uint32_t graphicsIndex = std::numeric_limits<uint32_t>::max();
  uint32_t presentIndex  = std::numeric_limits<uint32_t>::max();
  uint32_t computeIndex  = std::numeric_limits<uint32_t>::max();
};

struct PushConstants
{
  alignas(4) uint32_t colorspaceEnum;
  alignas(4) uint32_t formatEnum;
  alignas(4) uint32_t formatBits;
  alignas(4) uint32_t formatSrgb;
  alignas(4) float appMaxCLL;
  alignas(4) float appWhitepoint;
  alignas(4) vk::Bool32 leftHanded;

  alignas(4) uint32_t screenWidth;
  alignas(4) uint32_t screenHeight;
};

class App
{
public:
  uint32_t      minImageCountSwapchain;
  vk::Format    swapChainImageFormat = vk::Format::eUndefined;
  QueueFamilies queueFamilies;
  uint32_t      frameIndex = 0;

  void run(std::string path)
  {
    executableDirectory = path;
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }
  HDR::ColorInfo &getColorInfo()
  {
    return hdrManager->colorInfo;
  }
  vk::raii::Instance &getInstance()
  {
    return instance;
  }
  vk::raii::PhysicalDevice &getPhysicalDevice()
  {
    return physicalDevice;
  }
  vk::raii::Device &getDevice()
  {
    return device;
  }
  vk::raii::Queue &getGraphicsQueue()
  {
    return graphicsQueue;
  }
  std::vector<vk::Image> &getSwapChainImages()
  {
    return swapChainImages;
  }
  std::vector<vk::raii::CommandBuffer> &getCommandBuffers()
  {
    return commandBuffers;
  }
  vk::Extent2D &getSwapChainExtent()
  {
    return swapChainExtent;
  }
#if defined(_WIN32)
  Win32API::Win32Window *getWindowAPI()
  {
    return windowAPI.get();
  }
#elif defined(__linux__)
  WaylandAPI::WaylandAPI *getWindowAPI()
  {
    return windowAPI.get();
  }
#endif

private:
  std::string executableDirectory;
#if defined(_WIN32)
  std::unique_ptr<Win32API::Win32Window> windowAPI;
#elif defined(__linux__)
  std::unique_ptr<WaylandAPI::WaylandAPI> windowAPI;
#endif
  vk::raii::Context                    context;
  vk::raii::Instance                   instance       = nullptr;
  vk::raii::DebugUtilsMessengerEXT     debugMessenger = nullptr;
  vk::raii::SurfaceKHR                 surface        = nullptr;
  vk::raii::PhysicalDevice             physicalDevice = nullptr;
  vk::raii::Device                     device         = nullptr;
  vk::raii::Queue                      presentQueue   = nullptr;
  vk::raii::Queue                      graphicsQueue  = nullptr;
  vk::raii::SwapchainKHR               swapChain      = nullptr;
  std::vector<vk::Image>               swapChainImages;
  vk::Extent2D                         swapChainExtent;
  std::vector<vk::raii::ImageView>     swapChainImageViews;
  bool                                 swapChainInvalid = false;
  vk::raii::PipelineLayout             pipelineLayout   = nullptr;
  vk::raii::Pipeline                   graphicsPipeline = nullptr;
  vk::raii::CommandPool                commandPool      = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers;
  std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
  std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore>     timelineSemaphores;
  std::vector<uint64_t>                timelineValues;
  std::vector<const char *>            deviceExtensions       = {vk::KHRSwapchainExtensionName};
  const vk::Format                     sdrPreferredFormat     = vk::Format::eB8G8R8A8Unorm;
  const vk::ColorSpaceKHR              sdrPreferredColorspace = vk::ColorSpaceKHR::eSrgbNonlinear;
  const vk::Format                     hdrPreferredFormat     = vk::Format::eR16G16B16A16Sfloat;
#if defined(_WIN32)
  const vk::ColorSpaceKHR hdrPreferredColorspace = vk::ColorSpaceKHR::eExtendedSrgbLinearEXT;
#elif defined(__linux__)
  const vk::ColorSpaceKHR hdrPreferredColorspace = vk::ColorSpaceKHR::ePassThroughEXT;
#endif
  std::unique_ptr<ImguiInstance>   imguiInstance;
  std::unique_ptr<HDR::HDRManager> hdrManager;

  // 1st Level
  void initWindow();
  void initVulkan();
  void mainLoop();
  void cleanup();

  // 2nd Level
  void                 initImgui();
  void                 createInstance();
  void                 createSurface();
  void                 getDeviceFeatures();
  bool                 isDeviceSuitable(vk::raii::PhysicalDevice pDevice);
  void                 pickPhysicalDevice();
  void                 createLogicalDevice();
  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
  vk::PresentModeKHR   chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
  vk::Extent2D         chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);
  void                 createSwapChain();
  void                 cleanupSwapchain();
  void                 recreateSwapChain();
  void                 createGraphicsPipeline();
  void                 recreateGraphicsPipeline();
  void                 recordCommandBuffer(uint32_t imageIndex);
  void                 drawFrame();
  void                 setFullscreenState();

  // HDR
  bool isHDRInstanceSupported();
  bool isHDRDeviceSupported();
  void updateHDRVariables(vk::Format format, vk::ColorSpaceKHR csp);

  // Memory
  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::SharingMode sharingMode,
                    vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory);
  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size);
  void createLocalBuffer(vk::raii::Buffer &localBuffer, vk::raii::DeviceMemory &localMemory,
                         vk::BufferUsageFlags usageLocal, vk::DeviceSize bufferSize, const void *localData);
  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
  void     createCommandPool();
  void     createCommandBuffers();
  void     createSyncObjects();

  // Utility
  void                    setupDebugMessenger();
  vk::raii::ImageView     createImageView(const vk::Image &image, vk::Format format, vk::ImageAspectFlags aspectFlags,
                                          uint32_t _mipLevels);
  void                    createImageViews();
  vk::raii::CommandBuffer beginSingleTimeCommands();
  void                    endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer);
  void transition_image_layout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                               vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
                               vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
                               vk::ImageAspectFlags imageAspectFlags);
  [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const;
  bool findQueueFamilies(vk::raii::PhysicalDevice pDevice, uint32_t &graphicsIndex, uint32_t &presentIndex,
                         uint32_t &computeIndex);
};

} // namespace VulkanApp