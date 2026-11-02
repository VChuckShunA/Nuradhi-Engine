#include "engine/render/blood_render_system.hpp"
#include "engine/effects/blood.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"

//std
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace nrd {

	namespace {
		//matches the push constant block in blood.frag
		struct BloodPushConstants {
			glm::vec4 colour; //rgb blood, linear
		};

		struct Batch {
			std::uint32_t firstVertex;
			std::uint32_t vertexCount;
			BloodPushConstants push;
		};

		constexpr float SPLAT_LIFT = 0.002f; //splats sit a hair above the floor so they don't flicker into it
		constexpr float STREAK = 0.02f; //droplets stretch this much per unit per second of their speed on screen
	}

	BloodRenderSystem::BloodRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
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

	BloodRenderSystem::~BloodRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void BloodRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(BloodPushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create blood pipeline layout");
		}
	}

	void BloodRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //tested against the scene, but doesn't hide what's behind
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; //splats are seen from above, whichever way they wind
		config.bindingDescription = { { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
		config.attributeDescription = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
			{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, data) },
		};
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/blood.vert.spv", "shaders/blood.frag.spv", config);
	}

	void BloodRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		const glm::mat4& inverseView = frameInfo.camera.getInverseView();
		const glm::vec3 cameraRight = glm::normalize(glm::vec3(inverseView[0]));
		const glm::vec3 cameraDown = glm::normalize(glm::vec3(inverseView[1])); //view space +y is down
		const glm::vec3 cameraForward = glm::normalize(glm::vec3(inverseView[2]));

		vertices.clear();
		std::vector<Batch> batches;
		const std::size_t capacity = static_cast<std::size_t>(MAX_QUADS) * 6;
		const auto addQuad = [&](const glm::vec3& centre, const glm::vec3& axisA, const glm::vec3& axisB, const glm::vec4& data) {
			if (vertices.size() + 6 > capacity) return;
			const Vertex corners[4] = {
				{ centre - axisA - axisB, { 0.f, 0.f }, data },
				{ centre + axisA - axisB, { 1.f, 0.f }, data },
				{ centre + axisA + axisB, { 1.f, 1.f }, data },
				{ centre - axisA + axisB, { 0.f, 1.f }, data },
			};
			for (const int corner : { 0, 1, 2, 2, 3, 0 }) vertices.push_back(corners[corner]);
		};

		frameInfo.registry.each<BloodEffect>([&](ecs::Entity, BloodEffect& blood) {
			const std::uint32_t first = static_cast<std::uint32_t>(vertices.size());
			//splats first: they lie on the floor, under the droplets
			for (const BloodEffect::Splat& splat : blood.splatsOnGround) {
				const float fade = blood.splatFade > 0.f ? std::clamp((blood.splatLife + blood.splatFade - splat.age) / blood.splatFade, 0.f, 1.f) : 1.f;
				//washed by rain: it spreads out a little as it thins (a bigger quad), and the shader pales and softens it
				const float half = splat.size * 0.5f * (1.f + 0.4f * splat.washed);
				addQuad(splat.position - glm::vec3{ 0.f, SPLAT_LIFT, 0.f }, { half, 0.f, 0.f }, { 0.f, 0.f, half }, { 1.f, 0.9f * fade * (1.f - splat.washed), splat.seed, splat.washed });
			}
			//droplets: stretched along their motion as the camera sees it
			for (const BloodEffect::Droplet& droplet : blood.droplets) {
				const glm::vec3 onScreen = droplet.velocity - cameraForward * glm::dot(droplet.velocity, cameraForward);
				const float speed = glm::length(onScreen);
				const float half = droplet.size * 0.5f;
				glm::vec3 along = cameraDown * half;
				glm::vec3 across = cameraRight * half;
				if (speed > 1e-3f) {
					const glm::vec3 direction = onScreen / speed;
					along = direction * (half + speed * STREAK * 0.5f);
					across = glm::normalize(glm::cross(direction, cameraForward)) * half;
				}
				addQuad(droplet.position, across, along, { 0.f, 1.f, 0.f, 0.f });
			}
			const std::uint32_t count = static_cast<std::uint32_t>(vertices.size()) - first;
			if (count > 0) batches.push_back({ first, count, { glm::vec4(blood.colour, 1.f) } });
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
			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloodPushConstants), &batch.push);
			vkCmdDraw(frameInfo.commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
		}
	}
}
