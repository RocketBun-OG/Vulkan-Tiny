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

  window = glfwCreateWindow(WIDTH, HEIGHT, "This is a lot of work", nullptr,
                            nullptr);
}

void vulkan_app::initVulkan() {
  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
}

void vulkan_app::createSurface() {
  if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface!");
  }
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
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // add it to the infos vector
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};

  // set up the device struct
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  // tell it about the queue creation info we just set up

  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = 1;

  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount = 0;
  // add the validation layers, if they exist
  if (enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
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
  vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0,
                   &graphicsQueue);
  vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0,
                   &presentQueue);
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
  return indices.isComplete();
}

vulkan_app::QueueFamilyIndices
vulkan_app::findQueueFamilies(VkPhysicalDevice device) {
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
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Vulkan Adventure";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "The Unibox";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  // create a struct to hold critical vulkan info
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  // find out how many extensions glfw needs and store the info
  auto extensions = getRequiredExtensions();
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  // if the validation layers are turned on, add them to the struct. Otherwise,
  // tell the struct there are no layers turnd on.
  if (enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
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
  createInfo = {};
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
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
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
  if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr,
                                   &debugMessenger) != VK_SUCCESS) {
    throw std::runtime_error("Failed to set up the debug messenger!");
  }
}

// this is such a pain in the ass oh my GOD
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
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