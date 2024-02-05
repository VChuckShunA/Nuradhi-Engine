#include "simple_render_system.hpp"
#include <stdexcept>
#include <array>
//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace lve {
	struct SimplePushConstantData {
		glm::mat2 transform{ 1.f };
		glm::vec2 offset;
		alignas(16) glm::vec3 colour; //*IMPORTANT!*
		//using align as to account for how this info is Stored
	};
	SimpleRenderSystem::SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass):
		lveDevice{device}
	{
		createPipelineLayout();
		createPipeline(renderPass);
	}
	SimpleRenderSystem::~SimpleRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}
	
	void SimpleRenderSystem::createPipelineLayout()
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
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
	}




	void SimpleRenderSystem::renderGameObect(VkCommandBuffer commandBuffer, std::vector<LveGameObject>& gameObjects)
	{
		lvePipeline->bind(commandBuffer);
		//update 
		int i = 0;
		for (auto& obj : gameObjects) {
			i += 1;
			obj.transform2d.rotation = glm::mod(obj.transform2d.rotation + 0.001f * i, 2.f * glm::pi<float>());

			SimplePushConstantData push{};
			push.offset = obj.transform2d.translation;
			push.colour = obj.colour;
			push.transform = obj.transform2d.mat2();

			vkCmdPushConstants(
				commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData),
				&push);
			obj.model->bind(commandBuffer);
			obj.model->draw(commandBuffer);
		}
	}
}