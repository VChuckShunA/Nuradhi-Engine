#include "engine/render/nuradhi_rain_render_system.hpp"
#include "engine/rain/nuradhi_rain.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"

//std
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace nrd {

	namespace {
		//matches the push constant block in nuradhi_rain.frag
		struct NuradhiRainPushConstants {
			glm::vec4 colour; //rgb water, w light glint
			glm::vec4 strength; //x ripples, y splashes
		};

		struct Batch {
			std::uint32_t firstVertex;
			std::uint32_t vertexCount;
			NuradhiRainPushConstants push;
		};

		constexpr float RIPPLE_LIFT = 0.003f; //ripples sit a hair above the floor so they don't flicker into it
		constexpr float DROPLET_LIFE = 0.6f; //droplets fade over this long (they usually land sooner)
	}

	NuradhiRainRenderSystem::NuradhiRainRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
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

	NuradhiRainRenderSystem::~NuradhiRainRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void NuradhiRainRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(NuradhiRainPushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Nuradhi Rain pipeline layout");
		}
	}

	void NuradhiRainRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //see-through: tested against the scene, but doesn't hide what's behind
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; //ripples are seen from above, whichever way they wind
		config.bindingDescription = { { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
		config.attributeDescription = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
			{ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, data) },
		};
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/nuradhi_rain.vert.spv", "shaders/nuradhi_rain.frag.spv", config);
	}

	void NuradhiRainRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		const glm::mat4& inverseView = frameInfo.camera.getInverseView();
		const glm::vec3 cameraRight = glm::normalize(glm::vec3(inverseView[0]));
		const glm::vec3 cameraDown = glm::normalize(glm::vec3(inverseView[1])); //view space +y is down

		vertices.clear();
		std::vector<Batch> batches;
		const std::size_t capacity = static_cast<std::size_t>(MAX_QUADS) * 6;
		const auto addQuad = [&](const glm::vec3& centre, const glm::vec3& axisA, const glm::vec3& axisB, float kind, float age) {
			if (vertices.size() + 6 > capacity) return;
			const Vertex corners[4] = {
				{ centre - axisA - axisB, { 0.f, 0.f }, { kind, age } },
				{ centre + axisA - axisB, { 1.f, 0.f }, { kind, age } },
				{ centre + axisA + axisB, { 1.f, 1.f }, { kind, age } },
				{ centre - axisA + axisB, { 0.f, 1.f }, { kind, age } },
			};
			for (const int corner : { 0, 1, 2, 2, 3, 0 }) vertices.push_back(corners[corner]);
		};

		frameInfo.registry.each<NuradhiRain>([&](ecs::Entity, NuradhiRain& rain) {
			const std::uint32_t first = static_cast<std::uint32_t>(vertices.size());
			//ripples first: they lie on the floor, under the splashes
			const float rippleLife = std::max(rain.rippleLife, 0.05f);
			for (const NuradhiRain::Ripple& ripple : rain.activeRipples) {
				const glm::vec3 centre = ripple.position - glm::vec3{ 0.f, RIPPLE_LIFT, 0.f };
				addQuad(centre, { rain.rippleSize, 0.f, 0.f }, { 0.f, 0.f, rain.rippleSize }, 0.f, ripple.age / rippleLife);
			}
			const float half = rain.splashSize * 0.5f;
			for (const NuradhiRain::Droplet& droplet : rain.activeDroplets) {
				addQuad(droplet.position, cameraRight * half, cameraDown * half, 1.f, std::min(droplet.age / DROPLET_LIFE, 1.f));
			}
			const std::uint32_t count = static_cast<std::uint32_t>(vertices.size()) - first;
			if (count > 0) {
				batches.push_back({ first, count, { glm::vec4(rain.colour, rain.lightGlint), { rain.rippleStrength, rain.splashStrength, 0.f, 0.f } } });
			}
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
			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(NuradhiRainPushConstants), &batch.push);
			vkCmdDraw(frameInfo.commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
		}
	}
}
