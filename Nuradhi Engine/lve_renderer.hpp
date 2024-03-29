#pragma once

#include "lve_device.hpp"
#include "lve_swap_chain.hpp"
#include "lve_window.hpp"
#include "lve_frame_info.hpp"
#include <memory>
#include <vector>
#include <cassert>

namespace lve {
	class LveRenderer {
	public:

		LveRenderer(LveWindow& lveWindow,LveDevice& lveDevice);
		~LveRenderer();

		//Delete these cuz we're using a pointer to our GLFW window
		LveRenderer(const LveRenderer&) = delete;
		LveRenderer& operator=(const LveRenderer&) = delete;

		VkRenderPass getSwapChainRenderPass()const { return lveSwapChain->getRenderPass(); }
		float getAspectRatio() const { return lveSwapChain->extentAspectRatio(); }
		bool isFrameInProgress()const { return isFrameStarted; }

		VkCommandBuffer getCurrentCommandBuffer() const { 
			assert(isFrameStarted && "Cannot get command buffer when from not in progress");
			return commandBuffers[currentFrameIndex]; }

		int getFrameIndex() const {
			assert(isFrameStarted && "Cannot get frame index when from not in progress");
			return currentFrameIndex; }

		//splitting the draw frame function
		VkCommandBuffer beginFrame();
		void endFrame();

		void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
		void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
		void bindPipelineEssentials(VkCommandBuffer commandBuffer, lve::FrameInfo frameInfo);

	private:
		void createCommandBuffers();
		void freeCommandBuffers();
		void recreateSwapChain();
		
		LveWindow& lveWindow;
		LveDevice& lveDevice;
		std::unique_ptr<LveSwapChain> lveSwapChain;
		std::vector<VkCommandBuffer> commandBuffers;

		uint32_t currentImageIndex;
		int currentFrameIndex;
		bool isFrameStarted;
	};
}