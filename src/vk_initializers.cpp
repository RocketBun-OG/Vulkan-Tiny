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

// tell the pipeline how to rasterize things
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
  info.depthBiasClamp          = 0.0f;
  info.depthBiasSlopeFactor    = 0.0f;

  return info;
}

// tell the pipeline how to do multisampling
VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo() {
  VkPipelineMultisampleStateCreateInfo info{};

  info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.sampleShadingEnable = false;

  info.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
  info.minSampleShading      = 1.0f;
  info.alphaToCoverageEnable = VK_FALSE;
  info.alphaToOneEnable      = VK_FALSE;

  return info;
}

// control how the pipeline blends colors
VkPipelineColorBlendAttachmentState colorBlendAttachmentState() {
  // dont do any color blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  return colorBlendAttachment;
}

VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo() {
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;

  // empty defaults
  info.flags                  = 0;
  info.setLayoutCount         = 0;
  info.pSetLayouts            = VK_FALSE;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges    = nullptr;
  return info;
}
} // namespace vkinit