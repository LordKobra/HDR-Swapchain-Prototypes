// all other functions that arent relevant to hdr and bloat the vulkanApp.cpp
#include "vulkanApp.h"

namespace VulkanApp
{

// first: all sort of stuff to create and allocate buffers and other gpu objects
void App::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::SharingMode sharingMode,
                       vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer,
                       vk::raii::DeviceMemory &bufferMemory)
{
  vk::BufferCreateInfo bufferInfo{.size = size, .usage = usage, .sharingMode = sharingMode};
  buffer                                 = vk::raii::Buffer(device, bufferInfo);
  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo memoryAllocateInfo{
      .allocationSize  = memRequirements.size,
      .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
  };
  bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
  buffer.bindMemory(*bufferMemory, 0);
}

void App::copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
{
  vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
  commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
  endSingleTimeCommands(commandCopyBuffer);
}

void App::createLocalBuffer(vk::raii::Buffer &localBuffer, vk::raii::DeviceMemory &localMemory,
                            vk::BufferUsageFlags usageLocal, vk::DeviceSize bufferSize, const void *localData)
{
  vk::BufferUsageFlags    usage       = vk::BufferUsageFlagBits::eTransferSrc;
  vk::SharingMode         sharingMode = vk::SharingMode::eExclusive;
  vk::MemoryPropertyFlags properties =
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

  vk::raii::Buffer       stagingBuffer = nullptr;
  vk::raii::DeviceMemory stagingMemory = nullptr;
  createBuffer(bufferSize, usage, sharingMode, properties, stagingBuffer, stagingMemory);
  void *dataStaging = stagingMemory.mapMemory(0, bufferSize);
  memcpy(dataStaging, localData, bufferSize);
  stagingMemory.unmapMemory();

  usage       = usageLocal | vk::BufferUsageFlagBits::eTransferDst;
  sharingMode = vk::SharingMode::eExclusive;
  properties  = vk::MemoryPropertyFlagBits::eDeviceLocal;
  createBuffer(bufferSize, usage, sharingMode, properties, localBuffer, localMemory);
  copyBuffer(stagingBuffer, localBuffer, bufferSize);
}

uint32_t App::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
  vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
  {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
    {
      return i; // We can iterate the type count, since the typeFilter is also
                // indexed by typeCount
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void App::createCommandPool()
{
  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = queueFamilies.graphicsIndex
  };
  commandPool = vk::raii::CommandPool(device, poolInfo);
}

void App::createCommandBuffers()
{
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT
  };
  commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void App::createSyncObjects()
{
  renderFinishedSemaphores.reserve(swapChainImages.size());
  for (size_t i = 0; i < swapChainImages.size(); i++)
  {
    renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
  }
  presentCompleteSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
  timelineSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
  timelineValues.reserve(MAX_FRAMES_IN_FLIGHT);
  vk::SemaphoreTypeCreateInfoKHR semaphoreType{.semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0};
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    timelineSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{.pNext = &semaphoreType});
    timelineValues.emplace_back(0);
  }
}

void App::setupDebugMessenger()
{
  if (!enableValidationLayers)
    return;
  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
  vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                         vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                         vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
  vk::DebugUtilsMessengerCreateInfoEXT  DebugUtilsMessengerCreateInfo{
      .messageSeverity = severityFlags, .messageType = messageTypeFlags, .pfnUserCallback = debugCallback
  };
  debugMessenger = instance.createDebugUtilsMessengerEXT(DebugUtilsMessengerCreateInfo);
};

void App::createImageViews()
{
  swapChainImageViews.clear();
  swapChainImageViews.reserve(swapChainImages.size());

  for (auto image : swapChainImages)
  {
    swapChainImageViews.push_back(createImageView(image, swapChainImageFormat, vk::ImageAspectFlagBits::eColor, 1));
  }
}

vk::raii::ImageView App::createImageView(const vk::Image &image, vk::Format format, vk::ImageAspectFlags aspectFlags,
                                         uint32_t _mipLevels)
{
  vk::ImageViewCreateInfo viewInfo{
      .image            = image,
      .viewType         = vk::ImageViewType::e2D,
      .format           = format,
      .subresourceRange = {aspectFlags, 0, _mipLevels, 0, 1}
  };
  return vk::raii::ImageView(device, viewInfo);
}

vk::raii::CommandBuffer App::beginSingleTimeCommands()
{
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1
  };
  vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffer.begin(beginInfo);
  return commandBuffer;
}

void App::endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer)
{
  commandBuffer.end();
  vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
  graphicsQueue.submit(submitInfo, nullptr);
  graphicsQueue.waitIdle();
}

void App::transition_image_layout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                  vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
                                  vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
                                  vk::ImageAspectFlags imageAspectFlags)
{
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask        = srcStageMask,
      .srcAccessMask       = srcAccessMask,
      .dstStageMask        = dstStageMask,
      .dstAccessMask       = dstAccessMask,
      .oldLayout           = oldLayout,
      .newLayout           = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image               = image,
      .subresourceRange    = {
                              .aspectMask = imageAspectFlags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1
      }
  };
  vk::DependencyInfo dependencyinfo = {
      .dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier
  };
  commandBuffers[frameIndex].pipelineBarrier2(dependencyinfo);
}

[[nodiscard]] vk::raii::ShaderModule App::createShaderModule(const std::vector<char> &code) const
{
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t *>(code.data())
  };
  vk::raii::ShaderModule shaderModule{device, createInfo};
  return shaderModule;
}

bool App::findQueueFamilies(vk::raii::PhysicalDevice pDevice, uint32_t &graphicsIndex, uint32_t &presentIndex,
                            uint32_t &computeIndex)
{
  // find the index of the first queue family that supports graphics
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties = pDevice.getQueueFamilyProperties();

  bool foundGraphics = false, foundPresent = false;
  for (size_t i = 0; i < queueFamilyProperties.size(); i++)
  {
    bool supportsGraphicsAndCompute = uint32_t(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics &&
                                               queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute);
    bool supportsPresent            = pDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface);
    // find single family who supports both
    if (supportsGraphicsAndCompute && supportsPresent)
    {
      graphicsIndex = static_cast<uint32_t>(i);
      computeIndex  = static_cast<uint32_t>(i);
      presentIndex  = static_cast<uint32_t>(i);
      return true;
    }
    if (supportsGraphicsAndCompute && !foundGraphics)
    {
      graphicsIndex = static_cast<uint32_t>(i);
      computeIndex  = static_cast<uint32_t>(i);
      foundGraphics = true;
    }
    if (supportsPresent && !foundPresent)
    {
      presentIndex = static_cast<uint32_t>(i);
      foundPresent = true;
    }
  }

  return foundGraphics & foundPresent;
}

} // namespace VulkanApp