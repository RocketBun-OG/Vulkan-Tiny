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

const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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
  // devices
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice logicalDevice;
  VkSwapchainKHR swapChain;
  // handle for the graphics queue
  VkQueue graphicsQueue;
  // handle for the present queue
  VkQueue presentQueue;

  // vector full of images!
  std::vector<VkImage> swapChainImages;
  // vector full of image views!
  std::vector<VkImageView> swapChainImageViews;

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

  void createGraphicsPipeline();
  void createRenderPass();

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
