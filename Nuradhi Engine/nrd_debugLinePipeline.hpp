#pragma once

#include "lve_device.hpp"

//std
#include<string>
#include<vector>
namespace nrd {
	struct NrdDebugLinePipelineConfigInfo {
		NrdDebugLinePipelineConfigInfo() = default;
		NrdDebugLinePipelineConfigInfo(const NrdDebugLinePipelineConfigInfo&) = delete;
		NrdDebugLinePipelineConfigInfo& operator=(const NrdDebugLinePipelineConfigInfo&) = delete;

		std::vector<VkVertexInputBindingDescription> bindingDescription{};
		std::vector<VkVertexInputAttributeDescription> attributeDescription{};
		VkPipelineViewportStateCreateInfo viewportInfo;
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
		VkPipelineRasterizationStateCreateInfo rasterizationInfo;
		VkPipelineMultisampleStateCreateInfo multisampleInfo;
		VkPipelineColorBlendAttachmentState colorBlendAttachment;
		VkPipelineColorBlendStateCreateInfo colorBlendInfo;
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
		std::vector<VkDynamicState> dynamicStateEnabler;
		VkPipelineDynamicStateCreateInfo dynamicStateInfo;
		VkPipelineLayout pipelineLayout = nullptr;
		VkRenderPass renderPass = nullptr;
		uint32_t subpass = 0;
	};
	class NrdDebugLinePipeline {
	public:
		NrdDebugLinePipeline(
			lve::LveDevice& device,
			const std::string& vertFilepath,
			const std::string& fragFilepath,
			const NrdDebugLinePipelineConfigInfo& configInfo);
		~NrdDebugLinePipeline();

		//deleting copy constructors to avoid accidentally duplicating pointers to our vulkan objects
		NrdDebugLinePipeline(const NrdDebugLinePipeline&) = delete;
		NrdDebugLinePipeline& operator=(const NrdDebugLinePipeline&) = delete;

		void bind(VkCommandBuffer commandBuffer);

		//public static function to create a default pipeline configuration
		static void defaultPipelineConfigInfo(NrdDebugLinePipelineConfigInfo& configInfo);
		static void enableAlphaBlending(NrdDebugLinePipelineConfigInfo& configInfo);
	private:
		static std::vector<char> readFile(const std::string& filePath);

		void createGraphicsPipeline(
			const std::string& vertFilepath,
			const std::string& fragFilepath,
			const NrdDebugLinePipelineConfigInfo& configInfo);

		void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

		lve::LveDevice& lveDevice;//store device reference
		VkPipeline graphicsPipeline;//Handle to our Vulkan pipeline object
		VkShaderModule vertShaderModule; //Access Vertex Shader
		VkShaderModule fragShaderModule; //Access Fragment Shader
	};
}