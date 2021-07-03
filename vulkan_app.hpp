#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>
#include <set>
class vulkan_app {
public:
  void run();

private:
  GLFWwindow *window;
  VkInstance instance;

  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice logicalDevice;
  // handle for the graphics queue
  VkQueue graphicsQueue;
  //handle for the present queue
  VkQueue presentQueue;

  struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
      return graphicsFamily.has_value() && presentFamily.has_value();
    }

  };

  const uint32_t WIDTH = 800;
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
  void pickPhysicalDevice();
  bool isDeviceSuitable(VkPhysicalDevice device);
  void createLogicalDevice();
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
  void createSurface();

  void mainLoop();
  void cleanup();

  // HERE BE DEBUG DRAGONS
  //----------------------------------------------------------------
  const std::vector<const char *> validationLayers = {
      "VK_LAYER_KHRONOS_validation"};

  VkDebugUtilsMessengerEXT debugMessenger;

  bool checkValidationLayerSupport();

  std::vector<const char *> getRequiredExtensions();

  void populateDebugMessengerCreateInfo(
      VkDebugUtilsMessengerCreateInfoEXT &createInfo);

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData);

  void setupDebugMessenger();
}; // end of class

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger);
void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator);
