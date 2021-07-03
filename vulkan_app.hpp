#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

// TODO: fix semaphores
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// how many frames are we allowed to process at once?
const int MAX_FRAMES_IN_FLIGHT = 2;

class vulkan_app {
public:
  void run();

private:
  // things for drawing to the screen
  GLFWwindow *window;
  VkInstance instance;
  VkSurfaceKHR surface;

  // pipeline shenanigans
  VkRenderPass renderPass;
  VkPipelineLayout pipelineLayout;
  VkPipeline graphicsPipeline;
  VkCommandPool commandPool;
  // devices
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice logicalDevice;
  VkSwapchainKHR swapChain;
  // handle for the graphics queue
  VkQueue graphicsQueue;
  // handle for the present queue
  VkQueue presentQueue;

  // signals to tell the application the state of rendering
  VkSemaphore imageAvailableSemaphore;
  VkSemaphore renderFinishedSemaphore;

  // vectors full of semaphores for each frame
  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;

  // vectors full of fences. These are for CPU-GPU sync
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;

  // vector full of images!
  std::vector<VkImage> swapChainImages;
  // vector full of image views!
  std::vector<VkImageView> swapChainImageViews;
  // vector full of framebuffers, one for each image
  std::vector<VkFramebuffer> swapChainFramebuffers;
  // vector full of command buffers, one for each image
  std::vector<VkCommandBuffer> commandBuffers;

  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;

  struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
  };

  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };

  const uint32_t WIDTH  = 800;
  const uint32_t HEIGHT = 600;

  size_t currentFrame = 0;
  // const char* creates a pointer to a string literal, in this case
  // VK_LAYER_KHRONOS_validation

#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif

  void initWindow();
  void initVulkan();
  void createInstance();

  void createSurface();

  void pickPhysicalDevice();
  bool isDeviceSuitable(VkPhysicalDevice device);
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

  VkSurfaceFormatKHR
  chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR
  chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  VkShaderModule createShaderModule(const std::vector<char> code);

  void createLogicalDevice();

  void createSwapChain();
  void createImageViews();
  void createRenderPass();
  void createGraphicsPipeline();
  void createFramebuffers();
  void createCommandPool();
  void createCommandBuffers();

  void createSyncObjects();

  void drawFrame();

  void mainLoop();
  void cleanup();

  static std::vector<char> readFile(const std::string &filename);

  // HERE BE DEBUG DRAGONS
  //----------------------------------------------------------------
  const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

  VkDebugUtilsMessengerEXT debugMessenger;

  bool checkValidationLayerSupport();

  std::vector<const char *> getRequiredExtensions();

  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageType,
      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

  void setupDebugMessenger();
}; // end of class

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);
void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator);
