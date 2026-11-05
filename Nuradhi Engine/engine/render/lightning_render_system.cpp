#include "engine/render/lightning_render_system.hpp"
#include "engine/effects/lightning.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"

//std
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace nrd {

	namespace {
		//matches the push constant block in lightning.frag
		struct LightningPushConstants {
			glm::vec4 colour; //rgb the glow, linear
		};

		struct Batch {
			std::uint32_t firstVertex;
			std::uint32_t vertexCount;
			LightningPushConstants push;
		};
	}

	LightningRenderSystem::LightningRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: lveDevice{ device }
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);

		for (int frame = 0; frame < lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT; frame++) {
			auto buffer = std::make_unique<lve::LveBuffer>(
				lveDevice,
				sizeof(Vertex),
				MAX_QUADS * 6,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			buffer->map();
			vertexBuffers.push_back(std::move(buffer));
		}
		vertices.reserve(MAX_QUADS * 6);
	}

	LightningRenderSystem::~LightningRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void LightningRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(LightningPushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create lightning pipeline layout");
		}
	}

	void LightningRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //tested against the scene, but doesn't hide what's behind
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
		config.bindingDescription = { { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
		config.attributeDescription = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
			{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, data) },
		};
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/lightning.vert.spv", "shaders/lightning.frag.spv", config);
	}

	void LightningRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		vertices.clear();
		std::vector<Batch> batches;
		const std::size_t capacity = static_cast<std::size_t>(MAX_QUADS) * 6;

		frameInfo.registry.each<Lightning>([&](ecs::Entity, Lightning& lightning) {
			if (!lightning.showBolt || lightning.bolt.empty()) return;
			const float level = std::clamp(lightning.flash, 0.f, 1.f); //the bolt is only there while the sky is lit
			if (level <= 0.01f) return;

			const std::uint32_t first = static_cast<std::uint32_t>(vertices.size());
			const float glow = std::max(lightning.glow, 1.f);
			for (const Lightning::Segment& segment : lightning.bolt) {
				if (vertices.size() + 6 > capacity) break;
				const glm::vec3 along = segment.to - segment.from;
				const float length = glm::length(along);
				if (length < 1e-6f) continue;
				const glm::vec3 direction = along / length;
				const glm::vec3 side = glm::vec3{ -direction.y, direction.x, 0.f };
				const float half = segment.width * 0.5f * glow; //the whole ribbon, halo included
				//ribbons reach a little past each end, so the joints between segments close up
				const glm::vec3 from = segment.from - direction * segment.width;
				const glm::vec3 to = segment.to + direction * segment.width;
				const glm::vec4 data{ 1.f / glow, segment.brightness * level, 0.f, 0.f };
				const Vertex corners[4] = {
					{ from - side * half, { 0.f, 0.f }, data },
					{ from + side * half, { 1.f, 0.f }, data },
					{ to + side * half, { 1.f, 1.f }, data },
					{ to - side * half, { 0.f, 1.f }, data },
				};
				for (const int corner : { 0, 1, 2, 2, 3, 0 }) vertices.push_back(corners[corner]);
			}
			const std::uint32_t count = static_cast<std::uint32_t>(vertices.size()) - first;
			if (count > 0) batches.push_back({ first, count, { glm::vec4(lightning.colour, 1.f) } });
		});
		if (batches.empty()) return;

		lve::LveBuffer& buffer = *vertexBuffers[frameInfo.frameIndex];
		buffer.writeToBuffer(vertices.data(), vertices.size() * sizeof(Vertex));

		pipeline->bind(frameInfo.commandBuffer);
		vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
		const VkBuffer vertexBuffer = buffer.getBuffer();
		const VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, &vertexBuffer, &offset);
		for (const Batch& batch : batches) {
			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightningPushConstants), &batch.push);
			vkCmdDraw(frameInfo.commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
		}
	}
}
