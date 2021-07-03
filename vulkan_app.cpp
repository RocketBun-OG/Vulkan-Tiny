#include "vulkan_app.hpp"

void vulkan_app::run() {
  initWindow();
  initVulkan();
  mainLoop();
  cleanup();
}

void vulkan_app::initWindow() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  window = glfwCreateWindow(WIDTH, HEIGHT, "This is a lot of work", nullptr, nullptr);
}

void vulkan_app::initVulkan() {
  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
  createImageViews();
  createRenderPass();
  createGraphicsPipeline();
}

void vulkan_app::createSurface() {
  if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface!");
  }
}

std::vector<char> vulkan_app::readFile(const std::string &filename) {
  // read it in as binary, and start from the back so we can get the size of the file
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file!");
  }
  // since the current read position is the end, fileSize is the size of the binary!
  size_t fileSize = (size_t)file.tellg();
  // make a char buffer the size of the file
  std::vector<char> buffer(fileSize);

  // seek to the beginning of the file
  file.seekg(0);
  // read the entire thing into the buffer
  file.read(buffer.data(), fileSize);

  file.close();
  return buffer;
}

// gets the details of the graphics card's swapchain support
vulkan_app::SwapChainSupportDetails
vulkan_app::querySwapChainSupport(VkPhysicalDevice device) {

  SwapChainSupportDetails details;
  // surface deets grabbing
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  // format grabbing
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         details.formats.data());
  }

  // present mode grabbing
  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &formatCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                              details.presentModes.data());
  }

  return details;
}

void vulkan_app::createLogicalDevice() {
  // queue creation bullshit
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::vector<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                               indices.presentFamily.value()};

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

  VkPhysicalDeviceFeatures deviceFeatures{};

  // set up the device struct
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  // tell it about the queue creation info we just set up

  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos    = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = 1;

  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();
  // add the validation layers, if they exist
  if (enableValidationLayers) {
    createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  // actually create the device!
  if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical device!");
  }
  // stick the queues to their handles
  vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
}

void vulkan_app::createSwapChain() {

  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
  // minimum amount of images the swapchain needs to function, +1 to avoid
  // driver hangups
  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

  // if there is a max # of images, and we've exceeded it, set it to the max instead
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {

    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode     = chooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent                = chooseSwapExtent(swapChainSupport.capabilities);

  // struct time!
  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface         = surface;
  createInfo.minImageCount   = imageCount;
  createInfo.imageFormat     = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent     = extent;
  // this is always 1 unless you're doing stereoscopic 3D
  createInfo.imageArrayLayers = 1;
  // tell vulkan we're rendering these images directly
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices    = findQueueFamilies(physicalDevice);
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

  if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create swapchain!");
  }

  // now we have to retrieve the swapchain images!
  // first, query the actual number of images (not the minimum, which we defined at the
  // beginning of the function)
  vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
  // then, we resize the vector to fit the actual number of images
  swapChainImages.resize(imageCount);
  // then, do the grad student shuffle!
  vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

  // store the format and extent for later
  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent      = extent;
}

void vulkan_app::createImageViews() {
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
    if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create image views!");
    }
  }
}

void vulkan_app::createGraphicsPipeline() {
  auto vertShaderCode = readFile("shaders/shader.vert.spv");
  auto fragShaderCode = readFile("shaders/shader.frag.spv");

  VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

  // assign the shaders to certain pipeline stages
  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  // this tells the shader what the entry point is. In this case, it's just the main() in
  // the .vert/.frag
  vertShaderStageInfo.pName = "main";

  // do the same shit again but for the frag shader
  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName  = "main";

  // shove the stage info into an array for later
  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  // tell the pipeline there's no vertex data to load
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount   = 0;
  vertexInputInfo.pVertexBindingDescriptions      = nullptr; // optional
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions    = nullptr; // optional

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  // tell the pipeline how to assemble the vertices. In this case, just tell it to make
  // triangles out of them
  inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // VK_TRUE here is only useful for strip topologies
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // set up a stock viewport
  VkViewport viewport{};
  // viewport position
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  // viewport size
  viewport.width  = (float)swapChainExtent.width;
  viewport.height = (float)swapChainExtent.height;
  // viewport z-depth
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  // set the scissor to not cover anything
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapChainExtent;

  // meld the scissor and the viewport into a single viewport state
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports    = &viewport;
  viewportState.scissorCount  = 1;
  viewportState.pScissors     = &scissor;

  // rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  // get rid of fragments that are beyond the clipping planes
  rasterizer.depthClampEnable = VK_FALSE;
  // if you set this to true, the framebuffer never receives any geometry
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  // tell the rasterizer how to generate fragments for geometry
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  // does what it says it does
  rasterizer.lineWidth = 1.0f;

  // enable backface culling
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  // determines vertex order that makes something front-facing
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

  // disable depth biasing
  rasterizer.depthBiasEnable         = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f; // Optional
  rasterizer.depthBiasClamp          = 0.0f; // Optional
  rasterizer.depthBiasSlopeFactor    = 0.0f; // Optional

  // leave multisampling disabled for now
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable   = VK_FALSE;
  multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading      = 1.0f;     // Optional
  multisampling.pSampleMask           = nullptr;  // Optional
  multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
  multisampling.alphaToOneEnable      = VK_FALSE; // Optional

  // we only have one framebuffer, so we don't need to blend it.
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType         = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments    = &colorBlendAttachment;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts    = nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges    = nullptr;

  // Make the pipeline layout, panic if it fails.
  if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr,
                             &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout!");
  }

  // compilation of the shaders to machine code doesn't happen until the graphics pipeline
  // gets created, so we can throw these away without consequence
  vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
}

void vulkan_app::createRenderPass() {
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

  // TODO: Understand attachment references and subpasses. Brain hurty, no sleepy.
  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  // you can also bind this to compute. Very interesting...
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &colorAttachmentRef;
}

VkShaderModule vulkan_app::createShaderModule(const std::vector<char> code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();

  // have to re-cast the code to uint32 for the struct to accept it
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  // make the fuckin' shader!

  if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create shader module!");
  }
  return shaderModule;
}

void vulkan_app::pickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  if (deviceCount == 0) {
    throw std::runtime_error("No GPUs with Vulkan support found!");
  }
  // boilerplate, starting to understand this particular flow
  std::vector<VkPhysicalDevice> deviceList(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

  for (const auto &device : deviceList) {
    if (isDeviceSuitable(device)) {
      physicalDevice = device;
      break;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to find a suitable gpu!");
  }
}

bool vulkan_app::isDeviceSuitable(VkPhysicalDevice device) {
  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);

  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
  // fun fact: tabnine is starting to get it too!

  QueueFamilyIndices indices = findQueueFamilies(device);

  bool extensionsSupported = checkDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
    swapChainAdequate =
        !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
  }

  return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool vulkan_app::checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

  // are you familiar with this pattern yet? :)
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                           deviceExtensions.end());

  // for each extension in the list of available extensions, erase it from the
  // list of required extensions. If there are any required extensions left at
  // the end, it returns false. If not, it returns true. Nifty, eh?
  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

VkSurfaceFormatKHR vulkan_app::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {

  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }
  // if srgb isnt there, return the first format suggested.
  return availableFormats[0];
}

// choose the vsync/present mode. see obsidian note "Vulkan present modes" for
// details.
VkPresentModeKHR vulkan_app::chooseSwapPresentMode(
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
VkExtent2D vulkan_app::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {

  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

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

vulkan_app::QueueFamilyIndices vulkan_app::findQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;
  // do ya get it yet?
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilyProperties.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilyProperties) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    // check to see if this queue family supports drawing on the surface
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }
    i++;
  }
  return indices;
}

void vulkan_app::mainLoop() {
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }
}

void vulkan_app::cleanup() {
  // kill the pipeline layout
  vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
  // kill all the image views we made in createImageViews()
  for (auto imageView : swapChainImageViews) {
    vkDestroyImageView(logicalDevice, imageView, nullptr);
  }
  // kill the swapchain
  std::cout << "where'd my swapchain go, dad?";
  vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
  // kill the logical device
  vkDestroyDevice(logicalDevice, nullptr);
  // if we're in debug mode, kill the debugger.
  if (enableValidationLayers) {
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
  }

  // ALWAYS DESTROY THE SURFACE BEFORE THE INSTANCE!!
  vkDestroySurfaceKHR(instance, surface, nullptr);

  // delete the vulkan instance
  vkDestroyInstance(instance, nullptr);
  // destroy the window
  glfwDestroyWindow(window);
  // shut off glfw
  glfwTerminate();
}

void vulkan_app::createInstance() {
  // if the validation layers are turned on, but aren't available on the system,
  // panic.
  if (enableValidationLayers && !checkValidationLayerSupport()) {
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

  // find out how many extensions glfw needs and store the info
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

  // if this goes well, the vulkan instance is accessible via the (very
  // creatively named) instance variable
  if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw std::runtime_error("failed to create instance!");
  }
}

// HERE BE DEBUG DRAGONS
//----------------------------------------------------------------
bool vulkan_app::checkValidationLayerSupport() {
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

void vulkan_app::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo       = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  // this is so verbose it's absolutely fucking insane
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  // pointer to the callback function
  createInfo.pfnUserCallback = debugCallback;
}

std::vector<const char *> vulkan_app::getRequiredExtensions() {
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  // makes a vector of extensions. I don't really understand what the second
  // param is doing.
  std::vector<const char *> extensions(glfwExtensions,
                                       glfwExtensions + glfwExtensionCount);

  if (enableValidationLayers) {
    // adds the debug utils layer to the end of the extensions list
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_app::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
  // the -> is basically like pCallbackData.pMessage() but with a pointer. No
  // idea why it's like that, but it is.
  std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
  return false;
}

void vulkan_app::setupDebugMessenger() {
  if (!enableValidationLayers)
    return;
  // populate the debug messenger with an actual debug messenger
  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  populateDebugMessengerCreateInfo(createInfo);
  // if that fails, panic
  if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to set up the debug messenger!");
  }
}

// this is such a pain in the ass oh my GOD
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}
//----------------------------------------------------------------