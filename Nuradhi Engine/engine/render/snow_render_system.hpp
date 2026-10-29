#pragma once

#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"

//std
#include <memory>

namespace nrd {

	//Draws every enabled SnowVolume as upright, camera-facing layers from the farthest to the nearest, alpha
	//blended without writing depth. The layers are generated in the vertex shader; the flakes and mist are
	//procedural, in the fragment shader.
	class SnowRenderSystem {
	public:
		SnowRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~SnowRenderSystem();

		SnowRenderSystem(const SnowRenderSystem&) = delete;
		SnowRenderSystem& operator=(const SnowRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo);

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);

		lve::LveDevice& lveDevice;
		VkPipelineLayout pipelineLayout;
		std::unique_ptr<lve::LvePipeline> pipeline;
	};
}
