#include "simple_render_system.hpp"
#include "nrd_debugLinePipeline.hpp"
#include <stdexcept>
#include <array>
//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace lve {
	
	SimpleRenderSystem::SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout):
		lveDevice{device}
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}
	SimpleRenderSystem::~SimpleRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}
	
	void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ globalSetLayout };

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Pipeline layout");
		}
	}
	void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig = {};
		LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		lvePipeline = std::make_unique<LvePipeline>(
			lveDevice,
			"simple_shader.vert.spv",
			"simple_shader.frag.spv",
			pipelineConfig
		);

		// Create NrdDebugLinePipeline and NrdDebugLine objects
		nrd::NrdDebugLinePipelineConfigInfo debugPipeLineConfigInfo = {};
		nrd::NrdDebugLinePipeline::defaultPipelineConfigInfo(debugPipeLineConfigInfo);

		debugPipeLineConfigInfo.renderPass = renderPass;
		debugPipeLineConfigInfo.rasterizationInfo.lineWidth = 5;
		debugPipeLineConfigInfo.pipelineLayout = pipelineLayout;
		nrdDebugLinePipeline = std::make_unique <nrd::NrdDebugLinePipeline> ( lveDevice, "green_shader.vert.spv", "green_shader.frag.spv", debugPipeLineConfigInfo );
		//nrdDebugLinePipeline = make_unique<nrd::NrdDebugLinePipeline>(...(constructor args)...);
		
	}




	void SimpleRenderSystem::bindDescriptorSets(FrameInfo& frameInfo)
	{
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,
			&frameInfo.globalDescriptorSet,
			0,
			nullptr);
	}

	void SimpleRenderSystem::renderGameObects(
		FrameInfo & frameInfo)
	{
		//lvePipeline->bind(frameInfo.commandBuffer);


		//update
		for (auto& obj : frameInfo.gameObjects) {
			if (obj->model == nullptr) continue;//IF THE Gameobj->ct doesn't have a model
			//obj->transform.rotation.y = glm::mod(obj->transform.rotation.y + 0.0001f, glm::two_pi<float>());
			//obj->transform.rotation.x = glm::mod(obj->transform.rotation.x + 0.0001f, glm::two_pi<float>());

			
			obj->bind(frameInfo);
			obj->draw(frameInfo, pipelineLayout);
		}
	}
}