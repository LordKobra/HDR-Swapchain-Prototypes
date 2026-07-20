#include "vulkanApp.h"
#include "hdrManager.h"
#include "utility.h"
#include "vulkanImgui.h"

#include <set>

#include <vk_enum_string_helper.h>

namespace VulkanApp
{
// 1st Level
void App::initWindow()
{
  hdrManager = std::make_unique<HDR::HDRManager>();
#if defined(__linux__)
  windowAPI = std::make_unique<WaylandAPI::WaylandAPI>();
  windowAPI->init(hdrManager.get());
#elif defined(_WIN32)
  windowAPI = std::make_unique<Win32API::Win32Window>();
  windowAPI->init(L"Vulkan App", hdrManager.get());
#endif
}

void App::initVulkan()
{
  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
  createImageViews();
  createGraphicsPipeline();
  createCommandPool();
  createCommandBuffers();
  createSyncObjects();
  initImgui();
}

void App::mainLoop()
{
  while (windowAPI->dispatch() && !windowAPI->windowShouldClose())
  {
    if (windowAPI->getState().frame_done) // only draw new frame if compositor is ready
    {
      hdrManager->updateHDRVariables(); // @todo we only spam this because we lack a user button callback
      windowAPI->updateState();
      setFullscreenState();

      swapChainInvalid = (windowAPI->getState().swapChainInvalid || hdrManager->colorInfo.swapChainInvalid);
      if (swapChainInvalid)
      {
        recreateSwapChain();
      }
      // note: dispatch is a blocking function on wayland -> use fine-grained functions for unblocked function.
      drawFrame();
    }
  }
  device.waitIdle();
}

void App::cleanup()
{
  imguiInstance->cleanupImgui();
  imguiInstance.reset();
  imguiInstance = nullptr;

  cleanupSwapchain();

  windowAPI->terminate();
  windowAPI.reset();
  windowAPI = nullptr;

  hdrManager.reset();
  hdrManager = nullptr;
}

// 2nd Level
void App::initImgui()
{
  imguiInstance = std::make_unique<ImguiInstance>();
  imguiInstance->initImgui(this);
}

void App::createInstance()
{
  constexpr vk::ApplicationInfo appInfo{
      .pApplicationName   = "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName        = "No Engine",
      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion         = vk::ApiVersion14
  };

  // Validation Layers
  std::vector<char const *> requiredLayers;
  if (enableValidationLayers)
  {
    requiredLayers.assign(validationLayers.begin(), validationLayers.end());
  }

  auto layerProperties = context.enumerateInstanceLayerProperties();
  // Personal Note: Really? This lambda mess is supposed to beat a classic
  // two-iteration loop?
  if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const &requiredLayer) {
        return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) {
          return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
      }))
  {
    throw std::runtime_error("One or more required layers are not supported!");
  }

  // Extensions
  std::vector<const char *> extensions;
  // wayland extensions
  auto     windowExtensions     = windowAPI->getRequiredInstanceExtensions();
  uint32_t windowExtensionCount = windowExtensions.size();

  // Check if the required window extensions are supported by the Vulkan
  // implementation.
  auto extensionProperties = context.enumerateInstanceExtensionProperties();
  for (uint32_t i = 0; i < windowExtensionCount; i++)
  {
    if (std::ranges::none_of(extensionProperties,
                             [windowExtension = windowExtensions[i]](auto const &extensionProperty) {
                               return strcmp(extensionProperty.extensionName, windowExtension) == 0;
                             }))
    {
      throw std::runtime_error("Required window extension not supported: " + std::string(windowExtensions[i]));
    }
  }
  extensions.insert(extensions.end(), windowExtensions.begin(), windowExtensions.end());

  // HDR support
  if (isHDRInstanceSupported())
  {
    // Fullscreen exclusive
    extensions.push_back(vk::KHRGetPhysicalDeviceProperties2ExtensionName); // instance // or vulkan 1.1
    extensions.push_back(vk::KHRGetSurfaceCapabilities2ExtensionName);      // instance

    // HDR swapchain
    extensions.push_back(vk::KHRSurfaceExtensionName);             // instance
    extensions.push_back(vk::EXTSwapchainColorSpaceExtensionName); // instance
  }

  // Validation Layer extension
  if (enableValidationLayers)
  {
    extensions.push_back(vk::EXTDebugUtilsExtensionName);
  }

  // Create instance
  vk::InstanceCreateInfo createInfo{
      .pNext                   = nullptr,
      .pApplicationInfo        = &appInfo,
      .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
      .ppEnabledLayerNames     = requiredLayers.data(),
      .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data()
  };

  instance = vk::raii::Instance(context, createInfo);
}

void App::createSurface()
{
  windowAPI->createSurface(instance, surface);
}

void App::getDeviceFeatures()
{
  vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();

  bool dynamicRenderingSupported   = physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_3;
  bool timelineSemaphoresSupported = physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_2;

  auto availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();
  for (const auto &extension : availableExtensions)
  {
    if (strcmp(extension.extensionName, vk::KHRDynamicRenderingExtensionName) == 0)
    {
      dynamicRenderingSupported = true;
    }
    if (strcmp(extension.extensionName, vk::KHRTimelineSemaphoreExtensionName) == 0)
    {
      timelineSemaphoresSupported = true;
    }
  }

  hdrManager->colorInfo.hdrGraphicsSupported = isHDRInstanceSupported() && isHDRDeviceSupported();

  std::cout << "HDR Graphics API supported:    " << hdrManager->colorInfo.hdrGraphicsSupported << std::endl;
  std::cout << "Dynamic Rendering supported:   " << dynamicRenderingSupported << std::endl;
  std::cout << "Timeline Semaphores supported: " << timelineSemaphoresSupported << std::endl;
}

bool App::isDeviceSuitable(vk::raii::PhysicalDevice pDevice)
{
  bool     isSuitable       = pDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;
  auto     deviceProperties = pDevice.getProperties();
  auto     deviceFeatures   = pDevice.getFeatures();
  uint32_t graphicsDecoy, surfaceDecoy, computeDecoy = 0;
  auto     queueFamiliesFound = findQueueFamilies(pDevice, graphicsDecoy, surfaceDecoy, computeDecoy);
  isSuitable &= queueFamiliesFound;
  auto extensions          = pDevice.enumerateDeviceExtensionProperties();
  bool extensionsAvailable = true;
  for (auto const &extension : deviceExtensions)
  {
    auto extensionFound =
        std::ranges::any_of(extensions, [extension](auto const &ext) { return strcmp(ext.extensionName, extension); });
    extensionsAvailable = extensionsAvailable && extensionFound;
  }
  isSuitable &= extensionsAvailable;
  if (isSuitable)
  {
    return true;
  }

  return false;
}

void App::pickPhysicalDevice()
{
  auto devices = instance.enumeratePhysicalDevices();
  if (devices.empty())
  {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }
  for (const auto &device : devices)
  {
    if (isDeviceSuitable(device))
    {
      physicalDevice = device;
      getDeviceFeatures();
      break;
    }
  }
  if (physicalDevice == nullptr)
  {
    throw std::runtime_error("failed to find a suitable GPU");
  }

  return;
}

void App::createLogicalDevice()
{
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
  uint32_t                               presentIndex          = std::numeric_limits<uint32_t>::max();
  uint32_t                               graphicsIndex         = std::numeric_limits<uint32_t>::max();
  uint32_t                               computeIndex          = std::numeric_limits<uint32_t>::max();
  findQueueFamilies(physicalDevice, graphicsIndex, presentIndex, computeIndex);
  std::set<uint32_t> uniqueQueueFamilies = {presentIndex, graphicsIndex};
  queueFamilies =
      QueueFamilies{.graphicsIndex = graphicsIndex, .presentIndex = presentIndex, .computeIndex = computeIndex};
  float                                  queuePriority = 0.5;
  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  for (uint32_t queueFamily : uniqueQueueFamilies)
  {
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex = queueFamily, .queueCount = 1, .pQueuePriorities = &queuePriority
    };
    queueCreateInfos.push_back(deviceQueueCreateInfo);
  }

  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan12Features,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {
          {},
          {.shaderDrawParameters = true},
          {.timelineSemaphore = true},
          {.synchronization2 = true, .dynamicRendering = true},
          {.extendedDynamicState = true}
  };
#if defined(_WIN32)
  // HDR
  if (hdrManager->colorInfo.hdrSupported)
  {
    deviceExtensions.push_back(vk::EXTFullScreenExclusiveExtensionName);
  }
#endif
  vk::DeviceCreateInfo DeviceCreateInfo{
      .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos       = queueCreateInfos.data(),
      .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data()
  };
  device        = vk::raii::Device(physicalDevice, DeviceCreateInfo);
  graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
  presentQueue  = vk::raii::Queue(device, presentIndex, 0);
}

vk::SurfaceFormatKHR App::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
  bool useHDRFormat                   = hdrManager->colorInfo.hdrPossible;
  hdrManager->colorInfo.hdrGameActive = false;
  auto finalFormat                    = availableFormats[0];
  for (const auto &availableFormat : availableFormats)
  {
    // std::cout << string_VkFormat(VkFormat(availableFormat.format)) << "   "
    //           << string_VkColorSpaceKHR(VkColorSpaceKHR(availableFormat.colorSpace)) << std::endl;
    if (availableFormat.format == hdrPreferredFormat && availableFormat.colorSpace == hdrPreferredColorspace &&
        useHDRFormat)
    {
      finalFormat                         = availableFormat;
      hdrManager->colorInfo.hdrGameActive = true;
    }
    else if (availableFormat.format == sdrPreferredFormat && availableFormat.colorSpace == sdrPreferredColorspace)
    {
      finalFormat = availableFormat;
    }
  }

  updateHDRVariables(finalFormat.format, finalFormat.colorSpace);
  return finalFormat;
}

vk::PresentModeKHR App::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D App::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }
  int width, height;
  windowAPI->getFramebufferSize(width, height);
  return {
      std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
      std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
  };
}

void App::createSwapChain()
{
  auto                              surfaceCapabilities    = physicalDevice.getSurfaceCapabilitiesKHR(surface);
  std::vector<vk::SurfaceFormatKHR> availableFormats       = physicalDevice.getSurfaceFormatsKHR(surface);
  std::vector<vk::PresentModeKHR>   availablePresentModes  = physicalDevice.getSurfacePresentModesKHR(surface);
  auto                              swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
  auto                              swapChainPresentMode   = chooseSwapPresentMode(availablePresentModes);
  auto                              _swapChainExtent       = chooseSwapExtent(surfaceCapabilities);
  auto                              minImageCount          = std::max(3u, surfaceCapabilities.minImageCount);
  minImageCount          = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount)
                               ? surfaceCapabilities.maxImageCount
                               : minImageCount;
  minImageCountSwapchain = minImageCount;
  auto     imageSharingMode      = vk::SharingMode::eConcurrent;
  uint32_t queueFamilyIndexCount = 2;
  uint32_t queueFamilyIndices[]  = {queueFamilies.graphicsIndex, queueFamilies.presentIndex};
  auto     pQueueFamilyIndices   = queueFamilyIndices;

  if (queueFamilies.graphicsIndex == queueFamilies.presentIndex)
  {
    imageSharingMode      = vk::SharingMode::eExclusive;
    queueFamilyIndexCount = 0;
    pQueueFamilyIndices   = nullptr;
  }

  vk::SwapchainCreateInfoKHR swapChainCreateInfo{
      .flags                 = vk::SwapchainCreateFlagsKHR(),
      .surface               = surface,
      .minImageCount         = minImageCount,
      .imageFormat           = swapChainSurfaceFormat.format,
      .imageColorSpace       = swapChainSurfaceFormat.colorSpace,
      .imageExtent           = _swapChainExtent,
      .imageArrayLayers      = 1,
      .imageUsage            = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode      = imageSharingMode,
      .queueFamilyIndexCount = queueFamilyIndexCount,
      .pQueueFamilyIndices   = pQueueFamilyIndices,
      .preTransform          = surfaceCapabilities.currentTransform,
      .compositeAlpha        = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode           = swapChainPresentMode,
      .clipped               = true,
      .oldSwapchain          = nullptr
  };
#if defined(_WIN32)
  if (hdrManager->colorInfo.hdrSupported)
  {
    vk::SurfaceFullScreenExclusiveInfoEXT fseInfo{.fullScreenExclusive = vk::FullScreenExclusiveEXT::eDisallowed};
    swapChainCreateInfo.pNext = &fseInfo;
  }
#endif
  swapChain            = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
  swapChainImages      = swapChain.getImages();
  swapChainImageFormat = swapChainSurfaceFormat.format;
  swapChainExtent      = _swapChainExtent;
}

void App::cleanupSwapchain()
{
  swapChainImageViews.clear();
  swapChain = nullptr;
}

void App::recreateSwapChain()
{
  int width = 0, height = 0;
  windowAPI->getFramebufferSize(width, height);
  while ((width == 0 || height == 0) && windowAPI->dispatch() && !windowAPI->windowShouldClose())
  {
    windowAPI->updateState();
    windowAPI->getFramebufferSize(width, height);
  }
  device.waitIdle();
  if (windowAPI->windowShouldClose())
    return;

  cleanupSwapchain();
  createSwapChain();
  createImageViews();
  // need to update pipeline due to possible buffer format changes
  recreateGraphicsPipeline();
  imguiInstance->updateImgui();

  // sync timeline semaphore
  timelineValues[frameIndex] = timelineSemaphores[frameIndex].getCounterValue();

  swapChainInvalid = false;
}

void App::setFullscreenState()
{
// only on windows we set fullscreen state, because on wayland it is auto-managed by the window
#if defined(_WIN32)
  if (windowAPI->getState().is_fullscreen == windowAPI->getState().want_fullscreen)
    return; // we dont need to update

  std::cout << "Changing to fullscreen mode: " << windowAPI->getState().want_fullscreen << std::endl;
  windowAPI->setFullScreenState();
  windowAPI->getState().is_fullscreen = windowAPI->getState().want_fullscreen;
#endif
}

void App::createGraphicsPipeline()
{
  auto                   shaderDir    = executableDirectory + "/shaders/slang.spv";
  auto                   shaderCode   = Utility::readFile(shaderDir);
  vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);
  shaderCode.clear();

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"
  };
  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"
  };
  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  std::vector                        dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()
  };
  vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount   = 0,
      .pVertexBindingDescriptions      = NULL,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions    = NULL
  };
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable        = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode             = vk::PolygonMode::eFill,
      .cullMode                = vk::CullModeFlagBits::eBack,
      .frontFace               = vk::FrontFace::eClockwise,
      .depthBiasEnable         = vk::False,
      .depthBiasSlopeFactor    = 1.0,
      .lineWidth               = 1.0
  };
  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False
  };
  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable    = vk::False,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
  };
  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable   = vk::False,
      .logicOp         = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments    = &colorBlendAttachment
  };

  vk::PushConstantRange pushConstantRange{
      .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex,
      .offset     = 0,
      .size       = sizeof(PushConstants)
  };

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
      .setLayoutCount = 0, .pSetLayouts = NULL, .pushConstantRangeCount = 1, .pPushConstantRanges = &pushConstantRange
  };
  pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
      .colorAttachmentCount    = 1,
      .pColorAttachmentFormats = &swapChainImageFormat,
  };
  std::cout << "New Pipeline format: " << string_VkFormat(VkFormat(swapChainImageFormat)) << std::endl;
  vk::GraphicsPipelineCreateInfo pipelineInfo{
      .pNext               = &pipelineRenderingCreateInfo,
      .stageCount          = 2,
      .pStages             = shaderStages,
      .pVertexInputState   = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState      = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState   = &multisampling,
      .pColorBlendState    = &colorBlending,
      .pDynamicState       = &dynamicState,
      .layout              = pipelineLayout,
      .renderPass          = nullptr
  };

  graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

void App::recreateGraphicsPipeline()
{
  pipelineLayout   = nullptr;
  graphicsPipeline = nullptr;
  createGraphicsPipeline();
}

void App::recordCommandBuffer(uint32_t imageIndex)
{
  vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);

  vk::RenderingAttachmentInfo attachmentInfo = {
      .imageView   = swapChainImageViews[imageIndex],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eStore,
      .clearValue  = clearColor
  };

  vk::RenderingInfo renderingInfo = {
      .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
      .layerCount           = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments    = &attachmentInfo,
  };
  vk::Viewport viewport{
      0.0, 0.0, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0, 1.0
  };
  vk::Rect2D scissorRect{
      vk::Offset2D{0, 0},
      swapChainExtent
  };

  PushConstants pushConstants{
      .colorspaceEnum = hdrManager->colorInfo.colorspaceEnum,
      .formatEnum     = hdrManager->colorInfo.formatEnum,
      .formatBits     = hdrManager->colorInfo.formatBits,
      .formatSrgb     = hdrManager->colorInfo.formatSrgb,
      .appMaxCLL      = hdrManager->colorInfo.appMaxCLL,
      .appWhitepoint  = hdrManager->colorInfo.appWhitepoint,
      .leftHanded     = false,

      .screenWidth  = swapChainExtent.width,
      .screenHeight = swapChainExtent.height
  };

  commandBuffers[frameIndex].begin({});
  // Before starting rendering, transition the swapchain image to
  // COLOR_ATTACHMENT_OPTIMAL
  transition_image_layout(swapChainImages[imageIndex],
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          {}, // srcAccessMask (no need to wait for previous operations)
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                          vk::ImageAspectFlagBits::eColor);

  commandBuffers[frameIndex].beginRendering(renderingInfo);
  commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
  commandBuffers[frameIndex].setViewport(0, viewport);
  commandBuffers[frameIndex].setScissor(0, scissorRect);
  commandBuffers[frameIndex].pushConstants<PushConstants>(
      pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, {pushConstants});
  commandBuffers[frameIndex].draw(3, 1, 0, 0);

  imguiInstance->imguiRender();

  commandBuffers[frameIndex].endRendering();
  // After rendering, transition the swapchain image to PRESENT_SRC
  transition_image_layout(swapChainImages[imageIndex],
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                          {},                                                 // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                          vk::ImageAspectFlagBits::eColor);
  commandBuffers[frameIndex].end();
}

void App::drawFrame()
{
  uint64_t frameWaitValue      = timelineValues[frameIndex];
  uint64_t graphicsSignalValue = ++timelineValues[frameIndex];

  vk::SemaphoreWaitInfo waitInfo{
      .semaphoreCount = 1, .pSemaphores = &*timelineSemaphores[frameIndex], .pValues = &frameWaitValue
  };
  auto semaphoreResult = device.waitSemaphores(waitInfo, UINT64_MAX);
  if (semaphoreResult != vk::Result::eSuccess)
  {
    throw std::runtime_error("failed to wait for semaphore!");
  }
  auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, presentCompleteSemaphores[frameIndex], nullptr);
  if (result == vk::Result::eErrorOutOfDateKHR)
  {
    recreateSwapChain();
    return;
  }
  else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
  {
    assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
    throw std::runtime_error("failed to aquire swap chain image!");
  }

  imguiInstance->imguiStartFrame();
  recordCommandBuffer(imageIndex);

  vk::Semaphore signalSemaphores[] = {timelineSemaphores[frameIndex], renderFinishedSemaphores[imageIndex]};
  uint64_t      signalValues[]     = {graphicsSignalValue, graphicsSignalValue};
  vk::TimelineSemaphoreSubmitInfo graphicsTimelineInfo{
      .waitSemaphoreValueCount   = 1,
      .pWaitSemaphoreValues      = 0,
      .signalSemaphoreValueCount = 2,
      .pSignalSemaphoreValues    = signalValues
  };
  vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
  vk::SubmitInfo         submitInfo{
      .pNext                = &graphicsTimelineInfo,
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
      .pWaitDstStageMask    = &waitDestinationStageMask,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &*commandBuffers[frameIndex],
      .signalSemaphoreCount = 2,
      .pSignalSemaphores    = signalSemaphores
  };
  graphicsQueue.submit(submitInfo, nullptr);
  const vk::PresentInfoKHR presentInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
      .swapchainCount     = 1,
      .pSwapchains        = &*swapChain,
      .pImageIndices      = &imageIndex
  };

  windowAPI->preparePresent();

  result = presentQueue.presentKHR(presentInfo);
  if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR))
  {
    recreateSwapChain();
  }
  else
  {
    assert(result == vk::Result::eSuccess);
  }

  frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace VulkanApp