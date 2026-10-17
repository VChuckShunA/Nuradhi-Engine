#include "engine/render/point_light_system.hpp"
#include "engine/core/components.hpp"
#include "engine/render/render_components.hpp"

//std
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nrd {

	namespace {
		struct PointLightPushConstants {
			glm::vec4 position{};
			glm::vec4 colour{};
			float radius;
		};
	}

	PointLightSystem::PointLightSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) :
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

		lve::PipelineConfigInfo pipelineConfig = {};
		lve::LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
		lve::LvePipeline::enableAlphaBlending(pipelineConfig);
		pipelineConfig.attributeDescription.clear();
		pipelineConfig.bindingDescription.clear();
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		lvePipeline = std::make_unique<lve::LvePipeline>(
			lveDevice,
			"shaders/point_light.vert.spv",
			"shaders/point_light.frag.spv",
			pipelineConfig);
	}

	void PointLightSystem::update(lve::FrameInfo& frameInfo, lve::GlobalUbo& ubo)
	{
		int lightIndex = 0;
		frameInfo.registry.each<Transform, PointLight>([&](ecs::Entity, Transform& transform, PointLight& light) {
			assert(lightIndex < MAX_LIGHTS && "Point lights exceed maximum specified");
			if (lightIndex >= MAX_LIGHTS) return;
			ubo.pointLight[lightIndex].position = glm::vec4(transform.position, 1.f);
			ubo.pointLight[lightIndex].colour = glm::vec4(light.colour, light.intensity);
			lightIndex++;
		});
		ubo.numLights = lightIndex;
	}

	void PointLightSystem::render(lve::FrameInfo& frameInfo)
	{
		//billboards blend, so draw them back to front
		std::vector<std::pair<float, PointLightPushConstants>> lights;
		const glm::vec3 cameraPosition = frameInfo.camera.getPosition();
		frameInfo.registry.each<Transform, PointLight>([&](ecs::Entity, Transform& transform, PointLight& light) {
			const glm::vec3 offset = cameraPosition - transform.position;
			PointLightPushConstants push{};
			push.position = glm::vec4(transform.position, 1.f);
			push.colour = glm::vec4(light.colour, light.intensity);
			push.radius = light.radius;
			lights.emplace_back(glm::dot(offset, offset), push);
		});
		std::sort(lights.begin(), lights.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

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

		for (const auto& [distanceSquared, push] : lights) {
			vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PointLightPushConstants),
				&push);
			vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
		}
	}
}
