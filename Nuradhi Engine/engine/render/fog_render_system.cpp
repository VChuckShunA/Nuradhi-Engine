#include "engine/render/fog_render_system.hpp"
#include "engine/core/components.hpp"
#include "engine/fog/fog_volume.hpp"

//std
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace nrd {

	namespace {
		//matches the push constant block in fog.vert and fog.frag
		struct FogPushConstants {
			glm::vec4 centre; //xyz ground, w time
			glm::vec4 extents; //half width, half depth, height, slices
			glm::vec4 colour; //rgb, a density
			glm::vec4 shape; //coverage, height falloff, noise scale, turbulence
			glm::vec4 motion; //wind x, wind z, shadow, light scatter
			glm::vec4 style; //tone bands, halftone, halftone size
		};
		static_assert(sizeof(FogPushConstants) <= 128, "push constants are only guaranteed 128 bytes");
	}

	FogRenderSystem::FogRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: lveDevice{ device }
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}

	FogRenderSystem::~FogRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void FogRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(FogPushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create fog pipeline layout");
		}
	}

	void FogRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //tested against the scene, so it wraps around whoever stands in it
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; //slices are seen from above or below
		config.bindingDescription.clear(); //no vertex buffer: the slices come from gl_VertexIndex and gl_InstanceIndex
		config.attributeDescription.clear();
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/fog.vert.spv", "shaders/fog.frag.spv", config);
	}

	void FogRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		const glm::vec3 cameraPosition = glm::vec3(frameInfo.camera.getInverseView()[3]);
		const glm::vec3 cameraForward = glm::normalize(glm::vec3(frameInfo.camera.getInverseView()[2]));

		//volumes are transparent too: the farthest first
		std::vector<std::pair<float, FogPushConstants>> volumes;
		frameInfo.registry.each<FogVolume, Transform>([&](ecs::Entity, FogVolume& fog, Transform& transform) {
			if (!fog.enabled || fog.slices <= 0 || fog.height <= 0.f) return;
			FogPushConstants push{};
			push.centre = glm::vec4(transform.position, fog.time);
			push.extents = { fog.halfExtents.x, fog.halfExtents.y, fog.height, static_cast<float>(fog.slices) };
			push.colour = glm::vec4(fog.colour, fog.density);
			push.shape = { fog.coverage, fog.heightFalloff, fog.noiseScale, fog.turbulence };
			push.motion = { fog.wind.x, fog.wind.y, fog.shadow, fog.lightScatter };
			push.style = { static_cast<float>(std::max(fog.toneBands, 0)), fog.halftone, std::max(fog.halftoneSize, 2.f), 0.f };
			volumes.push_back({ glm::dot(transform.position - cameraPosition, cameraForward), push });
		});
		if (volumes.empty()) return;
		std::sort(volumes.begin(), volumes.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

		pipeline->bind(frameInfo.commandBuffer);
		vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
		for (const auto& [depth, push] : volumes) {
			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(FogPushConstants), &push);
			vkCmdDraw(frameInfo.commandBuffer, 6, static_cast<uint32_t>(push.extents.w), 0, 0);
		}
	}
}
