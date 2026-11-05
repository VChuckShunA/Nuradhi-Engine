#include "engine/render/lightning_render_system.hpp"
#include "engine/effects/lightning.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"

//std
#include <algorithm>
#include <cmath>
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
			if (!lightning.showBolt || lightning.bolt.empty()) return;			const float level = std::clamp(lightning.flash, 0.f, 1.f); //the bolt is only there while the sky is lit
			if (level <= 0.01f) return;

			const std::uint32_t first = static_cast<std::uint32_t>(vertices.size());
			const float glow = std::max(lightning.glow, 1.f);
			//Each stroke is one strip with mitred joints: the two sides of every joint are shared, so neighbouring pieces
			//never overlap (no bright dots where they would have been blended twice). The ends are rounded off with a cap
			//whose distance falls off radially in the shader, so nothing is cut square.
			struct Row {
				glm::vec3 left, right;
				float along; //0 in the body, -1 / +1 at the tip of the start / end cap
				float brightness;
			};
			std::vector<Row> rows;
			for (const Lightning::Stroke& stroke : lightning.bolt) {
				const std::vector<Lightning::Point>& points = stroke.points;
				const std::size_t count = points.size();
				if (count < 2) continue;

				std::vector<glm::vec3> directions(count - 1);
				for (std::size_t i = 0; i + 1 < count; i++) {
					const glm::vec3 delta = points[i + 1].position - points[i].position;
					const float length = glm::length(delta);
					directions[i] = length > 1e-6f ? delta / length : (i > 0 ? directions[i - 1] : glm::vec3{ 0.f, 1.f, 0.f });
				}

				rows.clear();
				for (std::size_t i = 0; i < count; i++) {
					const glm::vec3 before = directions[i > 0 ? i - 1 : 0];
					const glm::vec3 after = directions[i + 1 < count ? i : count - 2];
					const glm::vec3 beforeSide{ -before.y, before.x, 0.f };
					const glm::vec3 afterSide{ -after.y, after.x, 0.f };
					glm::vec3 side = beforeSide + afterSide;
					const float sideLength = glm::length(side);
					side = sideLength > 1e-5f ? side / sideLength : beforeSide;
					const float miter = 1.f / std::max(glm::dot(side, beforeSide), 0.6f); //longer across a bend, so the ribbon keeps its width
					const float half = points[i].width * 0.5f * glow; //the whole ribbon, halo included
					rows.push_back({ points[i].position - side * half * miter, points[i].position + side * half * miter, 0.f, points[i].brightness });
				}

				if (!stroke.fork) {
					const float half = points[0].width * 0.5f * glow;
					const glm::vec3 side{ -directions[0].y, directions[0].x, 0.f };
					const glm::vec3 tip = points[0].position - directions[0] * half;
					rows.insert(rows.begin(), { tip - side * half, tip + side * half, -1.f, points[0].brightness });
				}
				{
					const float half = points[count - 1].width * 0.5f * glow;
					const glm::vec3 side{ -directions[count - 2].y, directions[count - 2].x, 0.f };
					const glm::vec3 tip = points[count - 1].position + directions[count - 2] * half;
					rows.push_back({ tip - side * half, tip + side * half, 1.f, points[count - 1].brightness });
				}

				if (vertices.size() + (rows.size() - 1) * 6 > capacity) break;
				const auto vertexOf = [&](const Row& row, bool right) {
					return Vertex{ right ? row.right : row.left, { right ? 1.f : 0.f, row.along }, { 1.f / glow, row.brightness * level, 0.f, 0.f } };
				};
				for (std::size_t i = 0; i + 1 < rows.size(); i++) {
					const Vertex a = vertexOf(rows[i], false), b = vertexOf(rows[i], true);
					const Vertex c = vertexOf(rows[i + 1], true), d = vertexOf(rows[i + 1], false);
					for (const Vertex& vertex : { a, b, c, c, d, a }) vertices.push_back(vertex);
				}
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
