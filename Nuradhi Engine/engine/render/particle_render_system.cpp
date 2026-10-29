#include "engine/render/particle_render_system.hpp"
#include "engine/particles/particles.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"

//std
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>

namespace nrd {

	namespace {
		struct DrawItem {
			float depth; //distance along the view direction; far ones draw first
			const Particle* particle;
			const ParticleEmitter* emitter;
		};

		float fadeOf(const ParticleSettings& settings, const Particle& particle)
		{
			float fade = 1.f;
			if (settings.fadeIn > 0.f) fade = std::min(fade, particle.age / settings.fadeIn);
			if (settings.fadeOut > 0.f) fade = std::min(fade, (particle.lifetime - particle.age) / settings.fadeOut);
			return glm::clamp(fade, 0.f, 1.f);
		}
	}

	ParticleRenderSystem::ParticleRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: lveDevice{ device }
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);

		for (int frame = 0; frame < lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT; frame++) {
			auto buffer = std::make_unique<lve::LveBuffer>(
				lveDevice,
				sizeof(Vertex),
				MAX_PARTICLES * 6,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			buffer->map();
			vertexBuffers.push_back(std::move(buffer));
		}
		vertices.reserve(MAX_PARTICLES * 6);
	}

	ParticleRenderSystem::~ParticleRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void ParticleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create particle pipeline layout");
		}
	}

	void ParticleRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //see-through: tested against the scene, but doesn't hide what's behind
		config.bindingDescription = { { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
		config.attributeDescription = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
			{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, colour) },
			{ 3, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex, softness) },
		};
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/particle.vert.spv", "shaders/particle.frag.spv", config);
	}

	void ParticleRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		//camera axes in world space
		const glm::mat4& inverseView = frameInfo.camera.getInverseView();
		const glm::vec3 cameraRight = glm::normalize(glm::vec3(inverseView[0]));
		const glm::vec3 cameraDown = glm::normalize(glm::vec3(inverseView[1])); //view space +y is down
		const glm::vec3 cameraForward = glm::normalize(glm::vec3(inverseView[2]));
		const glm::vec3 cameraPosition = glm::vec3(inverseView[3]);

		std::vector<DrawItem> items;
		frameInfo.registry.each<ParticleEmitter>([&](ecs::Entity, ParticleEmitter& emitter) {
			for (const Particle& particle : emitter.particles) {
				items.push_back({ glm::dot(particle.position - cameraPosition, cameraForward), &particle, &emitter });
			}
		});
		if (items.empty()) return;

		//transparent: back to front. Over budget, the farthest particles are the ones left out.
		std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) { return a.depth > b.depth; });
		if (items.size() > MAX_PARTICLES) items.erase(items.begin(), items.end() - MAX_PARTICLES);

		vertices.clear();
		for (const DrawItem& item : items) {
			const ParticleSettings& settings = item.emitter->settings;
			const Particle& particle = *item.particle;
			glm::vec4 colour = settings.colour;
			colour.a *= fadeOf(settings, particle);
			if (colour.a <= 0.f) continue;

			//billboard: across the screen and down it. Streaks: along the velocity as seen by the camera, and across it.
			glm::vec3 lengthAxis = cameraDown;
			glm::vec3 widthAxis = cameraRight;
			float halfLength = particle.size * 0.5f;
			const float halfWidth = particle.size * 0.5f;
			if (settings.stretch > 0.f) {
				const glm::vec3 onScreen = particle.velocity - cameraForward * glm::dot(particle.velocity, cameraForward);
				const float speed = glm::length(onScreen);
				if (speed > 1e-4f) {
					lengthAxis = onScreen / speed;
					widthAxis = glm::normalize(glm::cross(lengthAxis, cameraForward));
					halfLength += speed * settings.stretch * 0.5f;
				}
			}

			const glm::vec3 across = widthAxis * halfWidth;
			const glm::vec3 along = lengthAxis * halfLength;
			const Vertex corners[4] = {
				{ particle.position - across - along, { 0.f, 0.f }, colour, settings.softness },
				{ particle.position + across - along, { 1.f, 0.f }, colour, settings.softness },
				{ particle.position + across + along, { 1.f, 1.f }, colour, settings.softness },
				{ particle.position - across + along, { 0.f, 1.f }, colour, settings.softness },
			};
			for (const int corner : { 0, 1, 2, 2, 3, 0 }) vertices.push_back(corners[corner]);
		}
		if (vertices.empty()) return;

		lve::LveBuffer& buffer = *vertexBuffers[frameInfo.frameIndex];
		buffer.writeToBuffer(vertices.data(), vertices.size() * sizeof(Vertex));

		pipeline->bind(frameInfo.commandBuffer);
		vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
		const VkBuffer vertexBuffer = buffer.getBuffer();
		const VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, &vertexBuffer, &offset);
		vkCmdDraw(frameInfo.commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
	}
}
