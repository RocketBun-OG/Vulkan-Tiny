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
  createFramebuffers();
  createCommandPool();
  createCommandBuffers();
  createSyncObjects();
}

// make a surface to draw on
void vulkan_app::createSurface() {
  if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface!");
  }
}

// used for reading shaders into a usable format
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
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount); // 1887432816
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                              details.presentModes.data());
  }

  return details;
}

// create a vulkan context from the gpu
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

// make the swapchain, the holy grail of image presentation
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

// tell the context what the fuck we're using the images for
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

// bundle our render operations into a renderpass
void vulkan_app::createRenderPass() {
  // make a color buffer
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
  // The index of the attachment in this array is directly referenced from the fragment
  // shader with the layout(location = 0) out vec4 outColor directive!
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &colorAttachmentRef;

  // specify memory and execution dependencies between subpasses. Have to do this because
  // of the transition at the start of render time from no image -> image
  VkSubpassDependency dependency{};
  // index of the dependency
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  // index of the dependent subpass
  dependency.dstSubpass = 0;

  // wait on the swapchain to finish reading the image before allowing the dependent to
  // access it
  dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;

  // what should the dependency wait to do? in this case, coloring!
  dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments    = &colorAttachment;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  // what dependencies do we got?
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  // create the render pass, panic if we can't
  if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

// make the chonky boi
void vulkan_app::createGraphicsPipeline() {
  auto vertShaderCode = readFile("../shaders/shader.vert.spv");
  auto fragShaderCode = readFile("../shaders/shader.frag.spv");

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

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;

  // inhale all that shit we just made into the pipeline struct
  pipelineInfo.pStages             = shaderStages;
  pipelineInfo.pVertexInputState   = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState      = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState   = &multisampling;
  pipelineInfo.pDepthStencilState  = nullptr; // Optional
  pipelineInfo.pColorBlendState    = &colorBlending;
  pipelineInfo.pDynamicState       = nullptr; // Optional

  pipelineInfo.layout = pipelineLayout;

  pipelineInfo.renderPass = renderPass;
  // index of the subpass where the pipeline will be used
  pipelineInfo.subpass = 0;
  // TODO: look at basePipelineHandle
  if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                &graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create graphics pipeline!");
  }
  // compilation of the shaders to machine code doesn't happen until the graphics pipeline
  // gets created, so we can throw these away without consequence
  vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
}

// create framebuffers to hold each swapchain image
void vulkan_app::createFramebuffers() {
  swapChainFramebuffers.resize(swapChainImageViews.size());

  for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
    // iterate through the image views and make framebuffers for them
    VkImageView attachments[]{swapChainImageViews[i]};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments    = attachments;
    framebufferInfo.width           = swapChainExtent.width;
    framebufferInfo.height          = swapChainExtent.height;
    framebufferInfo.layers          = 1;

    if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr,
                            &swapChainFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create framebuffer!");
    }
  }
}

// make a command pool to allow us to draw shit
void vulkan_app::createCommandPool() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

  // each command pool can only execute commmands for one queue family, and in this case
  // we've picked graphics
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
  poolInfo.flags            = 0; // optional

  // final creation of command pool
  if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }
}

// populate the command pool with buffers that allow us to do things
void vulkan_app::createCommandBuffers() {
  commandBuffers.resize(swapChainFramebuffers.size());

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = commandPool;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

  // cram the command buffers into commandBuffers[]
  if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate command buffers!");
  }

  // start recording command buffers
  for (size_t i = 0; i < commandBuffers.size(); ++i) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = 0;       // optional
    beginInfo.pInheritanceInfo = nullptr; // optional

    if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
      throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    // which render pass are we talking about?
    renderPassInfo.renderPass = renderPass;
    // what attachments is it using?
    renderPassInfo.framebuffer = swapChainFramebuffers[i];

    // tell it the size of the rendering area
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    // what color are we using for blanking? used for VK_ATTACHMENT_LOAD_OP_CLEAR
    VkClearValue clearColor        = {0.0f, 0.0f, 0.0f, 1.0f};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues    = &clearColor;

    // begin the render pass spec
    vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // bind the renderpass to the pipeline. second param tells us if its compute or
    // graphics
    vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphicsPipeline);

    vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

    // finish the render pass spec
    vkCmdEndRenderPass(commandBuffers[i]);

    if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to record command buffer!");
    }
  }
}

// draw images to the screen
void vulkan_app::drawFrame() {
  // wait for last draw call on this image to finish before starting again
  vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

  // 1: acquire an image from the swapchain
  uint32_t imageIndex;

  // imageAvailableSemaphore trips when the present engine is finished using the image.
  vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX,
                        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
                        &imageIndex);

  // if this image is in flight, wait for it to finish before proceeding
  if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(logicalDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  // set this frame as in flight
  imagesInFlight[imageIndex] = inFlightFences[currentFrame];

  // 2: execute the command buffer with that image as an attachment in the framebuffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
  // wait until the image is available to write colors on it
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  // assign the wait semaphores
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores    = waitSemaphores;
  submitInfo.pWaitDstStageMask  = waitStages;

  // tell it what command buffers to actually submit for execution
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &commandBuffers[imageIndex];

  // tell it what semaphores to signal once the buffers are done
  VkSemaphore signalSemaphores[]  = {renderFinishedSemaphores[currentFrame]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = signalSemaphores;

  vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
  // 3: return the image to the swapchain for presentation
  // the fence will send a signal every time the command buffer finishes execution

  if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = signalSemaphores;

  // make an array of swapchains
  VkSwapchainKHR swapChains[] = {swapChain};
  presentInfo.swapchainCount  = 1;
  // which swapchain are we presenting images to?
  presentInfo.pSwapchains = swapChains;
  // index of image for each swapchain
  presentInfo.pImageIndices = &imageIndex;

  // tells you whether the presentation is successful for each individual swapchain
  presentInfo.pResults = nullptr; // Optional

  // draw the goddamn triangle on the screen
  vkQueuePresentKHR(presentQueue, &presentInfo);

  // iterate the framecount. Loops around every time it passes MAX_FRAMES_IN_FLIGHT
  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// make flags for timing and shit
void vulkan_app::createSyncObjects() {
  // resize the semaphore vectors to the amount of concurrent frames possible
  imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
  imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

  // grug fill in tiny semaphore struct
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.flags = 0;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  // turn the fence on by default so rendering can actually start.
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  // make semaphores for image availability and render status, and make fences for cpu-gpu
  // sync.
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr,
                          &imageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr,
                          &renderFinishedSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) !=
            VK_SUCCESS) {

      throw std::runtime_error("failed to create sync objects for a frame!");
    }
  }
}

// make a shader module for vulkan to use, based on passed-in bytecode
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

// pick a GPU to run on
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
    std::cout << device << " is the current device in the list" << std::endl;
    if (isDeviceSuitable(device)) {
      physicalDevice = device;
      break;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to find a suitable gpu!");
  }
}

// see if the device can support the extensions and swapchain specs we ask of it
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

// see if our device can support all the extensions we need
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

// pick the color display format for the surface
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

// TODO: revisit queue families. This is fuzzy
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
    drawFrame();
  }

  vkDeviceWaitIdle(logicalDevice);
}

// clean up our mess
void vulkan_app::cleanup() {
  // kill the semaphores and fences. bye-bye, little flag dudes...
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
    vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
    vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
  }
  // kill the command pool
  vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
  // kill all the framebuffers we made in createFramebuffers()
  for (auto framebuffer : swapChainFramebuffers) {
    vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
  }
  // kill the pipeline
  vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
  // kill the pipeline layout
  vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
  vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
  // kill all the image views we made in createImageViews()
  for (auto imageView : swapChainImageViews) {
    vkDestroyImageView(logicalDevice, imageView, nullptr);
  }
  // kill the swapchain
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

// set up a vulkan instance
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