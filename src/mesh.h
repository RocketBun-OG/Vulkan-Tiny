#pragma once

#include "vk_types.h"
#include <glm/glm.hpp>
#include <vector>

// mesh shit
struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;

  static VertexInputDescription getVertexDescription();
};

struct Mesh {
  // our vertex data
  std::vector<Vertex> vertices;
  // where the gpu copy of that vertex data is stored
  AllocatedBuffer vertexBuffer;
};
