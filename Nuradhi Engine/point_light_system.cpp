#include "point_light_system.hpp"
#include <stdexcept>
#include <array>
#include<map>
//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace lve {
	
	struct PointLightPushConstants {
		glm::vec4 position{};
		glm::vec4 colour{};
		float radius;
	};

	PointLightSystem::PointLightSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) :
		lveDevice{ device }
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}
	PointLightSystem::~PointLightSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(PointLightPushConstants);
		
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
	void PointLightSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig = {};
		LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
		LvePipeline::enableAlphaBlending(pipelineConfig);
		pipelineConfig.attributeDescription.clear();
		pipelineConfig.bindingDescription.clear();
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		lvePipeline = std::make_unique<LvePipeline>(
			lveDevice,
			"point_light.vert.spv",
			"point_light.frag.spv",
			pipelineConfig
		);
	}




	void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo)
	{
		
		auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime,{ 0.f,-1.f,0.f });
		

		int lightIndex = 0;
		for (auto& obj : frameInfo.gameObjects) {
			if (obj->pointLight == nullptr) continue;

			assert(lightIndex < MAX_LIGHTS && "Point lights exceed maximum specified");

			//update light position;
			obj->transform.translation = glm::vec3(rotateLight * glm::vec4(obj->transform.translation, 1.f));
			//copy light to ubo
			ubo.pointLight[lightIndex].position = glm::vec4(obj->transform.translation, 1.f);
			ubo.pointLight[lightIndex].colour = glm::vec4(obj->colour, obj->pointLight->lightIntensity);

			lightIndex += 1;
		}
		ubo.numLights = lightIndex;
	}

	void PointLightSystem::render(
		FrameInfo& frameInfo)
	{
		//sort lights
		std::map<float, LveGameObject::id_t> sorted;
		for (auto& obj : frameInfo.gameObjects) {
			if (obj->pointLight == nullptr) continue;

			//calculate distance
			auto offset = frameInfo.camera.getPosition()-obj->transform.translation;
			float disSquared = glm::dot(offset, offset);
			sorted[disSquared] = obj->getId();
		}

		lvePipeline->bind(frameInfo.commandBuffer);

		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,
			&frameInfo.globalDescriptorSet,
			0,
			nullptr);

		//iterate through sorted lights in reverse order
		for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
			//use game obj id to find light object
			auto& obj = frameInfo.gameObjects.at(it->second);

			PointLightPushConstants push{};
			push.position= glm::vec4(obj->transform.translation, 1.f);
			push.colour= glm::vec4(obj->colour, obj->pointLight->lightIntensity);
			push.radius = obj->transform.scale.x;

			vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PointLightPushConstants),
				&push
			);
			vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);//drawing point lights
		}
	}
}