#pragma once

#include "engine/core/components.hpp"
#include "engine/render/render_components.hpp"
#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"
#include "engine/render/vulkan/nrd_debugLinePipeline.hpp"

//std
#include <memory>

namespace nrd {

	//Draws every MeshRenderer and Sprite entity. The same pass runs filled, or as a wireframe for the debug overlay.
	class MeshRenderSystem {
	public:
		MeshRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~MeshRenderSystem();

		MeshRenderSystem(const MeshRenderSystem&) = delete;
		MeshRenderSystem& operator=(const MeshRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo, bool wireframe);

		//layout shared by both pipelines; the debug overlay draws with it too
		VkPipelineLayout pipelineLayout() const { return layout; }

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipelines(VkRenderPass renderPass);
		void bindTexture(lve::FrameInfo& frameInfo, TextureHandle texture);
		void pushConstants(lve::FrameInfo& frameInfo, const Transform& transform, const glm::vec4& uvRect);

		lve::LveDevice& lveDevice;
		VkPipelineLayout layout;
		std::unique_ptr<lve::LvePipeline> filledPipeline;
		std::unique_ptr<NrdDebugLinePipeline> wireframePipeline;
	};
}
