#pragma once

#include "engine/render/vulkan/lve_buffer.hpp"
#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <memory>
#include <vector>

namespace nrd {

	//Draws the bolt of every Lightning that is flashing: a ribbon along each segment with a white-hot core and a soft
	//halo, alpha blended without writing depth (so the stage hides it). Built on the CPU each frame into a per-frame
	//vertex buffer.
	class LightningRenderSystem {
	public:
		static constexpr std::uint32_t MAX_QUADS = 1200;

		LightningRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~LightningRenderSystem();

		LightningRenderSystem(const LightningRenderSystem&) = delete;
		LightningRenderSystem& operator=(const LightningRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo);

		struct Vertex {
			glm::vec3 position;
			glm::vec2 uv;
			glm::vec4 data; //x the core's share of the ribbon's half width; y brightness
		};

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);

		lve::LveDevice& lveDevice;
		VkPipelineLayout pipelineLayout;
		std::unique_ptr<lve::LvePipeline> pipeline;
		std::vector<std::unique_ptr<lve::LveBuffer>> vertexBuffers; //one per frame in flight, kept mapped
		std::vector<Vertex> vertices; //scratch, reused every frame
	};
}
