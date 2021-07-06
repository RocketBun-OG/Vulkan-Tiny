#pragma once
#include "mesh.h"
#include "pipeline_builder.h"
#include "vk_initializers.h"
#include "vk_types.h"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Struct for holding all the deletion functions for Vulkan objects
struct DeletionQueue {
  std::deque<std::function<void()>> deleters;
  void pushFunction(std::function<void()> &&function) { deleters.push_back(function); }

  // for every function in the deletion queue, run it, and then wipe it
  void flush() {
    for (auto it = deleters.rbegin(); it != deleters.rend(); it++) {
      (*it)();
    }
    deleters.clear();
  }
};

// Struct for holding objects for each frame in the swapchain
struct FrameData {
  VkSemaphore renderSemaphore, presentSemaphore;
  VkFence renderFence;

  VkCommandPool graphicsCommandPool, computeCommandPool;
  VkCommandBuffer graphicsCommandBuffer, computeCommandBuffer;
};

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

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
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;
  std::vector<VkFramebuffer> swapChainFramebuffers;

  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;

  VkRenderPass renderPass;

  VkPipelineLayout pipelineLayout;
  VkPipeline renderPipeline;

  VmaAllocator allocator;

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
  SwapChainSupportDetails swapChainSupport;
  // validation layer list
  const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation",
                                                      "VK_LAYER_LUNARG_monitor"};
  // necessary device extension list
  const std::vector<const char *> requiredDeviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  bool isInitialized{false};

  unsigned int frameNumber{0};
  unsigned int selectedShader{0};

  FrameData bufferFrames[MAX_FRAMES_IN_FLIGHT];

  DeletionQueue mainDeletionQueue;

  // default window size.
  VkExtent2D windowExtent{800, 600};

  glm::vec3 camPos{0.f, 0.f, -3.f};

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

  // The functions here are ordered mostly in terms of when they're used in the chain

  std::vector<const char *> getRequiredExtensions();
  void createInstance();

  void createSurface();
  // GPU selection functions
  void pickPhysicalDevice();
  bool isDeviceSuitable(VkPhysicalDevice GPU);
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice GPU);

  // Logical device creation
  void createDevice();

  // Swapchain creation functions
  void createSwapChain();
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice GPU);
  VkPresentModeKHR
  chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkSurfaceFormatKHR
  chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  void createImageViews();

  // Command queue/buffer setup
  void initGraphicsCommands();
  void initComputeCommands();
  void initCommands();

  // Set up for rendering
  void createRenderPass();
  void createFramebuffers();
  void createSyncStructures();

  // Load shaders from SPIR-V into renderer modules
  bool loadShaderModule(const char *filePath, VkShaderModule *outShaderModule);

  void createPipelines();

  // Returns the associated struct for the current frame, based on MAX_FRAMES_IN_FLIGHT
  FrameData &getCurrentFrame();

  // These are for resizes
  void cleanupSwapChain();
  void recreateSwapChain();

  void createMemAllocator();

  // Not currently used
  void loadMeshes();
  void uploadMesh(Mesh &mesh);

  // HERE BE DEBUG DRAGONS
  //----------------------------------------------------------------
  bool checkValidationSupport();

  //----------------------------------------------------------------
};
