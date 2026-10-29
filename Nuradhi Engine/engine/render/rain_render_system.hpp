#pragma once

#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"

//std
#include <memory>

namespace nrd {

	//Draws every enabled RainVolume: in realistic mode first the ripples on the ground, then upright, camera-facing
	//layers from the farthest to the nearest, alpha blended without writing depth. The quads are generated in the
	//vertex shader; the drops, splashes, ripples and mist are procedural, in the fragment shader.
	class RainRenderSystem {
	public:
		RainRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~RainRenderSystem();

		RainRenderSystem(const RainRenderSystem&) = delete;
		RainRenderSystem& operator=(const RainRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo);

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);

		lve::LveDevice& lveDevice;
		VkPipelineLayout pipelineLayout;
		std::unique_ptr<lve::LvePipeline> pipeline;
	};
}
