#include "lve_renderer.hpp"
#include <stdexcept>
#include <cassert>
#include <array>
namespace lve {
	
	LveRenderer::LveRenderer(LveWindow& window, LveDevice& device) : 
		lveWindow{window}, lveDevice{device}
	{
		recreateSwapChain();
		createCommandBuffers();
	}
	LveRenderer::~LveRenderer()
	{
		freeCommandBuffers();
	}
	
	
	
	
	VkCommandBuffer LveRenderer::beginFrame()
	{
		assert(!isFrameStarted && "Can't call begin Frame while already in progress");
		
		
		//get index of the next image to render
		auto result = lveSwapChain->acquireNextImage(&currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return nullptr; //indicate that the frame has not successfully started
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		isFrameStarted = true;

		auto commandBuffer = getCurrentCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recrding command Buffers!");
		}
		
		return commandBuffer;
	}

	void LveRenderer::endFrame()
	{
		assert(isFrameStarted && "Can't call endframe while frame is not in progress");
		auto commandBuffer = getCurrentCommandBuffer();

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to Record command Buffer!");
		}


		//submit provided swapchain to GPU
		auto result = lveSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
			lveWindow.wasWindowResized()) {
			lveWindow.resetWindowResizedFlag();
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to present Swap Chain Image!");
		}

		isFrameStarted = false;
		currentFrameIndex = (currentFrameIndex + 1) % LveSwapChain::MAX_FRAMES_IN_FLIGHT;


	}

	void LveRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted && "Can't call beginSwapChainRenderPass while frame is not in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");

		//Begin Retnder pass
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = lveSwapChain->getRenderPass();
		renderPassInfo.framebuffer = lveSwapChain->getFrameBuffer(currentImageIndex);

		renderPassInfo.renderArea.offset = { 0,0 };
		renderPassInfo.renderArea.extent = lveSwapChain->getSwapChainExtent();

		//set the clear values
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.01f, 0.01f,0.01f,1.0f };
		clearValues[1].depthStencil = { 1.0f,0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		//Record to command buffer
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		

	}

	void LveRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted && "Can't call endSwapChainRenderPass while frame is not in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't endSwapChainRenderPass on command buffer from a different frame");


		//finish recording
		vkCmdEndRenderPass(commandBuffer);
	}

	void LveRenderer::bindPipelineEssentials(VkCommandBuffer commandBuffer, lve::FrameInfo frameInfo)
	{
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(lveSwapChain->getSwapChainExtent().width);
		viewport.height = static_cast<float>(lveSwapChain->getSwapChainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0,0},lveSwapChain->getSwapChainExtent() };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void LveRenderer::createCommandBuffers()
	{
		commandBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);//resize the command buffer's vector to be the swap chain's image count(likely 2 o 3 depending on device)

		VkCommandBufferAllocateInfo allocInfo{};//allocate command buffer
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = lveDevice.getCommandPool();
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command Buffers!");
		}
	}
	void LveRenderer::freeCommandBuffers()
	{
		vkFreeCommandBuffers(
			lveDevice.device(),
			lveDevice.getCommandPool(),
			static_cast<float>(commandBuffers.size()),
			commandBuffers.data());
		commandBuffers.clear();
	}
	
	void LveRenderer::recreateSwapChain()
	{
		auto extent = lveWindow.getExtent();
		while (extent.width == 0 || extent.height == 0) {
			extent = lveWindow.getExtent();
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(lveDevice.device());

		if (lveSwapChain == nullptr)
		{
			//lveSwapChain = nullptr; //This line should later be removed in the tutorial once we provide the old swap chain to the new one during creation.
			lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent);
		}
		else
		{
			std::shared_ptr<LveSwapChain> oldSwapChain = std::move(lveSwapChain);
			lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, oldSwapChain);

			if (!oldSwapChain->compareSwapFormats(*lveSwapChain.get())) {
				throw std::runtime_error("Swap chain image(or depth) format has changed");
			}
			
		}

		//TO DO: if render passes are compatible, do nothing else
		//we'll come back to this
	}
	
	
}