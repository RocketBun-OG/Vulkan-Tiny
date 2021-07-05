#pragma once
#include "pipeline_builder.h"
#include "vk_initializers.h"
#include "vk_types.h"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

class VulkanEngine {
public:
  VkInstance instance;

  VkSurfaceKHR displaySurface;

  VkQueue computeQueue;
  VkQueue presentQueue;
  VkQueue graphicsQueue;

  VkPhysicalDevice chosenGPU;
  VkPhysicalDeviceFeatures GPUFeatures;

  VkDevice device;

  VkSwapchainKHR swapChain;

  // vector full of images!
  std::vector<VkImage> swapChainImages;
  // vector full of image views!
  std::vector<VkImageView> swapChainImageViews;
  // vector full of framebuffers, one for each image
  std::vector<VkFramebuffer> swapChainFramebuffers;

  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;

  VkCommandPool graphicalCommandPool;
  VkCommandPool computeCommandPool;

  VkCommandBuffer graphicalCommandBuffer;
  VkCommandBuffer computeCommandBuffer;

  VkRenderPass renderPass;

  VkSemaphore presentSemaphore;
  VkSemaphore renderSemaphore;

  VkFence renderFence;

  VkPipelineLayout pipelineLayout;

  VkPipeline renderPipeline;

  // indices of the queue families, which send out commands from their respective queues
  // each queue family can only submit one type of command, so we need multiple queues.
  struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    bool isComplete() {
      return graphicsFamily.has_value() && presentFamily.has_value() &&
             computeFamily.has_value();
    }
  };

  // details on how the swapchain is supported by the surface
  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };

  // validation layer list
  const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
  // necessary device extension list
  const std::vector<const char *> requiredDeviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  bool isInitialized{false};
  bool framebufferResized{false};

  int frameNumber{0};

  int selectedShader{0};

  size_t currentFrame = 0;

  const int MAX_FRAMES_IN_FLIGHT{2};

  VkExtent2D windowExtent{800, 600};

  // nifty forward declaration shit
  struct SDL_Window *window{nullptr};

  // boot up the engine
  void init();
  // draw things to the screen
  void draw();
  // run the main loop
  void run();
  // shut off the engine
  void cleanup();

private:
  void initVulkan();

  std::vector<const char *> getRequiredExtensions();
  void createInstance();

  void createSurface();
  // gpu selection functions
  void pickPhysicalDevice();
  bool isDeviceSuitable(VkPhysicalDevice GPU);
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice GPU);

  // logical device creation
  void createDevice();

  // swapchain creation functions
  void createSwapChain();
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice GPU);
  VkPresentModeKHR
  chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkSurfaceFormatKHR
  chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  void createImageViews();

  void initGraphicsCommands();
  void initComputeCommands();
  void initCommands();

  void createRenderPass();
  void createFramebuffers();
  void createSyncStructures();

  bool loadShaderModule(const char *filePath, VkShaderModule *outShaderModule);

  void createPipelines();

  void cleanupSwapChain();
  void recreateSwapChain();
  // HERE BE DEBUG DRAGONS
  //----------------------------------------------------------------
  bool checkValidationSupport();

  //----------------------------------------------------------------
};
