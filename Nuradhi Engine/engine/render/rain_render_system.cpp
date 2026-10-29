#include "engine/render/rain_render_system.hpp"
#include "engine/core/components.hpp"
#include "engine/rain/rain_volume.hpp"

//std
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace nrd {

	namespace {
		//matches the push constant block in rain.vert and rain.frag
		struct RainPushConstants {
			glm::vec4 centre; //xyz ground, w time
			glm::vec4 extents; //half width, height, depth, layers
			glm::vec4 drops; //size, length, spacing, density
			glm::vec4 motion; //fall speed, wind x, wind z, gusts
			glm::vec4 colour; //rgb, a opacity
			glm::vec4 lens; //depth of field, focus, mist, light scatter
			glm::vec4 ripples; //rate, size, life, strength
			glm::vec4 style; //splashes, realistic, depth scaling, tail fade
		};
		static_assert(sizeof(RainPushConstants) <= 128, "push constants are only guaranteed 128 bytes");
	}

	RainRenderSystem::RainRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: lveDevice{ device }
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}

	RainRenderSystem::~RainRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void RainRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(RainPushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create rain pipeline layout");
		}
	}

	void RainRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //tested against the scene, so rain falls behind and in front of the fighters
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
		config.bindingDescription.clear(); //no vertex buffer: the quads come from gl_VertexIndex and gl_InstanceIndex
		config.attributeDescription.clear();
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/rain.vert.spv", "shaders/rain.frag.spv", config);
	}

	void RainRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		const glm::vec3 cameraPosition = glm::vec3(frameInfo.camera.getInverseView()[3]);
		const glm::vec3 cameraForward = glm::normalize(glm::vec3(frameInfo.camera.getInverseView()[2]));

		//volumes are transparent: the farthest first
		std::vector<std::pair<float, RainPushConstants>> volumes;
		frameInfo.registry.each<RainVolume, Transform>([&](ecs::Entity, RainVolume& rain, Transform& transform) {
			if (!rain.enabled || rain.layers <= 0 || rain.height <= 0.f || rain.spacing <= 0.f || rain.fallSpeed <= 0.f) return;
			const glm::vec2 wind = rain.windVector();
			RainPushConstants push{};
			push.centre = glm::vec4(transform.position, rain.time);
			push.extents = { rain.halfWidth, rain.height, rain.depth, static_cast<float>(rain.layers) };
			push.drops = { rain.dropSize, std::max(rain.dropLength, 0.001f), rain.spacing, rain.density };
			push.motion = { rain.fallSpeed, wind.x, wind.y, rain.gusts };
			push.colour = rain.colour;
			push.lens = { rain.depthOfField, rain.focus, rain.mist, rain.lightScatter };
			push.ripples = { rain.rippleRate, rain.rippleSize, rain.rippleLife, rain.rippleStrength };
			push.style = { rain.splashes, rain.realistic ? 1.f : 0.f, rain.depthScaling, rain.tailFade };
			volumes.push_back({ glm::dot(transform.position - cameraPosition, cameraForward), push });
		});
		if (volumes.empty()) return;
		std::sort(volumes.begin(), volumes.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

		pipeline->bind(frameInfo.commandBuffer);
		vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
		for (const auto& [depth, push] : volumes) {
			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RainPushConstants), &push);
			//instance 0 is the rippling ground, drawn first (it's under everything); the layers follow
			const bool realistic = push.style.y > 0.5f;
			const uint32_t layers = static_cast<uint32_t>(push.extents.w);
			vkCmdDraw(frameInfo.commandBuffer, 6, realistic ? layers + 1 : layers, 0, realistic ? 0 : 1);
		}
	}
}
