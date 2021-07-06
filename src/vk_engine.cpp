#include "vk_engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "vk_initializers.h"
#include "vk_types.h"

#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// boot up the engine
void VulkanEngine::init() {

  // Initialize SDL and make a window with it
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  window =
      SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       windowExtent.width, windowExtent.height, window_flags);

  initVulkan();

  // if we reach this, everything went fine
  isInitialized = true;
}

// FUNDAMENTAL FUNCTIONS
//------------------------------------------------------------------------
void VulkanEngine::initVulkan() {
  createInstance();
  createSurface();
  pickPhysicalDevice();
  createDevice();
  createSwapChain();
  createImageViews();
  initCommands();
  createRenderPass();
  createFramebuffers();
  createSyncStructures();
  createPipelines();

  createMemAllocator();
}

// TODO: refactor this to cleanup using the push stack thing
void VulkanEngine::cleanup() {

  vkDeviceWaitIdle(device);
  // destroy all sync structures

  vmaDestroyBuffer(allocator, triangleMesh.vertexBuffer.memBuffer,
                   triangleMesh.vertexBuffer.allocation);

  vkDestroySemaphore(device, presentSemaphore, nullptr);
  vkDestroySemaphore(device, renderSemaphore, nullptr);
  vkDestroyFence(device, renderFence, nullptr);

  // destroy the command pools
  vkDestroyCommandPool(device, graphicalCommandPool, nullptr);
  vkDestroyCommandPool(device, computeCommandPool, nullptr);

  // destroy framebuffers
  for (auto framebuffer : swapChainFramebuffers) {
    vkDestroyFramebuffer(device, framebuffer, nullptr);
  }

  vkDestroyPipeline(device, renderPipeline, nullptr);

  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  // destroy the render pass
  vkDestroyRenderPass(device, renderPass, nullptr);

  // destroy all image views
  for (auto imageView : swapChainImageViews) {
    vkDestroyImageView(device, imageView, nullptr);
  }

  // destroy swapchain
  vkDestroySwapchainKHR(device, swapChain, nullptr);

  // kill the logical device
  vkDestroyDevice(device, nullptr);

  // destroy the render surface
  vkDestroySurfaceKHR(instance, displaySurface, nullptr);

  // destroy the vulkan context
  vkDestroyInstance(instance, nullptr);
  // delete the window if it actually exists
  if (isInitialized) {
    SDL_DestroyWindow(window);
  }
}

// TODO: make slightly less monolithic
void VulkanEngine::draw() {
  // wait for the gpu to finish its work before starting to draw
  vkWaitForFences(device, 1, &renderFence, true, UINT64_MAX);
  vkResetFences(device, 1, &renderFence);

  uint32_t swapChainImageIndex;
  VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT32_MAX, presentSemaphore,
                                          nullptr, &swapChainImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Failed to acquire next image!");
  }

  // reset the command buffer so we can send new stuff to it
  if (vkResetCommandBuffer(graphicalCommandBuffer, 0) != VK_SUCCESS) {
    throw std::runtime_error("Failed to reset command buffer!");
  }

  // rename for less typing
  VkCommandBuffer graphBuffer = graphicalCommandBuffer;

  // boot up the command buffer
  VkCommandBufferBeginInfo graphBeginInfo{};
  graphBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  graphBeginInfo.pNext = nullptr;

  // tell vulkan this buffer is only executing once per frame
  graphBeginInfo.pInheritanceInfo = nullptr;
  graphBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  if (vkBeginCommandBuffer(graphBuffer, &graphBeginInfo) != VK_SUCCESS) {
    throw std::runtime_error("Failed to start recording the command buffer!");
  }

  // set the blanking color
  VkClearValue blankValue;
  blankValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  // create render pass
  VkRenderPassBeginInfo rpInfo{};
  rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpInfo.pNext = nullptr;

  rpInfo.renderPass          = renderPass;
  rpInfo.renderArea.offset.x = 0;
  rpInfo.renderArea.offset.y = 0;

  rpInfo.renderArea.extent = windowExtent;

  rpInfo.framebuffer = swapChainFramebuffers[swapChainImageIndex];

  rpInfo.clearValueCount = 1;
  rpInfo.pClearValues    = &blankValue;

  // begin the renderpass
  vkCmdBeginRenderPass(graphBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  // bind the pipeline
  vkCmdBindPipeline(graphBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline);

  // its high noon
  vkCmdDraw(graphBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(graphBuffer);

  if (vkEndCommandBuffer(graphBuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to end the commmand buffer!");
  }

  // submit buffer to GPU
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = nullptr;

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.pWaitDstStageMask = &waitStage;

  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores    = &presentSemaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores    = &renderSemaphore;

  submit.commandBufferCount = 1;
  submit.pCommandBuffers    = &graphBuffer;

  result = vkQueueSubmit(graphicsQueue, 1, &submit, renderFence);

  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to submit image to queue!");
  }
  // display image to the screen

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;

  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains    = &swapChain;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = &renderSemaphore;

  presentInfo.pImageIndices = &swapChainImageIndex;

  result = vkQueuePresentKHR(graphicsQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {

    recreateSwapChain();

  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to present swapchain image!");
  }

  // increment number of frames since start.
  frameNumber++;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool bQuit = false;

  // main loop

  while (!bQuit) {

    // ask SDL for everything that's happened since the last frame
    while (SDL_PollEvent(&e) != 0) {
      // if you hit  the x button, close the damn window
      if (e.type == SDL_QUIT) {
        bQuit = true;
      }
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_a) {
          camPos[0] += .05;
        }
        if (e.key.keysym.sym == SDLK_d) {
          camPos[0] -= .05;
        }
        if (e.key.keysym.sym == SDLK_w) {
          camPos[2] += .05;
        }
        if (e.key.keysym.sym == SDLK_s) {
          camPos[2] -= .05;
        }
      }
    }

    // then draw the picture
    draw();
  }
}

// get the extensions we need for the app to run
std::vector<const char *> VulkanEngine::getRequiredExtensions() {

  // get the extensions SDL needs
  uint32_t extensionCount{0};

  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);

  std::vector<const char *> extensions(extensionCount);
  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

  if (enableValidationLayers) {
    // adds the debug utils layer to the end of the extensions list
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

// create the vulkan context
void VulkanEngine::createInstance() {
  // if the validation layers are turned on, but aren't available on the system,
  // panic.
  if (enableValidationLayers && !checkValidationSupport()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  // create a struct to hold application info
  VkApplicationInfo appInfo{};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName   = "Vulkan Adventure";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName        = "The Unibox";
  appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion         = VK_API_VERSION_1_0;

  // create a struct to hold critical vulkan info
  VkInstanceCreateInfo createInfo{};
  createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.pNext            = nullptr;

  // add the extensions SDL needs to the struct, plus the validation layers if necessary
  auto extensions                    = getRequiredExtensions();
  createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  // if the validation layers are turned on, add them to the struct. Otherwise,
  // tell the struct there are no layers turnd on.

  if (enableValidationLayers) {
    createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

  } else {
    createInfo.enabledLayerCount = 0;
  }

  // make the instance
  if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create instance!");
  }
}

// make the surface to present to
void VulkanEngine::createSurface() {
  if (SDL_Vulkan_CreateSurface(window, instance, &displaySurface) != SDL_TRUE) {
    std::cout << "can't create surface!";
  };
  SDL_SetWindowResizable(window, SDL_TRUE);
}
//------------------------------------------------------------------------

// GPU SELECTION FUNCTIONS
//------------------------------------------------------------------------
// get the gpus on the system
void VulkanEngine::pickPhysicalDevice() {
  // first, get a list of all the devices on the system.
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  // if we can't find any devices, throw a runtime error
  if (deviceCount == 0) {
    throw std::runtime_error("No GPUs with Vulkan support found!");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
  std::cout << "found " << deviceCount << " GPUs!" << std::endl;
  for (const auto GPU : devices) {
    if (isDeviceSuitable(GPU)) {
      chosenGPU = GPU;
      return;
    }
  }
  // if we make it out of the loop without finding a qualified gpu, throw a runtime error.
  throw std::runtime_error("No device found that supports all extensions!");
}

// check to see if the GPU has all the features we require
bool VulkanEngine::isDeviceSuitable(VkPhysicalDevice GPU) {
  // first, get the basic properties of the device
  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(GPU, &deviceProperties);

  // then, get its supported extensions
  uint32_t numExtensions{0};
  vkEnumerateDeviceExtensionProperties(GPU, nullptr, &numExtensions, nullptr);

  std::cout << "Your " << deviceProperties.deviceName << " supports " << numExtensions
            << " extensions!" << std::endl;

  std::vector<VkExtensionProperties> supportedExtensions(numExtensions);
  vkEnumerateDeviceExtensionProperties(GPU, nullptr, &numExtensions,
                                       supportedExtensions.data());

  // then, check if the gpu supports all the extensions we need

  // for every extension that the engine requires, check to see if the gpu has one in its
  // supported list that matches. If there is even one missing, the check fails and you
  // get a runtime error.
  for (const char *requiredExtension : requiredDeviceExtensions) {
    std::cout << "Checking GPU support for " << requiredExtension << "..." << std::endl;

    bool extensionFound = false;
    for (const auto &extension : supportedExtensions) {
      if (strcmp(extension.extensionName, requiredExtension) == 0) {
        std::cout << "extension " << requiredExtension << " found!" << std::endl;
        extensionFound = true;
        break;
      }
    }
    if (!extensionFound) {
      throw std::runtime_error("couldn't find all the extensions required!");
    }
  } // if we make out of the loop, the gpu supports all the extensions we asked for.

  // next, get its features, and pass them to the GPUFeatures variable, just in case.
  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(GPU, &deviceFeatures);

  GPUFeatures = deviceFeatures;

  // next, see if it has adequate support for making a swapchain
  bool swapChainAdequate{false};
  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(GPU);

  // if it has at least one present mode and surface format, we're good.
  swapChainAdequate =
      !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

  // finally, check to see if it supports all the queue families we need.
  QueueFamilyIndices indices = findQueueFamilies(GPU);

  // return the result of all our queries.
  return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
         indices.isComplete() && indices.isComplete();
}

// check which queue families the device supports, and then mark down their indices
VulkanEngine::QueueFamilyIndices VulkanEngine::findQueueFamilies(VkPhysicalDevice GPU) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount{0};
  // first, get the number of queue families the device supports
  vkGetPhysicalDeviceQueueFamilyProperties(GPU, &queueFamilyCount, nullptr);

  // then, get the properties of those families.
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(GPU, &queueFamilyCount, queueFamilies.data());

  uint32_t i = 0;
  for (const auto &queueFamily : queueFamilies) {
    // check to see if this gpu supports graphics. (lol)
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      // ignore these type conversion errors, they're bullshit.
      indices.graphicsFamily = i;
    }

    if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
      indices.computeFamily = i;
    }

    // then, check to see if this family supports drawing on the SDL surface.
    VkBool32 presentSupport{false};
    vkGetPhysicalDeviceSurfaceSupportKHR(GPU, i, displaySurface, &presentSupport);

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }
    ++i;
  }
  return indices;
}
//------------------------------------------------------------------------

// COMMAND INITIALIZATION FUNCTIONS
//------------------------------------------------------------------------
// set up a command pool for graphical commands, complete with buffers
void VulkanEngine::initGraphicsCommands() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(chosenGPU);

  // each command pool can only execute commmands for one queue family, and in this case
  // we've picked graphics
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
  poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // optional

  // final creation of command pool
  if (vkCreateCommandPool(device, &poolInfo, nullptr, &graphicalCommandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }

  // now attach a command buffer to it
  VkCommandBufferAllocateInfo commandBufferInfo{};
  commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferInfo.pNext = nullptr;

  // tell the buffer which command pool it belongs to
  commandBufferInfo.commandPool = graphicalCommandPool;
  // allocate a single command buffer
  commandBufferInfo.commandBufferCount = 1;
  // command level is Primary
  commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  if (vkAllocateCommandBuffers(device, &commandBufferInfo, &graphicalCommandBuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate graphical command buffer!");
  }
}

// set up a command pool for compute commands, complete with buffers
void VulkanEngine::initComputeCommands() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(chosenGPU);

  // each command pool can only execute commmands for one queue family, and in this case
  // we've picked graphics
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();
  poolInfo.flags            = 0; // optional

  // final creation of command pool
  if (vkCreateCommandPool(device, &poolInfo, nullptr, &computeCommandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }

  // now attach a command buffer to it
  VkCommandBufferAllocateInfo commandBufferInfo{};
  commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferInfo.pNext = nullptr;

  // tell the buffer which command pool it belongs to
  commandBufferInfo.commandPool = computeCommandPool;
  // allocate a single command buffer
  commandBufferInfo.commandBufferCount = 1;
  // command level is Primary
  commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  if (vkAllocateCommandBuffers(device, &commandBufferInfo, &computeCommandBuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate graphical command buffer!");
  }
}

void VulkanEngine::initCommands() {
  initGraphicsCommands();
  initComputeCommands();
}
//------------------------------------------------------------------------

// LOGICAL DEVICE CREATION
//------------------------------------------------------------------------
void VulkanEngine::createDevice() {
  // QUEUE CREATION
  // make a struct to hold queue info
  QueueFamilyIndices indices = findQueueFamilies(chosenGPU);
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::vector<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                               indices.presentFamily.value(),
                                               indices.computeFamily.value()};

  // for each queue family, fill in its struct and add it to the
  // queueCreateInfos vector.
  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    // fill the struct
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // add it to the infos vector
    queueCreateInfos.push_back(queueCreateInfo);
  }
  // END QUEUE CREATION

  // DEVICE CREATION
  // make a struct to hold all the device info
  VkDeviceCreateInfo deviceInfo{};
  deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceInfo.pNext = nullptr;

  // give it the queue info we just put together
  deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  deviceInfo.pQueueCreateInfos    = queueCreateInfos.data();
  deviceInfo.queueCreateInfoCount = 1;

  // tell the device we're not using any special features
  VkPhysicalDeviceFeatures emptyFeatures{};
  deviceInfo.pEnabledFeatures = &emptyFeatures;

  // tell the device what device extensions we're using
  deviceInfo.enabledExtensionCount =
      static_cast<uint32_t>(requiredDeviceExtensions.size());
  deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

  // tell the device whether or not we're using validation layers.
  if (enableValidationLayers) {
    deviceInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
    deviceInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    deviceInfo.enabledLayerCount = 0;
  }

  // actually create the device!
  if (vkCreateDevice(chosenGPU, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical device!");
  }

  // END DEVICE CREATION

  // attach the queues to their handles
  vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
  vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(device, indices.computeFamily.value(), 0, &computeQueue);
}
//------------------------------------------------------------------------

// SWAPCHAIN CREATION FUNCTIONS
//-----------------------------------------------------------------------
// see if the GPU has minimum support for our swapchain
VulkanEngine::SwapChainSupportDetails
VulkanEngine::querySwapChainSupport(VkPhysicalDevice GPU) {

  SwapChainSupportDetails details;
  // Get the surface capabilities of our device
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GPU, displaySurface, &details.capabilities);

  // Get the supported surface formats
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(GPU, displaySurface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(GPU, displaySurface, &formatCount,
                                         details.formats.data());
  }

  // Get the supported present modes
  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(GPU, displaySurface, &presentModeCount,
                                            nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(GPU, displaySurface, &presentModeCount,
                                              details.presentModes.data());
  }
  return details;
}

// choose the surface color output format. Basically, pick a colorspace.
VkSurfaceFormatKHR VulkanEngine::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

// choose the vsync/present mode. see obsidian note "Vulkan present modes" for
// details.
VkPresentModeKHR VulkanEngine::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {

  // triple buffer if we have it
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }
  // otherwise standard vsync
  return VK_PRESENT_MODE_FIFO_KHR;
}

// set the resolution of the swapchain images, in pixels
VkExtent2D VulkanEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {

  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)};

    // clamp the size to between the surface's specified max and min
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);

    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return actualExtent;
  }
}

void VulkanEngine::createSwapChain() {

  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(chosenGPU);
  // minimum amount of images the swapchain needs to function, +1 to avoid
  // driver hangups
  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

  // if there is a max # of images, and we've exceeded it, set it to the max instead
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {

    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  // get the basic specs of our swapchain
  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode     = chooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent                = chooseSwapExtent(swapChainSupport.capabilities);

  // fill in the struct for swapchain creation
  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface         = displaySurface;
  createInfo.minImageCount   = imageCount;
  createInfo.imageFormat     = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent     = extent;

  // this is always 1 unless you're doing stereoscopic 3D
  createInfo.imageArrayLayers = 1;

  // tell vulkan we're rendering these images directly
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices    = findQueueFamilies(chosenGPU);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  // determines whether or not image sharing is sloppy or exclusive
  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices   = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;       // optional
    createInfo.pQueueFamilyIndices   = nullptr; // optional
  }

  // don't do anything special to the image
  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  // ignore the alpha channel for blending
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  createInfo.presentMode = presentMode;
  // ignore the color of pixels that are obscured
  createInfo.clipped = VK_TRUE;
  // this is for recreating a swapchain when needed, as we need the "old" version to do
  // that.
  createInfo.oldSwapchain = VK_NULL_HANDLE;
  // almost there!

  if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create swapchain!");
  }

  // now we have to retrieve the swapchain images!
  // first, query the actual number of images (not the minimum, which we defined at the
  // beginning of the function)
  vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
  // then, we resize the vector to fit the actual number of images
  swapChainImages.resize(imageCount);
  // then, do the grad student shuffle!
  vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

  // store the format and extent for later
  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent      = extent;
}

void VulkanEngine::cleanupSwapChain() {
  // destroy framebuffers
  for (auto framebuffer : swapChainFramebuffers) {
    vkDestroyFramebuffer(device, framebuffer, nullptr);
  }

  vkDestroyCommandPool(device, graphicalCommandPool, nullptr);
  vkDestroyCommandPool(device, computeCommandPool, nullptr);

  vkDestroyPipeline(device, renderPipeline, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  // destroy the render pass
  vkDestroyRenderPass(device, renderPass, nullptr);

  // destroy all image views
  for (auto imageView : swapChainImageViews) {
    vkDestroyImageView(device, imageView, nullptr);
  }
  vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void VulkanEngine::recreateSwapChain() {

  vkDeviceWaitIdle(device);

  // first set the new resolution of the window, so the swapchain doesn't get confused
  int windowX{};
  int windowY{};
  SDL_GetWindowSize(window, &windowX, &windowY);
  windowExtent.width  = static_cast<uint32_t>(windowX);
  windowExtent.height = static_cast<uint32_t>(windowY);
  std::cout << "window has been resized to " << windowX << " wide and " << windowY
            << " tall\n";

  cleanupSwapChain();

  createSwapChain();
  createImageViews();
  createRenderPass();
  createPipelines();
  createFramebuffers();
  initCommands();
}
//-----------------------------------------------------------------------

// PIPELINE SETUP FUNCTIONS
//-----------------------------------------------------------------------
// tell the vulkan context how to look at the images we make
void VulkanEngine::createImageViews() {
  // make the imageviews list as big as the actual swapchain
  swapChainImageViews.resize(swapChainImages.size());

  for (size_t i = 0; i < swapChainImages.size(); ++i) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image    = swapChainImages[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format   = swapChainImageFormat;

    // this allows you to swizzle all the colors around if you want. These settings make
    // it do nothing.
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // describe the image's purpose and specs
    createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel   = 0;
    createInfo.subresourceRange.levelCount     = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount     = 1;

    // make the image view for real
    if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create image views!");
    }
  }
}

void VulkanEngine::createRenderPass() {
  // an attachment is basically a render target
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format  = swapChainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

  // clear the framebuffer to black before drawing a new frame
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  // store the rendered shit in memory so it can... be rendered
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  // not using stencils, so we don't give a shit about the load and store
  colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  // we don't care what the previous layout of the image was
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // we're using the image to be presented
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  // you can also bind this to compute. Very interesting...
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  // The index of the attachment in this array is directly referenced from the fragment
  // shader with the layout(location = 0) out vec4 outColor directive!
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  // connect the color attachment to the render pass
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments    = &colorAttachment;
  // connect the subpass to the render pass
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses   = &subpass;

  if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create render pass!");
  }
}

void VulkanEngine::createFramebuffers() {
  // make as many framebuffers as there are images
  VkFramebufferCreateInfo fbInfo{};
  fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fbInfo.pNext = nullptr;

  fbInfo.renderPass      = renderPass;
  fbInfo.attachmentCount = 1;
  fbInfo.width           = windowExtent.width;
  fbInfo.height          = windowExtent.height;
  fbInfo.layers          = 1;

  const uint32_t swapChainImageCount = swapChainImages.size();
  swapChainFramebuffers.resize(swapChainImageCount);

  for (int i = 0; i < swapChainImageCount; ++i) {
    fbInfo.pAttachments = &swapChainImageViews[i];
    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &swapChainFramebuffers[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create framebuffer!");
    }
  }
}

void VulkanEngine::createSyncStructures() {
  // resize the semaphore vectors to the amount of concurrent frames possible

  // grug fill in tiny semaphore struct
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = nullptr;
  semaphoreInfo.flags = 0;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  // turn the fence on by default so rendering can actually start.
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  vkCreateFence(device, &fenceInfo, nullptr, &renderFence);
  // make semaphores for image availability and render status, and make fences for cpu-gpu
  // sync.

  vkCreateSemaphore(device, &semaphoreInfo, nullptr, &presentSemaphore);
  vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderSemaphore);
}

void VulkanEngine::createPipelines() {
  VkShaderModule fragShader;
  if (!loadShaderModule("shaders/shader.frag.spv", &fragShader)) {
    std::cerr << "Error when building the fragment shader module!" << std::endl;
  } else {
    std::cerr << "No problems building the fragment shader!" << std::endl;
  }

  VkShaderModule vertShader;
  if (!loadShaderModule("shaders/shader.vert.spv", &vertShader)) {
    std::cerr << "Error when building the vertex shader module!" << std::endl;
  } else {
    std::cerr << "No problems building the vertex shader!" << std::endl;
  }

  // create the pipeline layout in its default config
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipelineLayoutCreateInfo();

  vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

  // now make the pipeline

  PipelineBuilder pipelineBuilder;

  // tell the pipeline about our shader stages
  pipelineBuilder.shaderStages.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShader));
  pipelineBuilder.shaderStages.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader));

  // tell the pipeline about vertex buffers and shit
  pipelineBuilder.vertexInputInfo = vkinit::vertexInputStateCreateInfo();
  // tell the pipeline how to put verts together
  pipelineBuilder.inputAssembly =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // build the viewport
  pipelineBuilder.viewport.x        = 0.0f;
  pipelineBuilder.viewport.y        = 0.0f;
  pipelineBuilder.viewport.width    = (float)windowExtent.width;
  pipelineBuilder.viewport.height   = (float)windowExtent.height;
  pipelineBuilder.viewport.minDepth = 0.0f;
  pipelineBuilder.viewport.maxDepth = 0.0f;

  // build the scissor (this one does nothing)
  pipelineBuilder.scissor.offset = {0, 0};
  pipelineBuilder.scissor.extent = windowExtent;

  // build the rasterizer
  pipelineBuilder.rasterizer = vkinit::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

  // no multisampling
  pipelineBuilder.multisampling = vkinit::multisampleStateCreateInfo();
  // no blending
  pipelineBuilder.colorBlendAttachment = vkinit::colorBlendAttachmentState();

  // attach the layout
  pipelineBuilder.pipelineLayout = pipelineLayout;

  renderPipeline = pipelineBuilder.buildPipeline(device, renderPass);

  // destroy the shader modules, as we don't need them once the pipeline is created
  vkDestroyShaderModule(device, fragShader, nullptr);
  vkDestroyShaderModule(device, vertShader, nullptr);
}

//-----------------------------------------------------------------------

// BUFFER STUFF
//-----------------------------------------------------------------------
// initialize the memory manager
void VulkanEngine::createMemAllocator() {
  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = chosenGPU;
  allocatorInfo.device         = device;
  allocatorInfo.instance       = instance;
  vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanEngine::loadMeshes() {
  triangleMesh.vertices.resize(3);
  triangleMesh.vertices[0].position = {1.f, 1.f, 0.0f};
  triangleMesh.vertices[1].position = {-1.f, 1.f, 0.0f};
  triangleMesh.vertices[2].position = {0.f, -1.f, 0.0f};

  // vertex colors
  triangleMesh.vertices[0].color = {1.f, 0.f, 0.0f};
  triangleMesh.vertices[1].color = {0.f, 1.f, 0.0f};
  triangleMesh.vertices[2].color = {0.f, 0.f, 1.0f};

  // we don't care about the vertex normals

  uploadMesh(triangleMesh);
}

void VulkanEngine::uploadMesh(Mesh &mesh) {
  // allocate a vertex buffer
  VkBufferCreateInfo bufferInfo{};

  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

  // total size of the buffer we're allocating, in bytes
  bufferInfo.size = mesh.vertices.size() * sizeof(Vertex);
  // specify that its a vertex buffer
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  // tell the library that the data is writable by cpu, readable by gpu
  VmaAllocationCreateInfo vmaAllocInfo{};
  vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  if (vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &mesh.vertexBuffer.memBuffer,
                      &mesh.vertexBuffer.allocation, nullptr) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate vertex buffer!");
  }

  void *data;

  vmaMapMemory(allocator, mesh.vertexBuffer.allocation, &data);
  memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

  vmaUnmapMemory(allocator, mesh.vertexBuffer.allocation);
}

//-----------------------------------------------------------------------

// SHADER HANDLING FUNCTIONS
//-----------------------------------------------------------------------
// load a shader from a file and create a module out of it.
bool VulkanEngine::loadShaderModule(const char *filePath,
                                    VkShaderModule *outShaderModule) {
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return false;
  }

  // get the filesize in bytes
  size_t fileSize = (size_t)file.tellg();

  // make a vector capable of holding the whole file
  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

  // move the cursor to the start of the file
  file.seekg(0);

  // read the whole file into the buffer. Reminder: chars are integral
  file.read((char *)buffer.data(), fileSize);

  // close the file
  file.close();

  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pNext = nullptr;

  // codesize must be in bytes, so multiply ints in buffer by size of int to know actual
  // size.
  createInfo.codeSize = buffer.size() * sizeof(uint32_t);
  createInfo.pCode    = buffer.data();

  // make sure everything went okay.
  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    return false;
  }
  *outShaderModule = shaderModule;
  return true;
}

// HERE BE DEBUG DRAGONS
// just tiny ones tho
//-----------------------------------------------------------------------
bool VulkanEngine::checkValidationSupport() {
  uint32_t layerCount;
  // first, list all of the available layers
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);

  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
  // for each layer in the validationLayers list, check through the
  // availableLayers vector to see if there is a match. If ALL of the layers
  // exist in the availableLayers vector, return true.
  for (const char *layerName : validationLayers) {
    bool layerFound = false;
    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }
    if (!layerFound) {
      std::cout << "Validation layer request mismatch." << std::endl;
      return false;
    }
  }
  std::cout << "Validation layers enabled and available." << std::endl;
  return true;
}
//-----------------------------------------------------------------------