#include "engine/render/snow_render_system.hpp"
#include "engine/core/components.hpp"
#include "engine/snow/snow_volume.hpp"

//std
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace nrd {

	namespace {
		//matches the push constant block in snow.vert and snow.frag
		struct SnowPushConstants {
			glm::vec4 centre; //xyz ground, w time
			glm::vec4 extents; //half width, height, depth, layers
			glm::vec4 flakes; //size, spacing, density, fall speed
			glm::vec4 motion; //wind, sway amplitude, sway frequency, motion blur
			glm::vec4 colour; //rgb, a opacity
			glm::vec4 lens; //depth of field, focus, mist, light scatter
			glm::vec4 style; //outline, tone bands, halftone, halftone size
		};
		static_assert(sizeof(SnowPushConstants) <= 128, "push constants are only guaranteed 128 bytes");
	}

	SnowRenderSystem::SnowRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: lveDevice{ device }
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}

	SnowRenderSystem::~SnowRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void SnowRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SnowPushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create snow pipeline layout");
		}
	}

	void SnowRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //tested against the scene, so flakes fall behind and in front of the fighters
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
		config.bindingDescription.clear(); //no vertex buffer: the layers come from gl_VertexIndex and gl_InstanceIndex
		config.attributeDescription.clear();
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/snow.vert.spv", "shaders/snow.frag.spv", config);
	}

	void SnowRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		const glm::vec3 cameraPosition = glm::vec3(frameInfo.camera.getInverseView()[3]);
		const glm::vec3 cameraForward = glm::normalize(glm::vec3(frameInfo.camera.getInverseView()[2]));

		//volumes are transparent: the farthest first
		std::vector<std::pair<float, SnowPushConstants>> volumes;
		frameInfo.registry.each<SnowVolume, Transform>([&](ecs::Entity, SnowVolume& snow, Transform& transform) {
			if (!snow.enabled || snow.layers <= 0 || snow.height <= 0.f || snow.spacing <= 0.f) return;
			SnowPushConstants push{};
			push.centre = glm::vec4(transform.position, snow.time);
			push.extents = { snow.halfWidth, snow.height, snow.depth, static_cast<float>(snow.layers) };
			push.flakes = { snow.flakeSize, snow.spacing, snow.density, snow.fallSpeed };
			push.motion = { snow.wind, snow.sway.x, snow.sway.y, snow.motionBlur };
			push.colour = snow.colour;
			push.lens = { snow.depthOfField, snow.focus, snow.mist, snow.lightScatter };
			push.style = { snow.outline, static_cast<float>(std::max(snow.toneBands, 0)), snow.halftone, std::max(snow.halftoneSize, 2.f) };
			volumes.push_back({ glm::dot(transform.position - cameraPosition, cameraForward), push });
		});
		if (volumes.empty()) return;
		std::sort(volumes.begin(), volumes.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

		pipeline->bind(frameInfo.commandBuffer);
		vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
		for (const auto& [depth, push] : volumes) {
			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SnowPushConstants), &push);
			vkCmdDraw(frameInfo.commandBuffer, 6, static_cast<uint32_t>(push.extents.w), 0, 0);
		}
	}
}
