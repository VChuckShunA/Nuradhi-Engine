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

	//Draws Nuradhi Rain's ground effects: ripples as quads lying flat on the floor, splash droplets as small
	//camera-facing quads, alpha blended without writing depth. The drops themselves are particles, drawn by the
	//ParticleRenderSystem. The quads are built on the CPU each frame into a per-frame vertex buffer.
	class NuradhiRainRenderSystem {
	public:
		static constexpr std::uint32_t MAX_QUADS = 6000; //ripples and droplets per frame, across all rain

		NuradhiRainRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~NuradhiRainRenderSystem();

		NuradhiRainRenderSystem(const NuradhiRainRenderSystem&) = delete;
		NuradhiRainRenderSystem& operator=(const NuradhiRainRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo);

		struct Vertex {
			glm::vec3 position;
			glm::vec2 uv;
			glm::vec2 data; //x 0 for a ripple, 1 for a droplet; y how far through its life (0..1)
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
