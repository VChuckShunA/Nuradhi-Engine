#pragma once

#include "engine/ecs/registry.hpp"
#include "engine/render/vulkan/lve_buffer.hpp"
#include "engine/render/vulkan/lve_descriptors.hpp"
#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"

//std
#include <array>
#include <memory>
#include <unordered_map>

namespace nrd {

	//Draws every enabled FogVolume as a stack of horizontal slices, from the ground up, alpha blended without
	//writing depth. The slices are generated in the vertex shader; the fog's look is all in the fragment shader.
	//Interactive fog also gets its simulation as a small 3D texture, uploaded each frame by prepare().
	class FogRenderSystem {
	public:
		FogRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~FogRenderSystem();

		FogRenderSystem(const FogRenderSystem&) = delete;
		FogRenderSystem& operator=(const FogRenderSystem&) = delete;

		//records the simulation uploads; call before the render pass begins (copies can't run inside one)
		void prepare(lve::FrameInfo& frameInfo);
		void render(lve::FrameInfo& frameInfo);

	private:
		static constexpr int FRAMES = lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT;
		static constexpr std::uint32_t MAX_INTERACTIVE_VOLUMES = 8;

		//a 3D texture of fog state, with the buffer it's uploaded from and the descriptor set that samples it
		struct VolumeTexture {
			VkImage image = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkImageView view = VK_NULL_HANDLE;
			std::unique_ptr<lve::LveBuffer> staging;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		};

		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);
		VolumeTexture createVolumeTexture(int width, int height, int depth);
		void destroyVolumeTexture(VolumeTexture& texture);
		static void recordUpload(VkCommandBuffer commandBuffer, VolumeTexture& texture, int width, int height, int depth);

		lve::LveDevice& lveDevice;
		VkPipelineLayout pipelineLayout;
		std::unique_ptr<lve::LvePipeline> pipeline;

		std::unique_ptr<lve::LveDescriptorSetLayout> simulationSetLayout;
		std::unique_ptr<lve::LveDescriptorPool> descriptorPool;
		VkSampler sampler = VK_NULL_HANDLE;
		VolumeTexture undisturbed; //1 x 1 x 1: full density, no displacement, for fog that isn't interactive
		std::unordered_map<ecs::Entity, std::array<VolumeTexture, FRAMES>> volumes; //per interactive fog entity, per frame in flight
	};
}
