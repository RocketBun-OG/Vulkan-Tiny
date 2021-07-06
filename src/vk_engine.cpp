#include "vk_engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "vk_initializers.h"
#include "vk_types.h"

#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// PRIMARY FUNCTIONS
//------------------------------------------------------------------------

// Boot up the engine
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

// Spin up a vulkan context complete with everything needed to render
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

// Cleans up all the objects when the application is closed
void VulkanEngine::cleanup() {

  // Clean up the objects, so long as they exist.

  if (isInitialized) {
    // Wait for the GPU To finish doing stuff before we rip all the objects away from it
    vkDeviceWaitIdle(device);

    // Destroy everything that we added to the deletion queue
    mainDeletionQueue.flush();

    // destroy the render surface
    vkDestroySurfaceKHR(instance, displaySurface, nullptr);

    // Destroy the logical device
    vkDestroyDevice(device, nullptr);

    // Destroy the vulkan context
    vkDestroyInstance(instance, nullptr);

    // Destroy the SDL window
    SDL_DestroyWindow(window);
  }
}

// Draw to the screen
void VulkanEngine::draw() {
  // wait for the gpu to finish its work before starting to draw
  vkWaitForFences(device, 1, &getCurrentFrame().renderFence, true, UINT64_MAX);
  vkResetFences(device, 1, &getCurrentFrame().renderFence);
  // std::cerr << "\rthe current frame in flight is frame " << frameNumber %
  // MAX_FRAMES_IN_FLIGHT << " and the overall frame count is " << frameNumber << ' ' <<
  // std::flush;

  uint32_t swapChainImageIndex;
  VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT32_MAX,
                                          getCurrentFrame().presentSemaphore, nullptr,
                                          &swapChainImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Failed to acquire next image!");
  }

  // reset the command buffer so we can send new stuff to it
  if (vkResetCommandBuffer(getCurrentFrame().graphicsCommandBuffer, 0) != VK_SUCCESS) {
    throw std::runtime_error("Failed to reset command buffer!");
  }

  // rename for less typing
  VkCommandBuffer graphBuffer = getCurrentFrame().graphicsCommandBuffer;

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

  // tell it what area of screenspace to render to
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
  submit.pWaitSemaphores    = &getCurrentFrame().presentSemaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores    = &getCurrentFrame().renderSemaphore;

  submit.commandBufferCount = 1;
  submit.pCommandBuffers    = &graphBuffer;

  result = vkQueueSubmit(graphicsQueue, 1, &submit, getCurrentFrame().renderFence);

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
  presentInfo.pWaitSemaphores    = &getCurrentFrame().renderSemaphore;

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

// Primary loop of the engine.
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

// Get the extensions we need for the app to run
std::vector<const char *> VulkanEngine::getRequiredExtensions() {

  // get the extensions SDL needs
  uint32_t extensionCount{0};

  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);

  std::vector<const char *> extensions(extensionCount);
  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

  if (enableValidationLayers) {
    // adds the validation layers to the list if we've enabled them
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

// Create the vulkan context
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
// Get the gpus on the system
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

// Check to see if the GPU has all the features we require
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

// Check which queue families the device supports, and then mark down their indices
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
// Set up command pools for graphical commands, complete with buffers
void VulkanEngine::initGraphicsCommands() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(chosenGPU);

  // each command pool can only execute commmands for one queue family, and in this case
  // we've picked graphics
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
  poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // optional

  // generate a command pool for each frame in the buffer
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    // final creation of command pool
    if (vkCreateCommandPool(device, &poolInfo, nullptr,
                            &bufferFrames[i].graphicsCommandPool) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create command pool!");
    }

    // now attach a command buffer to it
    VkCommandBufferAllocateInfo commandBufferInfo{};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.pNext = nullptr;

    // tell the buffer which command pool it belongs to
    commandBufferInfo.commandPool = bufferFrames[i].graphicsCommandPool;
    // allocate a single command buffer
    commandBufferInfo.commandBufferCount = 1;
    // command level is Primary
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if (vkAllocateCommandBuffers(device, &commandBufferInfo,
                                 &bufferFrames[i].graphicsCommandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate graphical command buffer!");
    }
    mainDeletionQueue.pushFunction([=]() {
      vkDestroyCommandPool(device, bufferFrames[i].graphicsCommandPool, nullptr);
    });
  }
}

// Set up command pools for compute commands, complete with buffers.
void VulkanEngine::initComputeCommands() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(chosenGPU);

  // each command pool can only execute commmands for one queue family, and in this case
  // we've picked graphics
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();
  poolInfo.flags            = 0; // optional

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    // final creation of command pool
    if (vkCreateCommandPool(device, &poolInfo, nullptr,
                            &bufferFrames[i].computeCommandPool) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create command pool!");
    }

    // now attach a command buffer to it
    VkCommandBufferAllocateInfo commandBufferInfo{};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.pNext = nullptr;

    // tell the buffer which command pool it belongs to
    commandBufferInfo.commandPool = bufferFrames[i].computeCommandPool;
    // allocate a single command buffer
    commandBufferInfo.commandBufferCount = 1;
    // command level is Primary
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if (vkAllocateCommandBuffers(device, &commandBufferInfo,
                                 &bufferFrames[i].computeCommandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate graphical command buffer!");
    }
    mainDeletionQueue.pushFunction([=]() {
      vkDestroyCommandPool(device, bufferFrames[i].computeCommandPool, nullptr);
    });
  }
}

// Just calls the above two functions
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
// See if the GPU has minimum support for our swapchain
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

// Choose the surface color output format. Basically, pick a colorspace.
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

// Choose the vsync/present mode. see obsidian note "Vulkan present modes" for
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

// Set the resolution of the swapchain images, in pixels
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

// Build the swapchain
void VulkanEngine::createSwapChain() {

  swapChainSupport = querySwapChainSupport(chosenGPU);
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

  // add the swapchain to the deletion queue
  mainDeletionQueue.pushFunction(
      [=]() { vkDestroySwapchainKHR(device, swapChain, nullptr); });
}

// Wipe out the swapchain and its associated objects so we can rebuild it
void VulkanEngine::cleanupSwapChain() { mainDeletionQueue.flush(); }

// Rebuild the swapchain. Called when the window is resized or minimized.
void VulkanEngine::recreateSwapChain() {
  SDL_Event e;
  vkDeviceWaitIdle(device);

  // first set the new resolution of the window, so the swapchain doesn't get confused
  int width, height;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(chosenGPU, displaySurface,
                                            &swapChainSupport.capabilities);

  if (swapChainSupport.capabilities.currentExtent.width == 0 ||
      swapChainSupport.capabilities.currentExtent.height == 0) {
    // SDL_MinimizeWindow(window);
    // SDL_SetWindowSize(window,
    // swapChainSupport.capabilities.currentExtent.width,swapChainSupport.capabilities.currentExtent.height);
  }

  while (swapChainSupport.capabilities.currentExtent.width == 0 ||
         swapChainSupport.capabilities.currentExtent.height == 0) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(chosenGPU, displaySurface,
                                              &swapChainSupport.capabilities);
    SDL_WaitEvent(&e);
  }

  windowExtent.width =
      static_cast<uint32_t>(swapChainSupport.capabilities.currentExtent.width);
  windowExtent.height =
      static_cast<uint32_t>(swapChainSupport.capabilities.currentExtent.height);

  // std::cout << "window has been resized to " << windowX << " wide and " << windowY << "
  // tall\n";

  cleanupSwapChain();

  createSwapChain();
  createImageViews();
  createRenderPass();
  createPipelines();
  createFramebuffers();
  initCommands();
  createSyncStructures();
}
//-----------------------------------------------------------------------

// PIPELINE SETUP FUNCTIONS
//-----------------------------------------------------------------------
// Tell the vulkan context how to look at the images we make
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
    mainDeletionQueue.pushFunction(
        [=]() { vkDestroyImageView(device, swapChainImageViews[i], nullptr); });
  }
}

// Create a render pass
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

  mainDeletionQueue.pushFunction(
      [=]() { vkDestroyRenderPass(device, renderPass, nullptr); });
}

// Create framebuffers to put data into
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

    mainDeletionQueue.pushFunction(
        [=]() { vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr); });
  }
}
// Make semaphores and fences for each frame in the swapchain
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

  // make fences and semaphores for each frame in the buffer
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkCreateFence(device, &fenceInfo, nullptr, &bufferFrames[i].renderFence);
    // make semaphores for image availability and render status, and make fences for
    // cpu-gpu sync.

    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &bufferFrames[i].presentSemaphore);
    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &bufferFrames[i].renderSemaphore);

    mainDeletionQueue.pushFunction(
        [=]() { vkDestroyFence(device, bufferFrames[i].renderFence, nullptr); });

    mainDeletionQueue.pushFunction(
        [=]() { vkDestroySemaphore(device, bufferFrames[i].presentSemaphore, nullptr); });

    mainDeletionQueue.pushFunction(
        [=]() { vkDestroySemaphore(device, bufferFrames[i].renderSemaphore, nullptr); });
  }
}

// Set up the graphics pipeline(s)
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

  mainDeletionQueue.pushFunction(
      [=]() { vkDestroyPipeline(device, renderPipeline, nullptr); });
  mainDeletionQueue.pushFunction(
      [=]() { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); });
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

FrameData &VulkanEngine::getCurrentFrame() {
  return bufferFrames[frameNumber % MAX_FRAMES_IN_FLIGHT];
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