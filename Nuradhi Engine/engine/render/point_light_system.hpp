#pragma once

#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"

//std
#include <memory>

namespace nrd {

	//Feeds every PointLight entity to the shaders' light list and draws each one as a glowing billboard
	class PointLightSystem {
	public:
		PointLightSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~PointLightSystem();

		PointLightSystem(const PointLightSystem&) = delete;
		PointLightSystem& operator=(const PointLightSystem&) = delete;

		void update(lve::FrameInfo& frameInfo, lve::GlobalUbo& ubo);
		void render(lve::FrameInfo& frameInfo);

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);

		lve::LveDevice& lveDevice;
		std::unique_ptr<lve::LvePipeline> lvePipeline;
		VkPipelineLayout pipelineLayout;
	};
}
