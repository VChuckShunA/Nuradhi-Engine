#include "engine/render/mesh_render_system.hpp"
#include "engine/render/render_components.hpp"
#include "engine/sprite/sprite.hpp"

//std
#include <cassert>
#include <optional>
#include <stdexcept>
#include <vector>

namespace nrd {

	MeshRenderSystem::MeshRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: lveDevice{ device }
	{
		createPipelineLayout(globalSetLayout);
		createPipelines(renderPass);
	}

	MeshRenderSystem::~MeshRenderSystem()
	{
		vkDestroyPipelineLayout(lveDevice.device(), layout, nullptr);
	}

	void MeshRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(lve::SimplePushConstantData);

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ globalSetLayout };

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Pipeline layout");
		}
	}

	void MeshRenderSystem::createPipelines(VkRenderPass renderPass)
	{
		assert(layout != nullptr && "Cannot create pipeline before pipeline layout");

		lve::PipelineConfigInfo pipelineConfig = {};
		lve::LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = layout;
		filledPipeline = std::make_unique<lve::LvePipeline>(
			lveDevice,
			"shaders/simple_shader.vert.spv",
			"shaders/simple_shader.frag.spv",
			pipelineConfig);

		NrdDebugLinePipelineConfigInfo wireframeConfig = {};
		NrdDebugLinePipeline::defaultPipelineConfigInfo(wireframeConfig);
		wireframeConfig.renderPass = renderPass;
		wireframeConfig.rasterizationInfo.lineWidth = 5;
		wireframeConfig.pipelineLayout = layout;
		wireframePipeline = std::make_unique<NrdDebugLinePipeline>(lveDevice, "shaders/green_shader.vert.spv", "shaders/green_shader.frag.spv", wireframeConfig);
	}

	void MeshRenderSystem::bindTexture(lve::FrameInfo& frameInfo, TextureHandle texture)
	{
		//each texture has its own global descriptor set, so bind the one this entity samples from
		assert(texture.id < frameInfo.textureDescriptorSets.size() && "Unknown texture handle");
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			layout,
			0,
			1,
			&frameInfo.textureDescriptorSets[texture.id],
			0,
			nullptr);
	}

	void MeshRenderSystem::pushConstants(lve::FrameInfo& frameInfo, const Transform& transform, const glm::vec4& uvRect)
	{
		lve::SimplePushConstantData push{};
		push.modelMatrix = transform.mat4();
		push.normalMatrix = transform.normalMatrix();
		//the shaders only use the normal matrix's 3x3 part, so its last column carries the texture rectangle.
		//That keeps the push constants at 128 bytes, the size every Vulkan device guarantees.
		push.normalMatrix[3] = uvRect;
		vkCmdPushConstants(
			frameInfo.commandBuffer,
			layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(lve::SimplePushConstantData),
			&push);
	}

	void MeshRenderSystem::render(lve::FrameInfo& frameInfo, bool wireframe)
	{
		if (wireframe) wireframePipeline->bind(frameInfo.commandBuffer);
		else filledPipeline->bind(frameInfo.commandBuffer);

		frameInfo.registry.each<Transform, MeshRenderer>([&](ecs::Entity, Transform& transform, MeshRenderer& meshRenderer) {
			if (!meshRenderer.mesh.valid()) return;
			lve::LveModel& model = *frameInfo.meshes[meshRenderer.mesh.id];
			bindTexture(frameInfo, meshRenderer.texture);
			pushConstants(frameInfo, transform, meshRenderer.uvRect);
			model.bind(frameInfo.commandBuffer);
			model.draw(frameInfo.commandBuffer);
		});

		lve::LveModel& spriteQuad = *frameInfo.meshes[frameInfo.spriteQuadMesh];
		frameInfo.registry.each<Transform, Sprite>([&](ecs::Entity entity, Transform& transform, Sprite& sprite) {
			const Collider* collider = frameInfo.registry.tryGet<Collider>(entity);
			const std::optional<Aabb> body = collider ? std::optional<Aabb>{ collider->worldBounds(transform.position) } : std::nullopt;
			bindTexture(frameInfo, sprite.texture);
			pushConstants(frameInfo, sprite.frameTransform(transform.position, body ? &*body : nullptr), sprite.frameUvRect());
			spriteQuad.bind(frameInfo.commandBuffer);
			spriteQuad.draw(frameInfo.commandBuffer);
		});
	}
}
