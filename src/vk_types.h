#pragma once

#include "vk_mem_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>

// hold the memory buffer allocated alongside its data
struct AllocatedBuffer {
  VkBuffer memBuffer;
  VmaAllocation allocation;
};
