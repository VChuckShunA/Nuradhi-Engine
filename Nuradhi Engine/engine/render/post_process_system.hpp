#pragma once

#include "engine/render/post_process_settings.hpp"
#include "engine/render/vulkan/lve_descriptors.hpp"
#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"

//std
#include <array>
#include <memory>

namespace nrd {

	//Full-screen filters. While one is on, the scene is drawn into an offscreen colour and depth image (one pair
	//per frame in flight) instead of the swapchain; then, inside the swapchain's render pass, a full-screen
	//triangle samples it through the filters. The offscreen render pass has the swapchain's formats, so every
	//existing pipeline can draw into it unchanged.
	class PostProcessSystem {
	public:
		PostProcessSystem(lve::LveDevice& device, VkRenderPass swapchainRenderPass, VkFormat colorFormat, VkFormat depthFormat);
		~PostProcessSystem();

		PostProcessSystem(const PostProcessSystem&) = delete;
		PostProcessSystem& operator=(const PostProcessSystem&) = delete;

		static bool anyEnabled(const PostProcessSettings& settings) { return settings.noir.enabled || settings.crt.enabled; }

		//(re)creates the offscreen images when the window size changed; call before the frame's render passes
		void ensureTargets(VkExtent2D extent);
		void beginScenePass(VkCommandBuffer commandBuffer, int frameIndex);
		void endScenePass(VkCommandBuffer commandBuffer, int frameIndex);
		//draws the filtered scene; call inside the swapchain's render pass
		void draw(VkCommandBuffer commandBuffer, int frameIndex, const PostProcessSettings& settings, float time);

	private:
		static constexpr int FRAMES = lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT;

		struct Target {
			VkImage colorImage = VK_NULL_HANDLE;
			VkDeviceMemory colorMemory = VK_NULL_HANDLE;
			VkImageView colorView = VK_NULL_HANDLE;
			VkImage depthImage = VK_NULL_HANDLE;
			VkDeviceMemory depthMemory = VK_NULL_HANDLE;
			VkImageView depthView = VK_NULL_HANDLE;
			VkFramebuffer framebuffer = VK_NULL_HANDLE;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		};

		void createRenderPass();
		void createPipeline(VkRenderPass swapchainRenderPass);
		void createTargets();
		void destroyTargets();

		lve::LveDevice& lveDevice;
		VkFormat colorFormat;
		VkFormat depthFormat;
		VkExtent2D extent{ 0, 0 };

		VkRenderPass sceneRenderPass = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		std::unique_ptr<lve::LveDescriptorSetLayout> setLayout;
		std::unique_ptr<lve::LveDescriptorPool> descriptorPool;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		std::unique_ptr<lve::LvePipeline> pipeline;
		std::array<Target, FRAMES> targets;
	};
}
