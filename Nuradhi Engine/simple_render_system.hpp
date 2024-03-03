#pragma once

#include "lve_camera.hpp"
#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_pipeline.hpp"
#include "lve_frame_info.hpp"
#include "nrd_debugLinePipeline.hpp"

#include <memory>
#include <vector>


namespace lve {
	class SimpleRenderSystem {
	public:
		SimpleRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~SimpleRenderSystem();

		[[nodiscard]] nrd::NrdDebugLinePipeline* getDebugPipelinePtr() const { return nrdDebugLinePipeline.get(); }
		[[nodiscard]] lve::LvePipeline* getlvePipelinePtr() const { return lvePipeline.get(); }

		//Delete these cuz we're using a pointer to our GLFW window
		SimpleRenderSystem(const SimpleRenderSystem&) = delete;
		SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

		void bindDescriptorSets(FrameInfo& frameInfo);

		void renderGameObects(
			FrameInfo &frameInfo);
	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);

		LveDevice& lveDevice;

		std::unique_ptr<nrd::NrdDebugLinePipeline> nrdDebugLinePipeline;
		std::unique_ptr<LvePipeline> lvePipeline;
		VkPipelineLayout pipelineLayout;
	};
}