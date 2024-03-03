#include "nrd_debugLinePipeline.hpp"
#include "nrd_debugLine.hpp"

#include<fstream>
#include<stdexcept>
#include<iostream>
#include<cassert>
namespace nrd {
	NrdDebugLinePipeline::NrdDebugLinePipeline(
		lve::LveDevice& device, 
		const std::string& vertFilepath, 
		const std::string& fragFilepath, 
		const NrdDebugLinePipelineConfigInfo& configInfo) :lveDevice(device)
	{
		createGraphicsPipeline(vertFilepath, fragFilepath, configInfo);
	}
	NrdDebugLinePipeline::~NrdDebugLinePipeline()
	{
		vkDestroyShaderModule(lveDevice.device(), vertShaderModule, nullptr);
		vkDestroyShaderModule(lveDevice.device(), fragShaderModule, nullptr);
		vkDestroyPipeline(lveDevice.device(), graphicsPipeline, nullptr);
	}
	void NrdDebugLinePipeline::bind(VkCommandBuffer commandBuffer)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	}
	void NrdDebugLinePipeline::defaultPipelineConfigInfo(NrdDebugLinePipelineConfigInfo& configInfo)
	{
		//triangle info
		configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//triangle list
		configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;


		//combining viewport and scissors into a viewport-state info variable
		configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		configInfo.viewportInfo.viewportCount = 1;
		configInfo.viewportInfo.pViewports = nullptr;
		configInfo.viewportInfo.scissorCount = 1;
		configInfo.viewportInfo.pScissors = nullptr;

		//rasterization stage - breaks up geometry into fragments
		configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		configInfo.rasterizationInfo.depthClampEnable = VK_FALSE; //forces z to be between 0 and 1
		configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE; //discards all primitives before rastarization
		configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE; //drawing lines
		configInfo.rasterizationInfo.lineWidth = 1.0f;
		configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; //backface culling
		configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
		configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;  // Optional
		configInfo.rasterizationInfo.depthBiasClamp = 0.0f;           // Optional
		configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;     // Optional

		//Multisampling - Anti Aliassing
		configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
		configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		configInfo.multisampleInfo.minSampleShading = 1.0f;           // Optional
		configInfo.multisampleInfo.pSampleMask = nullptr;             // Optional
		configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;  // Optional
		configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;       // Optional

		//Colour Blending
		configInfo.colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		configInfo.colorBlendAttachment.blendEnable = VK_FALSE; //Enabling colour blending
		configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
		configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
		configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
		configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
		configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
		configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

		//Depth buffer -  depth testing
		configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
		configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;  // Optional
		configInfo.colorBlendInfo.attachmentCount = 1;
		configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
		configInfo.colorBlendInfo.blendConstants[0] = 0.0f;  // Optional
		configInfo.colorBlendInfo.blendConstants[1] = 0.0f;  // Optional
		configInfo.colorBlendInfo.blendConstants[2] = 0.0f;  // Optional
		configInfo.colorBlendInfo.blendConstants[3] = 0.0f;  // Optional

		configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
		configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
		configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
		configInfo.depthStencilInfo.minDepthBounds = 0.0f;  // Optional
		configInfo.depthStencilInfo.maxDepthBounds = 1.0f;  // Optional
		configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
		configInfo.depthStencilInfo.front = {};  // Optional
		configInfo.depthStencilInfo.back = {};   // Optional

		//configures the pipeline to expect a dynamic scissor and dynamic viewport to be expected
		configInfo.dynamicStateEnabler = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
		configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnabler.data();
		configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnabler.size());
		configInfo.dynamicStateInfo.flags = 0;

		configInfo.bindingDescription = nrd::NrdDebugLine::Vertex::getBindingDescription();
		configInfo.attributeDescription = nrd::NrdDebugLine::Vertex::getAttributeDescriptions();
	}
	void NrdDebugLinePipeline::enableAlphaBlending(NrdDebugLinePipelineConfigInfo& configInfo)
	{
		configInfo.colorBlendAttachment.blendEnable = VK_TRUE;

		configInfo.colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;// May want to change this in the future
		configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;// May want to change this in the future
		configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;// May want to change this in the future

	}
	std::vector<char> NrdDebugLinePipeline::readFile(const std::string& filePath)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file: " + filePath);
		}

		size_t filesize = static_cast<size_t>(file.tellg()); //size of the file, 8bit flag= end of the file, use tellg to get the last position
		std::vector<char> buffer(filesize);

		file.seekg(0);//go to the start of the file
		file.read(buffer.data(), filesize); //reading the file

		file.close();
		return buffer;
	}
	void NrdDebugLinePipeline::createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const NrdDebugLinePipelineConfigInfo& configInfo)
	{
		assert(configInfo.pipelineLayout != VK_NULL_HANDLE && "Cannot create graphics pipeline:: no pipelineLayout provided in configInfo");

		assert(configInfo.renderPass != VK_NULL_HANDLE && "Cannot create graphics pipeline:: no renderPass provided in configInfo");
		auto vertCode = readFile(vertFilepath);
		auto fragCode = readFile(fragFilepath);

		createShaderModule(vertCode, &vertShaderModule);
		createShaderModule(fragCode, &fragShaderModule);

		VkPipelineShaderStageCreateInfo shaderStages[2];
		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages[0].module = vertShaderModule;
		shaderStages[0].pName = "main";
		shaderStages[0].flags = 0;
		shaderStages[0].pNext = nullptr;
		shaderStages[0].pSpecializationInfo = nullptr;//used to customize shader functionality

		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = fragShaderModule;
		shaderStages[1].pName = "main";
		shaderStages[1].flags = 0;
		shaderStages[1].pNext = nullptr;
		shaderStages[1].pSpecializationInfo = nullptr;//used to customize shader functionality


		auto& bindingDescriptions = configInfo.bindingDescription;
		auto& attributeDescriptions = configInfo.attributeDescription;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();

		//VkPipelineViewportStateCreateInfo viewportInfo{};



		VkGraphicsPipelineCreateInfo pipelineinfo{};
		pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineinfo.stageCount = 2; //vertex and fragment shader(2)
		pipelineinfo.pStages = shaderStages;
		pipelineinfo.pVertexInputState = &vertexInputInfo;
		pipelineinfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
		pipelineinfo.pViewportState = &configInfo.viewportInfo;
		pipelineinfo.pRasterizationState = &configInfo.rasterizationInfo;
		pipelineinfo.pMultisampleState = &configInfo.multisampleInfo;
		pipelineinfo.pColorBlendState = &configInfo.colorBlendInfo;
		pipelineinfo.pDepthStencilState = &configInfo.depthStencilInfo;
		pipelineinfo.pDynamicState = &configInfo.dynamicStateInfo;

		pipelineinfo.layout = configInfo.pipelineLayout;
		pipelineinfo.renderPass = configInfo.renderPass;
		pipelineinfo.subpass = configInfo.subpass;

		//Used for optimization
		pipelineinfo.basePipelineIndex = -1;
		pipelineinfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(lveDevice.device(), VK_NULL_HANDLE, 1, &pipelineinfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Graphics Pipeline!");
		}

		std::cout << "Vertex Shader Code Size: " << vertCode.size() << '\n';
		std::cout << "Fragment Shader Code Size: " << fragCode.size() << '\n';
	}
	void NrdDebugLinePipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule)
	{//create local Vk shader module
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //pointer to our code

		//validating shader module
		if (vkCreateShaderModule(lveDevice.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Shader Module");
		}
	}
}
