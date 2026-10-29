#pragma once

#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"

//std
#include <memory>

namespace nrd {

	//Draws every enabled FogVolume as a stack of horizontal slices, from the ground up, alpha blended without
	//writing depth. The slices are generated in the vertex shader; the fog's look is all in the fragment shader.
	class FogRenderSystem {
	public:
		FogRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~FogRenderSystem();

		FogRenderSystem(const FogRenderSystem&) = delete;
		FogRenderSystem& operator=(const FogRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo);

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);

		lve::LveDevice& lveDevice;
		VkPipelineLayout pipelineLayout;
		std::unique_ptr<lve::LvePipeline> pipeline;
	};
}
