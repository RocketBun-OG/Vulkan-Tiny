#include "vk_initializers.h"

namespace vkinit {

// create a pipeline shader stage
VkPipelineShaderStageCreateInfo
pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule) {

  VkPipelineShaderStageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.pNext = nullptr;

  // which shader stage is it?
  info.stage = stage;

  // which module holds the code?
  info.module = shaderModule;
  // where in the shader do we enter?
  info.pName = "main";
  return info;
}

// contains info for input formats and vertex buffers
VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo() {
  VkPipelineVertexInputStateCreateInfo info{};

  info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  info.pNext = nullptr;

  // no vertex bindings or attributes
  info.vertexBindingDescriptionCount   = 0;
  info.vertexAttributeDescriptionCount = 0;

  return info;
}

// tell the pipeline how to assemble vertices
VkPipelineInputAssemblyStateCreateInfo
inputAssemblyCreateInfo(VkPrimitiveTopology topology) {
  VkPipelineInputAssemblyStateCreateInfo info{};

  info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.topology = topology;
  // don't use primitive restart
  info.primitiveRestartEnable = VK_FALSE;

  return info;
}
VkPipelineRasterizationStateCreateInfo
rasterizationStateCreateInfo(VkPolygonMode polygonMode) {
  VkPipelineRasterizationStateCreateInfo info{};

  info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.depthClampEnable = VK_FALSE;

  // don't discard primitives before rasterization
  info.rasterizerDiscardEnable = VK_FALSE;

  info.polygonMode = polygonMode;
  info.lineWidth   = 1.0f;
  info.cullMode    = VK_CULL_MODE_BACK_BIT;
  info.frontFace   = VK_FRONT_FACE_CLOCKWISE;

  // no depth biasing
  info.depthBiasEnable         = VK_FALSE;
  info.depthBiasConstantFactor = 0.0f;
  info.
}

} // namespace vkinit